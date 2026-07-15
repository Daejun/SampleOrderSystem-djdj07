#pragma once

#include <nlohmann/json.hpp>

#include "Order.h"

// OrderRepository와 ProductionQueue가 공통으로 사용하는 Order <-> JSON 변환.
// 필드가 늘어날 때마다 두 곳에서 각각 (역)직렬화 로직을 중복 유지하는 것을 막기 위해 분리했다.
nlohmann::ordered_json orderToJson(const Order& order);
Order orderFromJson(const nlohmann::ordered_json& json);
