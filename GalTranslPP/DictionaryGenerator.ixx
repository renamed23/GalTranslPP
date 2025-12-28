module;

#include <spdlog/spdlog.h>
#include <toml.hpp>
#include <ctpl_stl.h>

export module DictionaryGenerator;

import APIPool;
import Dictionary;

namespace fs = std::filesystem;
using json = nlohmann::json;

export {
    class DictionaryGenerator {

    private:
        std::unique_ptr<APIPool>& m_apiPool;
        fs::path m_tokenizeCachePath;
        std::shared_ptr<IController> m_controller;
        std::shared_ptr<spdlog::logger> m_logger;
        std::function<void(Sentence*)> m_preProcessFunc;
        std::function<std::string(std::string)> m_onPerformApi;
        std::function<NLPResult(const std::string&)> m_tokenizeSourceLangFunc;

        std::string m_systemPrompt;
        std::string m_userPrompt;
        std::string m_apiStrategy;
        std::string m_targetLang;
        int m_threadsNum;
        int m_apiTimeoutMs;
        int m_maxRetries;
        bool m_checkQuota;
        int m_totalSentences = 0;

        // 阶段一和二的结果
        std::unordered_map<std::string, EntityVec> m_tokenizeCacheMap;
        std::vector<std::string> m_segments;
        std::vector<std::unordered_set<std::string>> m_segmentWords;
        std::unordered_map<std::string, int> m_wordCounter;
        std::unordered_set<std::string> m_nameSet;

        // 阶段四的结果 (线程安全)
        std::vector<std::tuple<std::string, std::string, std::string>> m_finalDict;
        std::unordered_map<std::string, int> m_finalCounter;
        std::mutex m_resultMutex;

        void preprocessAndTokenize(const std::vector<fs::path>& jsonFiles);
        std::vector<int> solveSentenceSelection();
        void callLLMToGenerate(int segmentIndex, int threadId);

    public:
        DictionaryGenerator(std::shared_ptr<IController> controller, std::shared_ptr<spdlog::logger> logger, std::unique_ptr<APIPool>& apiPool, const std::function<NLPResult(const std::string&)>& tokenizeFunc, const fs::path& otherCacheDir,
            const std::function<void(Sentence*)>& preProcessFunc, const std::function<std::string(std::string)>& onPerformApi, const std::string& systemPrompt, const std::string& userPrompt, const std::string& apiStrategy, const std::string& targetLang,
            int maxRetries, int threadsNum, int apiTimeoutMs, bool checkQuota);
        void generate(const std::vector<fs::path>& jsonFiles, const fs::path& outputFilePath);

        ~DictionaryGenerator() {
            saveTokenizeCache(m_tokenizeCacheMap, m_tokenizeCachePath, m_logger);
        }
    };
}


module :private;

DictionaryGenerator::DictionaryGenerator(std::shared_ptr<IController> controller, std::shared_ptr<spdlog::logger> logger, std::unique_ptr<APIPool>& apiPool, const std::function<NLPResult(const std::string&)>& tokenizeFunc, const fs::path& otherCacheDir,
    const std::function<void(Sentence*)>& preProcessFunc, const std::function<std::string(std::string)>& onPerformApi, const std::string& systemPrompt, const std::string& userPrompt, const std::string& apiStrategy, const std::string& targetLang,
    int maxRetries, int threadsNum, int apiTimeoutMs, bool checkQuota)
    : m_controller(controller), m_logger(logger), m_apiPool(apiPool), m_tokenizeSourceLangFunc(tokenizeFunc), m_tokenizeCachePath(otherCacheDir / L"tokenizeCache_dictgen.json"),
    m_preProcessFunc(preProcessFunc), m_onPerformApi(onPerformApi), m_systemPrompt(systemPrompt), m_userPrompt(userPrompt), m_apiStrategy(apiStrategy), m_targetLang(targetLang),
    m_maxRetries(maxRetries), m_threadsNum(threadsNum), m_apiTimeoutMs(apiTimeoutMs), m_checkQuota(checkQuota)
{
    m_tokenizeCacheMap = loadTokenizeCache(m_tokenizeCachePath, m_logger);
}

void DictionaryGenerator::preprocessAndTokenize(const std::vector<fs::path>& jsonFiles) {
    m_logger->info("阶段一：预处理和分词...");
    std::string currentSegment;
    constexpr size_t MAX_SEGMENT_LEN = 512;

    std::ifstream ifs;
    for (const auto& jsonFile : jsonFiles)
    {
        ifs.open(jsonFile);
        json data = json::parse(ifs);
        ifs.close();

        for (const auto& item : data) {
            m_totalSentences++;
            Sentence se;
            if (item.contains("name")) {
                se.nameType = NameType::Single;
                se.name = item.value("name", "");
            }
            else if (item.contains("names")) {
                se.nameType = NameType::Multiple;
                se.names = item["names"].get<std::vector<std::string>>();
            }
            else {
                se.nameType = NameType::None;
            }
            se.original_text = item.value("message", "");

            m_preProcessFunc(&se);
            if (se.complete) {
                continue;
            }
            replaceStrInplace(se.pre_processed_text, "<br>", "");
            replaceStrInplace(se.pre_processed_text, "<tab>", "");

            if (se.nameType == NameType::Single && !se.name.empty()) {
                m_nameSet.insert(se.name);
                m_wordCounter[se.name] += 2;
            }
            else if (se.nameType == NameType::Multiple) {
                for (const auto& name : se.names | std::views::filter([](const std::string& name) { return !name.empty(); })) {
                    m_nameSet.insert(name);
                    m_wordCounter[name] += 2;
                }
            }

            std::string currentText = getNameString(&se);
            if (!currentText.empty()) {
                currentText += ": ";
            }
            currentText += se.pre_processed_text + "\n";
            currentSegment += currentText;
            if (currentSegment.length() > MAX_SEGMENT_LEN && countGraphemes(currentSegment) > MAX_SEGMENT_LEN) {
                m_segments.push_back(std::move(currentSegment));
                currentSegment.clear();
            }
        }

        if (!currentSegment.empty()) {
            m_segments.push_back(std::move(currentSegment));
            currentSegment.clear();
        }
    }
    
    if (!currentSegment.empty()) {
        m_segments.push_back(std::move(currentSegment));
    }

    m_logger->info("共分割成 {} 个文本块，开始进行分词(使用依赖 Python 且未进行 GPU加速 的分词器这步会非常慢)...", m_segments.size());
    m_controller->makeBar((int)m_segments.size(), 1);
    m_controller->addThreadNum();
    m_segmentWords.reserve(m_segments.size());
    std::unordered_set<std::string> wordsInSegment;
    auto procEntityVecFunc = [&](const EntityVec& entityVec, const std::string& segment)
        {
            for (const auto& entity : entityVec) {
                wordsInSegment.insert(entity.front());
                m_wordCounter[entity.front()]++;
            }
            if (m_logger->should_log(spdlog::level::trace)) {
                std::string entityStr;
                for (const auto& entity : entityVec) {
                    entityStr += "[" + entity.front() + ", " + entity[1] + "] ";
                }
                m_logger->trace("原文: {}\n分词实体结果: {}", segment, entityStr);
            }
        };
    for (const auto& segment : m_segments) {
        if (auto it = m_tokenizeCacheMap.find(segment); it != m_tokenizeCacheMap.end()) {
            EntityVec& entityVec = it->second;
            procEntityVecFunc(entityVec, segment);
        }
        else {
            NLPResult result = m_tokenizeSourceLangFunc(segment);
            EntityVec& entityVec = std::get<1>(result);
            std::erase_if(entityVec, [](const std::vector<std::string>& entity)
                {
                    static const std::set<std::string> excludeEntities =
                    {
                        "TITLE_AFFIX", "QUANTITY", "ORDINAL", "DATE", "MONEY"
                    };
                    if (excludeEntities.contains(entity[1])) {
                        return true;
                    }
                    return removePunctuation(entity.front()).empty();
                });
            procEntityVecFunc(entityVec, segment);
            m_tokenizeCacheMap.insert({ segment, std::move(entityVec) });
        }
        m_segmentWords.push_back(std::move(wordsInSegment));
        wordsInSegment.clear();

        m_controller->updateBar();
        if (m_controller->shouldStop()) {
            break;
        }
    }
    m_controller->reduceThreadNum();
}

std::vector<int> DictionaryGenerator::solveSentenceSelection() {
    if (m_controller->shouldStop()) {
        return {};
    }
    m_logger->info("阶段二：搜索并选择信息量最大的文本块(单线程)...");

    // 剔除出现次数小于2的词语，人名除外
    std::unordered_set<std::string> allWords;
    allWords.reserve(m_wordCounter.size()); // 预分配空间，减少哈希冲突
    for (const auto& [word, count] : m_wordCounter) {
        if (count >= 2 || m_nameSet.contains(word)) {
            allWords.insert(word);
        }
    }

    // 过滤每个 segment 中的词，只保留在 allWords 中的
    std::vector<std::unordered_set<std::string>> filteredSegmentWords;
    for (const auto& segment : m_segmentWords) {
        std::unordered_set<std::string> filteredSet;
        for (const auto& word : segment) {
            if (allWords.contains(word)) {
                filteredSet.insert(word);
            }
        }
        filteredSegmentWords.push_back(std::move(filteredSet));
    }

    std::unordered_set<std::string> coveredWords;
    coveredWords.reserve(allWords.size());
    std::vector<int> selectedIndices;
    std::vector<uint8_t> usedIndices(filteredSegmentWords.size(), 0);

    m_controller->makeBar((int)allWords.size(), 1);
    m_controller->addThreadNum();

    while (coveredWords.size() < allWords.size()) {
        int bestIndex = -1;
        size_t maxNewCoverage = 0;
        // 手动计算每个未被选择的段落能覆盖的新词数量
        for (size_t i = 0; i < filteredSegmentWords.size(); ++i) {
            if (usedIndices[i]) {
                continue;
            }
            size_t currentNewCoverage = 0;
            // 遍历候选段落中的词
            for (const auto& word : filteredSegmentWords[i]) {
                if (!coveredWords.contains(word)) {
                    currentNewCoverage++;
                }
            }
            if (currentNewCoverage > maxNewCoverage) {
                maxNewCoverage = currentNewCoverage;
                bestIndex = static_cast<int>(i);
            }
        }
        if (bestIndex != -1) {
            // 将找到的最佳段落标记为已使用
            usedIndices[bestIndex] = 1;
            selectedIndices.push_back(bestIndex);
            // 将新覆盖的词加入 coveredWords 集合
            for (const auto& word : filteredSegmentWords[bestIndex]) {
                coveredWords.insert(word);
            }
            m_controller->updateBar();
        }
        else {
            // 如果没有段落能提供新词，则提前退出
            break;
        }
    }
    m_controller->reduceThreadNum();
    return selectedIndices;
}

void DictionaryGenerator::callLLMToGenerate(int segmentIndex, int threadId) {
    if (m_controller->shouldStop()) {
        return;
    }
    std::string text = m_segments[segmentIndex];
    std::string hint;
    for (const auto& name : m_nameSet) {
        if (text.contains(name)) {
            hint += name + "\n";
        }
    }
    if (!hint.empty()) {
        hint = "These words in the input text should always be added into glossary: \n" + hint;
    }

    std::string prompt = m_userPrompt;
    replaceStrInplace(prompt, "[TargetLang]", m_targetLang);
    replaceStrInplace(prompt, "[Input]", text);
    replaceStrInplace(prompt, "[Hint]", hint.empty() ? "None" : hint);

    json messages = json::array({
        {{"role", "system"}, {"content", m_systemPrompt}},
        {{"role", "user"}, {"content", prompt}}
        });

    int retryCount = 0;
    while (retryCount < m_maxRetries) {
        if (m_controller->shouldStop()) {
            return;
        }
        std::optional<TranslationApi> apiOpt = m_apiStrategy == "random" ? m_apiPool->getApi() : m_apiPool->getFirstApi();
        if (!apiOpt) {
            throw std::runtime_error("没有可用的API Key了");
        }
        const TranslationApi& currentApi = apiOpt.value();
        json payload = { {"messages", messages} };

        std::string logBlock;
        if (!hint.empty()) {
            logBlock += "\nHint:\n" + hint;
        }
        logBlock += "\ninputBlock:\n" + text;
        m_logger->info("[线程 {}] 开始从段落中生成术语表:\n{}", threadId, logBlock);
        ApiResponse response = performApiRequest(payload, currentApi, m_onPerformApi, threadId, m_controller, m_logger, m_apiTimeoutMs);

        /*bool checkResponse(const ApiResponse& response, std::unique_ptr<APIPool>& m_apiPool, const TranslationApi& currentAPI,
            const std::filesystem::path& relInputPath, const std::string& m_apiStrategy, std::shared_ptr<spdlog::logger>& m_logger,
            int& retryCount, int threadId, bool m_checkQuota);*/
        if (!checkResponse(
            response, m_apiPool, currentApi, L"字典生成——段落输入", m_apiStrategy, m_logger, retryCount, threadId, m_checkQuota
        )) {
            continue;
        }
        else {
            m_logger->info("[线程 {}] AI 字典生成成功:\n {}", threadId, response.content);
            const auto lines = splitString(response.content, '\n');
            for (const auto& line : lines) {
                const auto parts = splitString(line, '\t');
                if (parts.size() < 3 || parts[0].starts_with("Original_terms") || parts[0].starts_with("日文原词") || parts[0].starts_with("NULL")) {
                    continue;
                }
                std::lock_guard<std::mutex> lock(m_resultMutex);
                int& counter = m_finalCounter[parts[0]];
                ++counter;
                if (counter == 2) {
                    m_logger->debug("发现重复术语: {}\t{}\t{}", parts[0], parts[1], parts[2]);
                }
                m_finalDict.emplace_back(parts[0], parts[1], parts[2]);
            }
            break;
        }
    }
    if (retryCount >= m_maxRetries) {
        m_logger->error("[线程 {}] AI 字典生成失败，已达到最大重试次数。", threadId);
    }

    m_controller->updateBar();
}

void DictionaryGenerator::generate(const std::vector<fs::path>& jsonFiles, const fs::path& outputFilePath) {
    if (jsonFiles.empty()) {
        throw std::runtime_error("没有输入文件，无法生成字典。");
    }
    preprocessAndTokenize(jsonFiles);
    std::vector<int> selectedIndices = solveSentenceSelection();
    if (int maxSelectedIndicesCount = std::max(m_totalSentences / 250, 128); selectedIndices.size() > maxSelectedIndicesCount) {
        selectedIndices.resize(maxSelectedIndicesCount);
    }
    if (m_controller->shouldStop()) {
        m_logger->info("任务终止，将不会生成字典文件。");
        return;
    }
    
    m_logger->info("阶段三：启动 {} 个线程，向 AI 发送 {} 个任务...", m_threadsNum, selectedIndices.size());
    m_controller->makeBar((int)selectedIndices.size(), m_threadsNum);
    ctpl::thread_pool pool(m_threadsNum);
    std::vector<std::future<void>> results;
    for (int segmentIdx : selectedIndices) {
        results.emplace_back(pool.push([=](int threadId) 
            {
                m_controller->addThreadNum();
                this->callLLMToGenerate(segmentIdx, threadId);
                m_controller->reduceThreadNum();
            }));
    }
    for (auto& res : results) {
        res.get();
    }

    if (m_controller->shouldStop()) {
        m_logger->info("任务终止，将不会生成字典文件。");
        return;
    }
    m_logger->info("阶段四：整理并保存结果...");
    std::vector<std::tuple<std::string, std::string, std::string>> finalList;

    // 按出现次数排序
    std::ranges::sort(m_finalDict, [&](const auto& a, const auto& b) 
        {
            return m_finalCounter[std::get<0>(a)] > m_finalCounter[std::get<0>(b)];
        });

    // 过滤
    for (const auto& item : m_finalDict) {
        const auto& src = std::get<0>(item);
        const auto& note = std::get<2>(item);
        if (m_finalCounter[src] > 1 || note.contains("人名") || note.contains("地名") || m_wordCounter.contains(src) || m_nameSet.contains(src)) {
            finalList.push_back(item);
        }
    }

    // 去重
    std::set<std::string> seen;
    std::erase_if(finalList, [&](const auto& item)
        {
            if (seen.contains(std::get<0>(item))) {
                return true;
            }
            seen.insert(std::get<0>(item));
            return false;
        });


    toml::ordered_value arr = toml::array{};
    
    for (const auto& item : finalList) {
        arr.push_back(toml::ordered_table{ { "org", std::get<0>(item) }, { "rep", std::get<1>(item) }, { "note", std::get<2>(item) } });
    }
    
    arr.as_array_fmt().fmt = toml::array_format::multiline;
    std::ofstream ofs(outputFilePath);
    ofs << toml::format(toml::ordered_value{ toml::ordered_table{{ "gptDict", arr }} });
    ofs.close();
    m_logger->info("字典生成完成，共 {} 个词语，已保存到 {}", finalList.size(), wide2Ascii(outputFilePath));
}
