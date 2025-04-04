#ifndef LIGHT_HPP
#define LIGHT_HPP

#include <cstdint>

class Light {

public:
    virtual ~Light() = default;
    virtual void setBrightness(uint8_t brightness) = 0;
};

#endif //LIGHT_HPP
