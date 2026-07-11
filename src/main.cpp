#include <cstdlib>
#include <iostream>
#include <string_view>

#include "index_fund_monitor/version.hpp"

namespace {

void print_help(std::ostream& os) {
    os << PROJECT_NAME << " - " << PROJECT_DESC << "\n"
       << "\n"
       << "Usage:\n"
       << "  " << PROJECT_NAME << " [options]\n"
       << "\n"
       << "Options:\n"
       << "  --help       Show this help message\n"
       << "  --version    Show version information\n"
       << "\n"
       << "Commands (to be implemented):\n"
       << "  init         Initialize database\n"
       << "  fund         Manage ETF watchlist\n"
       << "  update       Update valuation data\n"
       << "  show         Show ETF details\n"
       << "  screen       Filter ETFs by conditions\n"
       << "  alert        Manage alert rules\n"
       << "  export       Export data to CSV\n"
       << std::endl;
}

void print_version(std::ostream& os) {
    os << PROJECT_NAME << " " << PROJECT_VERSION << std::endl;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 1) {
        print_help(std::cout);
        return 0;
    }

    const std::string_view arg{argv[1]};

    if (arg == "--help") {
        print_help(std::cout);
        return 0;
    }

    if (arg == "--version") {
        print_version(std::cout);
        return 0;
    }

    std::cerr << "Unknown option: " << arg << "\n"
              << "Use --help for usage information." << std::endl;
    return 1;
}