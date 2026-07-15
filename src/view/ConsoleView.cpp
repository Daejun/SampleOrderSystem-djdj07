#include "ConsoleView.h"

#include <cstdio>

void ConsoleView::showMessage(const std::string& message) {
    std::printf("%s\n", message.c_str());
}

void ConsoleView::showError(const std::string& message) {
    std::printf("Error: %s\n", message.c_str());
}
