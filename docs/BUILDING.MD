# Build Baseline

## C++20

EDP-Persistence-ESP-IDF requires official C++20.

This requirement applies to the concrete Persistence providers, their selected EDP-Memory ByteOperations provider, and consuming applications.

## ESP-IDF

Pure ESP-IDF consumers compile the providers as C++20. The supplied PlatformIO ESP-IDF demo uses:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = espidf

build_flags =
    -std=gnu++20
```

A native ESP-IDF CMake application should equivalently require C++20 for the component target.

## Arduino-facing builds: prefer PIOArduino

The Arduino IDE / Arduino-ESP32 demonstration surfaces use the ESP-IDF providers from an Arduino runtime environment.

For PlatformIO-compatible Arduino builds, use the maintained PIOArduino platform package:

```ini
[env:esp32dev]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32dev
framework = arduino

build_unflags =
    -std=gnu++11

build_flags =
    -std=gnu++20
```

Developers who retain the conventional `platformio` command may still select that PIOArduino platform package explicitly. Do not rely on an unpinned/default `platform = espressif32` Arduino environment and assume it supplies the required C++20-capable Arduino-ESP32 baseline.

## EDP dependencies

Consumers must provide:

- EDP-Persistence-ESP-IDF;
- EDP-Persistence;
- EDP-Memory;
- EDP-Platform;
- a concrete ByteOperations provider such as EDP-Platform-Portable;
- EDP-System.

The application may substitute another valid EDP-Memory ByteOperations provider for EDP-Platform-Portable.
