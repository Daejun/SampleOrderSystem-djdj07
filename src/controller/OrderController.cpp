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

void OrderController::approveOrder(const std::string& orderNumber) {
    const auto result = repository_.approve(orderNumber);
    if (result.success && result.order.has_value()) {
        if (result.order->status == OrderStatus::PRODUCING) {
            view_.showMessage("주문 승인 완료 (재고 부족, 생산 대기): " + orderNumber);
        } else {
            view_.showMessage("주문 승인 완료: " + orderNumber);
        }
    } else {
        view_.showError(result.errorMessage);
    }
}

void OrderController::rejectOrder(const std::string& orderNumber) {
    const auto result = repository_.reject(orderNumber);
    if (result.success) {
        view_.showMessage("주문 거절 완료: " + orderNumber);
    } else {
        view_.showError(result.errorMessage);
    }
}

void OrderController::listReservedOrders() {
    view_.showOrders(repository_.listByStatus(OrderStatus::RESERVED));
}
