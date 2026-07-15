#include "OrderController.h"

OrderController::OrderController(OrderRepository& repository, IView& view)
    : repository_(repository), view_(view) {}

void OrderController::reserveOrder(const std::string& sampleId, const std::string& customerName, int quantity) {
    const auto result = repository_.reserve(sampleId, customerName, quantity);
    if (result.success && result.order.has_value()) {
        view_.showMessage("주문 접수 완료: " + result.order->orderNumber);
    } else {
        view_.showError(result.errorMessage);
    }
}

void OrderController::listOrders() {
    view_.showOrders(repository_.list());
}
