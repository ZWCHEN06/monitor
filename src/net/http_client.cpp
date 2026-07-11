#include "index_fund_monitor/net/http_client.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cstring>
#include <string>

namespace {

std::size_t write_callback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* body = static_cast<std::string*>(userdata);
    std::size_t total = size * nmemb;
    body->append(ptr, total);
    return total;
}

} // namespace

HttpClient::HttpClient() {
    handle_ = curl_easy_init();
}

HttpClient::~HttpClient() {
    if (handle_) {
        curl_easy_cleanup(handle_);
    }
}

HttpResponse HttpClient::get(const HttpRequestOptions& options) {
    HttpResponse resp;

    if (!handle_) {
        resp.error_message = "无法初始化HTTP客户端";
        return resp;
    }

    CURL* curl = static_cast<CURL*>(handle_);

    for (int attempt = 0; attempt <= options.max_retries; ++attempt) {
        std::string body;
        curl_easy_setopt(curl, CURLOPT_URL, options.url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, options.user_agent.c_str());
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(options.connect_timeout.count()));
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(options.total_timeout.count()));
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

        // Size limit
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 128L * 1024L);

        CURLcode res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status_code);

            // Check size limit
            if (static_cast<std::int64_t>(body.size()) > options.max_response_size) {
                resp.success = false;
                resp.is_size_limit = true;
                resp.error_message = "响应大小超过限制 (" + std::to_string(options.max_response_size) + " bytes)";
                return resp;
            }

            resp.success = true;
            resp.body = std::move(body);
            return resp;
        }

        // Handle error
        resp.body.clear();

        if (res == CURLE_OPERATION_TIMEDOUT) {
            resp.is_timeout = true;
            resp.error_message = "请求超时";
        } else if (res == CURLE_COULDNT_CONNECT || res == CURLE_COULDNT_RESOLVE_HOST) {
            resp.is_connection_error = true;
            resp.error_message = "连接失败：" + std::string(curl_easy_strerror(res));
        } else {
            resp.error_message = "请求失败：" + std::string(curl_easy_strerror(res));
        }

        // Check HTTP status code for retry decisions
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        resp.status_code = static_cast<int>(http_code);

        // 4xx should not retry
        if (http_code >= 400 && http_code < 500) {
            resp.error_message = "HTTP " + std::to_string(http_code) + " 错误";
            return resp;
        }

        // Last attempt, don't retry
        if (attempt == options.max_retries) {
            return resp;
        }
    }

    return resp;
}