#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include <sqlite3.h>

#include "index_fund_monitor/db/database.hpp"
#include "index_fund_monitor/db/schema.hpp"

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

std::string temp_db_path() {
    return std::filesystem::temp_directory_path().string() + "/test_monitor_" + std::to_string(std::rand()) + ".db";
}

bool table_exists(Database& db, const std::string& name) {
    sqlite3* handle = db.handle();
    if (!handle) return false;

    std::string sql = "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='" + name + "'";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(handle, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    bool exists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = sqlite3_column_int(stmt, 0) > 0;
    }
    sqlite3_finalize(stmt);
    return exists;
}

bool foreign_keys_enabled(Database& db) {
    sqlite3* handle = db.handle();
    if (!handle) return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(handle, "PRAGMA foreign_keys", -1, &stmt, nullptr) != SQLITE_OK) return false;

    bool enabled = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        enabled = sqlite3_column_int(stmt, 0) != 0;
    }
    sqlite3_finalize(stmt);
    return enabled;
}

void test_first_init() {
    std::string path = temp_db_path();
    Database db;
    CHECK(db.open(path), "open database for first init");
    CHECK(initialize_schema(db), "first init succeeds");
    CHECK_EQ(get_schema_version(db), 1, "schema version is 1");
    db.close();
    std::filesystem::remove(path);
}

void test_repeat_init() {
    std::string path = temp_db_path();
    {
        Database db;
        CHECK(db.open(path), "open for first init");
        CHECK(initialize_schema(db), "first init succeeds");
    }
    {
        Database db;
        CHECK(db.open(path), "open for second init");
        CHECK(initialize_schema(db), "repeat init succeeds (idempotent)");
        CHECK_EQ(get_schema_version(db), 1, "schema version still 1");
    }
    std::filesystem::remove(path);
}

void test_all_tables_exist() {
    std::string path = temp_db_path();
    Database db;
    CHECK(db.open(path), "open database");
    CHECK(initialize_schema(db), "schema initialized");

    const char* tables[] = {
        "schema_version", "fund", "fund_listing", "benchmark_index",
        "fund_benchmark", "index_valuation", "fund_distribution",
        "daily_price", "data_source", "collection_log", "alert_rule"
    };

    for (auto name : tables) {
        CHECK(table_exists(db, name), std::string("table exists: ") + name);
    }

    db.close();
    std::filesystem::remove(path);
}

void test_foreign_keys_enabled() {
    std::string path = temp_db_path();
    Database db;
    CHECK(db.open(path), "open database");
    CHECK(foreign_keys_enabled(db), "foreign keys enabled after open");
    db.close();
    std::filesystem::remove(path);
}

void test_invalid_path() {
    Database db;
    CHECK(!db.open("/nonexistent/dir/db.db"), "invalid path returns false");
}

void test_database_move() {
    std::string path = temp_db_path();
    Database db1;
    CHECK(db1.open(path), "db1 open");

    Database db2 = std::move(db1);
    CHECK(!db1.is_open(), "db1 closed after move");
    CHECK(db2.is_open(), "db2 is open after move from db1");
    CHECK(initialize_schema(db2), "schema init on moved db");
    db2.close();
    std::filesystem::remove(path);
}

void test_database_assign_move() {
    std::string path1 = temp_db_path();
    std::string path2 = temp_db_path();

    Database db1;
    CHECK(db1.open(path1), "db1 open");

    Database db2;
    CHECK(db2.open(path2), "db2 open");

    db2 = std::move(db1);
    CHECK(!db1.is_open(), "db1 closed after move assign");
    CHECK(db2.is_open(), "db2 still open after move assign");

    db2.close();
    std::filesystem::remove(path1);
    std::filesystem::remove(path2);
}

void test_default_database_path() {
    auto path = default_database_path();
    CHECK(!path.empty(), "default path not empty");
    CHECK(path.find("index_fund_monitor.db") != std::string::npos, "default path ends with db name");
}

} // namespace

int main() {
    test_first_init();
    test_repeat_init();
    test_all_tables_exist();
    test_foreign_keys_enabled();
    test_invalid_path();
    test_database_move();
    test_database_assign_move();
    test_default_database_path();

    std::cout << "\n=== " << tests << " tests, " << failures << " failures ===" << std::endl;
    return failures > 0 ? 1 : 0;
}