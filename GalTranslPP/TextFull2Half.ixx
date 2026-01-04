module;

#include <spdlog/spdlog.h>
#include <unicode/unistr.h>
#include <unicode/uchar.h>
#include <toml.hpp>

export module TextFull2Half;

import Tool;
export import IPlugin;

namespace fs = std::filesystem;

export {
    class TextFull2Half {
    private:
        std::unordered_map<std::string, std::string> m_customMap;
        std::unordered_map<char32_t, char32_t> m_conversionMap;
        std::shared_ptr<spdlog::logger> m_logger;
        bool m_replacePunctuation;
        bool m_reverseConversion;

        PluginRunTime m_runTime;

        void createConversionMap();
        std::string convertText(const std::string& text, bool jumpTag);

    public:
        TextFull2Half(const toml::value& projectConfig, std::shared_ptr<spdlog::logger> logger, PluginRunTime runTime);
        void dPreRun(Sentence* se);
        void preRun(Sentence* se);
        void postRun(Sentence* se);
        void dPostRun(Sentence* se);
        ~TextFull2Half() = default;
    };
}

module :private;

TextFull2Half::TextFull2Half(const toml::value& projectConfig, std::shared_ptr<spdlog::logger> logger, PluginRunTime runTime)
    : m_logger(logger), m_runTime(runTime)
{
    try {
        fs::path pluginConfigPath = textPluginConfigPath / std::format(L"TextFull2Half-{}.toml", ascii2Wide(pluginRunTimeNames[m_runTime]));
        if (!fs::exists(pluginConfigPath)) {
            pluginConfigPath = textPluginConfigPath / L"TextFull2Half.toml";
        }
        const auto pluginConfig = toml::uparse(pluginConfigPath);

        m_replacePunctuation = parseToml<bool>(projectConfig, pluginConfig, "plugins.TextFull2Half.是否替换标点");
        m_reverseConversion = parseToml<bool>(projectConfig, pluginConfig, "plugins.TextFull2Half.是否反向替换");

        createConversionMap();
        m_logger->info("TextFull2Half-{} 已加载 - 替换标点: {}, 反向替换: {}",
            pluginRunTimeNames[m_runTime], m_replacePunctuation, m_reverseConversion);
    }
    catch (const toml::exception& e) {
        m_logger->critical("TextFull2Half-{} 配置文件解析错误", pluginRunTimeNames[m_runTime]);
        throw std::runtime_error(e.what());
    }
}

void TextFull2Half::createConversionMap() {
    // 数字转换
    for (char32_t i = 0; i < 10; ++i) {
        m_conversionMap[U'０' + i] = U'0' + i;
    }

    // 大写字母
    for (char32_t i = 0; i < 26; ++i) {
        m_conversionMap[U'Ａ' + i] = U'A' + i;
    }

    // 小写字母 
    for (char32_t i = 0; i < 26; ++i) {
        m_conversionMap[U'ａ' + i] = U'a' + i;
    }

    // 标点符号
    if (m_replacePunctuation) {
        m_conversionMap[U'。'] = U'.';
        m_conversionMap[U'，'] = U',';
        m_conversionMap[U'、'] = U'\\';
        m_conversionMap[U'；'] = U';';
        m_conversionMap[U'：'] = U':';
        m_conversionMap[U'？'] = U'?';
        m_conversionMap[U'！'] = U'!';
        m_conversionMap[U'（'] = U'(';
        m_conversionMap[U'）'] = U')';
        m_conversionMap[U'【'] = U'[';
        m_conversionMap[U'】'] = U']';
        m_conversionMap[U'《'] = U'<';
        m_conversionMap[U'》'] = U'>';
        m_conversionMap[U'「'] = U'[';
        m_conversionMap[U'」'] = U']';
        m_conversionMap[U'『'] = U'{';
        m_conversionMap[U'』'] = U'}';
        m_conversionMap[U'〔'] = U'[';
        m_conversionMap[U'〕'] = U']';
        m_conversionMap[U'｛'] = U'{';
        m_conversionMap[U'｝'] = U'}';
        m_conversionMap[U'［'] = U'[';
        m_conversionMap[U'］'] = U']';
        m_conversionMap[U'％'] = U'%';
        m_conversionMap[U'＋'] = U'+';
        m_conversionMap[U'－'] = U'-';
        m_conversionMap[U'＊'] = U'*';
        m_conversionMap[U'／'] = U'/';
        m_conversionMap[U'＝'] = U'=';
        m_conversionMap[U'＜'] = U'<';
        m_conversionMap[U'＞'] = U'>';
        m_conversionMap[U'＆'] = U'&';
        m_conversionMap[U'＄'] = U'$';
        m_conversionMap[U'＃'] = U'#';
        m_conversionMap[U'＠'] = U'@';
        m_conversionMap[U'＾'] = U'^';
        m_conversionMap[U'｜'] = U'|';
        m_conversionMap[U'｀'] = U'`';
        m_conversionMap[U'　'] = U' '; // 全角空格
        m_conversionMap[U'…'] = U'.'; m_conversionMap[U'…'] = U'.'; m_conversionMap[U'…'] = U'.'; // 转为3个点
        m_conversionMap[U'—'] = U'-';
        m_conversionMap[U'－'] = U'-';
        m_conversionMap[U'ー'] = U'-';
        m_conversionMap[U'・'] = U'·';
        m_conversionMap[U'·'] = U'·';
        m_conversionMap[U'′'] = U'\'';
        m_conversionMap[U'″'] = U'"';
        m_conversionMap[U'〜'] = U'~';
        m_conversionMap[U'～'] = U'~';
        m_conversionMap[U'￥'] = U'¥';
        m_conversionMap[U'￠'] = U'¢';
        m_conversionMap[U'￡'] = U'£';
    }

    // 反向转换(半角→全角)
    if (m_reverseConversion) {
        auto originalMap = m_conversionMap; // 保存原有映射
        m_conversionMap.clear(); // 清空map

        // 构建反向映射(值→键)
        for (const auto& [full, half] : originalMap) {
            // 处理一个全角对应多个半角的情况(如省略号)
            if (full == U'…') continue; // 特殊处理

            m_conversionMap[half] = full;
        }

        // 特殊处理横线符号
        m_conversionMap[U'-'] = U'－'; // 半角横线优先转全角连字符

        // 特殊处理省略号
        if (originalMap.contains(U'…')) {
            m_conversionMap[U'.'] = U'…'; // 半角点→全角省略号
        }
    }
}

std::string TextFull2Half::convertText(const std::string& text, bool jumpTag) {
    std::string result;
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(text);

    for (int32_t i = 0; i < ustr.length();) {

        if (jumpTag) {
            icu::UnicodeString tempSubString = ustr.tempSubString(i);
            if (tempSubString.startsWith(u"<tab>")) {
                result.append("<tab>");
                i += 5;
                continue;
            }
            else if (tempSubString.startsWith(u"<br>")) {
                result.append("<br>");
                i += 4;
                continue;
            }
        }

        UChar32 c = ustr.char32At(i);
        i += U16_LENGTH(c);

        if (auto it = m_conversionMap.find(c); it != m_conversionMap.end()) {
            UChar32 converted = static_cast<UChar32>(it->second);
            icu::UnicodeString::fromUTF32(&converted, 1).toUTF8String(result);
        }
        else {
            UChar32 uc = static_cast<UChar32>(c);
            icu::UnicodeString::fromUTF32(&uc, 1).toUTF8String(result);
        }
    }
    return result;
}

void TextFull2Half::dPreRun(Sentence* se) {
    if (m_runTime != PluginRunTime::DPre) {
        return;
    }
    se->pre_processed_text = convertText(se->pre_processed_text, false);
}

void TextFull2Half::preRun(Sentence* se) {
    if (m_runTime != PluginRunTime::Pre) {
        return;
    }
    se->pre_processed_text = convertText(se->pre_processed_text, true);
}

void TextFull2Half::postRun(Sentence* se) {
    if (m_runTime != PluginRunTime::Post) {
        return;
    }
    se->translated_preview = convertText(se->translated_preview, true);
}

void TextFull2Half::dPostRun(Sentence* se) {
    if (m_runTime != PluginRunTime::DPost) {
        return;
    }
    se->translated_preview = convertText(se->translated_preview, false);
}
