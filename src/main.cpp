#include <cstdlib>
#include <iostream>
#include <string_view>

#include "index_fund_monitor/cli/parser.hpp"
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

void print_not_implemented(std::ostream& os, CommandType cmd) {
    os << "Command '" << Parser::command_name(cmd) << "' is not yet implemented." << std::endl;
}

int dispatch(const ParseResult& result) {
    switch (result.command) {
        case CommandType::Init:
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
            print_not_implemented(std::cout, result.command);
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