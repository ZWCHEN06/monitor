#ifndef INDEX_FUND_MONITOR_CLI_PARSER_HPP
#define INDEX_FUND_MONITOR_CLI_PARSER_HPP

#include <string>
#include <unordered_map>
#include <vector>

enum class CommandType {
    None,
    Init,
    FundAdd,
    FundList,
    FundRemove,
    BenchmarkSet,
    BenchmarkShow,
    Update,
    Show,
    Screen,
    AlertAdd,
    AlertList,
    AlertRemove,
    AlertCheck,
    Export,
};

struct ParseResult {
    CommandType command = CommandType::None;
    std::unordered_map<std::string, std::string> options;
    bool help = false;
    bool version = false;
    std::string error;
};

class Parser {
public:
    ParseResult parse(int argc, char* argv[]);
    static std::string command_name(CommandType cmd);

private:
    static bool is_option(std::string_view arg);
    static std::vector<std::string_view> required_options(CommandType cmd);
};

#endif