#pragma once

#include "core/common.h"

class ImageOutput {
public:
    ImageOutput();
    ~ImageOutput();
    
    void saveToFile(const std::string& filename);
    void displayToScreen();
    
private:
    
};