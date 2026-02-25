module;

#include "GPPMacros.hpp"
#include <spdlog/spdlog.h>
#include <toml.hpp>

export module NormalJsonTranslatorHelperTool;

import Tool;
import ITranslator;

using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;
namespace fs = std::filesystem;

export {

    std::string generateCacheKey(const Sentence* s);
    std::string generateCacheKey(const json& jsonArr, size_t i);

    std::string buildContextHistory(const std::vector<Sentence*>& batch, TransEngine transEngine, int contextHistorySize, int maxChars);

    void fillBlockAndMap(const std::vector<Sentence*>& batchToTransThisRound, absl::btree_map<int, Sentence*>& id2SentenceMap, std::string& inputBlock, TransEngine transEngine);

    void parseContent(std::string& content, std::vector<Sentence*>& batchToTransThisRound, absl::btree_map<int, Sentence*>& id2SentenceMap, const std::string& modelName,
        std::shared_ptr<IController>& controller, std::string& backgroudText, std::atomic<int>& completedSentences, int& parsedCount, TransEngine transEngine, bool showBackgroundText);

    void combineOutputFiles(const fs::path& originalRelFilePath, const absl::flat_hash_map<fs::path, bool>& splitFileParts,
        const fs::path& outputCacheDir, const fs::path& outputDir, std::shared_ptr<spdlog::logger>& logger);

    bool hasRetranslKey(const std::vector<CheckSeCondFunc>& retranslKeys, const json& item, const Sentence* currentSe);

    void saveCache(const std::vector<Sentence>& allSentences, const fs::path& cachePath);

    std::vector<ordered_json> splitJsonArrayNum(const ordered_json& originalData, int chunkSize);
    std::vector<ordered_json> splitJsonArrayEqual(const ordered_json& originalData, int numParts);

    int getSplittedFileIndex(const std::wstring& path);
    int calculateCachePartIndexDiff(const std::wstring& path1, const std::wstring& path2);

    json toml2Json(const toml::value& value);
    ordered_json toml2Json(const toml::ordered_value& tomlData);
}
