#include "index_fund_monitor/db/fund_repository.hpp"
#include "index_fund_monitor/db/database.hpp"
#include "index_fund_monitor/models/validation.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cctype>

namespace {

std::string exchange_prefixes(Exchange e) {
    switch (e) {
        case Exchange::SSE:  return "SSE";
        case Exchange::SZSE: return "SZSE";
    }
    return "";
}

void bind_text(sqlite3_stmt* stmt, int idx, const std::string& val) {
    sqlite3_bind_text(stmt, idx, val.c_str(), -1, SQLITE_TRANSIENT);
}

} // namespace

std::optional<Exchange> deduce_exchange(std::string_view symbol) {
    if (symbol.size() != 6) return std::nullopt;

    // Get first 2 digits as number
    int prefix = 0;
    for (std::size_t i = 0; i < 2; ++i) {
        auto c = static_cast<unsigned char>(symbol[i]);
        if (!std::isdigit(c)) return std::nullopt;
        prefix = prefix * 10 + (c - '0');
    }

    // SSE ETFs: 51xxxx, 56xxxx, 58xxxx
    if (prefix == 51 || prefix == 56 || prefix == 58) {
        return Exchange::SSE;
    }
    // SZSE ETFs: 15xxxx, 16xxxx
    if (prefix == 15 || prefix == 16) {
        return Exchange::SZSE;
    }

    return std::nullopt;
}

FundRepository::FundRepository(Database& db) : db_(db) {}

bool FundRepository::add(const std::string& symbol, Exchange exchange, const std::string& name) {
    if (!db_.is_open()) return false;

    sqlite3* handle = db_.handle();

    if (!db_.execute("BEGIN")) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;

    const char* insert_fund = "INSERT INTO fund (name) VALUES (?1);";
    if (sqlite3_prepare_v2(handle, insert_fund, -1, &stmt, nullptr) != SQLITE_OK) {
        db_.execute("ROLLBACK");
        return false;
    }
    bind_text(stmt, 1, name);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        db_.execute("ROLLBACK");
        return false;
    }

    int64_t fund_id = static_cast<int64_t>(sqlite3_last_insert_rowid(handle));

    const char* insert_listing = "INSERT INTO fund_listing (fund_id, exchange, symbol, currency) VALUES (?1, ?2, ?3, ?4);";
    if (sqlite3_prepare_v2(handle, insert_listing, -1, &stmt, nullptr) != SQLITE_OK) {
        db_.execute("ROLLBACK");
        return false;
    }
    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(fund_id));
    bind_text(stmt, 2, exchange_prefixes(exchange));
    bind_text(stmt, 3, symbol);
    bind_text(stmt, 4, "CNY");

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        db_.execute("ROLLBACK");
        return false;
    }

    return db_.execute("COMMIT");
}

bool FundRepository::remove(const std::string& symbol, Exchange exchange) {
    if (!db_.is_open()) return false;

    if (!exists(symbol, exchange)) return false;

    db_.execute("BEGIN TRANSACTION");
    sqlite3* handle = db_.handle();
    sqlite3_stmt* stmt = nullptr;

    const char* sql = R"(
        DELETE FROM fund_listing WHERE symbol = ?1 AND exchange = ?2;
    )";
    if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        db_.execute("ROLLBACK");
        return false;
    }
    bind_text(stmt, 1, symbol);
    bind_text(stmt, 2, exchange_prefixes(exchange));

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        db_.execute("ROLLBACK");
        return false;
    }

    db_.execute("COMMIT");
    return true;
}

std::vector<FundRecord> FundRepository::list() const {
    std::vector<FundRecord> records;
    if (!db_.is_open()) return records;

    sqlite3* handle = db_.handle();
    sqlite3_stmt* stmt = nullptr;

    const char* sql = R"(
        SELECT f.id, fl.symbol, fl.exchange, f.name, fl.currency,
               f.manager, f.fund_type, f.listing_status, f.created_at
        FROM fund f
        JOIN fund_listing fl ON f.id = fl.fund_id
        ORDER BY fl.exchange, fl.symbol
    )";

    if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return records;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FundRecord rec;
        rec.id = sqlite3_column_int64(stmt, 0);
        rec.symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string exch_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        rec.exchange = (exch_str == "SSE") ? Exchange::SSE : Exchange::SZSE;
        rec.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        rec.currency = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

        const char* mgr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        if (mgr) rec.manager = mgr;
        const char* ft = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        if (ft) rec.fund_type = ft;
        const char* ls = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        if (ls) rec.listing_status = ls;
        const char* ca = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        if (ca) rec.created_at = ca;

        records.push_back(std::move(rec));
    }

    sqlite3_finalize(stmt);
    return records;
}

std::optional<FundRecord> FundRepository::find(const std::string& symbol, Exchange exchange) const {
    if (!db_.is_open()) return std::nullopt;

    sqlite3* handle = db_.handle();
    sqlite3_stmt* stmt = nullptr;

    const char* sql = R"(
        SELECT f.id, fl.symbol, fl.exchange, f.name, fl.currency,
               f.manager, f.fund_type, f.listing_status, f.created_at
        FROM fund f
        JOIN fund_listing fl ON f.id = fl.fund_id
        WHERE fl.symbol = ?1 AND fl.exchange = ?2
    )";

    if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }
    bind_text(stmt, 1, symbol);
    bind_text(stmt, 2, exchange_prefixes(exchange));

    FundRecord rec;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        rec.id = sqlite3_column_int64(stmt, 0);
        rec.symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string exch_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        rec.exchange = (exch_str == "SSE") ? Exchange::SSE : Exchange::SZSE;
        rec.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        rec.currency = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

        const char* mgr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        if (mgr) rec.manager = mgr;
        const char* ft = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        if (ft) rec.fund_type = ft;
        const char* ls = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        if (ls) rec.listing_status = ls;
        const char* ca = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        if (ca) rec.created_at = ca;

        sqlite3_finalize(stmt);
        return rec;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool FundRepository::exists(const std::string& symbol, Exchange exchange) const {
    if (!db_.is_open()) return false;

    sqlite3* handle = db_.handle();
    sqlite3_stmt* stmt = nullptr;

    const char* sql = "SELECT COUNT(*) FROM fund_listing WHERE symbol = ?1 AND exchange = ?2";
    if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    bind_text(stmt, 1, symbol);
    bind_text(stmt, 2, exchange_prefixes(exchange));

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        found = sqlite3_column_int(stmt, 0) > 0;
    }

    sqlite3_finalize(stmt);
    return found;
}