module;

#define PYBIND11_HEADERS
#define PCRE2_HEADERS
#include "GPPMacros.hpp"
#include <spdlog/spdlog.h>
#include <toml.hpp>
#include <ctpl_stl.h>
#include <sol/sol.hpp>

export module NormalJsonTranslator;

import APIPool;
import Dictionary;
import IPlugin;
import ProblemAnalyzer;
import LuaManager;
import PythonManager;
export import ITranslator;

namespace fs = std::filesystem;

export {

    class NormalJsonTranslator : public ITranslator {

        friend void pybind11_init_gpp_plugin_api(::pybind11::module_& m);
        friend class LuaManager;

    protected:
        TransEngine m_transEngine;
        std::shared_ptr<IController> m_controller;
        std::shared_ptr<spdlog::logger> m_logger;

        fs::path m_inputDir;
        fs::path m_inputCacheDir;
        fs::path m_outputDir;
        fs::path m_outputCacheDir;
        fs::path m_transCacheDir;
        fs::path m_otherCacheDir;
        fs::path m_backgroundTextCachePath;
        fs::path m_projectDir;

        // relPathString, backgroundText
        std::mutex m_backgroundTextCacheMapMutex;
        std::map<std::string, std::string> m_backgroundTextCacheMap;

        std::string m_systemPrompt;
        std::string m_userPrompt;
        std::string m_targetLang;

        int m_totalSentences = 0;
        std::atomic<int> m_completedSentences = 0;

        bool m_pythonTranslator = false;

        int m_threadsNum;
        int m_batchSize;
        int m_contextHistorySize;
        int m_maxRetries;
        int m_saveCacheInterval;
        int m_apiTimeOutMs;
        bool m_checkQuota;
        bool m_smartRetry;
        bool m_usePreDictInName;
        bool m_usePostDictInName;
        bool m_usePreDictInMsg;
        bool m_usePostDictInMsg;
        bool m_useGptDictToReplaceName;
        bool m_outputWithSrc;

        std::string m_apiStrategy;
        std::string m_sortMethod;
        std::string m_splitFile;
        int m_splitFileNum;
        int m_cacheSearchDistance;
        std::string m_linebreakSymbol;
        std::vector<CheckSeCondFunc> m_retranslKeys;
        // first: 要忽略的 problem 正则表达式 pattern，second: 忽略条件
        using SkipProblemCondition = std::pair<jpc::Regex, std::optional<CheckSeCondFunc>>;
        std::vector<SkipProblemCondition> m_skipProblems;

        std::unique_ptr<PythonManager> m_pythonManager;
        std::unique_ptr<LuaManager> m_luaManager;

        bool m_needsCombining = false;
        std::shared_mutex m_transCacheMutex;
        std::mutex m_outputMutex;
        // 输入分割文件相对路径到原始json相对路径的映射
        std::unordered_map<fs::path, fs::path> m_splitFilePartsToJson;
        // 原始json相对路径到多个输入分割文件相对路径及其有没有完成的映射
        std::unordered_map<fs::path, std::unordered_map<fs::path, bool>> m_jsonToSplitFileParts;

        std::unordered_map<std::string, std::string> m_nameMap;
        toml::ordered_value m_problemOverview = toml::array{};
        std::function<void(fs::path)> m_onFileProcessed;
        std::function<std::string(std::string)> m_onPerformApi;
        std::function<DictList(DictList)> m_onDictProcessed;

        ctpl::thread_pool m_threadPool{1};
        std::unique_ptr<APIPool> m_apiPool;
        std::unique_ptr<GptDictionary> m_gptDictionary;
        std::unique_ptr<NormalDictionary> m_preDictionary;
        std::unique_ptr<NormalDictionary> m_postDictionary;
        std::unique_ptr<ProblemAnalyzer> m_problemAnalyzer;
        std::function<NLPResult(const std::string&)> m_tokenizeSourceLangFunc;
        std::vector<pro::proxy<PPlugin>> m_textPlugins;


        void preProcess(Sentence* se);

        void postProcess(Sentence* se);

        bool translateBatchWithRetry(const fs::path& relInputPath, std::vector<Sentence*>& batch, std::string& backgroundText, int threadId);

        void processFile(const fs::path& relInputPath, int threadId);

    public:
        NormalJsonTranslator(const fs::path& projectDir, const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger,
            std::optional<fs::path> inputDir = std::nullopt, std::optional<fs::path> inputCacheDir = std::nullopt,
            std::optional<fs::path> outputDir = std::nullopt, std::optional<fs::path> outputCacheDir = std::nullopt);

        virtual ~NormalJsonTranslator() override;

        void init();
        std::optional<std::vector<fs::path>> beforeRun();
        void process(std::vector<fs::path> relFilePaths);
        void afterRun();

        virtual void run() override;
    };
}
