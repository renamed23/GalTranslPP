module;

#define PYBIND11_HEADERS
#define PCRE2_HEADERS
#include "GPPMacros.hpp"
#include <spdlog/spdlog.h>
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
        bool m_skipH;

        void processSkipSentence(Sentence* se, const std::string& info);
        void skipImpl(Sentence* se);

        bool m_needReboot = false;
        bool m_hasPreRun;
        bool m_hasRun;

    public:
        SkipTrans(const fs::path& projectDir, const toml::value& projectConfig, PythonManager& pythonManager, LuaManager& luaManager,
            std::shared_ptr<spdlog::logger> logger, bool hasRun, bool hasPreRun);
        void run(Sentence* se);
        void preRun(Sentence* se);
        bool needReboot() const { return m_needReboot; }
        ~SkipTrans() = default;
    };
}

module :private;

SkipTrans::SkipTrans(const fs::path& projectDir, const toml::value& projectConfig, PythonManager& pythonManager, LuaManager& luaManager,
    std::shared_ptr<spdlog::logger> logger, bool hasRun, bool hasPreRun)
    : m_logger(logger), m_hasRun(hasRun), m_hasPreRun(hasPreRun)
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
                pattern.conditionReg.setPattern(elem.as_string()).setModifier(defaultRegCompileModifier).compile();
                if (!pattern.conditionReg) {
                    throw std::runtime_error(std::format("skipKeys 正则表达式 [{}] 编译失败", elem.as_string()));
                }
                GPPCondition gppCondition{ std::move(pattern) };
                CheckSeCondFunc checkFunc = [condr = std::move(gppCondition)](const Sentence* se) -> bool
                    {
                        return checkGppCondition(condr, se);
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

void SkipTrans::skipImpl(Sentence* se) {
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

void SkipTrans::run(Sentence* se) {
    if (!m_hasRun) {
        return;
    }
    skipImpl(se);
}

void SkipTrans::preRun(Sentence* se) {
    if (!m_hasPreRun) {
        return;
    }
    skipImpl(se);
}
