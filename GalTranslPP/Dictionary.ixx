module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"
#include <spdlog/spdlog.h>
#include <unicode/regex.h>
#include <unicode/unistr.h>
#include <toml.hpp>
#include <sol/sol.hpp>

export module Dictionary;

import Tool;
import ConditionTool;
import PythonManager;
import LuaManager;

namespace fs = std::filesystem;

export {

    struct GptDictEntry {
        int priority;
        std::string searchStr;
        std::string replaceStr;
        std::string note;
    };

    class GptDictionary {
    private:
        fs::path m_projectDir;
        fs::path m_tokenizeCachePath;
        std::unordered_map<std::string, WordPosVec> m_tokenizeCacheMap;
        std::shared_mutex m_tokenizeCacheMapMutex;
        std::vector<GptDictEntry> m_entries;
        LuaManager& m_luaManager;
        PythonManager& m_pythonManager;
        std::shared_ptr<spdlog::logger> m_logger;
        std::function<NLPResult(const std::string&)> m_tokenizeSourceLangFunc;

    public:

        GptDictionary(const fs::path& projectDir, const fs::path& otherCacheDir, std::function<NLPResult(const std::string&)> tokenizeSourceLangFunc,
            LuaManager& luaManager, PythonManager& pythonManager, std::shared_ptr<spdlog::logger> logger);

        ~GptDictionary() {
            saveTokenizeCache(m_tokenizeCacheMap, m_tokenizeCachePath, m_logger);
        }

        void sort();

        void loadFromFile(const fs::path& filePath, bool& needReboot);

        std::string generatePrompt(const std::vector<Sentence*>& batch, TransEngine transEngine);

        std::string doReplace(const Sentence* se, CachePart targetToModify);

        void checkDicUse(Sentence* sentence, CachePart base, CachePart check);
    };


    struct NormalDictEntry {
        int priority;

        std::string searchStr;
        std::string replaceStr;
        bool isReg;
        std::shared_ptr<icu::RegexPattern> searchReg;

        // 条件字典相关
        CheckSeCondFunc dictCondition;
    };

    class NormalDictionary {
    private:
        fs::path m_projectDir;
        LuaManager& m_luaManager;
        PythonManager& m_pythonManager;
        std::vector<NormalDictEntry> m_entries;
        std::shared_ptr<spdlog::logger> m_logger;

    public:

        NormalDictionary(const fs::path& projectDir,
            LuaManager& luaManager, PythonManager& pythonManager, std::shared_ptr<spdlog::logger> logger) 
            : m_projectDir(projectDir), m_luaManager(luaManager), m_pythonManager(pythonManager), m_logger(logger) {}

        void loadFromFile(const fs::path& filePath, bool& needReboot);

        void sort();

        std::string doReplace(const Sentence* sentence, CachePart targetToModify);
    };
}


module :private;

GptDictionary::GptDictionary(const fs::path& projectDir, const fs::path& otherCacheDir, std::function<NLPResult(const std::string&)> tokenizeSourceLangFunc,
    LuaManager& luaManager, PythonManager& pythonManager, std::shared_ptr<spdlog::logger> logger)
    : m_projectDir(projectDir), m_tokenizeCachePath(otherCacheDir / L"tokenizeCache_gptdict.json"),
    m_tokenizeSourceLangFunc(tokenizeSourceLangFunc),
    m_luaManager(luaManager), m_pythonManager(pythonManager), m_logger(logger)
{
    m_tokenizeCacheMap = loadTokenizeCache(m_tokenizeCachePath, m_logger);
}

void GptDictionary::sort() {
    std::ranges::sort(m_entries, [](const GptDictEntry& a, const GptDictEntry& b)
        {
            if (a.priority != b.priority) {
                return a.priority > b.priority;
            }
            return a.searchStr.length() > b.searchStr.length();
        });
}

std::string GptDictionary::generatePrompt(const std::vector<Sentence*>& batch, TransEngine transEngine) {
    std::string batchText;
    for (const auto& s : batch) {
        batchText += s->name + ":" + s->pre_processed_text + "\n";
    }

    std::string promptContent;
    for (const auto& entry : m_entries) {
        if (batchText.contains(entry.searchStr)) {
            // *** 根据 transEngine 选择格式 ***
            switch (transEngine) {
            case TransEngine::ForGalJson:
            case TransEngine::DeepseekJson:
                promptContent += "| " + entry.searchStr + " | " + entry.replaceStr + " |";
                if (!entry.note.empty()) {
                    promptContent += " " + entry.note;
                }
                promptContent += " |\n";
                break;

            case TransEngine::ForGalTsv:
            case TransEngine::ForNovelTsv:
            case TransEngine::NameTrans:
                promptContent += entry.searchStr + "\t" + entry.replaceStr;
                if (!entry.note.empty()) {
                    promptContent += "\t" + entry.note;
                }
                promptContent += "\n";
                break;

            case TransEngine::Sakura:
                promptContent += entry.searchStr + "->" + entry.replaceStr;
                if (!entry.note.empty()) {
                    promptContent += " #" + entry.note;
                }
                promptContent += "\n";
                break;

            default:
                throw std::runtime_error("Invalid prompt type");
            }
        }
    }

    if (promptContent.empty()) {
        return {};
    }

    // *** 根据 transEngine 添加标题 ***
    switch (transEngine) {
    case TransEngine::ForGalJson:
    case TransEngine::DeepseekJson:
        return "# Glossary\n| Src | Dst(/Dst2/..) | Note |\n| --- | --- | --- |\n" + promptContent;

    case TransEngine::ForGalTsv:
    case TransEngine::ForNovelTsv:
    case TransEngine::NameTrans:
        return "SRC\tDST\tNOTE\n" + promptContent;

    case TransEngine::Sakura:
        return promptContent; // Sakura 模式没有标题

    default:
        throw std::runtime_error("Invalid prompt type");
    }

    return {};
}

void GptDictionary::loadFromFile(const fs::path& filePath, bool& needReboot) {
    if (!fs::exists(filePath)) {
        m_logger->error("GPT 字典文件不存在: {}", wide2Ascii(filePath));
        return;
    }

    int count = 0;

    try {
        const auto dictData = toml::parse(filePath);
        if (!dictData.contains("gptDict")) {
            return;
        }
        const auto& dictTbls = dictData.at("gptDict").as_array();
        for (const auto& el : dictTbls) {
            GptDictEntry entry;
            if (!el.contains("org") && !el.contains("searchStr")) {
                continue;
            }
            if (!el.contains("rep") && !el.contains("replaceStr")) {
                continue;
            }

            entry.searchStr = el.contains("org") ? el.at("org").as_string() : el.at("searchStr").as_string();
            if (entry.searchStr.empty()) {
                continue;
            }
            entry.replaceStr = el.contains("rep") ? el.at("rep").as_string() : el.at("replaceStr").as_string();
            entry.note = toml::find_or(el, "note", "");
            entry.priority = toml::find_or(el, "priority", 0);
            m_entries.push_back(std::move(entry));
            ++count;
        }
    }
    catch (const toml::exception& e) {
        m_logger->critical("GPT 字典文件解析错误");
        throw std::runtime_error(e.what());
    }

    m_logger->info("已加载 GPT 字典: {}, 共 {} 个词条", wide2Ascii(filePath.filename()), count);
}

std::string GptDictionary::doReplace(const Sentence* se, CachePart targetToModify) {
    std::string textToModify = chooseString(se, targetToModify);

    for (const auto& entry : m_entries) {
        replaceStrInplace(textToModify, entry.searchStr, entry.replaceStr);
    }

    return textToModify;
}

void GptDictionary::checkDicUse(Sentence* sentence, CachePart base, CachePart check) {
    const std::string& origText = chooseStringRef(sentence, base);
    const std::string& transView = chooseStringRef(sentence, check);
    for (const auto& entry : m_entries) {
        // 如果原文中不包含这个词，就跳过检查
        if (!origText.contains(entry.searchStr)) {
            continue;
        }
        // 检查译文中是否使用了对应的词
        const auto& replaceWords = splitString(entry.replaceStr, '/');
        bool found = false;
        for (const auto& word : replaceWords) {
            if (transView.contains(word)) {
                found = true;
                break;
            }
        }
        if (found) {
            // 出现了则默认使用了字典
            continue;
        }
        else if (entry.searchStr.length() > 15) {
            // 如果字典长度大于 5 个汉字字符，则默认认为是字典未正确使用的情况
            sentence->problems.push_back("GPT字典 " + entry.searchStr + "->" + entry.replaceStr + " 未使用");
            continue;
        }

        // 未出现则分词检查原文中是否有完整的 searchStr 词组
        auto checkTokenFunc = [&](const WordPosVec& wordPosVec)
            {
                for (const auto& wordPos : wordPosVec) {
                    if (wordPos[0] == entry.searchStr) {
                        found = true;
                        break;
                    }
                }
            };

        std::optional<std::reference_wrapper<WordPosVec>> wordPosVecRef;
        {
            std::shared_lock<std::shared_mutex> lock(m_tokenizeCacheMapMutex);
            if (auto it = m_tokenizeCacheMap.find(origText); it != m_tokenizeCacheMap.end()) {
                wordPosVecRef = it->second;
            }
        }
        if (wordPosVecRef.has_value()) {
            checkTokenFunc(wordPosVecRef.value().get());
        }
        else {
            NLPResult tokens = m_tokenizeSourceLangFunc(origText);
            WordPosVec& wordPosVec = std::get<0>(tokens);
            checkTokenFunc(wordPosVec);
            std::lock_guard<std::shared_mutex> lock(m_tokenizeCacheMapMutex);
            m_tokenizeCacheMap.insert({ origText, std::move(wordPosVec) });
        }

        if (found) {
            // 如果原文有完整的 searchStr 词组且译文中没有使用对应的词，几乎可以肯定是字典未正确使用的情况
            sentence->problems.push_back("GPT字典 " + entry.searchStr + "->" + entry.replaceStr + " 未使用");
        }
        
    }
}




void NormalDictionary::loadFromFile(const fs::path& filePath, bool& needReboot) {
    if (!fs::exists(filePath)) {
        m_logger->warn("字典文件不存在: {}", wide2Ascii(filePath));
        return;
    }

    int count = 0;
    try {
        const auto dictData = toml::parse(filePath);
        if (!dictData.contains("normalDict")) {
            return;
        }
        const auto dicts = dictData.at("normalDict").as_array();
        for (const auto& el : dicts) {
            NormalDictEntry entry;
            if (!el.contains("org") && !el.contains("searchStr")) {
                continue;
            }
            if (!el.contains("rep") && !el.contains("replaceStr")) {
                continue;
            }
            entry.isReg = toml::find_or(el, "isReg", false);

            std::string str = el.contains("org") ? el.at("org").as_string() : el.at("searchStr").as_string();
            if (str.empty()) {
                continue;
            }

            if (entry.isReg) {
                UErrorCode status = U_ZERO_ERROR;
                icu::UnicodeString ustr(icu::UnicodeString::fromUTF8(str));
                entry.searchReg = std::shared_ptr<icu::RegexPattern>(icu::RegexPattern::compile(ustr, 0, status));
                if (U_FAILURE(status)) {
                    throw std::runtime_error(std::format("Normal 字典文件格式错误(正则表达式错误): {}  ——  {}", wide2Ascii(filePath), str));
                }
            }
            else {
                entry.searchStr = str;
            }
            
            entry.replaceStr = el.contains("rep") ? el.at("rep").as_string() : el.at("replaceStr").as_string();
            entry.priority = toml::find_or(el, "priority", 0);

            if (getConditionType(el) != ConditionType::None) {
                entry.dictCondition = getCheckSeCondFunc(el, m_projectDir, m_pythonManager, m_luaManager, m_logger, needReboot);
            }
            m_entries.push_back(std::move(entry));
            ++count;
        }
    }
    catch (const toml::exception& e) {
        m_logger->critical("Normal 字典文件解析错误: {}", wide2Ascii(filePath));
        throw std::runtime_error(e.what());
    }
    
    m_logger->info("已加载 Normal 字典: {}, 共 {} 个词条", wide2Ascii(filePath.filename()), count);
}

void NormalDictionary::sort() {
    std::ranges::sort(m_entries, [](const NormalDictEntry& a, const NormalDictEntry& b) 
        {
            if (a.priority != b.priority) {
                return a.priority > b.priority;
            }
            if (a.isReg && !b.isReg) {
                return true;
            }
            else if (!a.isReg && b.isReg) {
                return false;
            }
            return a.searchStr.length() > b.searchStr.length();
        });
}

std::string NormalDictionary::doReplace(const Sentence* sentence, CachePart targetToModify) {
    std::string textToModify = chooseString(sentence, targetToModify);

    for (const auto& entry : m_entries
        | std::views::filter([&](const auto& entry)
            {
                return !entry.dictCondition.operator bool() || entry.dictCondition(sentence);
            })) 
    {
        if (entry.isReg) {
            icu::UnicodeString ustr(icu::UnicodeString::fromUTF8(textToModify));
            UErrorCode status = U_ZERO_ERROR;
            std::unique_ptr<icu::RegexMatcher> matcher(entry.searchReg->matcher(ustr, status));
            if (U_FAILURE(status)) {
                m_logger->error("正则表达式创建matcher失败: {}, 句子: [{}]", u_errorName(status), textToModify);
                return textToModify;
            }
            icu::UnicodeString result = matcher->replaceAll(icu::UnicodeString::fromUTF8(entry.replaceStr), status);
            textToModify.clear();
            result.toUTF8String(textToModify);
        }
        else {
            replaceStrInplace(textToModify, entry.searchStr, entry.replaceStr);
        }
    }

    return textToModify;
}
