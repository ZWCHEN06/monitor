#include "index_fund_monitor/db/schema.hpp"
#include "index_fund_monitor/db/database.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <iostream>

namespace {

const char* schema_sql() {
    return R"SQL(
CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER PRIMARY KEY,
    applied_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS fund (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    manager TEXT,
    fund_type TEXT,
    listing_status TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS fund_listing (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    fund_id INTEGER NOT NULL,
    exchange TEXT NOT NULL,
    symbol TEXT NOT NULL,
    currency TEXT NOT NULL DEFAULT 'CNY',
    UNIQUE(exchange, symbol),
    FOREIGN KEY (fund_id) REFERENCES fund(id)
);

CREATE TABLE IF NOT EXISTS benchmark_index (
    code TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    provider TEXT
);

CREATE TABLE IF NOT EXISTS fund_benchmark (
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

CREATE TABLE IF NOT EXISTS index_valuation (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    benchmark_code TEXT NOT NULL,
    valuation_date TEXT NOT NULL,
    pe_ttm REAL,
    pb REAL,
    dividend_yield REAL,
    source TEXT NOT NULL,
    collected_at TEXT NOT NULL,
    UNIQUE(benchmark_code, valuation_date, source),
    FOREIGN KEY (benchmark_code) REFERENCES benchmark_index(code)
);

CREATE TABLE IF NOT EXISTS fund_distribution (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol TEXT NOT NULL,
    amount_per_share REAL,
    ex_date TEXT,
    record_date TEXT,
    pay_date TEXT,
    currency TEXT NOT NULL DEFAULT 'CNY',
    source TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS daily_price (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol TEXT NOT NULL,
    trade_date TEXT NOT NULL,
    open_price REAL,
    close_price REAL,
    high_price REAL,
    low_price REAL,
    volume REAL,
    amount REAL,
    source TEXT NOT NULL,
    collected_at TEXT NOT NULL,
    UNIQUE(symbol, trade_date, source)
);

CREATE TABLE IF NOT EXISTS data_source (
    name TEXT PRIMARY KEY,
    url TEXT,
    description TEXT
);

CREATE TABLE IF NOT EXISTS collection_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol TEXT,
    source TEXT NOT NULL,
    status TEXT NOT NULL,
    items_collected INTEGER DEFAULT 0,
    error_message TEXT,
    started_at TEXT NOT NULL,
    completed_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS alert_rule (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol TEXT NOT NULL,
    metric TEXT NOT NULL,
    operator TEXT NOT NULL,
    value REAL NOT NULL,
    enabled INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);
)SQL";
}

} // namespace

bool initialize_schema(Database& db) {
    if (!db.execute("BEGIN TRANSACTION")) {
        std::cerr << "错误：无法开始事务：" << db.last_error() << std::endl;
        return false;
    }

    if (!db.execute_batch(schema_sql())) {
        std::cerr << "错误：无法创建数据表：" << db.last_error() << std::endl;
        db.execute("ROLLBACK");
        return false;
    }

    // Record schema version
    if (!db.execute("INSERT OR IGNORE INTO schema_version (version, applied_at) VALUES (1, datetime('now'))")) {
        std::cerr << "错误：无法记录数据库版本：" << db.last_error() << std::endl;
        db.execute("ROLLBACK");
        return false;
    }

    if (!db.execute("COMMIT")) {
        std::cerr << "错误：无法提交事务：" << db.last_error() << std::endl;
        return false;
    }

    return true;
}

int get_schema_version(Database& db) {
    sqlite3* handle = db.handle();
    if (!handle) return -1;

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, "SELECT MAX(version) FROM schema_version", -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return -1;

    int version = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        version = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return version;
}

std::string default_database_path() {
    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (ec) return "data/index_fund_monitor.db";
    return (cwd / "data" / "index_fund_monitor.db").string();
}