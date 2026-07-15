#include "OrderInput.h"

#include <exception>

namespace orderinput {

bool parsePositiveQuantity(const std::string& raw, int& quantity) {
    try {
        std::size_t pos = 0;
        const int value = std::stoi(raw, &pos);
        if (pos != raw.size() || value <= 0) {
            return false;
        }
        quantity = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace orderinput
