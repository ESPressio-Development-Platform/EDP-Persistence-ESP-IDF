#!/usr/bin/env python3
"""True GPIO17 VDD cut test for immutable LittleFS generation files.

Protects every generation observed ACKed before the first cut request.
Existing file bytes are NEVER overwritten; a partially written candidate
after reset is allowed, but a vanished earlier acknowledged file is not.
"""
import pathlib
import re
import serial
import subprocess
import sys
import time

CONTROL="/srv/espressio/bin/esp32-integrated"

def command(value):
    r=subprocess.run([CONTROL,value],check=True,capture_output=True,text=True,timeout=12)
    print(f"HOST|{value}|{r.stdout.strip()}",flush=True)

def run(destination):
    root=pathlib.Path(destination)
    root.mkdir(parents=True,exist_ok=False)
    ack=0
    cut=False
    boot=False
    recovered=0
    started=time.monotonic()
    command("status")
    command("power-on")
    port=serial.Serial("/dev/serial0",115200,timeout=0.1)
    pending=bytearray()
    try:
        port.reset_input_buffer()
        with (root/"serial.raw").open("wb") as raw,(root/"serial.txt").open("w",encoding="utf8") as log:
            command("release")
            while time.monotonic()-started<110:
                buf=port.read(4096)
                if not buf:continue
                raw.write(buf);raw.flush()
                for octet in buf:
                    if octet!=10 and len(pending)<4095:
                        pending.append(octet);continue
                    line=pending.decode("utf8",errors="replace").strip()
                    pending.clear()
                    log.write(line+"\n");log.flush()
                    if "EDP_IMM|" not in line:continue
                    line=line[line.index("EDP_IMM|"):]
                    print("UART|"+line,flush=True)
                    if cut and "|BOOT|reset=1" in line:boot=True
                    m=re.search(r"\|ACK\|gen=(\d+)",line)
                    if m and not cut:ack=max(ack,int(m.group(1)))
                    m=re.search(r"\|WRITE_ENTER\|gen=(\d+)",line)
                    if m and not cut and int(m.group(1))>=5:
                        if ack<2:raise RuntimeError("expected 2 durable baseline ACKs")
                        print(f"HOST|CUT_AT={m.group(1)}|LAST_ACK={ack}",flush=True)
                        cut=True
                        command("power-off")
                        t=time.monotonic()
                        time.sleep(2)
                        command("status")
                        command("power-on")
                        print(f"HOST|POWER_OFF_S={time.monotonic()-t:.3f}",flush=True)
                    m=re.search(r"\|RECOVERED\|last=(\d+)\|first=1\|contiguous=1",line)
                    if cut and m:recovered=int(m.group(1))
                    if cut and "|PASS|IMMUTABLE_FILE_HISTORY_SURVIVED" in line:
                        okay=boot and recovered>=ack
                        print(f"HOST|TRUE_POWERON={boot}|LAST_ACK_BEFORE_CUT={ack}|LAST_CONTIGUOUS_RECOVERED={recovered}|PASS={okay}",flush=True)
                        return 0 if okay else 4
                    if "|HALT|" in line:
                        print("HOST|FIRMWARE_HALTED=FAIL",flush=True)
                        return 5
            print(f"HOST|TIMEOUT|cut={cut}|lastAck={ack}|recovered={recovered}",flush=True)
            return 6
    finally:
        port.close()
        command("power-off")

if __name__=="__main__":
    try:
        if len(sys.argv)!=2:raise ValueError("private unique evidence path required")
        sys.exit(run(sys.argv[1]))
    except Exception as e:
        print("HOST|ERROR",repr(e),flush=True)
        sys.exit(9)
