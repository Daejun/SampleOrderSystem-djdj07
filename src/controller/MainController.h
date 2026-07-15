#pragma once

#include <string>

#include "SampleController.h"
#include "../view/IView.h"

class MainController {
public:
    MainController(IView& view, SampleController& sampleController);

    void handleSelection(const std::string& input);
    bool isExitRequested() const { return exitRequested_; }

private:
    void runSampleMenu();

    IView& view_;
    SampleController& sampleController_;
    bool exitRequested_ = false;
};
