#ifndef INDEX_FUND_MONITOR_DB_BENCHMARK_REPOSITORY_HPP
#define INDEX_FUND_MONITOR_DB_BENCHMARK_REPOSITORY_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class Database;

struct BenchmarkRecord {
    int64_t id = 0;
    std::string fund_symbol;
    std::string benchmark_code;
    std::string benchmark_name;
    std::string provider;
    std::string effective_date;
    std::string source;
};

class BenchmarkRepository {
public:
    explicit BenchmarkRepository(Database& db);

    bool set(const std::string& fund_symbol,
             const std::string& benchmark_code,
             const std::string& benchmark_name,
             const std::string& provider,
             const std::string& source);

    std::optional<BenchmarkRecord> get_current(const std::string& fund_symbol) const;
    std::vector<BenchmarkRecord> get_history(const std::string& fund_symbol) const;
    bool exists(const std::string& fund_symbol) const;

private:
    Database& db_;
};

#endif