#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include <sqlite3.h>

#include "index_fund_monitor/db/database.hpp"
#include "index_fund_monitor/db/fund_repository.hpp"
#include "index_fund_monitor/db/schema.hpp"
#include "index_fund_monitor/models/models.hpp"
#include "index_fund_monitor/update/updater.hpp"

static int failures = 0;
static int tests = 0;

#define CHECK(cond, msg) do { \
    ++tests; \
    if (!(cond)) { \
        std::cerr << "FAIL [" << tests << "]: " << msg << std::endl; \
        ++failures; \
    } else { \
        std::cout << "OK   [" << tests << "]: " << msg << std::endl; \
    } \
} while(0)

#define CHECK_EQ(a, b, msg) CHECK((a) == (b), msg)

namespace {

std::string unique_db_path() {
    return std::filesystem::temp_directory_path().string()
        + "/test_upd_" + std::to_string(std::rand()) + ".db";
}

void insert_benchmark(Database& db, const std::string& fund_symbol,
                      const std::string& benchmark_code,
                      const std::string& benchmark_name,
                      const std::string& provider) {
    sqlite3* handle = db.handle();

    // Insert benchmark_index if not exists
    const char* ins_idx = R"(
        INSERT OR IGNORE INTO benchmark_index (code, name, provider)
        VALUES (?1, ?2, ?3)
    )";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(handle, ins_idx, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, benchmark_code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, benchmark_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, provider.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Insert fund_benchmark mapping
    const char* ins_fb = R"(
        INSERT OR IGNORE INTO fund_benchmark
            (fund_symbol, benchmark_code, benchmark_name, provider, source)
        VALUES (?1, ?2, ?3, ?4, 'manual')
    )";
    stmt = nullptr;
    sqlite3_prepare_v2(handle, ins_fb, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, fund_symbol.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, benchmark_code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, benchmark_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, provider.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

int count_rows(Database& db, const std::string& table) {
    sqlite3* handle = db.handle();
    std::string sql = "SELECT COUNT(*) FROM " + table;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(handle, sql.c_str(), -1, &stmt, nullptr);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

std::string get_log_status(Database& db) {
    sqlite3* handle = db.handle();
    const char* sql = "SELECT status FROM collection_log ORDER BY id";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    std::string statuses;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (!statuses.empty()) statuses += ",";
        statuses += reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return statuses;
}

void test_update_all_success() {
    auto path = unique_db_path();
    Database db;
    db.open(path);
    initialize_schema(db);

    FundRepository repo(db);
    repo.add("510300", Exchange::SSE, "沪深300ETF");
    repo.add("159915", Exchange::SZSE, "创业板ETF");

    insert_benchmark(db, "510300", "000300", "沪深300", "CSI");
    insert_benchmark(db, "159915", "399006", "创业板指", "CSI");

    Updater updater(db, "tests/fixtures");
    auto summary = updater.update_all();

    CHECK_EQ(summary.success, 2, "two funds updated successfully");
    CHECK_EQ(summary.skipped, 0, "no skips");
    CHECK_EQ(summary.failed, 0, "no failures");

    // Verify data written to index_valuation
    int val_count = count_rows(db, "index_valuation");
    CHECK(val_count > 0, "valuation data written");

    // Verify data written to daily_price
    int price_count = count_rows(db, "daily_price");
    CHECK(price_count > 0, "price data written");

    // Verify data written to fund_distribution
    int dist_count = count_rows(db, "fund_distribution");
    CHECK(dist_count > 0, "distribution data written");

    // Verify collection_log
    int log_count = count_rows(db, "collection_log");
    CHECK_EQ(log_count, 2, "two collection log entries");
    CHECK_EQ(get_log_status(db), "success,success", "both entries success");

    db.close();
    std::filesystem::remove(path);
}

void test_update_symbol() {
    auto path = unique_db_path();
    Database db;
    db.open(path);
    initialize_schema(db);

    FundRepository repo(db);
    repo.add("510300", Exchange::SSE, "沪深300ETF");
    insert_benchmark(db, "510300", "000300", "沪深300", "CSI");

    Updater updater(db, "tests/fixtures");
    auto summary = updater.update_symbol("510300");

    CHECK_EQ(summary.success, 1, "single fund updated");
    CHECK_EQ(summary.failed, 0, "no failures");

    int val_count = count_rows(db, "index_valuation");
    CHECK(val_count > 0, "valuation written for single symbol");

    db.close();
    std::filesystem::remove(path);
}

void test_update_no_benchmark_skipped() {
    auto path = unique_db_path();
    Database db;
    db.open(path);
    initialize_schema(db);

    FundRepository repo(db);
    repo.add("510300", Exchange::SSE, "沪深300ETF");
    // No benchmark mapping

    Updater updater(db, "tests/fixtures");
    auto summary = updater.update_all();

    CHECK_EQ(summary.success, 0, "no success");
    CHECK_EQ(summary.skipped, 1, "one skipped (no benchmark)");
    CHECK_EQ(summary.failed, 0, "no failures");

    int val_count = count_rows(db, "index_valuation");
    CHECK_EQ(val_count, 0, "no valuation written");

    db.close();
    std::filesystem::remove(path);
}

void test_update_idempotent() {
    auto path = unique_db_path();
    Database db;
    db.open(path);
    initialize_schema(db);

    FundRepository repo(db);
    repo.add("510300", Exchange::SSE, "沪深300ETF");
    insert_benchmark(db, "510300", "000300", "沪深300", "CSI");

    Updater updater(db, "tests/fixtures");

    auto s1 = updater.update_all();
    CHECK_EQ(s1.success, 1, "first update success");

    int val_count_1 = count_rows(db, "index_valuation");

    auto s2 = updater.update_all();
    CHECK_EQ(s2.success, 1, "second update success");

    int val_count_2 = count_rows(db, "index_valuation");
    CHECK_EQ(val_count_1, val_count_2, "no duplicate records after second run");

    db.close();
    std::filesystem::remove(path);
}

void test_update_empty_watchlist() {
    auto path = unique_db_path();
    Database db;
    db.open(path);
    initialize_schema(db);

    Updater updater(db, "tests/fixtures");
    auto summary = updater.update_all();

    CHECK_EQ(summary.success, 0, "no success");
    CHECK_EQ(summary.skipped, 0, "no skips");
    CHECK_EQ(summary.failed, 0, "no failures");

    db.close();
    std::filesystem::remove(path);
}

void test_update_mixed_results() {
    auto path = unique_db_path();
    Database db;
    db.open(path);
    initialize_schema(db);

    FundRepository repo(db);
    repo.add("510300", Exchange::SSE, "沪深300ETF");
    repo.add("510050", Exchange::SSE, "上证50ETF");

    insert_benchmark(db, "510300", "000300", "沪深300", "CSI");
    // 510050 has no benchmark -> skipped

    Updater updater(db, "tests/fixtures");
    auto summary = updater.update_all();

    CHECK_EQ(summary.success, 1, "one success");
    CHECK_EQ(summary.skipped, 1, "one skipped");
    CHECK_EQ(summary.failed, 0, "no failures");

    CHECK_EQ(get_log_status(db), "skipped,success", "log: skipped then success");

    db.close();
    std::filesystem::remove(path);
}

void test_update_unknown_symbol() {
    auto path = unique_db_path();
    Database db;
    db.open(path);
    initialize_schema(db);

    Updater updater(db, "tests/fixtures");
    auto summary = updater.update_symbol("999999");

    CHECK_EQ(summary.success, 0, "no success");
    CHECK_EQ(summary.skipped, 0, "no skips");
    CHECK_EQ(summary.failed, 0, "no failures");

    db.close();
    std::filesystem::remove(path);
}

} // namespace

int main() {
    test_update_all_success();
    test_update_symbol();
    test_update_no_benchmark_skipped();
    test_update_idempotent();
    test_update_empty_watchlist();
    test_update_mixed_results();
    test_update_unknown_symbol();

    std::cout << "\n=== " << tests << " tests, " << failures << " failures ===" << std::endl;
    return failures > 0 ? 1 : 0;
}