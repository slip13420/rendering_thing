#pragma once

#include "common.h"
#include "primitives.h"

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