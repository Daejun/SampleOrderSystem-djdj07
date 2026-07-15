#include "ConsoleView.h"

#include <cstdio>

void ConsoleView::showMessage(const std::string& message) {
    std::printf("%s\n", message.c_str());
}

void ConsoleView::showError(const std::string& message) {
    std::printf("Error: %s\n", message.c_str());
}

void ConsoleView::showSamples(const std::vector<Sample>& samples) {
    if (samples.empty()) {
        std::printf("등록된 시료가 없습니다.\n");
        return;
    }
    for (const auto& sample : samples) {
        showSample(sample);
    }
}

void ConsoleView::showSample(const Sample& sample) {
    std::printf("[%s] %s (생산시간 %.2f min/ea, 수율 %.2f, 재고 %d ea)\n",
                sample.id.c_str(), sample.name.c_str(), sample.avgProductionTimeMinutes,
                sample.yield, sample.stock);
}
