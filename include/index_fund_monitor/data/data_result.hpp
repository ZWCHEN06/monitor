#ifndef INDEX_FUND_MONITOR_DATA_DATA_RESULT_HPP
#define INDEX_FUND_MONITOR_DATA_DATA_RESULT_HPP

#include <string>

struct ProviderError {
    std::string message;
    bool is_network_error = false;
    bool is_format_error = false;
};

#endif