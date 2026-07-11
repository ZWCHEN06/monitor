#ifndef INDEX_FUND_MONITOR_DATA_IINDEX_VALUATION_PROVIDER_HPP
#define INDEX_FUND_MONITOR_DATA_IINDEX_VALUATION_PROVIDER_HPP

#include <string>
#include <vector>

#include "index_fund_monitor/data/data_result.hpp"
#include "index_fund_monitor/models/models.hpp"

struct IndexValuationResult {
    bool success = false;
    ProviderError error;
    std::vector<IndexValuation> valuations;
};

class IIndexValuationProvider {
public:
    virtual ~IIndexValuationProvider() = default;
    virtual IndexValuationResult fetch(const std::string& benchmark_code) = 0;
};

#endif