#ifndef INDEX_FUND_MONITOR_DB_DATABASE_HPP
#define INDEX_FUND_MONITOR_DB_DATABASE_HPP

#include <string>

struct sqlite3;

class Database {
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    bool open(const std::string& path);
    void close();
    bool is_open() const;

    bool execute(const std::string& sql);
    bool execute_batch(const std::string& sql);

    sqlite3* handle() const;
    std::string last_error() const;

private:
    sqlite3* db_ = nullptr;
};

#endif