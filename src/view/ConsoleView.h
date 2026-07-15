#pragma once

#include "IView.h"

class ConsoleView : public IView {
public:
    void showMessage(const std::string& message) override;
    void showError(const std::string& message) override;
};
