#include "index_fund_monitor/db/database.hpp"

#include <sqlite3.h>

#include <utility>

Database::~Database() {
    close();
}

Database::Database(Database&& other) noexcept
    : db_(std::exchange(other.db_, nullptr)) {}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        close();
        db_ = std::exchange(other.db_, nullptr);
    }
    return *this;
}

bool Database::open(const std::string& path) {
    close();
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        close();
        return false;
    }
    // Enable foreign keys
    execute("PRAGMA foreign_keys = ON");
    return true;
}

void Database::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::is_open() const {
    return db_ != nullptr;
}

bool Database::execute(const std::string& sql) {
    if (!db_) return false;
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

bool Database::execute_batch(const std::string& sql) {
    if (!db_) return false;
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

sqlite3* Database::handle() const {
    return db_;
}

std::string Database::last_error() const {
    if (db_) {
        return sqlite3_errmsg(db_);
    }
    return "database not open";
}