#pragma once

#include <string>

#include "OrderController.h"
#include "SampleController.h"
#include "../view/IView.h"

class MainController {
public:
    MainController(IView& view, SampleController& sampleController, OrderController& orderController);

    void handleSelection(const std::string& input);
    bool isExitRequested() const { return exitRequested_; }

private:
    void runSampleMenu();
    void runOrderMenu();
    void runApprovalMenu();

    IView& view_;
    SampleController& sampleController_;
    OrderController& orderController_;
    bool exitRequested_ = false;
};
