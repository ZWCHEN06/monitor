#include "index_fund_monitor/models/validation.hpp"
#include "index_fund_monitor/models/models.hpp"

#include <cctype>
#include <algorithm>

bool is_valid_etf_code(std::string_view code) {
    if (code.size() != 6) return false;
    return std::all_of(code.begin(), code.end(), [](char c) {
        return std::isdigit(static_cast<unsigned char>(c));
    });
}

bool is_valid_market(std::string_view market) {
    return market_from_string(market).has_value();
}

bool is_valid_exchange(std::string_view exchange) {
    return exchange_from_string(exchange).has_value();
}

bool is_valid_percentage(std::optional<double> value) {
    if (!value.has_value()) return true;
    double v = value.value();
    return v >= 0.0 && v <= 1.0;
}

bool is_valid_date_format(std::string_view date) {
    if (date.size() != 10) return false;
    if (date[4] != '-' || date[7] != '-') return false;

    auto is_digit = [](char c) { return std::isdigit(static_cast<unsigned char>(c)); };
    if (!is_digit(date[0]) || !is_digit(date[1]) || !is_digit(date[2]) || !is_digit(date[3])) return false;
    if (!is_digit(date[5]) || !is_digit(date[6])) return false;
    if (!is_digit(date[8]) || !is_digit(date[9])) return false;

    return true;
}