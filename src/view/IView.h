#pragma once

#include <string>
#include <vector>

#include "../model/Sample.h"

class IView {
public:
    virtual ~IView() = default;
    virtual void showMessage(const std::string& message) = 0;
    virtual void showError(const std::string& message) = 0;
    virtual void showSamples(const std::vector<Sample>& samples) = 0;
    virtual void showSample(const Sample& sample) = 0;
};
