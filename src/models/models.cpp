#include "index_fund_monitor/models/models.hpp"

#include <cctype>
#include <stdexcept>

std::string_view to_string(Market m) {
    switch (m) {
        case Market::CN: return "CN";
    }
    return "UNKNOWN";
}

std::string_view to_string(Exchange e) {
    switch (e) {
        case Exchange::SSE:  return "SSE";
        case Exchange::SZSE: return "SZSE";
    }
    return "UNKNOWN";
}

std::optional<Market> market_from_string(std::string_view s) {
    if (s == "CN") return Market::CN;
    return std::nullopt;
}

std::optional<Exchange> exchange_from_string(std::string_view s) {
    if (s == "SSE")  return Exchange::SSE;
    if (s == "SZSE") return Exchange::SZSE;
    return std::nullopt;
}