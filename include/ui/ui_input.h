#pragma once

#include "core/common.h"

class UIInput {
public:
    UIInput();
    ~UIInput();
    
    void processEvents();
    bool shouldQuit() const;
    
private:
    bool quit_requested_;
};