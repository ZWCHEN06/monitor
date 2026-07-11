#include "index_fund_monitor/net/http_client.hpp"

void MockHttpClient::set_response(HttpResponse resp) {
    canned_ = std::move(resp);
}

void MockHttpClient::set_fail_count(int count) {
    fail_before_ = count;
}

HttpResponse MockHttpClient::get(const HttpRequestOptions& options) {
    (void)options;
    ++call_count_;

    if (call_count_ <= fail_before_) {
        HttpResponse fail;
        fail.error_message = "模拟失败 (" + std::to_string(call_count_) + "/" + std::to_string(fail_before_) + ")";
        return fail;
    }

    return canned_;
}