# Concrete Providers Demo

This demo exercises both ESP-IDF concrete Persistence providers.

## Arduino targets

The Arduino IDE and PlatformIO Arduino variants use:

- Arduino `SD` to mount a removable FAT filesystem at `/sd`, then access it through `VfsFileStorage`;
- the Arduino runtime's initialized default NVS partition through `NvsKeyValueStorage`.

An SD card connected to the board's normal Arduino `SD` pins is required for the FileStorage portion.

## PlatformIO ESP-IDF target

The ESP-IDF variant is self-contained:

- a dedicated internal-flash FAT partition named `storage` is defined by `partitions.csv`;
- FATFS is mounted at `/edpfs` with wear levelling;
- NVS is initialized explicitly;
- both providers write, read and remove data;
- the key/value path also verifies a present zero-length value.

The VFS binding profiles deliberately use conservative `Restart` retention. The FAT paths are declared case-insensitive. The Arduino SD profile is removable; the internal-flash ESP-IDF profile is fixed.

All variants use C++20.
