#ifndef PLAYERBOARD_HPP
#define PLAYERBOARD_HPP

#include <Message.hpp>

#include "TabletopBoard.hpp"

class PlayerBoard final : public TabletopBoard {
public:
    explicit PlayerBoard(const IOConfig& config, TimestampT startupGeneration);
    ~PlayerBoard() override;
    void setup() override;
    void loop() override;

private:
    struct Impl;
    Impl* impl;
};


#endif  // PLAYERBOARD_HPP
