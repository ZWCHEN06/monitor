#ifndef INDEX_FUND_MONITOR_UPDATE_UPDATER_HPP
#define INDEX_FUND_MONITOR_UPDATE_UPDATER_HPP

#include <optional>
#include <string>
#include <vector>

class Database;

struct UpdateSummary {
    int success = 0;
    int skipped = 0;
    int failed = 0;
    std::vector<std::pair<std::string, std::string>> failures;

    void print() const;
};

class Updater {
public:
    Updater(Database& db, std::string fixture_dir);
    UpdateSummary update_all();
    UpdateSummary update_symbol(const std::string& symbol);

private:
    Database& db_;
    std::string fixture_dir_;

    struct BenchmarkMapping {
        std::string code;
        std::string name;
        std::string provider;
    };

    std::optional<BenchmarkMapping> find_benchmark(const std::string& symbol);
    UpdateSummary update_one(const std::string& symbol);
};

#endif