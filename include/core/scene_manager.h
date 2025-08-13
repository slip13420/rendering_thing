#pragma once

#include "core/common.h"
#include "core/primitives.h"

class SceneManager {
public:
    SceneManager();
    ~SceneManager();
    
    void initialize();
    void update();
    void shutdown();
    
private:
    bool initialized_;
};