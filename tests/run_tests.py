#!/usr/bin/env python3

from pathlib import Path
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import tempfile


def sibling(root, *names):
    for name in names:
        candidate = root.parent / name
        if candidate.is_dir():
            return candidate.resolve()

    return None


def existing_directory(value):
    if not value:
        return None

    path = Path(value).expanduser()

    if path.is_dir():
        return path.resolve()

    return None


def idf_root_from_idf_py(idf_py):
    if not idf_py:
        return None

    path = Path(idf_py).expanduser()

    try:
        path = path.resolve()
    except OSError:
        return None

    if path.name != "idf.py":
        return None

    root = path.parent.parent
    return root if (root / "tools" / "idf.py").is_file() else None


def discover_idf_root(explicit_path=None, explicit_idf_py=None):
    candidates = []

    if explicit_path:
        candidates.append(Path(explicit_path).expanduser())

    environment_path = os.environ.get("IDF_PATH")
    if environment_path:
        candidates.append(Path(environment_path).expanduser())

    explicit_root = idf_root_from_idf_py(explicit_idf_py)
    if explicit_root:
        candidates.append(explicit_root)

    path_idf_py = shutil.which("idf.py")
    path_root = idf_root_from_idf_py(path_idf_py)
    if path_root:
        candidates.append(path_root)

    home = Path.home()
    candidates.extend(
        (
            home / "esp" / "esp-idf",
            home / "esp-idf",
            home / "Projects" / "esp-idf",
            home / "Development" / "esp-idf",
            home / "Developer" / "esp-idf",
            home / "src" / "esp-idf",
            home / "Source" / "esp-idf",
            home / "DevProjects" / "esp-idf",
        )
    )

    versioned_roots = (
        home / "esp",
        home / ".espressif",
    )

    for versioned_root in versioned_roots:
        if not versioned_root.is_dir():
            continue

        candidates.extend(versioned_root.glob("*/esp-idf"))

    candidates.append(
        home / ".platformio" / "packages" / "framework-espidf"
    )

    seen = set()

    for candidate in candidates:
        try:
            resolved = candidate.resolve()
        except OSError:
            continue

        if resolved in seen:
            continue

        seen.add(resolved)

        if (resolved / "tools" / "idf.py").is_file():
            return resolved

    return None


def is_platformio_framework(idf_root, platformio_home):
    try:
        return idf_root.resolve() == (
            platformio_home / "packages" / "framework-espidf"
        ).resolve()
    except OSError:
        return False


def discover_platformio(explicit_platformio, platformio_home):
    if explicit_platformio:
        candidate = Path(explicit_platformio).expanduser()

        if candidate.is_file():
            return candidate.resolve()

    for command in ("platformio", "pio"):
        candidate = shutil.which(command)

        if candidate:
            return Path(candidate).resolve()

    for candidate in (
        platformio_home / "penv" / "bin" / "platformio",
        platformio_home / "penv" / "bin" / "pio",
    ):
        if candidate.is_file():
            return candidate.resolve()

    return None


def print_missing_dependency(name, detail):
    print(f"  - {name}: {detail}", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--idf-path")
    parser.add_argument("--idf-py")
    parser.add_argument("--platformio")
    parser.add_argument("--platformio-home")
    parser.add_argument("--persistence")
    parser.add_argument("--system")
    parser.add_argument("--keep-build", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    platformio_home = (
        Path(args.platformio_home).expanduser().resolve()
        if args.platformio_home
        else Path.home() / ".platformio"
    )
    persistence = (
        existing_directory(args.persistence)
        if args.persistence
        else sibling(root, "EDP-Persistence")
    )
    system = (
        existing_directory(args.system)
        if args.system
        else sibling(root, "EDP-System", "ESPressio-System")
    )
    idf_root = discover_idf_root(
        explicit_path=args.idf_path,
        explicit_idf_py=args.idf_py,
    )
    platformio_framework = (
        idf_root is not None
        and is_platformio_framework(idf_root, platformio_home)
    )
    platformio = (
        discover_platformio(args.platformio, platformio_home)
        if platformio_framework
        else None
    )

    missing = []

    if persistence is None:
        missing.append(
            (
                "EDP-Persistence",
                "no sibling checkout found; pass --persistence /path/to/EDP-Persistence",
            )
        )

    if system is None:
        missing.append(
            (
                "EDP-System",
                "no sibling checkout found; pass --system /path/to/EDP-System",
            )
        )

    if idf_root is None:
        missing.append(
            (
                "ESP-IDF",
                "no installation containing tools/idf.py was found; activate ESP-IDF, set IDF_PATH, or pass --idf-path /path/to/esp-idf",
            )
        )

    if platformio_framework and platformio is None:
        missing.append(
            (
                "PlatformIO",
                "framework-espidf was found under the PlatformIO package root, "
                "but neither platformio/pio on PATH nor the PlatformIO penv "
                "executable was found; pass --platformio /path/to/platformio",
            )
        )

    if missing:
        print("ERROR: missing required compile dependencies:", file=sys.stderr)

        for name, detail in missing:
            print_missing_dependency(name, detail)

        print(
            "\nESP-IDF discovery checks IDF_PATH, idf.py on PATH, "
            "~/esp/esp-idf, versioned ~/esp/*/esp-idf and "
            "~/.espressif/*/esp-idf installations, the PlatformIO "
            "framework-espidf package, and common development roots.",
            file=sys.stderr,
        )
        return 2

    idf_py = idf_root / "tools" / "idf.py"
    export_script = idf_root / "export.sh"

    build = Path(tempfile.mkdtemp(prefix="edp-persistence-idf-tests-"))

    try:
        main_dir = build / "main"
        main_dir.mkdir()
        shutil.copy2(
            root / "tests" / "ContractCompile.cpp",
            main_dir / "ContractCompile.cpp",
        )

        (build / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.16)\n"
            "include($ENV{IDF_PATH}/tools/cmake/project.cmake)\n"
            "project(edp_persistence_contract)\n"
        )

        (main_dir / "CMakeLists.txt").write_text(
            """idf_component_register(SRCS "ContractCompile.cpp" INCLUDE_DIRS "." REQUIRES nvs_flash)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_include_directories(${COMPONENT_LIB} PRIVATE
    "%s"
    "%s"
    "%s"
)
"""
            % (
                root / "src",
                persistence / "src",
                system / "src",
            )
        )

        if platformio_framework:
            (build / "platformio.ini").write_text(
                """[platformio]
src_dir = main

[env:esp32dev]
platform = espressif32
board = esp32dev
framework = espidf
"""
            )

        print(f"ESP-IDF root: {idf_root}")
        print(f"ESP-IDF idf.py: {idf_py}")

        if platformio_framework:
            print("ESP-IDF environment: PlatformIO")
            print(f"PlatformIO executable: {platformio}")
        else:
            print("ESP-IDF environment: standalone")
        print(f"EDP-Persistence-ESP-IDF: {root}")
        print(f"EDP-Persistence: {persistence}")
        print(f"EDP-System: {system}")
        print(f"Build directory: {build}")
        print("\n[1/1] Compiling ESP-IDF concrete contract...")

        if platformio_framework:
            command = [
                str(platformio),
                "run",
                "--project-dir",
                str(build),
                "--environment",
                "esp32dev",
            ]

            if args.verbose:
                print(" ".join(shlex.quote(value) for value in command))

            result = subprocess.run(
                command,
                check=False,
            )
        elif export_script.is_file():
            command = (
                f"source {shlex.quote(str(export_script))} >/dev/null "
                f"&& idf.py -C {shlex.quote(str(build))} build"
            )

            if args.verbose:
                print(f"bash -lc {shlex.quote(command)}")

            result = subprocess.run(
                ["bash", "-lc", command],
                check=False,
            )
        else:
            environment = os.environ.copy()
            environment["IDF_PATH"] = str(idf_root)
            command = [
                sys.executable,
                str(idf_py),
                "-C",
                str(build),
                "build",
            ]

            if args.verbose:
                print(" ".join(shlex.quote(value) for value in command))

            result = subprocess.run(
                command,
                check=False,
                env=environment,
            )

        if result.returncode != 0:
            print(
                "\nFAIL: ESP-IDF concrete contract did not compile.",
                file=sys.stderr,
            )
            return result.returncode

        print(
            "\nPASS: ESP-IDF concrete Persistence providers compiled "
            "and satisfied the abstract contract."
        )
        return 0
    finally:
        if args.keep_build:
            print(f"Build retained: {build}")
        else:
            shutil.rmtree(build, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
