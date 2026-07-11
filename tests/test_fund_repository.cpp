#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

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

// No TestDB helper - each test manages its own database inline

void test_add_fund() {
    auto path = std::filesystem::temp_directory_path().string() + "/test_af_" + std::to_string(std::rand()) + ".db";
    Database db;
    db.open(path);
    initialize_schema(db);
    FundRepository repo(db);

    CHECK(repo.add("510300", Exchange::SSE, "沪深300ETF"), "add SSE ETF");
    CHECK(repo.exists("510300", Exchange::SSE), "exists after add");
    db.close();
    std::filesystem::remove(path);
}

void test_duplicate_add() {
    auto path = std::filesystem::temp_directory_path().string() + "/test_da_" + std::to_string(std::rand()) + ".db";
    Database db;
    db.open(path);
    initialize_schema(db);
    FundRepository repo(db);

    CHECK(repo.add("510300", Exchange::SSE, "沪深300ETF"), "first add succeeds");
    CHECK(!repo.add("510300", Exchange::SSE, "沪深300ETF"), "duplicate add fails");
    db.close();
    std::filesystem::remove(path);
}

void test_same_symbol_diff_exchange() {
    auto path = std::filesystem::temp_directory_path().string() + "/test_ss_" + std::to_string(std::rand()) + ".db";
    Database db;
    db.open(path);
    initialize_schema(db);
    FundRepository repo(db);

    CHECK(repo.add("510300", Exchange::SSE, "SSE ETF"), "add SSE");
    CHECK(repo.add("510300", Exchange::SZSE, "SZSE ETF"), "add SZSE same code");

    auto records = repo.list();
    CHECK_EQ(records.size(), 2u, "two records for same code diff exchange");
    db.close();
    std::filesystem::remove(path);
}

void test_list_empty() {
    auto path = std::filesystem::temp_directory_path().string() + "/test_le_" + std::to_string(std::rand()) + ".db";
    Database db;
    db.open(path);
    initialize_schema(db);
    FundRepository repo(db);
    auto records = repo.list();
    CHECK(records.empty(), "empty list when no funds");
    db.close();
    std::filesystem::remove(path);
}

void test_list_multiple() {
    auto path = std::filesystem::temp_directory_path().string() + "/test_lm_" + std::to_string(std::rand()) + ".db";
    Database db;
    db.open(path);
    initialize_schema(db);
    FundRepository repo(db);

    repo.add("510300", Exchange::SSE, "沪深300ETF");
    repo.add("510050", Exchange::SSE, "上证50ETF");
    repo.add("159915", Exchange::SZSE, "创业板ETF");

    auto records = repo.list();
    CHECK_EQ(records.size(), 3u, "list returns 3 records");
    CHECK_EQ(records[0].exchange, Exchange::SSE, "first by order (SSE < SZSE alphabetically)");
    CHECK_EQ(records[0].symbol, "510050", "first sorted by symbol within SSE");
    db.close();
    std::filesystem::remove(path);
}

void test_remove_fund() {
    auto path = std::filesystem::temp_directory_path().string() + "/test_rf_" + std::to_string(std::rand()) + ".db";
    Database db;
    db.open(path);
    initialize_schema(db);
    FundRepository repo(db);

    repo.add("510300", Exchange::SSE, "沪深300ETF");
    CHECK(repo.remove("510300", Exchange::SSE), "remove succeeds");
    CHECK(!repo.exists("510300", Exchange::SSE), "not found after remove");
    CHECK(repo.list().empty(), "list empty after remove");
    db.close();
    std::filesystem::remove(path);
}

void test_remove_nonexistent() {
    auto path = std::filesystem::temp_directory_path().string() + "/test_rn_" + std::to_string(std::rand()) + ".db";
    Database db;
    db.open(path);
    initialize_schema(db);
    FundRepository repo(db);

    CHECK(!repo.remove("999999", Exchange::SSE), "remove nonexistent returns false");
    db.close();
    std::filesystem::remove(path);
}

void test_find_fund() {
    auto path = std::filesystem::temp_directory_path().string() + "/test_ff_" + std::to_string(std::rand()) + ".db";
    Database db;
    db.open(path);
    initialize_schema(db);
    FundRepository repo(db);

    repo.add("510300", Exchange::SSE, "沪深300ETF");

    auto found = repo.find("510300", Exchange::SSE);
    CHECK(found.has_value(), "find returns value");
    CHECK_EQ(found->symbol, "510300", "found symbol");
    CHECK_EQ(found->name, "沪深300ETF", "found name");
    CHECK_EQ(found->exchange, Exchange::SSE, "found exchange");

    auto not_found = repo.find("000000", Exchange::SSE);
    CHECK(!not_found.has_value(), "find nonexistent returns nullopt");
    db.close();
    std::filesystem::remove(path);
}

void test_deduce_exchange_sse() {
    auto e = deduce_exchange("510300");
    CHECK(e.has_value(), "510300 deducible");
    CHECK_EQ(e.value(), Exchange::SSE, "510300 -> SSE");

    e = deduce_exchange("560001");
    CHECK(e.has_value(), "560001 deducible");
    CHECK_EQ(e.value(), Exchange::SSE, "560001 -> SSE");

    e = deduce_exchange("588000");
    CHECK(e.has_value(), "588000 deducible");
    CHECK_EQ(e.value(), Exchange::SSE, "588000 -> SSE");
}

void test_deduce_exchange_szse() {
    auto e = deduce_exchange("159915");
    CHECK(e.has_value(), "159915 deducible");
    CHECK_EQ(e.value(), Exchange::SZSE, "159915 -> SZSE");

    e = deduce_exchange("160001");
    CHECK(e.has_value(), "160001 deducible");
    CHECK_EQ(e.value(), Exchange::SZSE, "160001 -> SZSE");
}

void test_deduce_exchange_invalid() {
    CHECK(!deduce_exchange("123456").has_value(), "unknown prefix returns nullopt");
    CHECK(!deduce_exchange("abc123").has_value(), "non-digit returns nullopt");
    CHECK(!deduce_exchange("12345").has_value(), "short code returns nullopt");
}

void test_data_persistence() {
    auto path = std::filesystem::temp_directory_path().string() + "/test_persist_" + std::to_string(std::rand()) + ".db";

    {
        Database db;
        db.open(path);
        initialize_schema(db);
        FundRepository repo(db);
        repo.add("510300", Exchange::SSE, "沪深300ETF");
        repo.add("159915", Exchange::SZSE, "创业板ETF");
    }

    {
        Database db;
        db.open(path);
        FundRepository repo(db);
        auto records = repo.list();
        CHECK_EQ(records.size(), 2u, "data persists after reconnect");
    }

    std::filesystem::remove(path);
}

} // namespace

int main() {
    test_add_fund();
    test_duplicate_add();
    test_same_symbol_diff_exchange();
    test_list_empty();
    test_list_multiple();
    test_remove_fund();
    test_remove_nonexistent();
    test_find_fund();
    test_deduce_exchange_sse();
    test_deduce_exchange_szse();
    test_deduce_exchange_invalid();
    test_data_persistence();

    std::cout << "\n=== " << tests << " tests, " << failures << " failures ===" << std::endl;
    return failures > 0 ? 1 : 0;
}