#include "OrderJson.h"

nlohmann::ordered_json orderToJson(const Order& o) {
    nlohmann::ordered_json j{
        {"orderNumber", o.orderNumber},
        {"sampleId", o.sampleId},
        {"customerName", o.customerName},
        {"quantity", o.quantity},
        {"shortageQuantity", o.shortageQuantity},
        {"productionQuantity", o.productionQuantity},
        {"status", orderStatusToString(o.status)}
    };
    j["productionStartedAtEpochSec"] =
        o.productionStartedAtEpochSec.has_value() ? nlohmann::ordered_json(*o.productionStartedAtEpochSec)
                                                   : nlohmann::ordered_json(nullptr);
    j["productionCompletesAtEpochSec"] =
        o.productionCompletesAtEpochSec.has_value() ? nlohmann::ordered_json(*o.productionCompletesAtEpochSec)
                                                     : nlohmann::ordered_json(nullptr);
    return j;
}

Order orderFromJson(const nlohmann::ordered_json& j) {
    Order o;
    o.orderNumber = j.at("orderNumber").get<std::string>();
    o.sampleId = j.at("sampleId").get<std::string>();
    o.customerName = j.at("customerName").get<std::string>();
    o.quantity = j.at("quantity").get<int>();
    // Phase 3~4까지 생성된 data.json에는 아래 필드가 없으므로 기본값으로 하위 호환.
    o.shortageQuantity = j.value("shortageQuantity", 0);
    o.productionQuantity = j.value("productionQuantity", 0);
    if (j.contains("productionStartedAtEpochSec") && !j.at("productionStartedAtEpochSec").is_null()) {
        o.productionStartedAtEpochSec = j.at("productionStartedAtEpochSec").get<std::int64_t>();
    }
    if (j.contains("productionCompletesAtEpochSec") && !j.at("productionCompletesAtEpochSec").is_null()) {
        o.productionCompletesAtEpochSec = j.at("productionCompletesAtEpochSec").get<std::int64_t>();
    }
    o.status = orderStatusFromString(j.at("status").get<std::string>());
    return o;
}
