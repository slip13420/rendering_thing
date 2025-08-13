#pragma once

#include "core/common.h"

class UIManager {
public:
    UIManager();
    ~UIManager();
    
    void initialize();
    void update();
    void render();
    void shutdown();
    
private:
    bool initialized_;
};