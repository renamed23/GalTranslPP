module;

#include "GPPMacros.hpp"
#include <spdlog/spdlog.h>

export module ProblemAnalyzer;

import Dictionary;
import GPPDefines;

export {

    struct ProblemCompareObj {
        bool use = false;
        CachePart base = CachePart::OrigText;
        CachePart check = CachePart::TransPreview;
    };

	struct Problems {
        ProblemCompareObj highFrequency;
        ProblemCompareObj punctsMiss;
        ProblemCompareObj remainJp;
        ProblemCompareObj introLatin;
        ProblemCompareObj introHangul;
        ProblemCompareObj introTraditionalChinese;
        ProblemCompareObj linebreakLost;
        ProblemCompareObj linebreakAdded;
        ProblemCompareObj longer;
        ProblemCompareObj strictlyLonger;
        ProblemCompareObj dictUnused;
        ProblemCompareObj notTargetLang;
        ProblemCompareObj invalidChar;
	};

	class ProblemAnalyzer {

	private:

        const std::unique_ptr<GptDictionary>& m_gptDictionary;
        std::move_only_function<std::string(const std::string&)> m_traditionalChineseExtractor;

        Problems m_problems;
        std::vector<std::string> m_punctsToCheck;
        double m_probabilityThreshold;
        std::string m_codePage;
        std::string m_targetLang;

        std::shared_ptr<spdlog::logger> m_logger;

	public:

        explicit ProblemAnalyzer(const std::unique_ptr<GptDictionary>& gptDictionary, const std::string& targetLang, const std::shared_ptr<spdlog::logger>& logger)
            : m_gptDictionary(gptDictionary), m_targetLang(targetLang), m_logger(logger) {}

        ~ProblemAnalyzer();

		void loadProblems(const std::vector<std::string>& problemList, const std::string& punctSet, const std::string& codePage, double langProbability);

        void overwriteCompareObj(const std::string& problemKey, const std::string& base, const std::string& check);

		void analyze(Sentence* sentence);
	};
}
