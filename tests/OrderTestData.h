#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <tuple>

#include <nlohmann/json.hpp>

// dummygen으로 주문(Order) 접수 테스트 데이터 중 고객명/수량 필드를 생성하기 위한 스키마와 변환 헬퍼.
// sampleId는 사전에 등록된 Sample을 참조해야 하므로 스키마에는 포함하지 않고 테스트에서 별도로 지정한다.
namespace testdata {

// SampleRepository::registerSample()의 quantity > 0 도메인 규칙을 반영한 스키마.
inline nlohmann::ordered_json orderRequestSchema() {
    return nlohmann::ordered_json::parse(R"({
        "type": "object",
        "required": ["customerName", "quantity"],
        "properties": {
            "customerName": { "type": "string" },
            "quantity": { "type": "integer", "minimum": 1 }
        }
    })");
}

struct OrderRequest {
    std::string customerName;
    int quantity = 0;
};

inline std::optional<OrderRequest> toOrderRequest(const nlohmann::ordered_json& json) {
    try {
        OrderRequest request;
        request.customerName = json.at("customerName").get<std::string>();
        request.quantity = json.at("quantity").get<int>();
        return request;
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

// 2026-04-16 09:00:00 UTC로 고정된 시계. OrderRepository::Clock으로 주입해 주문번호
// 생성(ORD-YYYYMMDD-NNNN)을 결정론적으로 검증할 수 있게 한다.
// Howard Hinnant의 days_from_civil 알고리즘을 사용해 플랫폼별 CRT 함수(mktime 등)에 의존하지 않는다.
inline std::chrono::system_clock::time_point fixedClock() {
    constexpr int year = 2026;
    constexpr int month = 4;
    constexpr int day = 16;
    constexpr int hour = 9;

    int y = year;
    const int m = month;
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const long long days = era * 146097LL + static_cast<long long>(doe) - 719468LL;
    const long long seconds = days * 86400LL + hour * 3600LL;

    return std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
}

} // namespace testdata
