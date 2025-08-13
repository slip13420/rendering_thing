#pragma once

#include "common.h"

class UIInput {
public:
    UIInput();
    ~UIInput();
    
    void processEvents();
    bool shouldQuit() const;
    
private:
    bool quit_requested_;
};