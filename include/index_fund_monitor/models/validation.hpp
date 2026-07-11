#ifndef INDEX_FUND_MONITOR_MODELS_VALIDATION_HPP
#define INDEX_FUND_MONITOR_MODELS_VALIDATION_HPP

#include <optional>
#include <string_view>

bool is_valid_etf_code(std::string_view code);
bool is_valid_market(std::string_view market);
bool is_valid_exchange(std::string_view exchange);
bool is_valid_percentage(std::optional<double> value);
bool is_valid_date_format(std::string_view date);

#endif