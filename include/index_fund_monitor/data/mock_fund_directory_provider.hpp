#ifndef INDEX_FUND_MONITOR_DATA_MOCK_FUND_DIRECTORY_PROVIDER_HPP
#define INDEX_FUND_MONITOR_DATA_MOCK_FUND_DIRECTORY_PROVIDER_HPP

#include <string>

#include "index_fund_monitor/data/ifund_directory_provider.hpp"

class MockFundDirectoryProvider : public IFundDirectoryProvider {
public:
    explicit MockFundDirectoryProvider(std::string fixture_dir);
    FundDirectoryResult fetch_all() override;

    void set_fixture(const std::string& filename);
    void set_simulate_network_error(bool v);
    void set_simulate_format_error(bool v);

private:
    std::string fixture_dir_;
    std::string fixture_file_ = "fund_directory_success.json";
    bool simulate_network_error_ = false;
    bool simulate_format_error_ = false;
};

#endif