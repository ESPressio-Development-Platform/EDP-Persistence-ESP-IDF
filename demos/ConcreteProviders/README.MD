# Concrete Providers Demo

This demo exercises both ESP-IDF concrete Persistence providers with an explicit EDP-Memory ByteOperations provider.

The demo selects `EDP-Platform-Portable::Memory::ByteOperationsProvider` as the standard C/C++ implementation of `EDP-Memory::ByteOperations`.

## Dependencies

Install or provide:

- EDP-Persistence-ESP-IDF;
- EDP-Persistence;
- EDP-Memory;
- EDP-Platform;
- EDP-Platform-Portable;
- EDP-System;
- ESP-IDF;
- Arduino-ESP32 for the Arduino-facing targets.

## Arduino targets

The Arduino IDE and PlatformIO Arduino variants use:

- Arduino `SD` to mount a removable FAT filesystem at `/sd`, then access it through `VfsFileStorage`;
- the Arduino runtime's initialized default NVS partition through `NvsKeyValueStorage`.

An SD card connected to the board's normal Arduino `SD` pins is required for the FileStorage portion.

The PlatformIO Arduino target uses the maintained PIOArduino platform package and C++20.

## PlatformIO ESP-IDF target

The pure ESP-IDF variant is self-contained:

- a dedicated internal-flash FAT partition named `storage` is defined by `partitions.csv`;
- FATFS is mounted at `/edpfs` with wear levelling;
- NVS is initialized explicitly;
- both providers write, read and remove data;
- the key/value path also verifies a present zero-length value.

The VFS binding profiles deliberately use conservative `Restart` retention. The FAT paths are declared case-insensitive. The Arduino SD profile is removable; the internal-flash ESP-IDF profile is fixed. Both FAT-backed demo bindings explicitly advertise `ConcurrentReads`, matching the documented thread-safe FATFS substrate; arbitrary VFS bindings remain `CallerSerialized` unless their profile opts into a stronger guarantee.

See [../../docs/BUILDING.MD](../../docs/BUILDING.MD) for the exact C++20 baseline and PIOArduino guidance for Arduino-facing builds.

## ISR boundary

The demo deliberately performs all Persistence work from ordinary execution context. These providers are not ISR-safe and must not be called from ISR context.
