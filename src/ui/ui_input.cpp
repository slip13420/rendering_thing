#include "ui_input.h"

UIInput::UIInput() : quit_requested_(false) {
    
}

UIInput::~UIInput() {
    
}

void UIInput::processEvents() {
    
}

bool UIInput::shouldQuit() const {
    return quit_requested_;
}