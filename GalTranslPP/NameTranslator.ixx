module;

#include "GPPMacros.hpp"
#include <spdlog/spdlog.h>

export module NameTranslator;

import APIPool;
import Dictionary;
import ITranslator;

namespace fs = std::filesystem;

export {
    class NameTranslator {
    private:
        std::shared_ptr<IController> m_controller;
        std::shared_ptr<spdlog::logger> m_logger;
        const std::unique_ptr<APIPool>& m_apiPool;
        const std::unique_ptr<GptDictionary>& m_gptDictionary;

        const std::function<std::string(std::string)>& m_onPerformApi;

        std::string m_systemPrompt;
        std::string m_userPrompt;
        std::string m_apiStrategy;
        std::string m_targetLang;
        int m_maxRetries;
        int m_apiTimeoutMs;
        bool m_checkQuota;

        // 内部辅助函数
        void translateBatch(std::span<std::string> batchNames, absl::flat_hash_map<std::string, std::string>& resultMap);

    public:
        NameTranslator(
            const std::shared_ptr<IController>& controller,
            const std::shared_ptr<spdlog::logger>& logger,
            const std::unique_ptr<APIPool>& apiPool,
            const std::unique_ptr<GptDictionary>& gptDictionary,
            const std::function<std::string(std::string)>& onPerformApi,
            const std::string& systemPrompt,
            const std::string& userPrompt,
            const std::string& apiStrategy,
            const std::string& targetLang,
            int maxRetries,
            int apiTimeoutMs,
            bool checkQuota
        );

        // 核心运行函数
        void run(const fs::path& nameTablePath);
    };
}
