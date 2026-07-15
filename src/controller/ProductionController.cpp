#include "ProductionController.h"

ProductionController::ProductionController(ProductionQueue& queue, IView& view) : queue_(queue), view_(view) {}

void ProductionController::showStatus() {
    queue_.advance();
    view_.showProductionStatus(queue_.activeProduction(), queue_.waitingQueue());
}
