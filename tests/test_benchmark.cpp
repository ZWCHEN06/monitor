#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include <sqlite3.h>

#include "index_fund_monitor/db/benchmark_repository.hpp"
#include "index_fund_monitor/db/database.hpp"
#include "index_fund_monitor/db/fund_repository.hpp"
#include "index_fund_monitor/db/schema.hpp"
#include "index_fund_monitor/models/models.hpp"

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
        + "/test_bm_" + std::to_string(std::rand()) + ".db";
}

Database setup_db(const std::string& path) {
    Database db;
    db.open(path);
    initialize_schema(db);
    return db;
}

void add_fund(Database& db, const std::string& symbol, Exchange exchange, const std::string& name) {
    FundRepository repo(db);
    repo.add(symbol, exchange, name);
}

int current_mapping_count(Database& db, const std::string& symbol) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db.handle(),
                           "SELECT COUNT(*) FROM fund_benchmark WHERE fund_symbol = ?1 AND is_current = 1",
                           -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_text(stmt, 1, symbol.c_str(), -1, SQLITE_TRANSIENT);
    const int count = sqlite3_step(stmt) == SQLITE_ROW ? sqlite3_column_int(stmt, 0) : -1;
    sqlite3_finalize(stmt);
    return count;
}

void test_set_benchmark() {
    auto path = unique_db_path();
    auto db = setup_db(path);
    add_fund(db, "510300", Exchange::SSE, "沪深300ETF");

    BenchmarkRepository repo(db);
    bool ok = repo.set("510300", "000300", "沪深300", "CSI", "manual");
    CHECK(ok, "set benchmark succeeds");

    auto current = repo.get_current("510300");
    CHECK(current.has_value(), "get_current returns value");
    CHECK_EQ(current->fund_symbol, "510300", "fund_symbol correct");
    CHECK_EQ(current->benchmark_code, "000300", "benchmark_code correct");
    CHECK_EQ(current->benchmark_name, "沪深300", "benchmark_name correct");
    CHECK_EQ(current->provider, "CSI", "provider correct");
    CHECK_EQ(current->source, "manual", "source correct");
    CHECK_EQ(current_mapping_count(db, "510300"), 1, "exactly one current mapping");

    db.close();
    std::filesystem::remove(path);
}

void test_get_current_returns_latest() {
    auto path = unique_db_path();
    auto db = setup_db(path);
    add_fund(db, "510300", Exchange::SSE, "沪深300ETF");

    BenchmarkRepository repo(db);
    repo.set("510300", "000300", "沪深300", "CSI", "manual");

    // Set again with different benchmark
    repo.set("510300", "000905", "中证500", "CSI", "manual");

    auto current = repo.get_current("510300");
    CHECK(current.has_value(), "get_current returns value");
    CHECK_EQ(current->benchmark_code, "000905", "second mapping is current");
    CHECK_EQ(current_mapping_count(db, "510300"), 1, "changing mapping keeps one current mapping");

    db.close();
    std::filesystem::remove(path);
}

void test_repeat_set_is_idempotent() {
    auto path = unique_db_path();
    auto db = setup_db(path);
    add_fund(db, "510300", Exchange::SSE, "沪深300ETF");

    BenchmarkRepository repo(db);
    CHECK(repo.set("510300", "000300", "沪深300", "CSI", "manual"), "first set succeeds");
    CHECK(repo.set("510300", "000300", "沪深300", "CSI", "manual"), "repeated set succeeds");

    const auto history = repo.get_history("510300");
    CHECK_EQ(history.size(), 1u, "repeated set does not create a duplicate history record");
    CHECK_EQ(current_mapping_count(db, "510300"), 1, "repeated set keeps one current mapping");

    db.close();
    std::filesystem::remove(path);
}

void test_get_history() {
    auto path = unique_db_path();
    auto db = setup_db(path);
    add_fund(db, "510300", Exchange::SSE, "沪深300ETF");

    BenchmarkRepository repo(db);
    repo.set("510300", "000300", "沪深300", "CSI", "manual");
    repo.set("510300", "000905", "中证500", "CSI", "manual");

    auto history = repo.get_history("510300");
    CHECK_EQ(history.size(), 2u, "two history records");

    db.close();
    std::filesystem::remove(path);
}

void test_exists() {
    auto path = unique_db_path();
    auto db = setup_db(path);
    add_fund(db, "510300", Exchange::SSE, "沪深300ETF");

    BenchmarkRepository repo(db);
    CHECK(!repo.exists("510300"), "no mapping before set");

    repo.set("510300", "000300", "沪深300", "CSI", "manual");
    CHECK(repo.exists("510300"), "mapping exists after set");

    db.close();
    std::filesystem::remove(path);
}

void test_no_mapping() {
    auto path = unique_db_path();
    auto db = setup_db(path);
    add_fund(db, "510300", Exchange::SSE, "沪深300ETF");

    BenchmarkRepository repo(db);
    auto current = repo.get_current("510300");
    CHECK(!current.has_value(), "no mapping returns nullopt");

    auto history = repo.get_history("510300");
    CHECK(history.empty(), "empty history");

    db.close();
    std::filesystem::remove(path);
}

void test_different_funds_independent() {
    auto path = unique_db_path();
    auto db = setup_db(path);
    add_fund(db, "510300", Exchange::SSE, "沪深300ETF");
    add_fund(db, "510050", Exchange::SSE, "上证50ETF");

    BenchmarkRepository repo(db);
    repo.set("510300", "000300", "沪深300", "CSI", "manual");
    repo.set("510050", "000016", "上证50", "CSI", "manual");

    auto c1 = repo.get_current("510300");
    auto c2 = repo.get_current("510050");
    CHECK(c1.has_value() && c2.has_value(), "both have mappings");
    CHECK_EQ(c1->benchmark_code, "000300", "510300 -> 000300");
    CHECK_EQ(c2->benchmark_code, "000016", "510050 -> 000016");

    db.close();
    std::filesystem::remove(path);
}

} // namespace

int main() {
    test_set_benchmark();
    test_get_current_returns_latest();
    test_get_history();
    test_repeat_set_is_idempotent();
    test_exists();
    test_no_mapping();
    test_different_funds_independent();

    std::cout << "\n=== " << tests << " tests, " << failures << " failures ===" << std::endl;
    return failures > 0 ? 1 : 0;
}
