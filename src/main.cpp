#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

#include "index_fund_monitor/cli/parser.hpp"
#include "index_fund_monitor/db/benchmark_repository.hpp"
#include "index_fund_monitor/db/database.hpp"
#include "index_fund_monitor/db/fund_repository.hpp"
#include "index_fund_monitor/db/schema.hpp"
#include "index_fund_monitor/models/validation.hpp"
#include "index_fund_monitor/update/updater.hpp"
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
        std::cerr << "错误：无法打开数据库 " << db_path << "：" << db.last_error() << std::endl;
        return db;
    }

    if (!initialize_schema(db)) {
        std::cerr << "错误：初始化数据库结构失败。" << std::endl;
        db.close();
        return db;
    }

    out_path = std::move(db_path);
    return db;
}

void print_help(std::ostream& os) {
    os << PROJECT_NAME << " - " << PROJECT_DESC << "\n"
       << "\n"
       << "用法：\n"
       << "  " << PROJECT_NAME << " <命令> [选项]\n"
       << "\n"
       << "命令：\n"
       << "  init              初始化数据库\n"
       << "  fund add          添加ETF到关注列表\n"
       << "  fund list         查看所有已关注ETF\n"
       << "  fund remove       从关注列表移除ETF\n"
       << "  update            更新估值数据\n"
       << "  show              查看ETF详情\n"
       << "  screen            按条件筛选ETF\n"
<< "  alert add         添加预警规则\n"
        << "  alert list        查看预警规则\n"
        << "  alert remove      删除预警规则\n"
        << "  alert check       检查预警规则\n"
        << "  benchmark set     设置跟踪指数映射\n"
        << "  benchmark show    查看跟踪指数映射\n"
        << "  export            导出数据到CSV\n"
       << "\n"
       << "通用选项：\n"
       << "  --help            显示此帮助信息\n"
       << "  --version         显示版本信息\n"
       << "  --database <路径> 数据库文件路径\n"
       << "  --market  <代码>  市场代码（CN）\n"
       << "  --symbol  <代码>  ETF代码（6位数字）\n"
<< "  --name    <名称>  ETF名称\n"
        << "  --output  <文件>  输出文件路径\n"
        << "  --benchmark-code <代码>  跟踪指数代码\n"
        << "  --benchmark-name <名称>  跟踪指数名称\n"
        << "  --provider <名称>  指数提供商\n"
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
    std::cout << "数据库已初始化：" << db_path << "\n"
              << "数据库版本：" << version << std::endl;
    return 0;
}

int cmd_fund_add(const ParseResult& result) {
    std::string symbol = result.options.at("--symbol");
    std::string name = result.options.at("--name");

    if (!is_valid_etf_code(symbol)) {
        std::cerr << "错误：无效的ETF代码 '" << symbol << "'，必须是6位数字。" << std::endl;
        return 1;
    }

    auto exchange = deduce_exchange(symbol);
    if (!exchange.has_value()) {
        std::cerr << "错误：无法确定代码 '" << symbol << "' 所属交易所。" << std::endl;
        return 1;
    }

    std::string db_path;
    auto db = open_db(result, db_path);
    if (!db.is_open()) return 1;

    FundRepository repo(db);
    if (repo.exists(symbol, exchange.value())) {
        std::cerr << "错误：ETF " << symbol << " 已在关注列表中。" << std::endl;
        return 1;
    }

    if (!repo.add(symbol, exchange.value(), name)) {
        std::cerr << "错误：添加ETF " << symbol << " 失败。" << std::endl;
        return 1;
    }

    std::cout << "已添加：" << to_string(exchange.value()) << "." << symbol << " " << name << std::endl;
    return 0;
}

int cmd_fund_list(const ParseResult& result) {
    std::string db_path;
    auto db = open_db(result, db_path);
    if (!db.is_open()) return 1;

    FundRepository repo(db);
    auto records = repo.list();

    if (records.empty()) {
        std::cout << "关注列表为空。" << std::endl;
        return 0;
    }

    std::cout << "ETF关注列表：\n";
    for (const auto& r : records) {
        std::cout << "  " << to_string(r.exchange) << "." << r.symbol << "  " << r.name << std::endl;
    }
    return 0;
}

int cmd_fund_remove(const ParseResult& result) {
    std::string symbol = result.options.at("--symbol");

    auto exchange = deduce_exchange(symbol);
    if (!exchange.has_value()) {
        std::cerr << "错误：无法确定代码 '" << symbol << "' 所属交易所。" << std::endl;
        return 1;
    }

    std::string db_path;
    auto db = open_db(result, db_path);
    if (!db.is_open()) return 1;

    FundRepository repo(db);
    if (!repo.exists(symbol, exchange.value())) {
        std::cerr << "错误：ETF " << symbol << " 不在关注列表中。" << std::endl;
        return 1;
    }

    if (!repo.remove(symbol, exchange.value())) {
        std::cerr << "错误：移除ETF " << symbol << " 失败。" << std::endl;
        return 1;
    }

    std::cout << "已移除：" << to_string(exchange.value()) << "." << symbol << std::endl;
    return 0;
}

int cmd_update(const ParseResult& result) {
    bool has_all = result.options.contains("--all");
    bool has_symbol = result.options.contains("--symbol");

    if (!has_all && !has_symbol) {
        std::cerr << "错误：update 命令需要 --all 或 --symbol 参数。" << std::endl;
        return 1;
    }

    std::string db_path;
    auto db = open_db(result, db_path);
    if (!db.is_open()) return 1;

    Updater updater(db, "tests/fixtures");
    UpdateSummary summary;

    if (has_symbol) {
        summary = updater.update_symbol(result.options.at("--symbol"));
    } else {
        summary = updater.update_all();
    }

    summary.print();
    return summary.failed > 0 ? 1 : 0;
}

int cmd_benchmark_set(const ParseResult& result) {
    std::string symbol = result.options.at("--symbol");
    std::string code = result.options.at("--benchmark-code");
    std::string name = result.options.at("--benchmark-name");

    if (!is_valid_etf_code(symbol)) {
        std::cerr << "错误：无效的ETF代码 '" << symbol << "'，必须是6位数字。" << std::endl;
        return 1;
    }

    auto exchange = deduce_exchange(symbol);
    if (!exchange.has_value()) {
        std::cerr << "错误：无法确定代码 '" << symbol << "' 所属交易所。" << std::endl;
        return 1;
    }

    std::string db_path;
    auto db = open_db(result, db_path);
    if (!db.is_open()) return 1;

    FundRepository fund_repo(db);
    if (!fund_repo.exists(symbol, exchange.value())) {
        std::cerr << "错误：ETF " << symbol << " 不在关注列表中，请先使用 fund add 添加。" << std::endl;
        return 1;
    }

    std::string provider = result.options.contains("--provider") ? result.options.at("--provider") : "";

    BenchmarkRepository bench_repo(db);
    if (bench_repo.exists(symbol)) {
        auto current = bench_repo.get_current(symbol);
        if (current.has_value()) {
            std::cout << "警告：ETF " << symbol
                      << " 已有跟踪指数映射：" << current->benchmark_code
                      << " " << current->benchmark_name
                      << "（来源：" << current->source << "）" << std::endl;
        }
    }

    if (!bench_repo.set(symbol, code, name, provider, "manual")) {
        std::cerr << "错误：设置跟踪指数映射失败。" << std::endl;
        return 1;
    }

    std::cout << "已设置：" << symbol << " → " << code << " " << name;
    if (!provider.empty()) {
        std::cout << "（" << provider << "）";
    }
    std::cout << std::endl;
    return 0;
}

int cmd_benchmark_show(const ParseResult& result) {
    std::string symbol = result.options.at("--symbol");

    std::string db_path;
    auto db = open_db(result, db_path);
    if (!db.is_open()) return 1;

    BenchmarkRepository repo(db);
    auto current = repo.get_current(symbol);

    if (!current.has_value()) {
        std::cout << "ETF " << symbol << " 没有跟踪指数映射。" << std::endl;
        return 0;
    }

    std::cout << "ETF " << symbol << " 的跟踪指数映射：" << std::endl;
    std::cout << "  指数代码：" << current->benchmark_code << std::endl;
    std::cout << "  指数名称：" << current->benchmark_name << std::endl;
    if (!current->provider.empty()) {
        std::cout << "  指数提供商：" << current->provider << std::endl;
    }
    std::cout << "  生效日期：" << current->effective_date << std::endl;
    std::cout << "  数据来源：" << current->source << std::endl;

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
            return cmd_update(result);
        case CommandType::BenchmarkSet:
            return cmd_benchmark_set(result);
        case CommandType::BenchmarkShow:
            return cmd_benchmark_show(result);
        case CommandType::Show:
        case CommandType::Screen:
        case CommandType::AlertAdd:
        case CommandType::AlertList:
        case CommandType::AlertRemove:
        case CommandType::AlertCheck:
        case CommandType::Export:
            std::cout << "命令 '" << Parser::command_name(result.command) << "' 尚未实现。" << std::endl;
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
        std::cerr << "错误：" << result.error << "\n"
                  << "使用 --help 查看帮助信息。" << std::endl;
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