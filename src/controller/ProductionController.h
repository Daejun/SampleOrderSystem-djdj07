#pragma once

#include "../model/ProductionQueue.h"
#include "../view/IView.h"

class ProductionController {
public:
    ProductionController(ProductionQueue& queue, IView& view);

    void showStatus();

private:
    ProductionQueue& queue_;
    IView& view_;
};
