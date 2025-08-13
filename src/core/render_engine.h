#pragma once

#include "common.h"

class RenderEngine {
public:
    RenderEngine();
    ~RenderEngine();
    
    void initialize();
    void render();
    void shutdown();
    
private:
    bool initialized_;
};