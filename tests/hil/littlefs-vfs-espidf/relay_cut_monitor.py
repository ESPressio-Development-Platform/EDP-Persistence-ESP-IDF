#!/usr/bin/env python3
"""RPI400 true VDD cut in the middle of an ESP-IDF LittleFS replace campaign.

Exact host ACK before first relay command must never be lost. A recovered
file is counted only when its full generation-specific payload is intact.
"""
import pathlib
import re
import serial
import subprocess
import sys
import time

CONTROL="/srv/espressio/bin/esp32-integrated"

def command(arg):
    p=subprocess.run([CONTROL,arg],check=True,text=True,capture_output=True,timeout=12)
    print(f"HOST|{arg}|{p.stdout.strip()}",flush=True)

def main(output):
    path=pathlib.Path(output)
    path.mkdir(parents=True,exist_ok=False)
    ack=0
    cut=False
    boot_after=False
    recovered=0
    started=time.monotonic()
    command("status")
    command("power-on")
    port=serial.Serial("/dev/serial0",115200,timeout=0.1)
    try:
        port.reset_input_buffer()
        with (path/"serial.raw").open("wb") as raw,(path/"serial.txt").open("w",encoding="utf8") as text:
            command("release")
            pending=bytearray()
            while time.monotonic()-started<100:
                chunk=port.read(4096)
                if not chunk:continue
                raw.write(chunk);raw.flush()
                for value in chunk:
                    if value!=10 and len(pending)<4096:
                        pending.append(value)
                        continue
                    line=pending.decode(errors="replace").strip()
                    pending.clear()
                    text.write(line+"\n");text.flush()
                    if "EDP_LFS|" not in line:continue
                    line=line[line.index("EDP_LFS|"):]
                    print("UART|"+line,flush=True)
                    if cut and "|BOOT|reset=1" in line:boot_after=True
                    m=re.search(r"\|ACK\|gen=(\d+)",line)
                    if m and not cut:ack=max(ack,int(m.group(1)))
                    m=re.search(r"\|WRITE_ENTER\|gen=(\d+)",line)
                    if m and not cut and int(m.group(1))>=4:
                        if ack<1:raise RuntimeError("no durable pre-cut ack")
                        print(f"HOST|CUT_AT={m.group(1)}|LAST_ACK={ack}",flush=True)
                        cut=True
                        command("power-off")
                        then=time.monotonic()
                        time.sleep(2)
                        command("status")
                        command("power-on")
                        print(f"HOST|POWER_OFF_SECONDS={time.monotonic()-then:.3f}",flush=True)
                    m=re.search(r"\|RECOVERY\|gen=(\d+)\|bytes=8192\|complete=1",line)
                    if cut and m:recovered=int(m.group(1))
                    if cut and "|PASS|ATOMIC_FILE_RECOVERY" in line:
                        ok=boot_after and recovered>=ack
                        print(f"HOST|SECOND_POWERON={boot_after}|ACK_BEFORE_CUT={ack}|RECOVERED={recovered}|ATOMIC_VALID={ok}",flush=True)
                        return 0 if ok else 4
                    if "|HALT|" in line:
                        print("HOST|DEVICE_HALT",flush=True)
                        return 5
            print("HOST|TIMEOUT",flush=True)
            return 6
    finally:
        port.close()
        command("power-off")

if __name__=="__main__":
    try:
        if len(sys.argv)!=2:raise ValueError("unique evidence dir required")
        sys.exit(main(sys.argv[1]))
    except Exception as exc:
        print(f"HOST|ERROR={exc!r}",flush=True)
        sys.exit(9)
