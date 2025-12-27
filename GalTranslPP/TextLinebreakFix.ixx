module;

#include <spdlog/spdlog.h>
#include <toml.hpp>

export module TextLinebreakFix;

import Tool;
import NLPTool;
export import IPlugin;

namespace fs = std::filesystem;

export {

	enum class LinebreakFixMode
	{
		None, Average, FixCharCount, KeepPositions, PreferPunctuations, NotFix
	};

	class TextLinebreakFix {

	private:

		std::function<NLPResult(const std::string&)> m_tokenizeTargetLangFunc;
		std::vector<std::string> splitIntoTokens(const std::string& text);

		fs::path m_tokenizeCachePath;
		std::unordered_map<std::string, WordPosVec> m_tokenizeCacheMap;
		std::shared_mutex m_tokenizeCacheMapMutex;
		double m_priorityThreshold;
		std::shared_ptr<spdlog::logger> m_logger;
		LinebreakFixMode m_mode;
		int m_segmentThreshold;
		int m_errorThreshold;
		bool m_forceFix;
		bool m_useTokenizer;
		bool m_needReboot = false;


	public:

		TextLinebreakFix(const fs::path& otherCacheDir, const toml::value& projectConfig, std::shared_ptr<spdlog::logger> logger);

		bool needReboot() const { return m_needReboot; }

		void run(Sentence* se);

		~TextLinebreakFix()
		{
			if (m_useTokenizer) {
				saveTokenizeCache(m_tokenizeCacheMap, m_tokenizeCachePath, m_logger);
			}
		}
	};
}


module :private;

TextLinebreakFix::TextLinebreakFix(const fs::path& otherCacheDir, const toml::value& projectConfig, std::shared_ptr<spdlog::logger> logger)
	: m_tokenizeCachePath(otherCacheDir / L"tokenizeCache_tlf.json"), m_logger(logger)
{
	try {
		const auto pluginConfig = toml::parse(postPluginConfigPath / L"TextLinebreakFix.toml");

		std::string linebreakMode = parseToml<std::string>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.换行模式");
		if (linebreakMode == "平均") {
			m_mode = LinebreakFixMode::Average;
		}
		else if (linebreakMode == "固定字数") {
			m_mode = LinebreakFixMode::FixCharCount;
		}
		else if (linebreakMode == "保留位置") {
			m_mode = LinebreakFixMode::KeepPositions;
		}
		else if (linebreakMode == "优先标点") {
			m_mode = LinebreakFixMode::PreferPunctuations;
		}
		else if (linebreakMode == "不修改") {
			m_mode = LinebreakFixMode::NotFix;
		}
		else {
			throw std::invalid_argument("TextLinebreakFix 无效的换行模式: " + linebreakMode);
		}
		m_priorityThreshold = parseToml<double>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.优先阈值");
		m_segmentThreshold = parseToml<int>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.分段字数阈值");
		m_forceFix = parseToml<bool>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.强制修复");
		m_errorThreshold = parseToml<int>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.报错阈值");
		m_useTokenizer = parseToml<bool>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.使用分词器");


		if (m_useTokenizer) {
			m_tokenizeCacheMap = loadTokenizeCache(m_tokenizeCachePath, m_logger);
			const std::string tokenizerBackend = parseToml<std::string>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.tokenizerBackend");
			if (tokenizerBackend == "MeCab") {
				const std::string mecabDictDir = parseToml<std::string>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.mecabDictDir");
				m_logger->info("TextLinebreakFix 正在检查 MeCab 环境...");
				m_tokenizeTargetLangFunc = getMeCabTokenizeFunc(mecabDictDir, m_logger);
				m_logger->info("TextLinebreakFix MeCab 环境检查完毕。");
			}
			else if (tokenizerBackend == "spaCy") {
				const std::string spaCyModelName = parseToml<std::string>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.spaCyModelName");
				m_logger->info("TextLinebreakFix 正在检查 spaCy 环境...");
				m_tokenizeTargetLangFunc = getNLPTokenizeFunc({ "spacy" }, "tokenizer_spacy", spaCyModelName, m_logger, m_needReboot);
				m_logger->info("TextLinebreakFix spaCy 环境检查完毕。");
			}
			else if (tokenizerBackend == "Stanza") {
				const std::string stanzaLang = parseToml<std::string>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.stanzaLang");
				m_logger->info("TextLinebreakFix 正在检查 Stanza 环境...");
				m_tokenizeTargetLangFunc = getNLPTokenizeFunc({ "stanza" }, "tokenizer_stanza", stanzaLang, m_logger, m_needReboot);
				m_logger->info("TextLinebreakFix Stanza 环境检查完毕。");
			}
			else if (tokenizerBackend == "pkuseg") {
				m_logger->info("TextLinebreakFix 正在检查 pkuseg 环境...");
				m_tokenizeTargetLangFunc = getNLPTokenizeFunc({ "setuptools", "nes-py", "cython", "pkuseg"}, "tokenizer_pkuseg", "default", m_logger, m_needReboot);
				m_logger->info("TextLinebreakFix pkuseg 环境检查完毕。");
			}
			else {
				throw std::invalid_argument("TextLinebreakFix 无效的 tokenizerBackend: " + tokenizerBackend);
			}
		}

		if (m_segmentThreshold <= 0) {
			throw std::runtime_error("TextLinebreakFix 分段字数阈值必须大于0");
		}
		if (m_errorThreshold <= 0) {
			throw std::runtime_error("TextLinebreakFix 报错阈值必须大于0");
		}

		m_logger->info("已加载插件 TextLinebreakFix, 换行模式: {}, 优先阈值 {:.2f}, 分段字数阈值: {}, 强制修复: {}, 报错阈值: {}",
			linebreakMode, m_priorityThreshold, m_segmentThreshold, m_forceFix, m_errorThreshold);
		if (m_useTokenizer) {
			m_logger->info("插件 TextLinebreakFix 分词器已启用");
		}
	}
	catch (const toml::exception& e) {
		m_logger->critical("TextLinebreakFix 插件配置文件解析错误");
		throw std::runtime_error(e.what());
	}
}

std::vector<std::string> TextLinebreakFix::splitIntoTokens(const std::string& text)
{
	{
		std::shared_lock<std::shared_mutex> lock(m_tokenizeCacheMapMutex);
		if (auto it = m_tokenizeCacheMap.find(text); it != m_tokenizeCacheMap.end()) {
			return ::splitIntoTokens(it->second, text);
		}
	}
	const NLPResult result = m_tokenizeTargetLangFunc(text);
	const WordPosVec& wordPosVec = std::get<0>(result);
	{
		std::lock_guard<std::shared_mutex> lock(m_tokenizeCacheMapMutex);
		m_tokenizeCacheMap.insert({ text, wordPosVec });
	}
	return ::splitIntoTokens(wordPosVec, text);
}

void TextLinebreakFix::run(Sentence* se)
{
	int origLinebreakCount = countSubstring(se->pre_processed_text, "<br>");
	int transLinebreakCount = countSubstring(se->translated_preview, "<br>");

	auto checkLineCharCountFunc = [&](const std::string& transViewToModify)
		{
			if (transViewToModify.length() > m_errorThreshold) {
				for (const auto& [index, newLineView] : transViewToModify | std::views::split(std::string_view{ "<br>" })
					| std::views::transform([](const auto& subStrView)
						{
							return std::string_view(subStrView.begin(), subStrView.end());
						}) 
					| std::views::enumerate)
				{
					if (size_t charCount = countGraphemes(newLineView); charCount > m_errorThreshold) {
						m_logger->trace("句子[{}](原有{}行)其中的 译文行[{}]超出报错阈值[{}/{}]", se->pre_processed_text, origLinebreakCount + 1, newLineView, charCount, m_errorThreshold);
						se->problems.push_back(std::format("第 {} 行字数超出报错阈值[{}/{}]", index + 1, charCount, m_errorThreshold));
					}
				}
			}
		};

	if (
		(m_mode == LinebreakFixMode::NotFix) ||
		(transLinebreakCount == origLinebreakCount && !m_forceFix)
		) {
		checkLineCharCountFunc(se->translated_preview);
		return;
	}

	m_logger->debug("需要修复换行的句子[{}]: 原文 {} 行, 译文 {} 行", se->pre_processed_text, origLinebreakCount + 1, transLinebreakCount + 1);

	std::string origTransPreview = se->translated_preview;
	std::string transViewToModify = se->translated_preview;
	replaceStrInplace(transViewToModify, "<br>", "");
	if (transViewToModify.empty()) {
		for (int i = 0; i < origLinebreakCount; i++) {
			transViewToModify += "<br>";
		}
		se->translated_preview = transViewToModify;

		se->other_info["换行修复"] = std::format("原文 {} 行, 译文 {} 行, 修正后 {} 行", origLinebreakCount + 1, transLinebreakCount + 1, transLinebreakCount + 1);
		m_logger->debug("译文[{}]({}行): 修正后译文[{}]({}行)", origTransPreview, origLinebreakCount + 1, se->translated_preview, transLinebreakCount + 1);
		return;
	}

	auto removeRepeat = [](std::vector<size_t>& positions)
		{
			std::set<size_t> seen;
			std::erase_if(positions, [&](size_t pos)
				{
					if (seen.contains(pos)) {
						return true;
					}
					seen.insert(pos);
					return false;
				});
		};

	switch (m_mode) 
	{
	case LinebreakFixMode::Average:
	{
		std::vector<std::string> tokens = m_useTokenizer ? splitIntoTokens(transViewToModify) : splitIntoGraphemes(transViewToModify);
		size_t totalCharCount = countGraphemes(transViewToModify);
		int linebreakAdded = 0;

		size_t charCountLine = totalCharCount / (origLinebreakCount + 1);
		if (charCountLine == 0) {
			charCountLine = 1;
		}

		transViewToModify.clear();
		for (size_t i = 0; i < tokens.size(); i++) {
			transViewToModify += tokens[i];
			if ((i + 1) % charCountLine == 0 && linebreakAdded < origLinebreakCount) {
				transViewToModify += "<br>";
				linebreakAdded++;
			}
		}
	}
	break;

	case LinebreakFixMode::FixCharCount:
	{
		std::vector<std::string> graphemes = splitIntoGraphemes(transViewToModify);
		transViewToModify.clear();
		for (size_t i = 0; i < graphemes.size(); i++) {
			transViewToModify += graphemes[i];
			if ((i + 1) % m_segmentThreshold == 0 && i != graphemes.size() - 1) {
				transViewToModify += "<br>";
			}
		}
	}
	break;

	case LinebreakFixMode::KeepPositions:
	{
		std::vector<std::string> tokens = m_useTokenizer ? splitIntoTokens(transViewToModify) : splitIntoGraphemes(transViewToModify);
		std::vector<double> relLinebreakPositions = getSubstringPositions(se->pre_processed_text, "<br>");
		std::vector<size_t> positionsToAddLinebreak; // 最终要在 transViewToModify 中插入换行符的位置

		size_t currentPos = 0;
		size_t currentTokenIndex = 0;
		for (const auto& relLinebreakPos : relLinebreakPositions) {
			while (currentPos / (double)transViewToModify.length() < relLinebreakPos) {
				currentPos += tokens[currentTokenIndex].length();
				currentTokenIndex++;
			}
			positionsToAddLinebreak.push_back(currentPos);
		}

		removeRepeat(positionsToAddLinebreak);
		for (const auto& posToAddLinebreak : positionsToAddLinebreak | std::views::reverse) {
			transViewToModify.insert(posToAddLinebreak, "<br>");
		}
	}
	break;

	case LinebreakFixMode::PreferPunctuations:
	{
		std::vector<std::string> graphemes = splitIntoGraphemes(transViewToModify);
		std::vector<std::string> tokens = m_useTokenizer ? splitIntoTokens(transViewToModify) : graphemes;
		static const std::set<std::string> excludePuncts =
		{ "『", "「", "“", "‘", "'", "《", "〈", "（", "【", "〔", "〖" };

		// 预处理标点信息，获取所有可以添加换行的标点位置
		std::vector<double> relLinebreakPositions = getSubstringPositions(se->pre_processed_text, "<br>");

		struct PunctInfo
		{
			size_t prePos;
			size_t postPos;
			double relPos;
		};
		std::vector<PunctInfo> punctPositions;
		size_t currentPos = 0;
		for (size_t i = 0; i < graphemes.size(); i++) {
			currentPos += graphemes[i].length();
			if (removePunctuation(graphemes[i]).empty()) {
				punctPositions.push_back({ currentPos - graphemes[i].length(), currentPos, currentPos / (double)transViewToModify.length() });
			}
			// 如果当前字符的后一个字符是全角空格或如上左边界字符，则把当前字符作为标点对待
			else if (
				i + 1 < graphemes.size() && 
				(graphemes[i + 1] == "　" || excludePuncts.contains(graphemes[i + 1]))
				) {
				punctPositions.push_back({ currentPos - graphemes[i].length(), currentPos, currentPos / (double)transViewToModify.length() });
			}
		}
		std::erase_if(punctPositions, [&](PunctInfo& pos)
			{
				if (pos.postPos >= transViewToModify.length()) {
					return true;
				}
				std::string punctStr = transViewToModify.substr(pos.prePos, pos.postPos - pos.prePos);
				// 不在这些标点后添加换行符
				if (excludePuncts.contains(punctStr)) {
					return true;
				}
				// 如果标点后面还有标点，则不插入换行符
				return std::ranges::any_of(punctPositions, [&](auto& otherPos)
					{
						return pos.postPos == otherPos.prePos;
					});
			});

		std::vector<size_t> positionsToAddLinebreak; // 最终要在 transViewToModify 中插入换行符的位置
		// 预处理信息完毕


		if (origLinebreakCount <= (int)punctPositions.size()) {
			currentPos = 0;
			size_t currentTokenIndex = 0;
			// 换行挑标点
			std::vector<PunctInfo> filteredPunctPositions; // 被挑出来的标点位置
			for (const auto& relLinebreakPos : relLinebreakPositions) {
				auto it = std::ranges::min_element(punctPositions, [&](const auto& a, const auto& b)
					{
						return calculateAbs(a.relPos, relLinebreakPos)
							< calculateAbs(b.relPos, relLinebreakPos);
					});
				filteredPunctPositions.push_back(*it);
				punctPositions.erase(it);
			}
			std::ranges::sort(filteredPunctPositions, [](const auto& a, const auto& b)
				{
					return a.prePos < b.prePos;
				});
			for (const auto& [index, punctPos] : filteredPunctPositions | std::views::enumerate) {
				if (double relLinebreakPos = relLinebreakPositions[index];  calculateAbs(punctPos.relPos, relLinebreakPos) > m_priorityThreshold) {
					while (currentTokenIndex < tokens.size()) {
						double relCur = currentPos / (double)transViewToModify.length();
						currentPos += tokens[currentTokenIndex].length();
						currentTokenIndex++;
						if (double newRelCur = currentPos / (double)transViewToModify.length(); newRelCur >= relLinebreakPos) {
							if (newRelCur - relLinebreakPos > relLinebreakPos - relCur) {
								currentTokenIndex--;
								currentPos -= tokens[currentTokenIndex].length();
							}
							break;
						}
					}
					positionsToAddLinebreak.push_back(currentPos);
				}
				else {
					positionsToAddLinebreak.push_back(punctPos.postPos);
				}
			}
		}
		else {
			currentPos = 0;
			size_t currentTokenIndex = 0;
			// 标点挑换行
			std::vector<double> filteredRelLinebreakPositions; // 被挑出来的换行位置
			for (const auto& punctPos : punctPositions) {
				auto it = std::ranges::min_element(relLinebreakPositions, [&](const auto& a, const auto& b)
					{
						return calculateAbs(a, punctPos.relPos)
							< calculateAbs(b, punctPos.relPos);
					});
				filteredRelLinebreakPositions.push_back(*it);
				relLinebreakPositions.erase(it);
			}
			std::ranges::sort(filteredRelLinebreakPositions);
			for (const auto& [index, relLinebreakPos] : filteredRelLinebreakPositions | std::views::enumerate) {
				if (double punctPos = punctPositions[index].relPos;  calculateAbs(relLinebreakPos, punctPos) > m_priorityThreshold) {
					while (currentTokenIndex < tokens.size()) {
						double relCur = currentPos / (double)transViewToModify.length();	
						currentPos += tokens[currentTokenIndex].length();
						currentTokenIndex++;
						if (double newRelCur = currentPos / (double)transViewToModify.length(); newRelCur >= relLinebreakPos) {
							if (newRelCur - relLinebreakPos > relLinebreakPos - relCur) {
								currentTokenIndex--;
								currentPos -= tokens[currentTokenIndex].length();
							}
							break;
						}
					}
					positionsToAddLinebreak.push_back(currentPos);
				}
				else {
					positionsToAddLinebreak.push_back(punctPositions[index].postPos);
				}
			}
			currentPos = 0;
			currentTokenIndex = 0;
			for (const auto& relLinebreakPos : relLinebreakPositions) {
				while (currentTokenIndex < tokens.size()) {
					double relCur = currentPos / (double)transViewToModify.length();
					currentPos += tokens[currentTokenIndex].length();
					currentTokenIndex++;
					if (double newRelCur = currentPos / (double)transViewToModify.length(); newRelCur >= relLinebreakPos) {
						if (newRelCur - relLinebreakPos > relLinebreakPos - relCur) {
							currentTokenIndex--;
							currentPos -= tokens[currentTokenIndex].length();
						}
						break;
					}
				}
				positionsToAddLinebreak.push_back(currentPos);
			}
		}

		removeRepeat(positionsToAddLinebreak);
		std::ranges::sort(positionsToAddLinebreak);

		for (const auto& posToAddLinebreak : positionsToAddLinebreak | std::views::reverse) {
			transViewToModify.insert(posToAddLinebreak, "<br>");
		}

		if (m_logger->should_log(spdlog::level::debug)) {
			std::string tokensStr;
			for (const auto& token : tokens) {
				tokensStr += "[" + token + "] ";
			}
			se->other_info["译文分词结果"] = tokensStr;
		}
	}
	break;

	}

	se->translated_preview = std::move(transViewToModify);
	checkLineCharCountFunc(se->translated_preview);

	int newLinebreakCount = countSubstring(se->translated_preview, "<br>");
	se->other_info["换行修复"] = std::format("原文 {} 行, 译文 {} 行, 修正后 {} 行", origLinebreakCount + 1, transLinebreakCount + 1, newLinebreakCount + 1);
	m_logger->debug("句子[{}]({}行): 修正后译文[{}]({}行)", origTransPreview, transLinebreakCount + 1, se->translated_preview, newLinebreakCount + 1);
}

