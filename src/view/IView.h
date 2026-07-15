#pragma once

#include <string>

class IView {
public:
    virtual ~IView() = default;
    virtual void showMessage(const std::string& message) = 0;
    virtual void showError(const std::string& message) = 0;
};
