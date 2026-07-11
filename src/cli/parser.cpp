#include "index_fund_monitor/cli/parser.hpp"

#include <algorithm>
#include <iostream>
#include <unordered_set>

namespace {

bool is_help_flag(std::string_view arg) {
    return arg == "--help";
}

bool is_version_flag(std::string_view arg) {
    return arg == "--version";
}

bool is_double_dash(std::string_view arg) {
    return arg == "--";
}

CommandType match_command(std::string_view cmd, std::string_view sub) {
    using CT = CommandType;

    if (cmd == "init")   return CT::Init;
    if (cmd == "update") return CT::Update;
    if (cmd == "show")   return CT::Show;
    if (cmd == "screen") return CT::Screen;
    if (cmd == "export") return CT::Export;

    if (cmd == "fund") {
        if (sub == "add")    return CT::FundAdd;
        if (sub == "list")   return CT::FundList;
        if (sub == "remove") return CT::FundRemove;
        return CT::None;
    }

    if (cmd == "alert") {
        if (sub == "add")    return CT::AlertAdd;
        if (sub == "list")   return CT::AlertList;
        if (sub == "remove") return CT::AlertRemove;
        if (sub == "check")  return CT::AlertCheck;
        return CT::None;
    }

    return CT::None;
}

} // namespace

bool Parser::is_option(std::string_view arg) {
    return arg.size() > 2 && arg[0] == '-' && arg[1] == '-';
}

std::string Parser::command_name(CommandType cmd) {
    using CT = CommandType;
    switch (cmd) {
        case CT::Init:        return "init";
        case CT::FundAdd:     return "fund add";
        case CT::FundList:    return "fund list";
        case CT::FundRemove:  return "fund remove";
        case CT::Update:      return "update";
        case CT::Show:        return "show";
        case CT::Screen:      return "screen";
        case CT::AlertAdd:    return "alert add";
        case CT::AlertList:   return "alert list";
        case CT::AlertRemove: return "alert remove";
        case CT::AlertCheck:  return "alert check";
        case CT::Export:      return "export";
        default:              return "";
    }
}

std::vector<std::string_view> Parser::required_options(CommandType cmd) {
    using CT = CommandType;
    switch (cmd) {
        case CT::FundAdd:    return {"--symbol", "--name"};
        case CT::FundRemove: return {"--symbol"};
        case CT::Show:       return {"--symbol"};
        case CT::Export:     return {"--output"};
        default:             return {};
    }
}

namespace {

const std::unordered_set<std::string>& known_options() {
    static const std::unordered_set<std::string> opts = {
        "--database", "--market", "--symbol", "--name",
        "--benchmark-code", "--benchmark-name",
        "--output", "--all",
    };
    return opts;
}

} // namespace

ParseResult Parser::parse(int argc, char* argv[]) {
    ParseResult result;
    std::vector<std::string_view> args;

    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    if (args.empty()) {
        return result;
    }

    // Check for top-level --help or --version before command parsing
    if (args.size() == 1) {
        if (is_help_flag(args[0]))    { result.help = true;    return result; }
        if (is_version_flag(args[0])) { result.version = true; return result; }
    }

    // Parse command
    std::string_view cmd = args[0];
    std::string_view sub;
    size_t idx = 1;

    // Check if next arg is a subcommand (not an option)
    if (idx < args.size() && !is_option(args[idx]) && !is_help_flag(args[idx]) && !is_version_flag(args[idx])) {
        sub = args[idx];
        ++idx;
    }

    result.command = match_command(cmd, sub);

    if (result.command == CommandType::None) {
        result.error = "Unknown command: " + std::string(cmd);
        if (!sub.empty()) {
            result.error += " " + std::string(sub);
        }
        return result;
    }

    // Parse options
    for (; idx < args.size(); ++idx) {
        std::string_view arg = args[idx];

        if (is_help_flag(arg)) {
            result.help = true;
            continue;
        }

        if (is_version_flag(arg)) {
            result.version = true;
            continue;
        }

        if (is_double_dash(arg)) {
            continue;
        }

        if (is_option(arg)) {
            // Check for duplicate
            std::string key(arg);
            if (result.options.contains(key)) {
                result.error = "Duplicate option: " + key;
                return result;
            }

            // Validate against known options
            if (!known_options().contains(key)) {
                result.error = "Unknown option: " + key;
                return result;
            }

            // Look ahead for value (next arg that is not an option)
            if (idx + 1 < args.size() && !is_option(args[idx + 1]) && !is_help_flag(args[idx + 1]) && !is_version_flag(args[idx + 1])) {
                ++idx;
                result.options[key] = std::string(args[idx]);
            } else {
                // Boolean flag (no value)
                result.options[key] = "";
            }
        } else {
            result.error = "Unexpected argument: " + std::string(arg);
            return result;
        }
    }

    // Validate required options
    auto required = required_options(result.command);
    for (auto opt : required) {
        if (!result.options.contains(std::string(opt))) {
            result.error = "Missing required option: " + std::string(opt);
            return result;
        }
    }

    return result;
}