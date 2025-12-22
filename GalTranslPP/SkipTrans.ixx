module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"
#include <spdlog/spdlog.h>
#include <unicode/unistr.h>
#include <unicode/uchar.h>
#include <unicode/regex.h>
#include <cpp-base64/base64.h>
#include <toml.hpp>
#include <sol/sol.hpp>

export module SkipTrans;

import Tool;
import ConditionTool;
import PythonManager;
import LuaManager;
export import IPlugin;

namespace fs = std::filesystem;

export {
    class SkipTrans {
    private:
        std::vector<std::string> m_hKeys;
        std::vector<CheckSeCondFunc> m_skipKeys;
        std::shared_ptr<spdlog::logger> m_logger;
        bool m_needReboot = false;
        bool m_skipH;

        void processSkipSentence(Sentence* se, const std::string& info);

    public:
        SkipTrans(const fs::path& projectDir, const toml::value& projectConfig, PythonManager& pythonManager, LuaManager& luaManager,
            std::shared_ptr<spdlog::logger> logger);
        void run(Sentence* se);
        bool needReboot() const { return m_needReboot; }
        ~SkipTrans() = default;
    };
}

module :private;

SkipTrans::SkipTrans(const fs::path& projectDir, const toml::value& projectConfig, PythonManager& pythonManager, LuaManager& luaManager,
    std::shared_ptr<spdlog::logger> logger)
    : m_logger(logger)
{
    try {
        const auto pluginConfig = toml::parse(prePluginConfigPath / L"SkipTrans.toml");

        m_skipH = parseToml<bool>(projectConfig, pluginConfig, "plugins.SkipTrans.skipH");
        if (m_skipH) {
            const auto& hKeysBase64 = parseToml<std::string>(projectConfig, pluginConfig, "plugins.SkipTrans.hKeys");
            std::string hKeysStr = base64_decode(hKeysBase64);
            m_hKeys = splitString(std::move(hKeysStr), '\n');
        }

        const auto& skipKeys = parseToml<toml::array>(projectConfig, pluginConfig, "plugins.SkipTrans.skipKeys");
        for (const auto& elem : skipKeys) {
            if (elem.is_string()) {
                GppConditionPattern pattern;
                pattern.conditionTarget = CachePart::PreprocText;
                icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(elem.as_string());
                UErrorCode status = U_ZERO_ERROR;
                pattern.conditionReg = std::shared_ptr<icu::RegexPattern>(icu::RegexPattern::compile(ustr, 0, status));
                if (U_FAILURE(status)) {
                    throw std::runtime_error(std::format("skipKeys 正则表达式 [{}] 编译失败", elem.as_string()));
                }
                GPPCondition gppCondition{ pattern };
                CheckSeCondFunc checkFunc = [=](const Sentence* se) -> bool
                    {
                        return checkGppCondition(gppCondition, se);
                    };
                m_skipKeys.push_back(std::move(checkFunc));
            }
            else if (elem.is_array() || elem.is_table()) {
                CheckSeCondFunc checkFunc = getCheckSeCondFunc(elem, projectDir, pythonManager, luaManager, m_logger, m_needReboot);
                m_skipKeys.push_back(std::move(checkFunc));
            }
            else {
                throw std::invalid_argument("skipKeys 元素必须是字符串、表或表数组");
            }
        }
        m_logger->info("译前插件 SkipTrans 已加载, skipH: {}",
            m_skipH);
    }
    catch (const toml::exception& e) {
        m_logger->critical("SkipTrans 配置文件解析错误");
        throw std::runtime_error(e.what());
    }
}

void SkipTrans::processSkipSentence(Sentence* se, const std::string& info) {
    se->pre_translated_text = se->pre_processed_text;
    se->other_info["SkipTrans"] = info;
    se->complete = true;
    se->notAnalyzeProblem = true;
}

void SkipTrans::run(Sentence* se) {

    if (
        m_skipH &&
        std::ranges::any_of(m_hKeys, [&](const auto& key)
            {
                if (se->pre_processed_text.contains(key)) {
                    processSkipSentence(se, "skipH: " + key);
                    return true;
                }
                return false;
            })
        ) {
        return;
    }

    for (const auto& [index, key] : m_skipKeys | std::views::enumerate) {
        if (key(se)) {
            processSkipSentence(se, std::format("被第 {} 个 skipKeys 条件匹配到", index + 1));
            return;
        }
    }

}
