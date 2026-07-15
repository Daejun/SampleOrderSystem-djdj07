#pragma once

#include <string>

#include "../model/OrderRepository.h"
#include "../view/IView.h"

class OrderController {
public:
    OrderController(OrderRepository& repository, IView& view);

    void reserveOrder(const std::string& sampleId, const std::string& customerName, int quantity);
    void listOrders();

private:
    OrderRepository& repository_;
    IView& view_;
};
