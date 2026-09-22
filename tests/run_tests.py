#!/usr/bin/env python3
"""Compile the concrete ESP-IDF Persistence provider contract."""

from pathlib import Path
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import tempfile


def sibling(root, *names):
    """Find a sibling repository by one of the supplied directory names."""
    for name in names:
        candidate = root.parent / name

        if candidate.is_dir():
            return candidate.resolve()

    return None


def existing_directory(value):
    """Resolve an explicitly supplied directory when it exists."""
    if not value:
        return None

    path = Path(value).expanduser()

    if path.is_dir():
        return path.resolve()

    return None


def first_existing(*paths):
    """Return the first existing regular file or directory."""
    for path in paths:
        if path is not None and path.exists():
            return path.resolve()

    return None


def idf_root_from_idf_py(idf_py):
    """Recover an ESP-IDF installation root from tools/idf.py."""
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


def discover_idf_root(
    explicit_path=None,
    explicit_idf_py=None,
    platformio_home=None,
):
    """Discover either a standalone or PlatformIO-owned ESP-IDF tree."""
    candidates = []

    if explicit_path:
        candidates.append(Path(explicit_path).expanduser())

    environment_path = os.environ.get("IDF_PATH")

    if environment_path:
        candidates.append(Path(environment_path).expanduser())

    explicit_root = idf_root_from_idf_py(explicit_idf_py)

    if explicit_root:
        candidates.append(explicit_root)

    path_root = idf_root_from_idf_py(shutil.which("idf.py"))

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

    for versioned_root in (
        home / "esp",
        home / ".espressif",
    ):
        if versioned_root.is_dir():
            candidates.extend(versioned_root.glob("*/esp-idf"))

    candidates.append(
        (platformio_home or (home / ".platformio"))
        / "packages"
        / "framework-espidf"
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
    """Report whether ESP-IDF is owned by this PlatformIO package tree."""
    try:
        return idf_root.resolve() == (
            platformio_home / "packages" / "framework-espidf"
        ).resolve()
    except OSError:
        return False


def discover_xtensa_compiler(platformio_home, explicit_compiler=None):
    """Find the installed ESP32 Xtensa C++ compiler."""
    if explicit_compiler:
        candidate = Path(explicit_compiler).expanduser()

        if candidate.is_file():
            return candidate.resolve()

    packages = platformio_home / "packages"

    return first_existing(
        packages
        / "toolchain-xtensa-esp-elf"
        / "bin"
        / "xtensa-esp32-elf-g++",
        packages
        / "toolchain-xtensa-esp-elf"
        / "bin"
        / "xtensa-esp-elf-g++",
        packages
        / "toolchain-xtensa-esp32"
        / "bin"
        / "xtensa-esp32-elf-g++",
    )


def idf_public_include_directories(idf_root):
    """Return only the ESP-IDF public headers consumed by this provider.

    The direct compile probe must not add every ESP-IDF component include
    directory. Host-only compatibility components such as components/linux
    intentionally provide headers named sys/cdefs.h and would shadow the
    Xtensa/Newlib target headers when placed on a raw compiler command line.
    """
    components = idf_root / "components"
    required = (
        components / "nvs_flash" / "include",
        components / "esp_common" / "include",
    )

    missing = [
        path
        for path in required
        if not path.is_dir()
    ]

    if missing:
        return []

    return [
        path.resolve()
        for path in required
    ]


def write_compile_probe_sdkconfig(config_directory):
    """Write the minimal target configuration required by public IDF headers."""
    config_directory.mkdir(parents=True, exist_ok=True)
    (config_directory / "sdkconfig.h").write_text(
        """#pragma once

#define CONFIG_IDF_TARGET_ESP32 1
#define CONFIG_IDF_TARGET "esp32"
#define CONFIG_COMPILER_OPTIMIZATION_PERF 0
#define CONFIG_COMPILER_STATIC_ANALYZER 0
"""
    )


def compile_platformio_framework(
    root,
    persistence,
    memory,
    platform,
    platform_portable,
    system,
    idf_root,
    platformio_home,
    compiler,
    build,
    verbose,
):
    """Compile the contract directly against PlatformIO's installed ESP-IDF."""
    include_directories = idf_public_include_directories(idf_root)

    if not include_directories:
        print(
            "ERROR: required ESP-IDF public include directories were not found "
            "(expected components/nvs_flash/include and "
            "components/esp_common/include).",
            file=sys.stderr,
        )
        return 2

    config_directory = build / "config"
    write_compile_probe_sdkconfig(config_directory)

    object_file = build / "ContractCompile.o"
    command = [
        str(compiler),
        "-std=gnu++20",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        "-DESP_PLATFORM",
        "-DIDF_TARGET_ESP32",
        "-c",
        str(root / "tests" / "ContractCompile.cpp"),
        "-o",
        str(object_file),
        "-I",
        str(config_directory),
        "-I",
        str(root / "src"),
        "-I",
        str(persistence / "src"),
        "-I",
        str(memory / "src"),
        "-I",
        str(platform / "src"),
        "-I",
        str(platform_portable / "src"),
        "-I",
        str(system / "src"),
    ]

    for include in include_directories:
        command.extend(("-isystem", str(include)))

    print(f"ESP-IDF environment: PlatformIO package, direct compiler")
    print(f"Compiler: {compiler}")
    print(
        "ESP-IDF public include roots: "
        + ", ".join(str(path) for path in include_directories)
    )

    if verbose:
        print(" ".join(shlex.quote(value) for value in command))

    result = subprocess.run(
        command,
        check=False,
    )

    if result.returncode != 0:
        print(
            "\nFAIL: ESP-IDF concrete contract did not compile.",
            file=sys.stderr,
        )

    return result.returncode


def compile_standalone_idf(
    root,
    persistence,
    memory,
    platform,
    platform_portable,
    system,
    idf_root,
    build,
    verbose,
):
    """Compile the contract through a normal standalone ESP-IDF installation."""
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
    "%s"
    "%s"
    "%s"
)
"""
        % (
            root / "src",
            persistence / "src",
            memory / "src",
            platform / "src",
            platform_portable / "src",
            system / "src",
        )
    )

    idf_py = idf_root / "tools" / "idf.py"
    export_script = idf_root / "export.sh"

    print("ESP-IDF environment: standalone")
    print(f"ESP-IDF idf.py: {idf_py}")

    if export_script.is_file():
        command = (
            f"source {shlex.quote(str(export_script))} >/dev/null "
            f"&& idf.py -C {shlex.quote(str(build))} build"
        )

        if verbose:
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

        if verbose:
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


def print_missing_dependency(name, detail):
    """Print one missing compile prerequisite."""
    print(f"  - {name}: {detail}", file=sys.stderr)


def main():
    """Run the concrete ESP-IDF compile-time contract probe."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--idf-path")
    parser.add_argument("--idf-py")
    parser.add_argument("--compiler")
    parser.add_argument("--host-compiler")
    parser.add_argument("--platformio-home")
    parser.add_argument("--persistence")
    parser.add_argument("--memory")
    parser.add_argument("--platform")
    parser.add_argument("--platform-portable")
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
    memory = (
        existing_directory(args.memory)
        if args.memory
        else sibling(root, "EDP-Memory")
    )
    platform = (
        existing_directory(args.platform)
        if args.platform
        else sibling(root, "EDP-Platform")
    )
    platform_portable = (
        existing_directory(args.platform_portable)
        if args.platform_portable
        else sibling(root, "EDP-Platform-Portable")
    )
    system = (
        existing_directory(args.system)
        if args.system
        else sibling(root, "EDP-System", "ESPressio-System")
    )
    idf_root = discover_idf_root(
        explicit_path=args.idf_path,
        explicit_idf_py=args.idf_py,
        platformio_home=platformio_home,
    )
    platformio_framework = (
        idf_root is not None
        and is_platformio_framework(idf_root, platformio_home)
    )
    compiler = (
        discover_xtensa_compiler(platformio_home, args.compiler)
        if platformio_framework
        else None
    )
    host_compiler_value = (
        args.host_compiler
        or shutil.which("c++")
        or shutil.which("clang++")
        or shutil.which("g++")
    )
    host_compiler = (
        Path(host_compiler_value).expanduser().resolve()
        if host_compiler_value
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

    if memory is None:
        missing.append(
            (
                "EDP-Memory",
                "no sibling checkout found; pass --memory /path/to/EDP-Memory",
            )
        )

    if platform is None:
        missing.append(
            (
                "EDP-Platform",
                "no sibling checkout found; pass --platform /path/to/EDP-Platform",
            )
        )

    if platform_portable is None:
        missing.append(
            (
                "EDP-Platform-Portable",
                "no sibling checkout found; pass --platform-portable /path/to/EDP-Platform-Portable",
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
                "no installation containing tools/idf.py was found; "
                "activate ESP-IDF, set IDF_PATH, or pass --idf-path /path/to/esp-idf",
            )
        )

    if platformio_framework and compiler is None:
        missing.append(
            (
                "Xtensa ESP32 C++ compiler",
                "PlatformIO-owned framework-espidf was found, but no compatible "
                "toolchain compiler was found under the PlatformIO package root; "
                "pass --compiler /path/to/compiler",
            )
        )

    if host_compiler is None:
        missing.append(
            (
                "host C++ compiler",
                "no c++, clang++, or g++ executable was found; "
                "pass --host-compiler /path/to/compiler",
            )
        )

    if missing:
        print("ERROR: missing required compile dependencies:", file=sys.stderr)

        for name, detail in missing:
            print_missing_dependency(name, detail)

        return 2

    build = Path(tempfile.mkdtemp(prefix="edp-persistence-idf-tests-"))

    try:
        print(f"ESP-IDF root: {idf_root}")
        print(f"Host compiler: {host_compiler}")
        print(f"EDP-Persistence-ESP-IDF: {root}")
        print(f"EDP-Persistence: {persistence}")
        print(f"EDP-Memory: {memory}")
        print(f"EDP-Platform: {platform}")
        print(f"EDP-Platform-Portable: {platform_portable}")
        print(f"EDP-System: {system}")
        print(f"Build directory: {build}")

        behavior_executable = build / "ProviderBehaviorTests"
        behavior_command = [
            str(host_compiler),
            "-std=c++20",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Werror",
            "-I",
            str(root / "tests" / "support" / "esp_idf"),
            "-I",
            str(root / "src"),
            "-I",
            str(persistence / "src"),
            "-I",
            str(memory / "src"),
            "-I",
            str(platform / "src"),
            "-I",
            str(platform_portable / "src"),
            "-I",
            str(system / "src"),
            str(root / "tests" / "ProviderBehaviorTests.cpp"),
            "-o",
            str(behavior_executable),
        ]

        print("\n[1/4] Compiling and running concrete provider behavior tests...")

        if args.verbose:
            print(" ".join(shlex.quote(value) for value in behavior_command))

        result = subprocess.run(
            behavior_command,
            check=False,
        )

        if result.returncode != 0:
            print(
                "\nFAIL: ESP-IDF concrete provider behavior tests did not compile.",
                file=sys.stderr,
            )
            return result.returncode

        result = subprocess.run(
            [str(behavior_executable)],
            check=False,
        )

        if result.returncode != 0:
            print(
                "\nFAIL: ESP-IDF concrete provider behavior tests failed.",
                file=sys.stderr,
            )
            return result.returncode

        demo_sources = (
            (
                "[2/4] Compiling PlatformIO Arduino demo source...",
                root / "demos" / "ConcreteProviders" / "PlatformIO_Arduino" / "src" / "main.cpp",
                build / "DemoPlatformIOArduino.o",
            ),
            (
                "[3/4] Compiling PlatformIO ESP-IDF demo source...",
                root / "demos" / "ConcreteProviders" / "PlatformIO_ESP-IDF" / "src" / "main.cpp",
                build / "DemoPlatformIOEspIdf.o",
            ),
        )

        for label, demo_source, demo_object in demo_sources:
            print(f"\n{label}")
            demo_command = [
                str(host_compiler),
                "-std=c++20",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Werror",
                "-I",
                str(root / "tests" / "support" / "esp_idf"),
                "-I",
                str(root / "src"),
                "-I",
                str(persistence / "src"),
                "-I",
                str(system / "src"),
                "-c",
                str(demo_source),
                "-o",
                str(demo_object),
            ]

            if args.verbose:
                print(" ".join(shlex.quote(value) for value in demo_command))

            result = subprocess.run(
                demo_command,
                check=False,
            )

            if result.returncode != 0:
                print(
                    "\nFAIL: ESP-IDF concrete provider demo source did not compile.",
                    file=sys.stderr,
                )
                return result.returncode

        print("\n[4/4] Compiling ESP-IDF concrete contract...")

        if platformio_framework:
            result = compile_platformio_framework(
                root,
                persistence,
                memory,
                platform,
                platform_portable,
                system,
                idf_root,
                platformio_home,
                compiler,
                build,
                args.verbose,
            )
        else:
            result = compile_standalone_idf(
                root,
                persistence,
                memory,
                platform,
                platform_portable,
                system,
                idf_root,
                build,
                args.verbose,
            )

        if result != 0:
            return result

        print(
            "\nPASS: ESP-IDF concrete provider behavior tests and demo source "
            "validation passed, and the providers compiled against the real SDK contract."
        )
        return 0
    finally:
        if args.keep_build:
            print(f"Build retained: {build}")
        else:
            shutil.rmtree(build, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
