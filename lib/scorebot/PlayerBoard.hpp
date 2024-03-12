#ifndef PLAYERBOARD_HPP
#define PLAYERBOARD_HPP

#include "TabletopBoard.hpp"

class PlayerBoard final : public TabletopBoard {
public:
    explicit PlayerBoard(const IOConfig& config);
    ~PlayerBoard() override;
    void setup() override;
    void loop() override;

private:
    struct Impl;
    Impl* impl;
};


#endif  // PLAYERBOARD_HPP
