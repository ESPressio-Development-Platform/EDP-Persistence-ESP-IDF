#!/usr/bin/env python3
from pathlib import Path
import argparse, shutil, subprocess, sys, tempfile

def sibling(root, *names):
    for name in names:
        p=root.parent/name
        if p.is_dir(): return p.resolve()
    return None

def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--idf-py"); ap.add_argument("--persistence"); ap.add_argument("--system"); ap.add_argument("--keep-build",action="store_true"); a=ap.parse_args()
    root=Path(__file__).resolve().parents[1]
    persistence=Path(a.persistence).resolve() if a.persistence else sibling(root,"EDP-Persistence")
    system=Path(a.system).resolve() if a.system else sibling(root,"EDP-System","ESPressio-System")
    idf=a.idf_py or shutil.which("idf.py")
    if not idf or not persistence or not system:
        print("ERROR: require active ESP-IDF (idf.py) plus sibling EDP-Persistence and EDP-System (or pass explicit paths).",file=sys.stderr); return 2
    build=Path(tempfile.mkdtemp(prefix="edp-persistence-idf-tests-"))
    try:
        main_dir=build/"main"; main_dir.mkdir(); shutil.copy2(root/"tests"/"ContractCompile.cpp",main_dir/"ContractCompile.cpp")
        (build/"CMakeLists.txt").write_text('cmake_minimum_required(VERSION 3.16)\ninclude($ENV{IDF_PATH}/tools/cmake/project.cmake)\nproject(edp_persistence_contract)\n')
        (main_dir/"CMakeLists.txt").write_text("""idf_component_register(SRCS "ContractCompile.cpp" INCLUDE_DIRS "." REQUIRES nvs_flash)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_include_directories(${COMPONENT_LIB} PRIVATE
    "%s"
    "%s"
    "%s"
)
""" % (root/"src",persistence/"src",system/"src"))
        print(f"ESP-IDF: {idf}\nEDP-Persistence-ESP-IDF: {root}\nEDP-Persistence: {persistence}\nEDP-System: {system}\nBuild directory: {build}\n\n[1/1] Compiling ESP-IDF concrete contract...")
        rc=subprocess.run([idf,"-C",str(build),"build"],check=False).returncode
        print("\nPASS: ESP-IDF concrete Persistence providers compiled and satisfied the abstract contract." if rc==0 else "\nFAIL: ESP-IDF concrete contract did not compile.")
        return rc
    finally:
        if a.keep_build: print(f"Build retained: {build}")
        else: shutil.rmtree(build,ignore_errors=True)
if __name__=="__main__": raise SystemExit(main())
