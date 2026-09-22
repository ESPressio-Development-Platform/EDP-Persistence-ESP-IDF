#pragma once


class SDStub final {
public:

    [[nodiscard]] bool begin() noexcept {
        return true;
    }

};


inline SDStub SD;
