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
    CHECK_EQ(get_schema_version(db), 2, "schema version is 2");
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
        CHECK_EQ(get_schema_version(db), 2, "schema version still 2");
    }
    std::filesystem::remove(path);
}

void test_current_benchmark_mapping_constraint() {
    std::string path = temp_db_path();
    Database db;
    CHECK(db.open(path), "open database for current mapping constraint");
    CHECK(initialize_schema(db), "schema initialized for current mapping constraint");
    CHECK(db.execute_batch(R"SQL(
        INSERT INTO benchmark_index (code, name) VALUES ('000300', '沪深300');
        INSERT INTO benchmark_index (code, name) VALUES ('000905', '中证500');
        INSERT INTO fund_benchmark
            (fund_symbol, benchmark_code, source, is_current)
        VALUES ('510300', '000300', 'test', 1);
    )SQL"), "first current mapping is accepted");
    CHECK(!db.execute(R"SQL(
        INSERT INTO fund_benchmark
            (fund_symbol, benchmark_code, source, is_current)
        VALUES ('510300', '000905', 'test', 1)
    )SQL"), "database rejects a second current mapping for one ETF");
    db.close();
    std::filesystem::remove(path);
}

void test_legacy_benchmark_mapping_migration() {
    std::string path = temp_db_path();
    Database db;
    CHECK(db.open(path), "open database for legacy mapping migration");
    CHECK(db.execute_batch(R"SQL(
        CREATE TABLE schema_version (version INTEGER PRIMARY KEY, applied_at TEXT NOT NULL);
        INSERT INTO schema_version (version, applied_at) VALUES (1, '2026-07-13');
        CREATE TABLE benchmark_index (code TEXT PRIMARY KEY, name TEXT NOT NULL, provider TEXT);
        INSERT INTO benchmark_index (code, name) VALUES ('000300', '沪深300');
        INSERT INTO benchmark_index (code, name) VALUES ('000905', '中证500');
        CREATE TABLE fund_benchmark (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            fund_symbol TEXT NOT NULL,
            benchmark_code TEXT NOT NULL,
            benchmark_name TEXT,
            provider TEXT,
            effective_date TEXT,
            source TEXT NOT NULL,
            UNIQUE(fund_symbol, benchmark_code, effective_date),
            FOREIGN KEY (benchmark_code) REFERENCES benchmark_index(code)
        );
        INSERT INTO fund_benchmark
            (fund_symbol, benchmark_code, effective_date, source)
        VALUES ('510300', '000300', '2026-07-12', 'test');
        INSERT INTO fund_benchmark
            (fund_symbol, benchmark_code, effective_date, source)
        VALUES ('510300', '000905', '2026-07-13', 'test');
    )SQL"), "legacy schema and mappings created");
    CHECK(initialize_schema(db), "legacy schema migrates successfully");

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT benchmark_code FROM fund_benchmark WHERE fund_symbol = '510300' AND is_current = 1";
    CHECK(sqlite3_prepare_v2(db.handle(), sql, -1, &stmt, nullptr) == SQLITE_OK,
          "prepare migrated current mapping query");
    CHECK(sqlite3_step(stmt) == SQLITE_ROW, "legacy mappings have one current record after migration");
    CHECK_EQ(std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))), "000905",
             "latest legacy mapping is current after migration");
    sqlite3_finalize(stmt);
    CHECK_EQ(get_schema_version(db), 2, "legacy schema version upgraded to 2");
    db.close();
    std::filesystem::remove(path);
}

void test_repeat_init_preserves_current_mapping_after_same_day_switch() {
    std::string path = temp_db_path();
    Database db;
    CHECK(db.open(path), "open database for current mapping preservation");
    CHECK(initialize_schema(db), "schema initialized for current mapping preservation");
    CHECK(db.execute_batch(R"SQL(
        INSERT INTO benchmark_index (code, name) VALUES ('000300', 'CSI 300');
        INSERT INTO benchmark_index (code, name) VALUES ('000905', 'CSI 500');
        INSERT INTO fund_benchmark
            (fund_symbol, benchmark_code, effective_date, source, is_current)
        VALUES ('510300', '000300', '2026-07-14', 'test', 0);
        INSERT INTO fund_benchmark
            (fund_symbol, benchmark_code, effective_date, source, is_current)
        VALUES ('510300', '000905', '2026-07-14', 'test', 1);
        UPDATE fund_benchmark
        SET is_current = 0
        WHERE fund_symbol = '510300' AND benchmark_code = '000905';
        UPDATE fund_benchmark
        SET is_current = 1
        WHERE fund_symbol = '510300' AND benchmark_code = '000300';
    )SQL"), "same-day A to B to A mapping switch is recorded");
    CHECK(initialize_schema(db), "repeat init succeeds after same-day mapping switch");

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT benchmark_code FROM fund_benchmark WHERE fund_symbol = '510300' AND is_current = 1";
    CHECK(sqlite3_prepare_v2(db.handle(), sql, -1, &stmt, nullptr) == SQLITE_OK,
          "prepare preserved current mapping query");
    CHECK(sqlite3_step(stmt) == SQLITE_ROW, "same-day switch retains one current mapping after repeat init");
    CHECK_EQ(std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))), "000300",
             "repeat init preserves the explicitly selected A mapping");
    sqlite3_finalize(stmt);

    db.close();
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
    test_current_benchmark_mapping_constraint();
    test_legacy_benchmark_mapping_migration();
    test_repeat_init_preserves_current_mapping_after_same_day_switch();
    test_all_tables_exist();
    test_foreign_keys_enabled();
    test_invalid_path();
    test_database_move();
    test_database_assign_move();
    test_default_database_path();

    std::cout << "\n=== " << tests << " tests, " << failures << " failures ===" << std::endl;
    return failures > 0 ? 1 : 0;
}
