#ifndef INDEX_FUND_MONITOR_NET_HTTP_CLIENT_HPP
#define INDEX_FUND_MONITOR_NET_HTTP_CLIENT_HPP

#include <chrono>
#include <cstdint>
#include <string>

struct HttpRequestOptions {
    std::string url;
    std::string user_agent = "index_fund_monitor/0.1.0";
    std::chrono::milliseconds connect_timeout{5000};
    std::chrono::milliseconds total_timeout{15000};
    std::int64_t max_response_size = 5 * 1024 * 1024;
    int max_retries = 3;
};

struct HttpResponse {
    bool success = false;
    int status_code = 0;
    std::string body;
    std::string error_message;
    bool is_timeout = false;
    bool is_connection_error = false;
    bool is_size_limit = false;
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual HttpResponse get(const HttpRequestOptions& options) = 0;
};

class HttpClient : public IHttpClient {
public:
    HttpClient();
    ~HttpClient() override;

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    HttpResponse get(const HttpRequestOptions& options) override;

private:
    void* handle_ = nullptr;
};

class MockHttpClient : public IHttpClient {
public:
    HttpResponse get(const HttpRequestOptions& options) override;

    void set_response(HttpResponse resp);
    void set_fail_count(int count);

private:
    HttpResponse canned_;
    int call_count_ = 0;
    int fail_before_ = 0;
};

#endif