module;

#define PCRE2_HEADERS
#include "GPPMacros.hpp"
#include <spdlog/spdlog.h>
#include <toml.hpp>
#include <unicode/unistr.h>

module NormalJsonTranslatorHelperTool;

import Tool;
namespace fs = std::filesystem;

int getSplittedFileIndex(const std::wstring& path) {
    const size_t pos1 = path.find_last_of(L'_') + 1;
    const size_t pos2 = path.find_last_of(L'.');
    return std::stoi(path.substr(pos1, pos2 - pos1));
}

/**
* @brief 根据句子的上下文生成唯一的缓存键，复刻 GalTransl 逻辑
*/
std::string generateCacheKey(const Sentence* s) {
    const std::string currentText = getNameString(s) + s->original_text + s->pre_processed_text;

    std::string prevText = "None";
    std::string nextText = "None";
    if (s->prev) {
        prevText = getNameString(s->prev) + s->prev->original_text + s->prev->pre_processed_text;
    }
    if (s->next) {
        nextText = getNameString(s->next) + s->next->original_text + s->next->pre_processed_text;
    }
    return prevText + currentText + nextText;
}

std::string generateCacheKey(const json& jsonArr, size_t i) {
    const auto& item = jsonArr[i];
    const std::string currentText = getNameString(item) + item.value("original_text", "") + item.value("pre_processed_text", "");

    std::string prevText = "None";
    std::string nextText = "None";
    if (i > 0) {
        const auto& lastItem = jsonArr[i - 1];
        prevText = getNameString(lastItem) + lastItem.value("original_text", "") + lastItem.value("pre_processed_text", "");
    }
    if (i + 1 < jsonArr.size()) {
        const auto& nextItem = jsonArr[i + 1];
        nextText = getNameString(nextItem) + nextItem.value("original_text", "") + nextItem.value("pre_processed_text", "");
    }
    return prevText + currentText + nextText;
}

std::string buildContextHistory(const std::vector<Sentence*>& batch, TransEngine transEngine, int contextHistorySize, int maxChars) {
    if (batch.empty() || !batch[0]->prev) {
        return {};
    }

    std::string history;

    switch (transEngine)
    {
    case TransEngine::ForGalTsv:
    {
        std::vector<std::string> contextLines;
        const Sentence* current = batch[0]->prev;
        const int limit = contextHistorySize * 2;
        for (int i = 0; current && (int)contextLines.size() < contextHistorySize && i < limit; ++i) {
            if (current->complete) {
                const std::string name = current->nameType == NameType::None ? "null" : getNameString(current);
                contextLines.push_back(std::format("{}\t{}\t{}", name, current->pre_translated_text, current->index));
            }
            current = current->prev;
        }
        if (contextLines.empty()) return {};
        history = "NAME\tDST\tID\n" + (contextLines | std::views::reverse | std::views::join_with('\n') | std::ranges::to<std::string>());
    }
    break;

    case TransEngine::ForNovelTsv:
    {
        std::vector<std::string> contextLines;
        const Sentence* current = batch[0]->prev;
        const int limit = contextHistorySize * 2;
        for (int i = 0; current && (int)contextLines.size() < contextHistorySize && i < limit; ++i) {
            if (current->complete) {
                contextLines.push_back(std::format("{}\t{}", current->pre_translated_text, current->index));
            }
            current = current->prev;
        }
        if (contextLines.empty()) return {};
        history = "DST\tID\n" + (contextLines | std::views::reverse | std::views::join_with('\n') | std::ranges::to<std::string>());
    }
    break;

    case TransEngine::ForGalJson:
    case TransEngine::DeepseekJson:
    {
        json historyJson = json::array();
        const Sentence* current = batch[0]->prev;
        const int limit = contextHistorySize * 2;
        for (int i = 0; current && (int)historyJson.size() < contextHistorySize && i < limit; ++i) {
            if (current->complete) {
                json item;
                item["id"] = current->index;
                if (current->nameType != NameType::None) {
                    item["name"] = getNameString(current);
                }
                item["dst"] = current->pre_translated_text;
                historyJson.push_back(std::move(item));
            }
            current = current->prev;
        }
        if (historyJson.empty()) return {};
        history = "```jsonline\n" + 
            (historyJson | std::views::reverse | std::views::transform([](const auto& item) { return item.dump(); })
            | std::views::join_with('\n') | std::ranges::to<std::string>()) 
    		    + "```";
    }
    break;

    case TransEngine::Sakura:
    {
        const Sentence* current = batch[0]->prev;
        std::vector<std::string> contextLines;
        const int limit = contextHistorySize * 2;
        for (int i = 0; current && (int)contextLines.size() < contextHistorySize && i < limit; ++i) {
            if (current->complete) {
                if (current->nameType != NameType::None) {
                    contextLines.push_back(std::format("{}:::::{}", getNameString(current), current->pre_translated_text)); // :::::
                }
                else {
                    contextLines.push_back(current->pre_translated_text);
                }
            }
            current = current->prev;
        }
        if (contextLines.empty()) return {};
        history = contextLines | std::views::reverse | std::views::join_with('\n') | std::ranges::to<std::string>();
    }
    break;

    default:
        throw std::runtime_error("未知的 PromptType");
    }

    // 缺tiktoken这一块
    const uint8_t* s = (uint8_t*)history.c_str();
    int32_t i = (int32_t)history.length();
    int32_t count = 0;

    // 从后往前数 maxChars 个码点
    // U8_BACK_1 宏会自动处理 UTF-8 的多字节边界，并将 idx 向前移动
    while (i > 0 && count < maxChars) {
        U8_BACK_1(s, 0, i);
        ++count;
    }

    // 如果 idx > 0，说明字符串长度超过了 maxChars，需要截断
    if (i > 0) {
        // 此时 idx 正好指向截断位置的开头
        history.replace(0, i, "...");
    }
    return history;
}

void fillBlockAndMap(const std::vector<Sentence*>& batchToTransThisRound, absl::btree_map<int, Sentence*>& id2SentenceMap, std::string& inputBlock, TransEngine transEngine) {
    switch (transEngine)
    {
    case TransEngine::ForGalTsv:
    {
        for (const auto& pSentence : batchToTransThisRound) {
            const std::string name = pSentence->nameType == NameType::None ? "null" : getNameString(pSentence);
            inputBlock += std::format("{}\t{}\t{}\n", name, pSentence->pre_processed_text, pSentence->index);
            id2SentenceMap[pSentence->index] = pSentence;
        }
    }
    break;

    case TransEngine::ForNovelTsv:
    {
        for (const auto& pSentence : batchToTransThisRound) {
            inputBlock += std::format("{}\t{}\n", pSentence->pre_processed_text, pSentence->index);
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
                inputBlock += std::format("{}:::::{}\n", getNameString(pSentence), pSentence->pre_processed_text);
            }
            else {
                inputBlock += pSentence->pre_processed_text + "\n";
            }
        }
        if (!inputBlock.empty()) {
            inputBlock.pop_back(); // 移除最后一个换行符
        }
    }
    break;

    default:
        throw std::runtime_error("不支持的 TransEngine 用于构建输入");
    }
}

void parseContent(std::string& content, std::vector<Sentence*>& batchToTransThisRound, absl::btree_map<int, Sentence*>& id2SentenceMap, const std::string& modelName,
    std::shared_ptr<IController>& controller, std::string& backgroudText, std::atomic<int>& completedSentences, int& parsedCount, TransEngine transEngine, bool showBackgroundText) {

    if (size_t pos = content.find("</think>"); pos != std::string::npos) {
        content = content.substr(pos + 8);
    }
    else if (pos = content.find("<end_think>"); pos != std::string::npos) {
        content = content.substr(pos + 11);
    }

    {
        static jpc::Regex backgroundRegex{ R"(<background>\n*([\S\s]*?)\n*</background>)", defaultRegCompileModifier };
        jpc::VecNum vecNum;
        jpc::RegexMatch rm(&backgroundRegex);
        rm.setSubject(&content).setNumberedSubstringVector(&vecNum);
        if (rm.match() > 0 && vecNum.size() > 0 && vecNum[0].size() > 1) {
            backgroudText = std::move(replaceStrInplace(vecNum[0][1], "<ORIGINAL>", backgroudText));

            uint8_t* s = (uint8_t*)backgroudText.c_str();
            int32_t length = (int32_t)backgroudText.length();
            int32_t i = 0;
            int32_t count = 0;
            constexpr int32_t limit = 256;

            // 从前往后数 256 个码点
            // U8_FWD_1 宏会自动将 idx 移动到下一个字符的开始位置
            while (i < length && count < limit) {
                U8_FWD_1(s, i, length);
                ++count;
            }

            // 如果 idx < len，说明字符串比 256 个码点长
            if (i < length) {
                backgroudText.replace(i, std::string::npos, "...");
            }
        }
        else {
            backgroudText.clear();
        }
    }

    switch (transEngine)
    {
    case TransEngine::ForGalTsv:
    {
        size_t start = content.find("NAME\tDST\tID");
        if (start == std::string::npos) {
            break;
        }

        const auto lines = splitString(content.substr(start), '\n');
        for (const auto& line : lines) {
            if (parsedCount == batchToTransThisRound.size()) {
                break;
            }
            if (line.empty() || line.contains("```")) {
                continue;
            }
            const auto parts = splitString(line, '\t');
            if (parts.size() < 3) {
                continue;
            }
            try {
                const int id = std::stoi(parts[2]);
                if (id2SentenceMap.contains(id) && !id2SentenceMap[id]->complete) {
                    if (parts[1].empty()) {
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
            catch (...) {}
        }
    }
    break;

    case TransEngine::ForNovelTsv:
    {
        const size_t start = content.find("DST\tID"); // or DST\tID
        if (start == std::string::npos) {
            break;
        }

        const auto lines = splitString(content.substr(start), '\n');
        for (const auto& line : lines) {
            if (parsedCount == batchToTransThisRound.size()) {
                break;
            }
            if (line.empty() || line.contains("```")) {
                continue;
            }
            const auto parts = splitString(line, '\t');
            if (parts.size() < 2) {
                continue;
            }
            try {
                const int id = std::stoi(parts[1]);
                if (id2SentenceMap.contains(id) && !id2SentenceMap[id]->complete) {
                    if (parts[0].empty()) {
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
            catch (...) {}
        }
    }
    break;

    case TransEngine::ForGalJson:
    case TransEngine::DeepseekJson:
    {
        const size_t start = std::min(content.find("{\"id\""), content.find("{\"dst\""));
        if (start == std::string::npos) {
            break;
        }

        const auto lines = splitString(content.substr(start), '\n');
        for (const auto& line : lines) {
            if (parsedCount == batchToTransThisRound.size()) {
                break;
            }
            if (line.empty() || !line.starts_with('{')) {
                continue;
            }
            try {
                json item = json::parse(line);
                if (const int id = item["id"]; id2SentenceMap.contains(id) && !id2SentenceMap[id]->complete) {
                    const std::string dst = item["dst"].get<std::string>();
                    if (dst.empty()) {
                        continue;
                    }
                    id2SentenceMap[id]->pre_translated_text = dst;
                    id2SentenceMap[id]->translated_by = modelName;
                    id2SentenceMap[id]->complete = true;
                    completedSentences++;
                    controller->updateBar(); // ForGalJson/DeepseekJson
                    parsedCount++;
                }
            }
            catch (...) {}
        }
    }
    break;

    case TransEngine::Sakura:
    {
        auto lines = splitString(content, '\n');
        // 核心检查：行数是否匹配
        if (lines.size() != batchToTransThisRound.size()) {
            break;
        }

        for (auto [translatedLine, currentSentence] : std::views::zip(lines, batchToTransThisRound)) {
            // 尝试剥离说话人
            if (!currentSentence->name.empty()) {
                if (const size_t msgStart = translatedLine.find(":::::"); msgStart != std::string::npos) {
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

void combineOutputFiles(const fs::path& originalRelFilePath, const absl::flat_hash_map<fs::path, bool>& splitFileParts,
    const fs::path& outputCacheDir, const fs::path& outputDir, std::shared_ptr<spdlog::logger>& logger) {

    ordered_json combinedJson = ordered_json::array();

    std::ifstream ifs;
    logger->debug("开始合并文件: {}", wide2Ascii(originalRelFilePath));

    std::vector<fs::path> partPaths;
    partPaths.insert_range(partPaths.end(), splitFileParts | std::views::keys);

    std::ranges::sort(partPaths, [&](const fs::path& a, const fs::path& b)
        {
            return getSplittedFileIndex(a.wstring()) < getSplittedFileIndex(b.wstring());
        });

    for (const auto& relPartPath : partPaths) {
        if (const fs::path partPath = outputCacheDir / relPartPath; fs::exists(partPath)) {
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

    const fs::path finalOutputPath = outputDir / originalRelFilePath;
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
        item.at("problems").get_to(se.problems);
    }
    if (item.contains("other_info")) {
        item.at("other_info").get_to(se.other_info);
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


std::vector<ordered_json> splitJsonArrayNum(const ordered_json& originalData, int chunkSize) {
    if (chunkSize <= 1 || originalData.empty()) {
        return { originalData };
    }
    std::vector<ordered_json> parts;
    parts.reserve((originalData.size() + chunkSize - 1) / chunkSize);
    const size_t totalSize = originalData.size();
    for (size_t i = 0; i < totalSize; i += chunkSize) {
        const size_t end = std::min(i + chunkSize, totalSize);
        parts.emplace_back(originalData.begin() + i, originalData.begin() + end);
    }
    return parts;
}


std::vector<ordered_json> splitJsonArrayEqual(const ordered_json& originalData, int numParts) {
    if (numParts == 1 || originalData.empty()) {
        return { originalData };
    }
    std::vector<ordered_json> parts;
    parts.reserve(numParts);
    const size_t totalSize = originalData.size();
    const size_t partSize = totalSize / numParts;
    const size_t remainder = totalSize % numParts;
    size_t currentIndex = 0;
    for (int i = 0; i < numParts; ++i) {
        const size_t currentPartSize = partSize + (i < remainder ? 1 : 0);
        if (currentPartSize == 0) {
            continue;
        }
        parts.emplace_back(originalData.begin() + currentIndex, originalData.begin() + currentIndex + currentPartSize);
        currentIndex += currentPartSize;
    }
    return parts;
}

int calculateCachePartIndexDiff(const std::wstring& path1, const std::wstring& path2) {
    return getSplittedFileIndex(path1) - getSplittedFileIndex(path2);
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
