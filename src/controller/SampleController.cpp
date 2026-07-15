#include "SampleController.h"

SampleController::SampleController(SampleRepository& repository, IView& view)
    : repository_(repository), view_(view) {}

void SampleController::registerSample(const std::string& id, const std::string& name,
                                       double avgProductionTimeMinutes, double yield) {
    Sample sample{id, name, avgProductionTimeMinutes, yield, 0};
    const auto result = repository_.registerSample(sample);
    if (result.success) {
        view_.showMessage("시료 등록 완료: " + id);
    } else {
        view_.showError(result.errorMessage);
    }
}

void SampleController::listSamples() {
    view_.showSamples(repository_.list());
}

void SampleController::searchSamples(const std::string& keyword) {
    view_.showSamples(repository_.search(keyword));
}
