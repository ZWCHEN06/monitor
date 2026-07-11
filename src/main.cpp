#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

#include "index_fund_monitor/cli/parser.hpp"
#include "index_fund_monitor/db/database.hpp"
#include "index_fund_monitor/db/fund_repository.hpp"
#include "index_fund_monitor/db/schema.hpp"
#include "index_fund_monitor/models/validation.hpp"
#include "index_fund_monitor/version.hpp"

namespace {

Database open_db(const ParseResult& result, std::string& out_path) {
    auto it = result.options.find("--database");
    std::string db_path = (it != result.options.end()) ? it->second : default_database_path();

    std::error_code ec;
    auto parent = std::filesystem::path(db_path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }

    Database db;
    if (!db.open(db_path)) {
        std::cerr << "Error: Cannot open database " << db_path << ": " << db.last_error() << std::endl;
        return db;
    }

    if (!initialize_schema(db)) {
        std::cerr << "Error: Failed to initialize schema." << std::endl;
        db.close();
        return db;
    }

    out_path = std::move(db_path);
    return db;
}

void print_help(std::ostream& os) {
    os << PROJECT_NAME << " - " << PROJECT_DESC << "\n"
       << "\n"
       << "Usage:\n"
       << "  " << PROJECT_NAME << " <command> [options]\n"
       << "\n"
       << "Commands:\n"
       << "  init              Initialize database\n"
       << "  fund add          Add ETF to watchlist\n"
       << "  fund list         List all watched ETFs\n"
       << "  fund remove       Remove ETF from watchlist\n"
       << "  update            Update valuation data\n"
       << "  show              Show ETF details\n"
       << "  screen            Filter ETFs by conditions\n"
       << "  alert add         Add alert rule\n"
       << "  alert list        List alert rules\n"
       << "  alert remove      Remove alert rule\n"
       << "  alert check       Check alert rules\n"
       << "  export            Export data to CSV\n"
       << "\n"
       << "General options:\n"
       << "  --help            Show this help message\n"
       << "  --version         Show version information\n"
       << "  --database <path> Database file path\n"
       << "  --market  <code>  Market code (CN)\n"
       << "  --symbol  <code>  ETF symbol (6 digits)\n"
       << "  --name    <name>  ETF name\n"
       << "  --output  <file>  Output file path\n"
       << std::endl;
}

void print_version(std::ostream& os) {
    os << PROJECT_NAME << " " << PROJECT_VERSION << std::endl;
}

int cmd_init(const ParseResult& result) {
    std::string db_path;
    auto db = open_db(result, db_path);
    if (!db.is_open()) return 1;

    int version = get_schema_version(db);
    std::cout << "Database initialized: " << db_path << "\n"
              << "Schema version: " << version << std::endl;
    return 0;
}

int cmd_fund_add(const ParseResult& result) {
    std::string symbol = result.options.at("--symbol");
    std::string name = result.options.at("--name");

    // Validate symbol
    if (!is_valid_etf_code(symbol)) {
        std::cerr << "Error: Invalid ETF code '" << symbol << "'. Must be 6 digits." << std::endl;
        return 1;
    }

    // Deduce exchange
    auto exchange = deduce_exchange(symbol);
    if (!exchange.has_value()) {
        std::cerr << "Error: Cannot determine exchange for symbol '" << symbol << "'." << std::endl;
        return 1;
    }

    std::string db_path;
    auto db = open_db(result, db_path);
    if (!db.is_open()) return 1;

    FundRepository repo(db);
    if (repo.exists(symbol, exchange.value())) {
        std::cerr << "Error: ETF " << symbol << " already exists in watchlist." << std::endl;
        return 1;
    }

    if (!repo.add(symbol, exchange.value(), name)) {
        std::cerr << "Error: Failed to add ETF " << symbol << "." << std::endl;
        return 1;
    }

    std::cout << "Added: " << to_string(exchange.value()) << "." << symbol << " " << name << std::endl;
    return 0;
}

int cmd_fund_list(const ParseResult& result) {
    std::string db_path;
    auto db = open_db(result, db_path);
    if (!db.is_open()) return 1;

    FundRepository repo(db);
    auto records = repo.list();

    if (records.empty()) {
        std::cout << "No ETFs in watchlist." << std::endl;
        return 0;
    }

    std::cout << "ETF Watchlist:\n";
    for (const auto& r : records) {
        std::cout << "  " << to_string(r.exchange) << "." << r.symbol << "  " << r.name << std::endl;
    }
    return 0;
}

int cmd_fund_remove(const ParseResult& result) {
    std::string symbol = result.options.at("--symbol");

    auto exchange = deduce_exchange(symbol);
    if (!exchange.has_value()) {
        std::cerr << "Error: Cannot determine exchange for symbol '" << symbol << "'." << std::endl;
        return 1;
    }

    std::string db_path;
    auto db = open_db(result, db_path);
    if (!db.is_open()) return 1;

    FundRepository repo(db);
    if (!repo.exists(symbol, exchange.value())) {
        std::cerr << "Error: ETF " << symbol << " not found in watchlist." << std::endl;
        return 1;
    }

    if (!repo.remove(symbol, exchange.value())) {
        std::cerr << "Error: Failed to remove ETF " << symbol << "." << std::endl;
        return 1;
    }

    std::cout << "Removed: " << to_string(exchange.value()) << "." << symbol << std::endl;
    return 0;
}

int dispatch(const ParseResult& result) {
    switch (result.command) {
        case CommandType::Init:
            return cmd_init(result);
        case CommandType::FundAdd:
            return cmd_fund_add(result);
        case CommandType::FundList:
            return cmd_fund_list(result);
        case CommandType::FundRemove:
            return cmd_fund_remove(result);
        case CommandType::Update:
        case CommandType::Show:
        case CommandType::Screen:
        case CommandType::AlertAdd:
        case CommandType::AlertList:
        case CommandType::AlertRemove:
        case CommandType::AlertCheck:
        case CommandType::Export:
            std::cout << "Command '" << Parser::command_name(result.command) << "' is not yet implemented." << std::endl;
            return 0;
        default:
            return 0;
    }
}

} // namespace

int main(int argc, char* argv[]) {
    Parser parser;
    auto result = parser.parse(argc, argv);

    if (!result.error.empty()) {
        std::cerr << "Error: " << result.error << "\n"
                  << "Use --help for usage information." << std::endl;
        return 1;
    }

    if (result.help) {
        print_help(std::cout);
        return 0;
    }

    if (result.version) {
        print_version(std::cout);
        return 0;
    }

    if (result.command == CommandType::None) {
        print_help(std::cout);
        return 0;
    }

    return dispatch(result);
}