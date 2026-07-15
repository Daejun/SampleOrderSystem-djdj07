#pragma once

#include <string>

#include "../view/IView.h"

class MainController {
public:
    explicit MainController(IView& view);

    void handleSelection(const std::string& input);
    bool isExitRequested() const { return exitRequested_; }

private:
    IView& view_;
    bool exitRequested_ = false;
};
