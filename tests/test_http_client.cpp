#include <cstdlib>
#include <iostream>
#include <string>

#include "index_fund_monitor/net/http_client.hpp"

static int failures = 0;
static int tests = 0;

#define CHECK(cond, msg) do { ++tests; if (!(cond)) { std::cerr << "FAIL [" << tests << "]: " << msg << std::endl; ++failures; } else { std::cout << "OK   [" << tests << "]: " << msg << std::endl; } } while(0)
#define CHECK_EQ(a, b, msg) CHECK((a) == (b), msg)

namespace {

void test_mock_success_response() {
    MockHttpClient mock;
    HttpResponse expected;
    expected.success = true;
    expected.status_code = 200;
    expected.body = "{\"key\":\"value\"}";
    mock.set_response(expected);

    HttpRequestOptions opts;
    opts.url = "http://example.com/api";

    auto resp = mock.get(opts);
    CHECK(resp.success, "mock returns success");
    CHECK_EQ(resp.status_code, 200, "status code 200");
    CHECK_EQ(resp.body, "{\"key\":\"value\"}", "response body preserved");
}

void test_mock_retry_then_success() {
    MockHttpClient mock;
    HttpResponse expected;
    expected.success = true;
    expected.status_code = 200;
    expected.body = "ok";
    mock.set_response(expected);
    mock.set_fail_count(2);  // Fail first 2 calls, succeed on 3rd

    HttpRequestOptions opts;
    opts.url = "http://example.com/api";

    // First call fails
    auto r1 = mock.get(opts);
    CHECK(!r1.success, "retry: first call fails");

    // Second call fails
    auto r2 = mock.get(opts);
    CHECK(!r2.success, "retry: second call fails");

    // Third call succeeds
    auto r3 = mock.get(opts);
    CHECK(r3.success, "retry: third call succeeds");
    CHECK_EQ(r3.body, "ok", "retry: correct body after retry");
}

void test_mock_timeout() {
    MockHttpClient mock;
    HttpResponse timeout;
    timeout.is_timeout = true;
    timeout.error_message = "超时";
    mock.set_response(timeout);

    HttpRequestOptions opts;
    opts.url = "http://example.com/slow";

    auto resp = mock.get(opts);
    CHECK(!resp.success, "timeout returns failure");
    CHECK(resp.is_timeout, "timeout flag set");
    CHECK(!resp.error_message.empty(), "error message present");
}

void test_mock_connection_error() {
    MockHttpClient mock;
    HttpResponse conn_err;
    conn_err.is_connection_error = true;
    conn_err.error_message = "连接被拒绝";
    mock.set_response(conn_err);

    HttpRequestOptions opts;
    opts.url = "http://nonexistent.example.com";

    auto resp = mock.get(opts);
    CHECK(!resp.success, "connection error returns failure");
    CHECK(resp.is_connection_error, "connection error flag set");
}

void test_mock_size_limit() {
    MockHttpClient mock;
    HttpResponse size_limit;
    size_limit.is_size_limit = true;
    size_limit.error_message = "响应超过大小限制";
    mock.set_response(size_limit);

    HttpRequestOptions opts;
    opts.url = "http://example.com/large";

    auto resp = mock.get(opts);
    CHECK(!resp.success, "size limit returns failure");
    CHECK(resp.is_size_limit, "size limit flag set");
}

void test_mock_http_4xx_error() {
    MockHttpClient mock;
    HttpResponse http_err;
    http_err.status_code = 404;
    http_err.error_message = "404 未找到";
    mock.set_response(http_err);

    HttpRequestOptions opts;
    opts.url = "http://example.com/notfound";

    auto resp = mock.get(opts);
    CHECK(!resp.success, "404 returns failure");
    CHECK_EQ(resp.status_code, 404, "status code preserved");
}

void test_request_options_configurable() {
    HttpRequestOptions opts;
    opts.url = "http://test.com";
    opts.connect_timeout = std::chrono::milliseconds(1000);
    opts.total_timeout = std::chrono::milliseconds(5000);
    opts.max_response_size = 1024 * 1024;
    opts.max_retries = 1;

    CHECK_EQ(opts.connect_timeout.count(), 1000, "connect timeout set");
    CHECK_EQ(opts.total_timeout.count(), 5000, "total timeout set");
    CHECK_EQ(opts.max_response_size, 1024L * 1024L, "max size set");
    CHECK_EQ(opts.max_retries, 1, "max retries set");
}

void test_http_client_interface_polymorphism() {
    std::unique_ptr<IHttpClient> client = std::make_unique<MockHttpClient>();

    HttpResponse resp;
    resp.success = true;
    resp.status_code = 200;
    resp.body = "data";
    dynamic_cast<MockHttpClient*>(client.get())->set_response(resp);

    HttpRequestOptions opts;
    opts.url = "http://test.com";

    auto result = client->get(opts);
    CHECK(result.success, "polymorphic call works");
    CHECK_EQ(result.body, "data", "data returned through interface");
}

void test_http_client_creation() {
    // Just test that HttpClient can be created (no actual network call)
    HttpClient client;
    CHECK(true, "HttpClient constructed successfully");
}

} // namespace

int main() {
    test_mock_success_response();
    test_mock_retry_then_success();
    test_mock_timeout();
    test_mock_connection_error();
    test_mock_size_limit();
    test_mock_http_4xx_error();
    test_request_options_configurable();
    test_http_client_interface_polymorphism();
    test_http_client_creation();

    std::cout << "\n=== " << tests << " tests, " << failures << " failures ===" << std::endl;
    return failures > 0 ? 1 : 0;
}