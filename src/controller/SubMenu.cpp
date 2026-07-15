#include "SubMenu.h"

#include <iostream>

namespace submenu {

void run(const std::function<void()>& printMenu, IView& view, const std::map<std::string, Handler>& handlers) {
    std::string line;
    while (true) {
        printMenu();
        if (!std::getline(std::cin, line)) {
            return;
        }

        if (line == "0") {
            return;
        }

        const auto it = handlers.find(line);
        if (it != handlers.end()) {
            it->second();
        } else {
            view.showError("알 수 없는 선택입니다: " + line);
        }
    }
}

} // namespace submenu
