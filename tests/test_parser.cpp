#include <cstdlib>
#include <iostream>
#include <string>

#include "index_fund_monitor/cli/parser.hpp"

static int failures = 0;
static int tests = 0;

#define CHECK(cond, msg) do { \
    ++tests; \
    if (!(cond)) { \
        std::cerr << "FAIL [" << tests << "]: " << msg << std::endl; \
        ++failures; \
    } else { \
        std::cout << "OK   [" << tests << "]: " << msg << std::endl; \
    } \
} while(0)

#define CHECK_EQ(a, b, msg) CHECK((a) == (b), msg)

namespace {

// Helper to build argv
struct Argv {
    std::vector<std::string> storage;
    std::vector<char*> ptrs;
    int argc;

    Argv(std::initializer_list<std::string> args) : storage(args), argc(static_cast<int>(args.size())) {
        for (auto& s : storage) {
            ptrs.push_back(s.data());
        }
    }
};

void test_correct_commands() {
    Parser p;

    // init
    {
        Argv a{"prog", "init"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK_EQ(r.command, CommandType::Init, "init command");
        CHECK(r.error.empty(), "init no error");
    }

    // fund add
    {
        Argv a{"prog", "fund", "add", "--market", "CN", "--symbol", "510300", "--name", "TestETF"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK_EQ(r.command, CommandType::FundAdd, "fund add command");
        CHECK_EQ(r.options["--market"], "CN", "fund add --market");
        CHECK_EQ(r.options["--symbol"], "510300", "fund add --symbol");
        CHECK_EQ(r.options["--name"], "TestETF", "fund add --name");
        CHECK(r.error.empty(), "fund add no error");
    }

    // fund list
    {
        Argv a{"prog", "fund", "list"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK_EQ(r.command, CommandType::FundList, "fund list command");
        CHECK(r.error.empty(), "fund list no error");
    }

    // fund remove
    {
        Argv a{"prog", "fund", "remove", "--market", "CN", "--symbol", "510300"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK_EQ(r.command, CommandType::FundRemove, "fund remove command");
        CHECK(r.error.empty(), "fund remove no error");
    }

    // update
    {
        Argv a{"prog", "update", "--all"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK_EQ(r.command, CommandType::Update, "update command");
        CHECK(r.error.empty(), "update no error");
    }

    // show
    {
        Argv a{"prog", "show", "--symbol", "510300"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK_EQ(r.command, CommandType::Show, "show command");
        CHECK(r.error.empty(), "show no error");
    }

    // screen
    {
        Argv a{"prog", "screen", "--market", "CN"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK_EQ(r.command, CommandType::Screen, "screen command");
        CHECK(r.error.empty(), "screen no error");
    }

    // alert add
    {
        Argv a{"prog", "alert", "add", "--symbol", "510300"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK_EQ(r.command, CommandType::AlertAdd, "alert add command");
        CHECK(r.error.empty(), "alert add no error");
    }

    // alert list
    {
        Argv a{"prog", "alert", "list"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK_EQ(r.command, CommandType::AlertList, "alert list command");
        CHECK(r.error.empty(), "alert list no error");
    }

    // alert remove
    {
        Argv a{"prog", "alert", "remove", "--symbol", "510300"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK_EQ(r.command, CommandType::AlertRemove, "alert remove command");
        CHECK(r.error.empty(), "alert remove no error");
    }

    // alert check
    {
        Argv a{"prog", "alert", "check"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK_EQ(r.command, CommandType::AlertCheck, "alert check command");
        CHECK(r.error.empty(), "alert check no error");
    }

    // export
    {
        Argv a{"prog", "export", "--output", "report.csv"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK_EQ(r.command, CommandType::Export, "export command");
        CHECK(r.error.empty(), "export no error");
    }
}

void test_missing_required() {
    Parser p;

    // fund add missing --symbol
    {
        Argv a{"prog", "fund", "add", "--market", "CN", "--name", "Test"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK(!r.error.empty(), "fund add missing --symbol");
        CHECK(r.error.find("--symbol") != std::string::npos, "error mentions --symbol");
    }

    // fund remove missing --market
    {
        Argv a{"prog", "fund", "remove", "--symbol", "510300"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK(!r.error.empty(), "fund remove missing --market");
    }

    // show missing --symbol
    {
        Argv a{"prog", "show"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK(!r.error.empty(), "show missing --symbol");
    }

    // export missing --output
    {
        Argv a{"prog", "export"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK(!r.error.empty(), "export missing --output");
    }
}

void test_unknown_command() {
    Parser p;

    {
        Argv a{"prog", "unknown"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK(!r.error.empty(), "unknown command");
        CHECK(r.error.find("Unknown command") != std::string::npos, "error mentions Unknown command");
    }

    {
        Argv a{"prog", "fund", "unknown"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK(!r.error.empty(), "unknown subcommand");
    }

    {
        Argv a{"prog", "alert", "unknown"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK(!r.error.empty(), "unknown alert subcommand");
    }
}

void test_unknown_option() {
    Parser p;

    {
        Argv a{"prog", "init", "--invalid"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK(!r.error.empty(), "unknown option");
    }
}

void test_duplicate_option() {
    Parser p;

    {
        Argv a{"prog", "show", "--symbol", "510300", "--symbol", "510050"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK(!r.error.empty(), "duplicate option");
        CHECK(r.error.find("Duplicate") != std::string::npos, "error mentions Duplicate");
    }
}

void test_help_flag() {
    Parser p;

    // standalone --help
    {
        Argv a{"prog", "--help"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK(r.help, "standalone --help");
        CHECK_EQ(r.command, CommandType::None, "standalone --help no command");
        CHECK(r.error.empty(), "standalone --help no error");
    }

    // --help with command
    {
        Argv a{"prog", "init", "--help"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK(r.help, "command with --help");
        CHECK_EQ(r.command, CommandType::Init, "command with --help still parsed");
        CHECK(r.error.empty(), "command with --help no error");
    }
}

void test_version_flag() {
    Parser p;

    // standalone --version
    {
        Argv a{"prog", "--version"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK(r.version, "standalone --version");
        CHECK_EQ(r.command, CommandType::None, "standalone --version no command");
        CHECK(r.error.empty(), "standalone --version no error");
    }
}

void test_no_args() {
    Parser p;

    {
        Argv a{"prog"};
        auto r = p.parse(a.argc, a.ptrs.data());
        CHECK_EQ(r.command, CommandType::None, "no args command None");
        CHECK(!r.help, "no args help false");
        CHECK(!r.version, "no args version false");
        CHECK(r.error.empty(), "no args no error");
    }
}

} // namespace

int main() {
    test_correct_commands();
    test_missing_required();
    test_unknown_command();
    test_unknown_option();
    test_duplicate_option();
    test_help_flag();
    test_version_flag();
    test_no_args();

    std::cout << "\n=== " << tests << " tests, " << failures << " failures ===" << std::endl;
    return failures > 0 ? 1 : 0;
}