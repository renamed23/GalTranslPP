module;

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif
#include <spdlog/spdlog.h>
#include <boost/regex.hpp>
#include <cpr/cpr.h>

export module API_Tool;

export import Tool;
export import ITranslator;

using json = nlohmann::json;

export {
    struct TranslationApi {
        std::string apikey;
        std::string apiurl;
        std::string modelName;
        std::optional<double> temperature;
        std::optional<double> topP;
        std::optional<double> frequencyPenalty;
        std::optional<double> presencePenalty;
        std::chrono::steady_clock::time_point lastReportTime = std::chrono::steady_clock::now();
        int reportCount = 0;
        bool stream;
    };

    struct ApiResponse {
        bool success = false;
        std::string content; // 成功时的内容 或 失败时的错误信息
        long statusCode = 0;   // HTTP 状态码
    };

    ApiResponse performApiRequest(json& payload, const TranslationApi& api, const std::function<std::string(std::string)>& onPerformApi, int threadId,
        std::shared_ptr<IController> controller, std::shared_ptr<spdlog::logger> logger, int apiTimeOutMs);

    std::string cvt2StdApiUrl(const std::string& url);
}


module :private;

#ifdef _WIN32
// Windows下获取系统代理的辅助函数
std::string getSystemProxyUrl() {
    std::string proxyUrl;
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxyConfig;
    // 获取当前用户的代理配置
    if (WinHttpGetIEProxyConfigForCurrentUser(&proxyConfig)) {
        if (proxyConfig.lpszProxy) {
            // lpszProxy 是宽字符 (wstring)，需要转换为 string
            proxyUrl = wide2Ascii(proxyConfig.lpszProxy);
            // 这里的 proxyUrl 可能是 "127.0.0.1:7890" 这种格式
            // 或者是 "http=127.0.0.1:7890;https=..." 这种复杂格式
            // 为了简单起见，如果包含分号，我们只取第一个
            if (size_t pos = proxyUrl.find(';'); pos != std::string::npos) {
                proxyUrl = proxyUrl.substr(0, pos);
            }
            // 如果没有协议头，补上 http:// (cpr需要)
            if (!proxyUrl.contains("://") && !proxyUrl.contains("=")) {
                proxyUrl = "http://" + proxyUrl;
            }
            GlobalFree(proxyConfig.lpszProxy);
        }
        if (proxyConfig.lpszAutoConfigUrl) GlobalFree(proxyConfig.lpszAutoConfigUrl);
        if (proxyConfig.lpszProxyBypass) GlobalFree(proxyConfig.lpszProxyBypass);
    }
    return proxyUrl;
}
#else
// Linux/Mac 下获取环境变量
std::string getSystemProxyUrl() {
    const char* proxy = std::getenv("http_proxy");
    if (proxy) return std::string(proxy);
    proxy = std::getenv("HTTP_PROXY");
    if (proxy) return std::string(proxy);
    return "";
}
#endif

ApiResponse performApiRequest(json& payload, const TranslationApi& api, const std::function<std::string(std::string)>& onPerformApi, int threadId,
    std::shared_ptr<IController> controller, std::shared_ptr<spdlog::logger> logger, const int apiTimeOutMs) {
    ApiResponse apiResponse;

    payload["model"] = api.modelName;
    if (api.temperature.has_value()) {
        payload["temperature"] = api.temperature.value();
    }
    if (api.topP.has_value()) {
        payload["top_p"] = api.topP.value();
    }
    if (api.frequencyPenalty.has_value()) {
        payload["frequency_penalty"] = api.frequencyPenalty.value();
    }
    if (api.presencePenalty.has_value()) {
        payload["presence_penalty"] = api.presencePenalty.value();
    }
    if (api.stream) {
        payload["stream"] = true;
    }

    std::string payloadStr = payload.dump();
    if (onPerformApi) {
        payloadStr = onPerformApi(payloadStr);
    }

    cpr::Proxies proxies;
    if (std::string systemProxy = getSystemProxyUrl(); !systemProxy.empty()) {
        // 同时设置 http 和 https 代理
        logger->debug("使用系统代理: [{}]", systemProxy);
        proxies = cpr::Proxies{ {"http", systemProxy}, {"https", systemProxy} };
    }

    if (api.stream) {
        // =================================================
        // ===========   流式请求处理路径   ================
        // =================================================
        std::string concatenatedContent;
        std::string sseBuffer;

        // 1. 定义一个符合 cpr::WriteCallback 构造函数要求的 lambda
        auto callbackLambda = [&](std::string_view data, intptr_t userdata) -> bool
            {
                // 将接收到的数据块(string_view)追加到缓冲区(string)
                logger->trace("[线程 {}] 接收到流式数据块: {}", threadId, replaceStr(std::string(data), "\n", "[n]"));
                sseBuffer.append(data);
                size_t pos;
                while ((pos = sseBuffer.find('\n')) != std::string::npos) {
                    std::string line = sseBuffer.substr(0, pos);
                    sseBuffer.erase(0, pos + 1);

                    if (line.starts_with("data: ")) {
                        std::string jsonDataStr = line.substr(6);
                        if (jsonDataStr == "[DONE]") {
                            return true;
                        }
                        try {
                            json chunk = json::parse(jsonDataStr);
                            json& contentNode = chunk["choices"][0]["delta"]["content"];
                            if (contentNode.is_string()) {
                                concatenatedContent += contentNode.get<std::string>();
                            }
                        }
                        catch (...) {
                            
                        }
                    }
                }
                // 继续接收数据
                return !controller->shouldStop();
            };

        // 2. 使用上面定义的 lambda 来构造一个 cpr::WriteCallback 类的实例
        cpr::WriteCallback writeCallbackInstance(callbackLambda);

        // 3. 将该实例传递给 cpr::Post
        cpr::Response response = cpr::Post(
            cpr::Url{ api.apiurl },
            cpr::Body{ payloadStr },
            cpr::Header{ {"Content-Type", "application/json"}, {"Authorization", "Bearer " + api.apikey} },
            cpr::Timeout{ apiTimeOutMs },
            writeCallbackInstance, // 传递类的实例
            proxies
        );

        apiResponse.statusCode = response.status_code;
        if (response.status_code == 200) {
            apiResponse.success = true;
            apiResponse.content = concatenatedContent;
        }
        else {
            apiResponse.success = false;
            apiResponse.content = response.text;
            logger->error("[线程 {}] API 流式请求失败，状态码: {}, 错误: {}", threadId, response.status_code, response.text);
        }
    }
    else {
        // =================================================
        // =========   非流式请求处理路径   =========
        // =================================================
        cpr::Response response = cpr::Post(
            cpr::Url{ api.apiurl },
            cpr::Body{ payloadStr },
            cpr::Header{ {"Content-Type", "application/json"}, {"Authorization", "Bearer " + api.apikey} },
            cpr::Timeout{ apiTimeOutMs },
            proxies
        );

        apiResponse.statusCode = response.status_code;
        apiResponse.content = response.text; // 先记录原始响应体

        if (response.status_code == 200) {
            try {
                // 解析完整的JSON响应
                apiResponse.content = json::parse(response.text)["choices"][0]["message"]["content"];
                apiResponse.success = true;
            }
            catch (const json::exception& e) {
                logger->error("[线程 {}] 成功响应但JSON解析失败: {}, 错误: {}", threadId, response.text, e.what());
                apiResponse.success = false;
            }
        }
        else {
            logger->error("[线程 {}] API 非流请求失败，状态码: {}, 错误: {}", threadId, response.status_code, response.text);
            apiResponse.success = false;
        }
    }

    return apiResponse;
}

std::string cvt2StdApiUrl(const std::string& url) {
    std::string ret = url;
    if (ret.ends_with("/")) {
        ret.pop_back();
    }
    if (ret.ends_with("/chat/completions")) {
        return ret;
    }
    if (ret.ends_with("/chat")) {
        return ret + "/completions";
    }
    if (boost::regex_search(ret, boost::regex(R"(/v\d+$)"))) {
        return ret + "/chat/completions";
    }
    return ret + "/v1/chat/completions";
}
