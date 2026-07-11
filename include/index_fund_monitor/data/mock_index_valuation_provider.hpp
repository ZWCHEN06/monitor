#ifndef INDEX_FUND_MONITOR_DATA_MOCK_INDEX_VALUATION_PROVIDER_HPP
#define INDEX_FUND_MONITOR_DATA_MOCK_INDEX_VALUATION_PROVIDER_HPP

#include <string>

#include "index_fund_monitor/data/iindex_valuation_provider.hpp"

class MockIndexValuationProvider : public IIndexValuationProvider {
public:
    explicit MockIndexValuationProvider(std::string fixture_dir);
    IndexValuationResult fetch(const std::string& benchmark_code) override;

    void set_fixture(const std::string& filename);
    void set_simulate_network_error(bool v);
    void set_simulate_format_error(bool v);

private:
    std::string fixture_dir_;
    std::string fixture_file_ = "index_valuation_success.json";
    bool simulate_network_error_ = false;
    bool simulate_format_error_ = false;
};

#endif