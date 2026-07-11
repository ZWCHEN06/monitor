#ifndef INDEX_FUND_MONITOR_DB_SCHEMA_HPP
#define INDEX_FUND_MONITOR_DB_SCHEMA_HPP

#include <string>

class Database;

bool initialize_schema(Database& db);
int get_schema_version(Database& db);
std::string default_database_path();

#endif