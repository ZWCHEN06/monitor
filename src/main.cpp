#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

#include "index_fund_monitor/cli/parser.hpp"
#include "index_fund_monitor/db/database.hpp"
#include "index_fund_monitor/db/schema.hpp"
#include "index_fund_monitor/version.hpp"

namespace {

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
    auto it = result.options.find("--database");
    std::string db_path = (it != result.options.end()) ? it->second : default_database_path();

    // Auto-create parent directory
    std::error_code ec;
    auto parent = std::filesystem::path(db_path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            std::cerr << "Error: Cannot create directory " << parent << ": " << ec.message() << std::endl;
            return 1;
        }
    }

    Database db;
    if (!db.open(db_path)) {
        std::cerr << "Error: Cannot open database " << db_path << ": " << db.last_error() << std::endl;
        return 1;
    }

    if (!initialize_schema(db)) {
        std::cerr << "Error: Failed to initialize schema." << std::endl;
        return 1;
    }

    int version = get_schema_version(db);
    std::cout << "Database initialized: " << db_path << "\n"
              << "Schema version: " << version << std::endl;
    return 0;
}

int dispatch(const ParseResult& result) {
    switch (result.command) {
        case CommandType::Init:
            return cmd_init(result);
        case CommandType::FundAdd:
        case CommandType::FundList:
        case CommandType::FundRemove:
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