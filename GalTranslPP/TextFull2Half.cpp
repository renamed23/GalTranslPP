module;

#include "GPPMacros.hpp"
#include <spdlog/spdlog.h>
#include <unicode/unistr.h>
#include <unicode/uchar.h>
#include <utf8cpp/utf8.h>
#include <toml.hpp>

module TextFull2Half;

import Tool;

namespace fs = std::filesystem;

TextFull2Half::TextFull2Half(const toml::value& projectConfig, const std::shared_ptr<spdlog::logger>& logger, PluginRunTime runTime)
    : m_logger(logger), m_runTime(runTime)
{
    try {

        const fs::path pluginConfigPath = [&]()
	        {
		        fs::path ret = textPluginConfigPath / std::format(L"TextFull2Half-{}.toml", ascii2Wide(pluginRunTimeNames[m_runTime]));
                if (!fs::exists(ret)) {
	                ret = textPluginConfigPath / L"TextFull2Half.toml";
                }
                return ret;
            }();
        const auto pluginConfig = toml::uparse(pluginConfigPath);

        m_replacePunctuation = parseToml<bool>(projectConfig, pluginConfig, "plugins.TextFull2Half.是否替换标点");
        m_reverseConversion = parseToml<bool>(projectConfig, pluginConfig, "plugins.TextFull2Half.是否反向替换");

        createConversionMap();

        const std::string excludeChars = parseToml<std::string>(projectConfig, pluginConfig, "plugins.TextFull2Half.不转换的字符");
        const icu::UnicodeString uExcludeChars = icu::UnicodeString::fromUTF8(excludeChars);
        for (int32_t i = 0; i < uExcludeChars.length();) {
            const UChar32 c = uExcludeChars.char32At(i);
            m_conversionMap.erase(c);
            i += U16_LENGTH(c);
        }

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
        m_conversionMap[U'…'] = U'.';
        m_conversionMap[U'—'] = U'-';
        m_conversionMap[U'－'] = U'-';
        m_conversionMap[U'ー'] = U'-';
        m_conversionMap[U'・'] = U'·';
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
            m_conversionMap[half] = full;
        }

        // 特殊处理
        m_conversionMap[U'-'] = U'－'; // 半角横线优先转全角连字符
        m_conversionMap[U'.'] = U'。';
    }
}

std::string TextFull2Half::convertText(const std::string& text, bool jumpTag) {
    std::string result;
    const icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(text);

    for (int32_t i = 0; i < ustr.length();) {

        if (jumpTag) {
            const icu::UnicodeString tempSubString = ustr.tempSubString(i);
            if (tempSubString.startsWith(u"<tab>")) {
                result.append("<tab>");
                i += 5;
                continue;
            }
            if (tempSubString.startsWith(u"<br>")) {
                result.append("<br>");
                i += 4;
                continue;
            }
            if (tempSubString.startsWith(u"(Failed to translate)")) {
                result.append("(Failed to translate)");
                i += 21;
                continue;
            }
        }

        const UChar32 c = ustr.char32At(i);
        i += U16_LENGTH(c);

        if (auto it = m_conversionMap.find(c); it != m_conversionMap.end()) {
            if (c == U'…') {
                result.append("...");
            }
            else {
                utf8::unchecked::append(it->second, std::back_inserter(result));
            }
        }
        else {
            utf8::unchecked::append(c, std::back_inserter(result));
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
