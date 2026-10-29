#pragma once

#include "../source/host.h"

class StubHost : public Host { 
    public:
    StubHost();      
    void stubInput(uint8_t kdown, uint8_t kheld);
    void stubMouse(int16_t x, int16_t y, uint8_t btn);
};