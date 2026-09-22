#pragma once

#include <cstdint>


class SerialStub final {
public:

    void begin(std::uint32_t BaudRate) noexcept {
        static_cast<void>(BaudRate);
    }

    void println(const char* Text) noexcept {
        static_cast<void>(Text);
    }

};


inline SerialStub Serial;


inline void delay(std::uint32_t Milliseconds) noexcept {
    static_cast<void>(Milliseconds);
}
