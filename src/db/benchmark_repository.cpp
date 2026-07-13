#include "index_fund_monitor/db/benchmark_repository.hpp"

#include <cstdint>
#include <ctime>
#include <sqlite3.h>

#include "index_fund_monitor/db/database.hpp"

namespace {

std::string today_date() {
    std::time_t t = std::time(nullptr);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::gmtime(&t));
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

BenchmarkRecord row_to_record(sqlite3_stmt* stmt) {
    BenchmarkRecord rec;
    rec.id = static_cast<int64_t>(sqlite3_column_int64(stmt, 0));
    rec.fund_symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    rec.benchmark_code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    const char* bn = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    if (bn) rec.benchmark_name = bn;
    const char* pv = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    if (pv) rec.provider = pv;
    const char* ed = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    if (ed) rec.effective_date = ed;
    const char* sc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    if (sc) rec.source = sc;
    return rec;
}

} // namespace

BenchmarkRepository::BenchmarkRepository(Database& db) : db_(db) {}

bool BenchmarkRepository::set(const std::string& fund_symbol,
                              const std::string& benchmark_code,
                              const std::string& benchmark_name,
                              const std::string& provider,
                              const std::string& source) {
    if (!db_.is_open()) return false;

    sqlite3* handle = db_.handle();
    if (!db_.execute("BEGIN")) return false;

    // Ensure benchmark_index exists
    const char* upsert_idx = R"(
        INSERT INTO benchmark_index (code, name, provider) VALUES (?1, ?2, ?3)
        ON CONFLICT(code) DO UPDATE SET name = ?2, provider = ?3
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(handle, upsert_idx, -1, &stmt, nullptr) != SQLITE_OK) {
        db_.execute("ROLLBACK");
        return false;
    }
    bind_text(stmt, 1, benchmark_code);
    bind_text(stmt, 2, benchmark_name);
    bind_nullable_text(stmt, 3, provider);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        db_.execute("ROLLBACK");
        return false;
    }

    // Insert fund_benchmark mapping
    std::string eff_date = today_date();
    const char* ins_fb = R"(
        INSERT INTO fund_benchmark (fund_symbol, benchmark_code, benchmark_name, provider, effective_date, source)
        VALUES (?1, ?2, ?3, ?4, ?5, ?6)
    )";
    stmt = nullptr;
    if (sqlite3_prepare_v2(handle, ins_fb, -1, &stmt, nullptr) != SQLITE_OK) {
        db_.execute("ROLLBACK");
        return false;
    }
    bind_text(stmt, 1, fund_symbol);
    bind_text(stmt, 2, benchmark_code);
    bind_text(stmt, 3, benchmark_name);
    bind_nullable_text(stmt, 4, provider);
    bind_text(stmt, 5, eff_date);
    bind_text(stmt, 6, source);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        db_.execute("ROLLBACK");
        return false;
    }

    if (!db_.execute("COMMIT")) {
        db_.execute("ROLLBACK");
        return false;
    }

    return true;
}

std::optional<BenchmarkRecord> BenchmarkRepository::get_current(const std::string& fund_symbol) const {
    if (!db_.is_open()) return std::nullopt;

    sqlite3* handle = db_.handle();
    const char* sql = R"(
        SELECT id, fund_symbol, benchmark_code, benchmark_name, provider, effective_date, source
        FROM fund_benchmark
        WHERE fund_symbol = ?1
        ORDER BY effective_date DESC, id DESC
        LIMIT 1
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }
    bind_text(stmt, 1, fund_symbol);

    std::optional<BenchmarkRecord> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_record(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<BenchmarkRecord> BenchmarkRepository::get_history(const std::string& fund_symbol) const {
    std::vector<BenchmarkRecord> records;
    if (!db_.is_open()) return records;

    sqlite3* handle = db_.handle();
    const char* sql = R"(
        SELECT id, fund_symbol, benchmark_code, benchmark_name, provider, effective_date, source
        FROM fund_benchmark
        WHERE fund_symbol = ?1
        ORDER BY effective_date DESC, id DESC
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return records;
    }
    bind_text(stmt, 1, fund_symbol);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        records.push_back(row_to_record(stmt));
    }

    sqlite3_finalize(stmt);
    return records;
}

bool BenchmarkRepository::exists(const std::string& fund_symbol) const {
    if (!db_.is_open()) return false;

    sqlite3* handle = db_.handle();
    const char* sql = "SELECT COUNT(*) FROM fund_benchmark WHERE fund_symbol = ?1";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    bind_text(stmt, 1, fund_symbol);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        found = sqlite3_column_int(stmt, 0) > 0;
    }

    sqlite3_finalize(stmt);
    return found;
}