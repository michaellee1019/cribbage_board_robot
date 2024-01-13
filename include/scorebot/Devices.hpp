#ifndef DEVICES_HPP
#define DEVICES_HPP

class IODevice {
public:
    virtual ~IODevice() = default;
    virtual void setup() = 0;
    virtual void loop() = 0;
};


#endif //DEVICES_HPP
