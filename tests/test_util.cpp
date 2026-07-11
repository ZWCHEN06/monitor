#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "index_fund_monitor/util/json_reader.hpp"
#include "index_fund_monitor/util/csv_reader.hpp"

static int failures = 0;
static int tests = 0;

#define CHECK(cond, msg) do { ++tests; if (!(cond)) { std::cerr << "FAIL [" << tests << "]: " << msg << std::endl; ++failures; } else { std::cout << "OK   [" << tests << "]: " << msg << std::endl; } } while(0)
#define CHECK_EQ(a, b, msg) CHECK((a) == (b), msg)

namespace {

std::string fixture(const std::string& name) {
    return (std::filesystem::path(__FILE__).parent_path() / "fixtures" / name).string();
}

// ===== JSON Tests =====

void test_json_parse_normal() {
    JsonReader reader;
    auto r = reader.parse_string(R"({"name":"test","value":42,"pi":3.14,"ok":true})");
    CHECK(r.success, "json parse normal success");
    CHECK(reader.is_object(), "json is object");
    CHECK(reader.has_field("name"), "has field name");
    CHECK_EQ(reader.read_string("name").value_or(""), "test", "read string");
    CHECK_EQ(reader.read_int("value").value_or(0), 42, "read int");
    CHECK_EQ(reader.read_double("pi").value_or(0), 3.14, "read double");
    CHECK(reader.read_bool("ok").value_or(false), "read bool true");
}

void test_json_parse_file() {
    JsonReader reader;
    auto r = reader.parse_file(fixture("fund_directory_success.json"));
    CHECK(r.success, "json parse file success");
    CHECK(reader.is_array(), "file content is array");
    CHECK_EQ(reader.array_size(), 3u, "array has 3 elements");
}

void test_json_missing_field() {
    JsonReader reader;
    reader.parse_string(R"({"a":1})");
    CHECK(!reader.has_field("b"), "missing field not found");
    CHECK(!reader.read_string("b").has_value(), "missing string returns nullopt");
    CHECK(!reader.read_double("b").has_value(), "missing double returns nullopt");
}

void test_json_null_field() {
    JsonReader reader;
    reader.parse_string(R"({"a":null})");
    CHECK(!reader.has_field("a"), "null field treated as missing");
}

void test_json_type_error() {
    JsonReader reader;
    reader.parse_string(R"({"val":"not_a_number"})");
    auto d = reader.read_double("val");
    CHECK(!d.has_value(), "type mismatch returns nullopt");
    auto err = reader.last_error();
    CHECK(!err.message.empty(), "error message for type mismatch");
    CHECK(err.message.find("类型错误") != std::string::npos, "error mentions type error");
}

void test_json_int_type_error() {
    JsonReader reader;
    reader.parse_string(R"({"val":3.14})");
    auto i = reader.read_int("val");
    CHECK(!i.has_value(), "float can't be read as int");
}

void test_json_corrupted() {
    JsonReader reader;
    auto r = reader.parse_string("{invalid json}");
    CHECK(!r.success, "corrupted json fails");
    CHECK(!r.error.message.empty(), "error message present");
}

void test_json_empty_object() {
    JsonReader reader;
    reader.parse_string("{}");
    CHECK(reader.is_object(), "empty object");
    CHECK(!reader.has_field("anything"), "no fields in empty object");
}

void test_json_array_access() {
    JsonReader reader;
    reader.parse_string(R"(["a","b","c"])");
    CHECK(reader.is_array(), "is array");
    CHECK_EQ(reader.array_size(), 3u, "array size 3");
    CHECK_EQ(reader.read_string_at(0).value_or(""), "a", "first element");
    CHECK_EQ(reader.read_string_at(2).value_or(""), "c", "third element");
    CHECK(!reader.read_string_at(5).has_value(), "out of bounds returns nullopt");
}

// ===== CSV Tests =====

void test_csv_normal() {
    CsvReader reader;
    auto r = reader.parse_string("a,b,c\n1,2,3\n4,5,6");
    CHECK(r.success, "csv normal parse");
    CHECK_EQ(r.headers.size(), 3u, "3 headers");
    CHECK_EQ(r.headers[0], "a", "first header");
    CHECK_EQ(r.rows.size(), 2u, "2 data rows");
    CHECK_EQ(r.rows[0][1], "2", "row 0 col 1");
}

void test_csv_quoted_fields() {
    CsvReader reader;
    auto r = reader.parse_string(R"("name","desc")" "\n" R"("hello","world")");
    CHECK(r.success, "csv quoted parse");
    CHECK_EQ(r.headers[0], "name", "quoted header");
    CHECK_EQ(r.rows[0][0], "hello", "quoted value");
}

void test_csv_commas_in_quotes() {
    CsvReader reader;
    auto r = reader.parse_string(R"(code,desc)" "\n" R"(510300,"沪深300,ETF")");
    CHECK(r.success, "csv commas in quotes");
    CHECK_EQ(r.rows.size(), 1u, "one row");
    CHECK_EQ(r.rows[0][0], "510300", "first field");
    CHECK_EQ(r.rows[0][1], "沪深300,ETF", "field with comma");
}

void test_csv_escaped_quotes() {
    CsvReader reader;
    auto r = reader.parse_string(R"(desc)" "\n" R"("say ""hello""")");
    CHECK(r.success, "csv escaped quotes");
    CHECK_EQ(r.rows[0][0], R"(say "hello")", "escaped quotes resolved");
}

void test_csv_empty_fields() {
    CsvReader reader;
    auto r = reader.parse_string("a,b,c\n,,\n1,,3");
    CHECK(r.success, "csv empty fields");
    CHECK_EQ(r.rows[0].size(), 3u, "3 empty fields");
    CHECK(r.rows[0][0].empty(), "first field empty");
    CHECK_EQ(r.rows[1][0], "1", "first non-empty");
    CHECK(r.rows[1][1].empty(), "middle field empty");
}

void test_csv_crlf() {
    CsvReader reader;
    auto r = reader.parse_string("a,b\r\n1,2\r\n3,4\r\n");
    CHECK(r.success, "csv crlf parse");
    CHECK_EQ(r.rows.size(), 2u, "2 rows with CRLF");
}

void test_csv_bom() {
    // Create file with BOM
    auto path = fixture("sample_bom_test.csv");
    {
        std::ofstream file(path, std::ios::binary);
        unsigned char bom[] = {0xEF, 0xBB, 0xBF};
        file.write(reinterpret_cast<char*>(bom), 3);
        file << "a,b\n1,2\n";
    }

    CsvReader reader;
    auto r = reader.parse_file(path);
    CHECK(r.success, "csv with BOM parse");
    CHECK_EQ(r.headers[0], "a", "BOM header correct");
    CHECK_EQ(r.rows[0][0], "1", "BOM data correct");

    std::filesystem::remove(path);
}

void test_csv_from_file() {
    CsvReader reader;
    auto r = reader.parse_file(fixture("sample.csv"));
    CHECK(r.success, "csv file parse");
    CHECK_EQ(r.headers.size(), 3u, "3 headers from file");
    CHECK_EQ(r.rows.size(), 3u, "3 rows from file");
    CHECK_EQ(r.rows[0][0], "510300", "first code from file");
}

void test_csv_chinese() {
    CsvReader reader;
    auto r = reader.parse_file(fixture("sample_quoted.csv"));
    CHECK(r.success, "csv chinese parse");
    CHECK_EQ(r.rows[0][1], "沪深300ETF", "chinese name");
    CHECK_EQ(r.rows[1][2], "含有,逗号的描述", "chinese with comma");
    CHECK_EQ(r.rows[2][2], R"(他说"你好"啊)", "chinese with escaped quotes");
}

void test_csv_empty_lines() {
    CsvReader reader;
    auto r = reader.parse_string("a,b\n1,2\n\n\n");
    CHECK(r.success, "csv with trailing empty lines");
    CHECK_EQ(r.rows.size(), 1u, "only 1 data row");
}

void test_csv_single_line() {
    CsvReader reader;
    auto r = reader.parse_string("a,b,c");
    CHECK(r.success, "csv headers only");
    CHECK_EQ(r.headers.size(), 3u, "3 headers");
    CHECK(r.rows.empty(), "no data rows");
}

void test_csv_empty_content() {
    CsvReader reader;
    auto r = reader.parse_string("");
    CHECK(!r.success, "empty csv fails");
}

} // namespace

int main() {
    // JSON tests
    test_json_parse_normal();
    test_json_parse_file();
    test_json_missing_field();
    test_json_null_field();
    test_json_type_error();
    test_json_int_type_error();
    test_json_corrupted();
    test_json_empty_object();
    test_json_array_access();

    // CSV tests
    test_csv_normal();
    test_csv_quoted_fields();
    test_csv_commas_in_quotes();
    test_csv_escaped_quotes();
    test_csv_empty_fields();
    test_csv_crlf();
    test_csv_bom();
    test_csv_from_file();
    test_csv_chinese();
    test_csv_empty_lines();
    test_csv_single_line();
    test_csv_empty_content();

    std::cout << "\n=== " << tests << " tests, " << failures << " failures ===" << std::endl;
    return failures > 0 ? 1 : 0;
}