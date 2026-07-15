#pragma once

#include <string>

#include "../model/SampleRepository.h"
#include "../view/IView.h"

class SampleController {
public:
    SampleController(SampleRepository& repository, IView& view);

    void registerSample(const std::string& id, const std::string& name,
                         double avgProductionTimeMinutes, double yield);
    void listSamples();
    void searchSamples(const std::string& keyword);

private:
    SampleRepository& repository_;
    IView& view_;
};
