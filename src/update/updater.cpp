#include "index_fund_monitor/update/updater.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <sqlite3.h>

#include "index_fund_monitor/data/mock_fund_distribution_provider.hpp"
#include "index_fund_monitor/data/mock_index_valuation_provider.hpp"
#include "index_fund_monitor/data/mock_price_provider.hpp"
#include "index_fund_monitor/db/database.hpp"
#include "index_fund_monitor/db/fund_repository.hpp"

namespace {

std::string now_iso() {
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::gmtime(&t));
    return buf;
}

void bind_text(sqlite3_stmt* stmt, int idx, const std::string& val) {
    sqlite3_bind_text(stmt, idx, val.c_str(), -1, SQLITE_TRANSIENT);
}

void bind_nullable_text(sqlite3_stmt* stmt, int idx, const std::string& val) {
    if (val.empty()) {
        sqlite3_bind_null(stmt, idx);
    } else {
        sqlite3_bind_text(stmt, idx, val.c_str(), -1, SQLITE_TRANSIENT);
    }
}

void bind_nullable_double(sqlite3_stmt* stmt, int idx, const std::optional<double>& val) {
    if (val.has_value()) {
        sqlite3_bind_double(stmt, idx, val.value());
    } else {
        sqlite3_bind_null(stmt, idx);
    }
}

bool write_valuations(Database& db, const std::vector<IndexValuation>& valuations) {
    sqlite3* handle = db.handle();
    if (!handle) return false;

    const char* sql = R"(
        INSERT OR IGNORE INTO index_valuation
            (benchmark_code, valuation_date, pe_ttm, pb, dividend_yield, source, collected_at)
        VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)
    )";

    for (const auto& v : valuations) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        bind_text(stmt, 1, v.benchmark_code);
        bind_text(stmt, 2, v.valuation_date);
        bind_nullable_double(stmt, 3, v.pe_ttm);
        bind_nullable_double(stmt, 4, v.pb);
        bind_nullable_double(stmt, 5, v.dividend_yield);
        bind_text(stmt, 6, v.source);
        bind_text(stmt, 7, v.collected_at);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            return false;
        }
    }
    return true;
}

bool write_prices(Database& db, const std::vector<DailyPrice>& prices) {
    sqlite3* handle = db.handle();
    if (!handle) return false;

    const char* sql = R"(
        INSERT OR IGNORE INTO daily_price
            (symbol, trade_date, open_price, close_price, high_price, low_price, volume, amount, source, collected_at)
        VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)
    )";

    for (const auto& p : prices) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        bind_text(stmt, 1, p.symbol);
        bind_text(stmt, 2, p.trade_date);
        bind_nullable_double(stmt, 3, p.open_price);
        bind_nullable_double(stmt, 4, p.close_price);
        bind_nullable_double(stmt, 5, p.high_price);
        bind_nullable_double(stmt, 6, p.low_price);
        bind_nullable_double(stmt, 7, p.volume);
        bind_nullable_double(stmt, 8, p.amount);
        bind_text(stmt, 9, p.source);
        bind_text(stmt, 10, p.collected_at);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            return false;
        }
    }
    return true;
}

bool write_distributions(Database& db, const std::vector<FundDistribution>& dists) {
    sqlite3* handle = db.handle();
    if (!handle) return false;

    for (const auto& d : dists) {
        // Remove existing entries for same symbol+source to maintain idempotency
        const char* del_sql = "DELETE FROM fund_distribution WHERE symbol = ?1 AND source = ?2";
        sqlite3_stmt* del_stmt = nullptr;
        if (sqlite3_prepare_v2(handle, del_sql, -1, &del_stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        bind_text(del_stmt, 1, d.symbol);
        bind_text(del_stmt, 2, d.source);
        sqlite3_step(del_stmt);
        sqlite3_finalize(del_stmt);

        const char* ins_sql = R"(
            INSERT INTO fund_distribution
                (symbol, amount_per_share, ex_date, record_date, pay_date, currency, source)
            VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)
        )";
        sqlite3_stmt* ins_stmt = nullptr;
        if (sqlite3_prepare_v2(handle, ins_sql, -1, &ins_stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        bind_text(ins_stmt, 1, d.symbol);
        bind_nullable_double(ins_stmt, 2, d.amount_per_share);
        bind_nullable_text(ins_stmt, 3, d.ex_date);
        bind_nullable_text(ins_stmt, 4, d.record_date);
        bind_nullable_text(ins_stmt, 5, d.pay_date);
        bind_text(ins_stmt, 6, d.currency);
        bind_text(ins_stmt, 7, d.source);

        int rc = sqlite3_step(ins_stmt);
        sqlite3_finalize(ins_stmt);
        if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            return false;
        }
    }
    return true;
}

void log_collection(Database& db, const std::string& symbol, const std::string& source,
                    const std::string& status, int items, const std::string& error) {
    sqlite3* handle = db.handle();
    if (!handle) return;

    std::string started = now_iso();
    std::string completed = now_iso();

    const char* sql = R"(
        INSERT INTO collection_log (symbol, source, status, items_collected, error_message, started_at, completed_at)
        VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    bind_nullable_text(stmt, 1, symbol);
    bind_text(stmt, 2, source);
    bind_text(stmt, 3, status);
    sqlite3_bind_int(stmt, 4, items);
    bind_nullable_text(stmt, 5, error);
    bind_text(stmt, 6, started);
    bind_text(stmt, 7, completed);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

} // namespace

void UpdateSummary::print() const {
    std::cout << "更新完成\n"
              << "成功：" << success << "\n"
              << "跳过：" << skipped << "\n"
              << "失败：" << failed << std::endl;

    if (!failures.empty()) {
        std::cout << "\n失败：" << std::endl;
        for (const auto& f : failures) {
            std::cout << f.first << "：" << f.second << std::endl;
        }
    }
}

Updater::Updater(Database& db, std::string fixture_dir)
    : db_(db), fixture_dir_(std::move(fixture_dir)) {}

std::optional<Updater::BenchmarkMapping> Updater::find_benchmark(const std::string& symbol) {
    sqlite3* handle = db_.handle();
    if (!handle) return std::nullopt;

    const char* sql = R"(
        SELECT benchmark_code, benchmark_name, provider
        FROM fund_benchmark
        WHERE fund_symbol = ?1 AND is_current = 1
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }
    bind_text(stmt, 1, symbol);

    BenchmarkMapping mapping;
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        mapping.code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (name) mapping.name = name;
        const char* prov = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (prov) mapping.provider = prov;
        found = true;
    }

    sqlite3_finalize(stmt);
    if (found) return mapping;
    return std::nullopt;
}

UpdateSummary Updater::update_one(const std::string& symbol) {
    UpdateSummary summary;

    // Check ETF exists
    auto exchange = deduce_exchange(symbol);
    if (!exchange.has_value()) {
        return summary;
    }

    FundRepository repo(db_);
    if (!repo.exists(symbol, exchange.value())) {
        return summary;
    }

    auto benchmark = find_benchmark(symbol);
    if (!benchmark.has_value()) {
        summary.skipped = 1;
        log_collection(db_, symbol, "mock", "skipped", 0, "无跟踪指数映射");
        return summary;
    }

    // Fetch valuations
    MockIndexValuationProvider val_provider(fixture_dir_);
    auto val_result = val_provider.fetch(benchmark->code);

    // Fetch prices
    MockPriceProvider price_provider(fixture_dir_);
    price_provider.set_fixture("price_success.json");
    auto price_result = price_provider.fetch(symbol);

    // Fetch distributions
    MockFundDistributionProvider dist_provider(fixture_dir_);
    auto dist_result = dist_provider.fetch(symbol);

    if (!val_result.success) {
        summary.failed = 1;
        summary.failures.emplace_back(symbol, val_result.error.message);
        log_collection(db_, symbol, "mock", "failed", 0, val_result.error.message);
        return summary;
    }

    // Write to DB in a single transaction
    if (!db_.execute("BEGIN")) {
        summary.failed = 1;
        summary.failures.emplace_back(symbol, "无法开始数据库事务");
        log_collection(db_, symbol, "mock", "failed", 0, "无法开始数据库事务");
        return summary;
    }

    bool write_ok = true;
    int total_items = 0;

    if (!val_result.valuations.empty()) {
        write_ok = write_ok && write_valuations(db_, val_result.valuations);
        total_items += static_cast<int>(val_result.valuations.size());
    }

    if (price_result.success && !price_result.prices.empty()) {
        write_ok = write_ok && write_prices(db_, price_result.prices);
        total_items += static_cast<int>(price_result.prices.size());
    }

    if (dist_result.success && !dist_result.distributions.empty()) {
        write_ok = write_ok && write_distributions(db_, dist_result.distributions);
        total_items += static_cast<int>(dist_result.distributions.size());
    }

    if (!write_ok) {
        db_.execute("ROLLBACK");
        summary.failed = 1;
        summary.failures.emplace_back(symbol, "写入数据库失败");
        log_collection(db_, symbol, "mock", "failed", 0, "写入数据库失败");
        return summary;
    }

    if (!db_.execute("COMMIT")) {
        db_.execute("ROLLBACK");
        summary.failed = 1;
        summary.failures.emplace_back(symbol, "提交事务失败");
        log_collection(db_, symbol, "mock", "failed", 0, "提交事务失败");
        return summary;
    }

    summary.success = 1;
    log_collection(db_, symbol, "mock", "success", total_items, "");
    return summary;
}

UpdateSummary Updater::update_all() {
    FundRepository repo(db_);
    auto etfs = repo.list();

    UpdateSummary total;
    for (const auto& etf : etfs) {
        auto result = update_one(etf.symbol);
        total.success += result.success;
        total.skipped += result.skipped;
        total.failed += result.failed;
        for (auto& f : result.failures) {
            total.failures.push_back(std::move(f));
        }
    }
    return total;
}

UpdateSummary Updater::update_symbol(const std::string& symbol) {
    return update_one(symbol);
}
