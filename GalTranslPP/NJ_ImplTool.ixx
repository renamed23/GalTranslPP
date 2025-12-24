module;

#include <spdlog/spdlog.h>
#include <toml.hpp>
#include <unicode/regex.h>
#include <unicode/unistr.h>
#include <unicode/brkiter.h>
#include <boost/regex.hpp>

export module NJ_ImplTool;

export import Tool;

using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;
namespace fs = std::filesystem;

export {

    std::string generateCacheKey(const Sentence* s);

    std::string buildContextHistory(const std::vector<Sentence*>& batch, TransEngine transEngine, int contextHistorySize, int maxTokens);

    void fillBlockAndMap(const std::vector<Sentence*>& batchToTransThisRound, std::map<int, Sentence*>& id2SentenceMap, std::string& inputBlock, TransEngine transEngine);

    void parseContent(std::string& content, std::vector<Sentence*>& batchToTransThisRound, std::map<int, Sentence*>& id2SentenceMap, const std::string& modelName, bool showBackgroundText,
        std::string& backgroudText, TransEngine transEngine, bool& parseError, int& parsedCount, std::shared_ptr<IController> controller, std::atomic<int>& completedSentences);

    void combineOutputFiles(const fs::path& originalRelFilePath, const std::map<fs::path, bool>& splitFileParts,
        std::shared_ptr<spdlog::logger> logger, const fs::path& outputCacheDir, const fs::path& outputDir);

    bool hasRetranslKey(const std::vector<CheckSeCondFunc>& retranslKeys, const json& item, const Sentence* currentSe);

    void saveCache(const std::vector<Sentence>& allSentences, const fs::path& cachePath);

    std::vector<json> splitJsonArrayNum(const json& originalData, int chunkSize);

    std::vector<json> splitJsonArrayEqual(const json& originalData, int numParts);

    int calculateCachePartIndexDiff(const std::wstring& path1, const std::wstring& path2);

    json toml2Json(const toml::value& value);
    ordered_json toml2Json(const toml::ordered_value& tomlData);
}



module :private;

/**
* @brief 根据句子的上下文生成唯一的缓存键，复刻 GalTransl 逻辑
*/
std::string generateCacheKey(const Sentence* s) {
    std::string prevText = "None";
    const Sentence* temp = s->prev;
    if (temp) {
        prevText = getNameString(temp) + temp->original_text + temp->pre_processed_text;
    }

    std::string currentText = getNameString(s) + s->original_text + s->pre_processed_text;

    std::string nextText = "None";
    temp = s->next;
    if (temp) {
        nextText = getNameString(temp) + temp->original_text + temp->pre_processed_text;
    }

    return prevText + currentText + nextText;
}

std::string buildContextHistory(const std::vector<Sentence*>& batch, TransEngine transEngine, int contextHistorySize, int maxTokens) {
    if (batch.empty() || !batch[0]->prev) {
        return {};
    }

    std::string history;

    switch (transEngine) {
    case TransEngine::ForGalTsv: 
    {
        history = "NAME\tDST\tID\n"; // Or DST\tID for novel
        std::vector<std::string> contextLines;
        const Sentence* current = batch[0]->prev;
        int count = 0;
        while (current && count < contextHistorySize) {
            if (current->complete) {
                std::string name = current->nameType == NameType::None ? "null" : getNameString(current);
                contextLines.push_back(name + "\t" + current->pre_translated_text + "\t" + std::to_string(current->index));
                count++;
            }
            current = current->prev;
        }
        if (contextLines.empty()) return {};
        for (const auto& line : contextLines | std::views::reverse) {
            history += line + "\n";
        }
    }
    break;

    case TransEngine::ForNovelTsv: 
    {
        history = "DST\tID\n"; // Or DST\tID for novel
        std::vector<std::string> contextLines;
        const Sentence* current = batch[0]->prev;
        int count = 0;
        while (current && count < contextHistorySize) {
            if (current->complete) {
                contextLines.push_back(current->pre_translated_text + "\t" + std::to_string(current->index));
                count++;
            }
            current = current->prev;
        }
        if (contextLines.empty()) return {};
        for (const auto& line : contextLines | std::views::reverse) {
            history += line + "\n";
        }
    }
    break;

    case TransEngine::ForGalJson:
    case TransEngine::DeepseekJson:
    {
        json historyJson = json::array();
        const Sentence* current = batch[0]->prev;
        int count = 0;
        while (current && count < contextHistorySize) {
            if (current->complete) { // current->complete
                json item;
                item["id"] = current->index;
                if (current->nameType != NameType::None) {
                    item["name"] = getNameString(current);
                }
                item["dst"] = current->pre_translated_text;
                historyJson.push_back(item);
                count++;
            }
            current = current->prev;
        }
        if (historyJson.empty()) return {};
        for (const auto& item : historyJson | std::views::reverse) {
            history += item.dump() + "\n";
        }
        history = "```jsonline\n" + history + "```";
    }
    break;

    case TransEngine::Sakura:
    {
        const Sentence* current = batch[0]->prev;
        int count = 0;
        std::vector<std::string> contextLines;
        while (current && count < contextHistorySize) {
            if (current->complete) {
                if (current->nameType != NameType::None) {
                    contextLines.push_back(getNameString(current) + ":::::" + current->pre_translated_text); // :::::
                }
                else {
                    contextLines.push_back(current->pre_translated_text);
                }
                count++;
            }
            current = current->prev;
        }
        if (contextLines.empty()) return {};
        for (const auto& line : contextLines | std::views::reverse) {
            history += line + "\n";
        }
    }
    break;

    default:
        throw std::runtime_error("未知的 PromptType");
    }

    // 缺tiktoken这一块
    if (history.length() > maxTokens) {
        history = history.substr(history.length() - maxTokens);
    }
    return history;
}

void fillBlockAndMap(const std::vector<Sentence*>& batchToTransThisRound, std::map<int, Sentence*>& id2SentenceMap, std::string& inputBlock, TransEngine transEngine) {
    switch (transEngine) {
    case TransEngine::ForGalTsv: 
    {
        for (const auto& pSentence : batchToTransThisRound) {
            std::string name = pSentence->nameType == NameType::None ? "null" : getNameString(pSentence);
            inputBlock += name + "\t" + pSentence->pre_processed_text + "\t" + std::to_string(pSentence->index) + "\n";
            id2SentenceMap[pSentence->index] = pSentence;
        }
    }
    break;

    case TransEngine::ForNovelTsv: 
    {
        for (const auto& pSentence : batchToTransThisRound) {
            inputBlock += pSentence->pre_processed_text + "\t" + std::to_string(pSentence->index) + "\n";
            id2SentenceMap[pSentence->index] = pSentence;
        }
    }
    break;

    case TransEngine::ForGalJson:
    case TransEngine::DeepseekJson:
    {
        for (const auto& pSentence : batchToTransThisRound) {
            json item;
            item["id"] = pSentence->index;
            if (pSentence->nameType != NameType::None) {
                item["name"] = getNameString(pSentence);
            }
            item["src"] = pSentence->pre_processed_text;
            inputBlock += item.dump() + "\n";
            id2SentenceMap[pSentence->index] = pSentence;
        }
    }
    break;

    case TransEngine::Sakura:
    {
        for (const auto& pSentence : batchToTransThisRound) {
            if (pSentence->nameType != NameType::None) {
                inputBlock += getNameString(pSentence) + ":::::" + pSentence->pre_processed_text + "\n";
            }
            else {
                inputBlock += pSentence->pre_processed_text + "\n";
            }
        }
        inputBlock.pop_back(); // 移除最后一个换行符
    }
    break;

    default:
        throw std::runtime_error("不支持的 TransEngine 用于构建输入");
    }
}

void parseContent(std::string& content, std::vector<Sentence*>& batchToTransThisRound, std::map<int, Sentence*>& id2SentenceMap, const std::string& modelName, bool showBackgroundText,
    std::string& backgroudText, TransEngine transEngine, bool& parseError, int& parsedCount, std::shared_ptr<IController> controller, std::atomic<int>& completedSentences) {
    if (size_t pos = content.find("</think>"); pos != std::string::npos) {
        content = content.substr(pos + 8);
    }

    {
        static boost::regex backgroundRegex{ R"(<background>\n*([\S\s]*?)\n*</background>)" };
        boost::smatch match;
        if (boost::regex_search(content, match, backgroundRegex)) {
            std::string background = match[1].str();
            replaceStrInplace(background, "<ORIGINAL>", backgroudText);
            backgroudText = std::move(background);
            UErrorCode errorCode = U_ZERO_ERROR;
            icu::UnicodeString uString = icu::UnicodeString::fromUTF8(backgroudText);
            std::unique_ptr<icu::BreakIterator> breakIterator(
                icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), errorCode)
            );
            if (U_FAILURE(errorCode)) {
                throw std::runtime_error(std::format("Failed to create a character break iterator: {}", u_errorName(errorCode)));
            }
            breakIterator->setText(uString);
            int32_t pos = breakIterator->next(256);
            if (pos != icu::BreakIterator::DONE) {
                backgroudText.clear();
                uString.tempSubString(0, pos).toUTF8String(backgroudText);
            }
        }
        if (!showBackgroundText) {
            content = boost::regex_replace(content, backgroundRegex, "");
        }
    }

    switch (transEngine) {
    case TransEngine::ForGalTsv:
    {
        size_t start = content.find("NAME\tDST\tID");
        if (start == std::string::npos) {
            parseError = true;
            break;
        }

        std::stringstream ss(content.substr(start));
        std::string line;
        std::getline(ss, line); // Skip header

        while (parsedCount < batchToTransThisRound.size() && std::getline(ss, line)) {
            if (line.empty() || line.contains("```")) continue;
            const auto parts = splitString(line, '\t');
            if (parts.size() < 3) {
                parseError = true;
                continue;
            }
            try {
                int id = std::stoi(parts[2]);
                if (id2SentenceMap.contains(id)) {
                    if (parts[1].empty() && !id2SentenceMap[id]->pre_processed_text.empty()) {
                        parseError = true;
                        continue;
                    }
                    id2SentenceMap[id]->pre_translated_text = parts[1];
                    id2SentenceMap[id]->translated_by = modelName;
                    id2SentenceMap[id]->complete = true;
                    completedSentences++;
                    controller->updateBar(); // ForGalTsv
                    parsedCount++;
                }
            }
            catch (...) {
                parseError = true;
                continue;
            }
        }

    }
    break;

    case TransEngine::ForNovelTsv:
    {
        size_t start = content.find("DST\tID"); // or DST\tID
        if (start == std::string::npos) {
            parseError = true;
            break;
        }

        std::stringstream ss(content.substr(start));
        std::string line;
        std::getline(ss, line); // Skip header

        while (parsedCount < batchToTransThisRound.size() && std::getline(ss, line)) {
            if (line.empty() || line.contains("```")) continue;
            const auto parts = splitString(line, '\t');
            if (parts.size() < 2) {
                parseError = true;
                continue;
            }
            try {
                int id = std::stoi(parts[1]);
                if (id2SentenceMap.contains(id)) {
                    if (parts[0].empty() && !id2SentenceMap[id]->pre_processed_text.empty()) {
                        parseError = true;
                        continue;
                    }
                    id2SentenceMap[id]->pre_translated_text = parts[0];
                    id2SentenceMap[id]->translated_by = modelName;
                    id2SentenceMap[id]->complete = true;
                    completedSentences++;
                    controller->updateBar(); // ForNovelTsv
                    parsedCount++;
                }
            }
            catch (...) {
                parseError = true;
                continue;
            }
        }
    }
    break;

    case TransEngine::ForGalJson:
    case TransEngine::DeepseekJson:
    {
        size_t start = std::min(content.find("{\"id\""), content.find("{\"dst\""));
        if (start == std::string::npos) {
            parseError = true;
            break;
        }

        std::stringstream ss(content.substr(start));
        std::string line;
        while (parsedCount < batchToTransThisRound.size() && std::getline(ss, line)) {
            if (line.empty() || !line.starts_with('{')) continue;
            try {
                json item = json::parse(line);
                int id = item.at("id");
                if (id2SentenceMap.contains(id)) {
                    if (item.at("dst").empty() && !id2SentenceMap[id]->pre_processed_text.empty()) {
                        parseError = true;
                        continue;
                    }
                    id2SentenceMap[id]->pre_translated_text = item.at("dst");
                    id2SentenceMap[id]->translated_by = modelName;
                    id2SentenceMap[id]->complete = true;
                    completedSentences++;
                    controller->updateBar(); // ForGalJson/DeepseekJson
                    parsedCount++;
                }
            }
            catch (...) {
                parseError = true;
                continue;
            }
        }
    }
    break;

    case TransEngine::Sakura:
    {
        const auto lines = splitString(content, '\n');
        // 核心检查：行数是否匹配
        if (lines.size() != batchToTransThisRound.size()) {
            parseError = true;
            break;
        }

        for (size_t i = 0; i < lines.size(); ++i) {
            std::string translatedLine = lines[i];
            Sentence* currentSentence = batchToTransThisRound[i];

            // 尝试剥离说话人
            if (!currentSentence->name.empty()) {
                size_t msgStart = translatedLine.find(":::::");
                if (msgStart != std::string::npos) {
                    translatedLine = translatedLine.substr(msgStart + 5);
                }
            }

            currentSentence->pre_translated_text = translatedLine;
            currentSentence->translated_by = modelName;
            currentSentence->complete = true;
            completedSentences++;
            controller->updateBar(); // Sakura
            parsedCount++;
        }
    }
    break;

    default:
        throw std::runtime_error("不支持的 TransEngine 用于解析输出");
    }
}

void combineOutputFiles(const fs::path& originalRelFilePath, const std::map<fs::path, bool>& splitFileParts,
    std::shared_ptr<spdlog::logger> logger, const fs::path& outputCacheDir, const fs::path& outputDir) {

    ordered_json combinedJson = ordered_json::array();

    std::ifstream ifs;
    logger->debug("开始合并文件: {}", wide2Ascii(originalRelFilePath));

    std::vector<fs::path> partPaths;
    for (const auto& relPartPath : splitFileParts | std::views::keys) {
        partPaths.push_back(relPartPath);
    }

    std::ranges::sort(partPaths, [&](const fs::path& a, const fs::path& b)
        {
            size_t posA = a.filename().wstring().rfind(L"_part_");
            size_t posB = b.filename().wstring().rfind(L"_part_");
            if (posA == std::wstring::npos || posB == std::wstring::npos) {
                return false;
            }
            std::wstring numA = a.filename().wstring().substr(posA + 6, a.filename().wstring().length() - posA - 11);
            std::wstring numB = b.filename().wstring().substr(posB + 6, b.filename().wstring().length() - posB - 11);
            return std::stoi(numA) < std::stoi(numB);
        });

    for (const auto& relPartPath : partPaths) {
        fs::path partPath = outputCacheDir / relPartPath;
        if (fs::exists(partPath)) {
            try {
                ifs.open(partPath);
                ordered_json partData = ordered_json::parse(ifs);
                ifs.close();
                combinedJson.insert(combinedJson.end(), partData.begin(), partData.end());
            }
            catch (const json::exception& e) {
                ifs.close();
                logger->critical("合并文件 {} 时出错", wide2Ascii(partPath));
                throw std::runtime_error(e.what());
            }
        }
        else {
            throw std::runtime_error(std::format("试图合并 {} 时出错，缺少文件 {}", wide2Ascii(originalRelFilePath), wide2Ascii(partPath)));
        }
    }

    fs::path finalOutputPath = outputDir / originalRelFilePath;
    createParent(finalOutputPath);
    std::ofstream ofs(finalOutputPath);
    ofs << combinedJson.dump(2);
    ofs.close();
    logger->info("文件 {} 合并完成，已保存到 {}", wide2Ascii(originalRelFilePath), wide2Ascii(finalOutputPath));
}


bool hasRetranslKey(const std::vector<CheckSeCondFunc>& retranslKeys, const json& item, const Sentence* currentSe) {
    if (retranslKeys.empty()) {
        return false;
    }

    Sentence se;
    if (item.contains("name")) {
        se.nameType = NameType::Single;
        se.name = item.at("name");
        se.name_preview = item.at("name_preview");
    }
    else if (item.contains("names")) {
        se.nameType = NameType::Multiple;
        se.names = item.at("names");
        se.names_preview = item.at("names_preview");
    }
    se.original_text = item.value("original_text", "");
    se.pre_processed_text = item.value("pre_processed_text", "");
    se.pre_translated_text = item.value("pre_translated_text", "");
    if (item.contains("problems")) {
        se.problems = item.at("problems").get<std::vector<std::string>>();
    }
    if (item.contains("other_info")) {
        se.other_info = item.at("other_info").get<std::map<std::string, std::string>>();
    }
    se.translated_by = item.value("translated_by", "");
    se.translated_preview = item.value("translated_preview", "");
    se.prev = currentSe->prev;
    se.next = currentSe->next;

    return std::ranges::any_of(retranslKeys, [&](const CheckSeCondFunc& key)
        {
            return key(&se);
        });
}

void saveCache(const std::vector<Sentence>& allSentences, const fs::path& cachePath) {
    json cacheJson = json::array();
    for (const auto& se : allSentences) {
        if (!se.complete) {
            continue;
        }
        json cacheObj;
        cacheObj["index"] = se.index;
        if (se.nameType == NameType::Single) {
            cacheObj["name"] = se.name;
            cacheObj["name_preview"] = se.name_preview;
        }
        else if (se.nameType == NameType::Multiple) {
            cacheObj["names"] = se.names;
            cacheObj["names_preview"] = se.names_preview;
        }
        cacheObj["original_text"] = se.original_text;
        if (!se.other_info.empty()) {
            cacheObj["other_info"] = se.other_info;
        }
        cacheObj["pre_processed_text"] = se.pre_processed_text;
        cacheObj["pre_translated_text"] = se.pre_translated_text;
        if (!se.problems.empty()) {
            cacheObj["problems"] = se.problems;
        }
        cacheObj["translated_by"] = se.translated_by;
        cacheObj["translated_preview"] = se.translated_preview;
        cacheJson.push_back(std::move(cacheObj));
    }
    std::ofstream ofs(cachePath);
    ofs << cacheJson.dump(2);
    ofs.close();
}


std::vector<json> splitJsonArrayNum(const json& originalData, int chunkSize) {
    if (chunkSize <= 0 || !originalData.is_array() || originalData.empty()) {
        return { originalData };
    }

    std::vector<json> parts;
    size_t totalSize = originalData.size();

    for (size_t i = 0; i < totalSize; i += chunkSize) {
        size_t end = std::min(i + chunkSize, totalSize);
        json part = json::array();
        for (size_t j = i; j < end; ++j) {
            part.push_back(originalData[j]);
        }
        parts.push_back(part);
    }
    return parts;
}


std::vector<json> splitJsonArrayEqual(const json& originalData, int numParts) {
    if (numParts <= 1 || !originalData.is_array() || originalData.empty()) {
        return { originalData };
    }

    std::vector<json> parts;
    size_t totalSize = originalData.size();
    size_t partSize = totalSize / numParts;
    size_t remainder = totalSize % numParts;
    size_t currentIndex = 0;

    for (int i = 0; i < numParts; ++i) {
        size_t currentPartSize = partSize + (i < remainder ? 1 : 0);
        if (currentPartSize == 0) continue;

        json part = json::array();
        for (size_t j = 0; j < currentPartSize; ++j) {
            part.push_back(originalData[currentIndex + j]);
        }
        parts.push_back(part);
        currentIndex += currentPartSize;
    }
    return parts;
}

int calculateCachePartIndexDiff(const std::wstring& path1, const std::wstring& path2) {
    auto getIndexFunc = [](const std::wstring& path) -> int
        {
            size_t pos1 = path.find_last_of(L'_') + 1;
            size_t pos2 = path.find_last_of(L'.');
            return std::stoi(path.substr(pos1, pos2 - pos1));
        };
    return getIndexFunc(path1) - getIndexFunc(path2);
}

json toml2Json(const toml::value& value) {
    if (value.is_table()) {
        json resultMap = json::object();
        for (const auto& [key, val] : value.as_table()) {
            resultMap[key] = toml2Json(val);
        }
        return resultMap;
    }
    else if (value.is_array()) {
        json resultVec = json::array();
        for (const auto& elem : value.as_array()) {
            resultVec.push_back(toml2Json(elem));
        }
        return resultVec;
    }
    else if (value.is_string()) {
        return value.as_string();
    }
    else if (value.is_integer()) {
        return value.as_integer();
    }
    else if (value.is_floating()) {
        return value.as_floating();
    }
    else if (value.is_boolean()) {
        return value.as_boolean();
    }
    throw std::runtime_error("不支持的 TOML 数据类型");
}

ordered_json toml2Json(const toml::ordered_value& value) {
    if (value.is_table()) {
        ordered_json resultMap = ordered_json::object();
        for (const auto& [key, val] : value.as_table()) {
            resultMap[key] = toml2Json(val);
        }
        return resultMap;
    }
    else if (value.is_array()) {
        ordered_json resultVec = ordered_json::array();
        for (const auto& elem : value.as_array()) {
            resultVec.push_back(toml2Json(elem));
        }
        return resultVec;
    }
    else if (value.is_string()) {
        return value.as_string();
    }
    else if (value.is_integer()) {
        return value.as_integer();
    }
    else if (value.is_floating()) {
        return value.as_floating();
    }
    else if (value.is_boolean()) {
        return value.as_boolean();
    }
    throw std::runtime_error("不支持的 TOML 数据类型");
}
