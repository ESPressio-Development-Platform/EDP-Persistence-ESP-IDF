#!/usr/bin/env python3
"""Audit real ESP-IDF LittleFS partition API I/O counters in a UART transcript.

Numbers are logical esp_partition_* API operations, not guaranteed
NOR-chip endurance, vendor-internal wear, or a certified flash lifetime.
"""
from __future__ import annotations
from pathlib import Path
import sys

PREFIX="EDP_WEAR|"
FIELDS=("gen","written","write_ops","erase_ops","erased_blocks","max_per_block",
        "write_err","erase_err","heap","min_heap")
RECORD_BYTES=8192

def parse_records(text: str) -> list[dict[str,int]]:
    rows=[]
    for line in text.splitlines():
        if PREFIX not in line:
            continue
        fragment=line.split(PREFIX,1)[1].strip()
        record={}
        for field in fragment.split("|"):
            if "=" not in field:continue
            key,value=field.split("=",1)
            if key in FIELDS:
                try:record[key]=int(value)
                except ValueError:raise ValueError("nonnumeric wear measurement") from None
        if not all(key in record for key in FIELDS):
            raise ValueError("incomplete native partition measurement")
        rows.append(record)
    return rows

def audit(rows: list[dict[str,int]]) -> dict[str,int|float]:
    if len(rows)<3:
        raise ValueError("not enough native measurement checkpoints")
    if rows[0]["gen"]!=2 or rows[-1]["gen"]<20:
        raise ValueError("missing baseline or 20-generation checkpoint")
    prior=rows[0]
    for row in rows:
        if row["write_err"] or row["erase_err"]:
            raise ValueError("native flash operation returned failure")
        if row["erased_blocks"]>144 or row["max_per_block"]>row["erase_ops"]:
            raise ValueError("erase distribution impossible")
        if row["heap"]<0 or row["min_heap"]<0:
            raise ValueError("invalid heap observation")
        for name in ("gen","written","write_ops","erase_ops","erased_blocks",
                     "max_per_block","write_err","erase_err"):
            if row[name]<prior[name]:
                raise ValueError("partition API counter went backwards")
        prior=row
    start,end=rows[0],rows[-1]
    generations=end["gen"]-start["gen"]
    logical_bytes=generations*RECORD_BYTES
    programmed=end["written"]-start["written"]
    erases=end["erase_ops"]-start["erase_ops"]
    if logical_bytes<=0 or programmed<logical_bytes or erases==0:
        raise ValueError("wrapper counters are missing or physically implausible")
    return {
        "from_generation":start["gen"],"to_generation":end["gen"],
        "logical_payload_bytes":logical_bytes,
        "partition_write_bytes":programmed,
        "partition_write_amplification":round(programmed/logical_bytes,4),
        "erase_calls":erases,
        "distinct_erased_blocks_total":end["erased_blocks"],
        "maximum_erase_calls_per_block":end["max_per_block"],
        "minimum_free_heap_bytes":min(r["min_heap"] for r in rows),
        "record_count":len(rows),
    }

def self_test() -> None:
    def row(n,w,e):
        return dict(gen=n,written=w,write_ops=w//128,erase_ops=e,
                    erased_blocks=min(e,144),max_per_block=1,
                    write_err=0,erase_err=0,heap=280000,min_heap=270000)
    sample=[row(2,18000,4),row(10,190000,24),row(20,400000,55)]
    assert audit(sample)["partition_write_amplification"]>1
    assert len(parse_records("UART|"+PREFIX+"|".join(
        f"{k}={v}" for k,v in sample[0].items())+"\n"))==1
    for corrupted in (sample[:1],
                      [sample[0],dict(sample[1],write_err=1),sample[2]],
                      [sample[0],dict(sample[1],written=3),sample[2]],
                      [sample[0],sample[1],dict(sample[2],erase_ops=23)]):
        try: audit(corrupted)
        except ValueError: pass
        else: raise AssertionError("invalid measurement accepted")
    print("IO_PROFILE_HOST_SELF_TEST=PASS")

def main() -> None:
    if len(sys.argv)==2 and sys.argv[1]=="--self-test":
        self_test()
        return
    if len(sys.argv)!=2:raise SystemExit("Usage: verify_io_metrics.py <UART or run stdout>")
    result=audit(parse_records(Path(sys.argv[1]).read_text(errors="replace")))
    for key,value in result.items():
        print(f"IO_PROFILE|{key}={value}")
    print("IO_PROFILE|AUDIT=PASS")

if __name__=="__main__":
    main()
