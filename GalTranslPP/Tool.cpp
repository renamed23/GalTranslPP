module;

#include "GPPMacros.hpp"
#ifdef _WIN32
#include <Windows.h>
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "winhttp.lib")
#endif

#define BIT7Z_AUTO_FORMAT
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitfileextractor.hpp>
#include <spdlog/spdlog.h>
#include <unicode/unistr.h>
#include <unicode/brkiter.h>
#include <unicode/schriter.h>
#include <unicode/uscript.h>
#include <unicode/translit.h>
#include <boost/crc.hpp>
#include <toml.hpp>
#include <cpp-base64/base64.cpp>

#include <opencc/opencc.h>
#pragma comment(lib, "marisa.lib")
#pragma comment(lib, "opencc.lib")


module Tool;

using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;
namespace fs = std::filesystem;


#ifdef _WIN32
std::string wide2Ascii(const std::wstring& wide, UINT CodePage, LPBOOL usedDefaultChar) {
    int len = WideCharToMultiByte
    (CodePage, 0, wide.c_str(), (int)wide.length(), nullptr, 0, nullptr, usedDefaultChar);
    if (len == 0) return {};
    std::string ascii(len, '\0');
    WideCharToMultiByte
    (CodePage, 0, wide.c_str(), (int)wide.length(), ascii.data(), len, nullptr, nullptr);
    return ascii;
}

std::wstring ascii2Wide(const std::string& ascii, UINT CodePage) {
    int len = MultiByteToWideChar(CodePage, 0, ascii.c_str(), (int)ascii.length(), nullptr, 0);
    if (len == 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CodePage, 0, ascii.c_str(), (int)ascii.length(), wide.data(), len);
    return wide;
}

std::string ascii2Ascii(const std::string& ascii, UINT src, UINT dst, LPBOOL usedDefaultChar) {
    return wide2Ascii(ascii2Wide(ascii, src), dst, usedDefaultChar);
}

bool executeCommand(const std::wstring& program, const std::wstring& args, bool showWindow, int timeDelayAfterCommand) {

    std::wstring commandLineStr;

    if (showWindow) {
        // 如果需要显示窗口并延迟关闭：
        // 使用 cmd.exe /C 来执行命令。
        // 格式为: cmd.exe /C " "程序路径" 参数 & timeout /t 3 "
        // & 符号表示：无论前一个程序成功与否，都执行后面的 timeout
        // timeout /t 3 表示等待 3 秒
        // 注意：cmd /C 对引号的处理比较特殊，通常建议在最外层再包一对引号以防路径含空格出错
        commandLineStr = L"cmd.exe /C \"\"" + program + L"\" " + args + L" & timeout /t " + std::to_wstring(timeDelayAfterCommand) + L"\"";
    }
    else {
        // 如果隐藏窗口，保持原样直接运行，不需要 cmd 包装
        commandLineStr = L"\"" + program + L"\" " + args;
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = showWindow ? SW_SHOW : SW_HIDE;
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::vector<wchar_t> commandLineVec(commandLineStr.begin(), commandLineStr.end());
    commandLineVec.push_back(L'\0');

    DWORD creationFlags = 0;
    if (showWindow) {
        creationFlags |= CREATE_NEW_CONSOLE;
    }

    if (!CreateProcessW(NULL,
        &commandLineVec[0],
        NULL,
        NULL,
        FALSE,
        creationFlags,
        NULL,
        NULL,
        &si,
        &pi)) {
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
}

int getConsoleWidth() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) {
        return 80;
    }
    // 获取控制台屏幕缓冲区信息
    if (GetConsoleScreenBufferInfo(h, &csbi)) {
        // 宽度 = 右坐标 - 左坐标 + 1
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80; // 获取失败，返回默认值
}
#endif

std::string names2String(const std::vector<std::string>& names) {
    std::string result;
    for (const auto& name : names) {
        result += name + "|";
    }
    if (!result.empty()) {
        result.pop_back();
    }
    return result;
}

std::string names2String(const json& j) {
    std::string result;
    for (const auto& name : j) {
        result += name.get<std::string>() + "|";
    }
    if (!result.empty()) {
        result.pop_back();
    }
    return result;
}

std::string getNameString(const Sentence* se) {
    if (se->nameType == NameType::Single) {
        return se->name;
    }
    if (se->nameType == NameType::Multiple) {
        return names2String(se->names);
    }
    return {};
}

std::string getNameString(const json& j) {
    if (j.contains("name")) {
        return j["name"].get<std::string>();
    }
    if (j.contains("names")) {
        return names2String(j["names"]);
    }
    return {};
}

bool createParent(const fs::path& path) {
    if (path.has_parent_path() && !fs::exists(path.parent_path())) {
        return fs::create_directories(path.parent_path());
    }
    return false;
}

std::vector<std::string> splitTsvLine(const std::string& line, const std::vector<std::string>& delimiters) {
    std::vector<std::string> parts;
    size_t currentPos = 0;

    if (
        std::ranges::any_of(delimiters, [](const std::string& delimiter)
            {
                return delimiter.empty();
            })
        )
    {
        throw std::runtime_error("Empty delimiter is not allowed in TSV line splitting");
    }

    while (currentPos < line.length()) {
        size_t splitPos = std::string::npos;
        size_t delimiterLength = 0;
        for (const auto& delimiter : delimiters) {
            size_t pos = line.find(delimiter, currentPos);
            if (pos != std::string::npos && pos < splitPos) {
                splitPos = pos;
                delimiterLength = delimiter.length();
            }
        }
        std::string part = line.substr(currentPos, splitPos - currentPos);
        parts.push_back(std::move(part));

        if (splitPos == std::string::npos) {
            break;
        }
        currentPos = splitPos + delimiterLength;
    }

    return parts;
}


const std::string& chooseStringRef(const Sentence* sentence, CachePart tar) {
    switch (tar) {
    case CachePart::Name:
        return sentence->name;
        break;
    case CachePart::NamePreview:
        return sentence->name_preview;
        break;
    case CachePart::OrigText:
        return sentence->original_text;
        break;
    case CachePart::PreprocText:
        return sentence->pre_processed_text;
        break;
    case CachePart::PretransText:
        return sentence->pre_translated_text;
        break;
    case CachePart::TranslatedBy:
        return sentence->translated_by;
        break;
    case CachePart::TransPreview:
        return sentence->translated_preview;
        break;
    case CachePart::None:
        throw std::runtime_error("Invalid condition target: None");
    default:
        throw std::runtime_error("Invalid condition target to get string: " + std::to_string((int)tar));
    }
    return {};
}

std::string chooseString(const Sentence* sentence, CachePart tar) {
    return chooseStringRef(sentence, tar);
}

CachePart chooseCachePart(std::string_view partName) {
    CachePart part;
    if (partName == "name") {
        part = CachePart::Name;
    }
    else if (partName == "names") {
        part = CachePart::Names;
    }
    else if (partName == "name_preview") {
        part = CachePart::NamePreview;
    }
    else if (partName == "names_preview") {
        part = CachePart::NamesPreview;
    }
    else if (partName == "orig_text") {
        part = CachePart::OrigText;
    }
    else if (partName == "preproc_text") {
        part = CachePart::PreprocText;
    }
    else if (partName == "pretrans_text") {
        part = CachePart::PretransText;
    }
    else if (partName == "problems") {
        part = CachePart::Problems;
    }
    else if (partName == "other_info") {
        part = CachePart::OtherInfo;
    }
    else if (partName == "translated_by") {
        part = CachePart::TranslatedBy;
    }
    else if (partName == "trans_preview") {
        part = CachePart::TransPreview;
    }
    else {
        throw std::invalid_argument("无效的 CachePart: " + std::string(partName));
    }
    return part;
}

bool isSameExtension(const fs::path& filePath, const std::wstring& ext) {
    return str2Lower(filePath.extension()) == str2Lower(ext);
}


std::string removePunctuation(const std::string& text) {
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(text);
    icu::UnicodeString resultUstr;
    icu::StringCharacterIterator iter(ustr);
    UChar32 codePoint;
    while (iter.hasNext()) {
        codePoint = iter.next32PostInc();
        if (!u_ispunct(codePoint)) {
            resultUstr.append(codePoint);
        }
    }
    std::string utf8Output;
    return resultUstr.toUTF8String(utf8Output);
}

std::string removeWhitespace(const std::string& text) {
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(text);
    icu::UnicodeString resultUstr;
    icu::StringCharacterIterator iter(ustr);
    UChar32 codePoint;
    while (iter.hasNext()) {
        codePoint = iter.next32PostInc();
        if (!u_isspace(codePoint)) {
            resultUstr.append(codePoint);
        }
    }
    std::string utf8Output;
    return resultUstr.toUTF8String(utf8Output);
}

// 这两个函数遍历字形簇
std::pair<std::string, int> getMostCommonChar(const std::string& s) {
    if (s.empty()) {
        return { {}, 0 };
    }

    UErrorCode errorCode = U_ZERO_ERROR;
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(s);

    std::map<icu::UnicodeString, int> counts;

    std::unique_ptr<icu::BreakIterator> boundary(icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), errorCode));
    if (U_FAILURE(errorCode)) {
        throw std::runtime_error(std::format("Failed to create a character break iterator: {}", u_errorName(errorCode)));
    }
    boundary->setText(ustr);

    int32_t start = boundary->first();
    for (int32_t end = boundary->next(); end != icu::BreakIterator::DONE; start = end, end = boundary->next()) {
        icu::UnicodeString grapheme;
        ustr.extract(start, end - start, grapheme);
        counts[grapheme]++;
    }

    if (counts.empty()) {
        return { {}, 0 };
    }

    auto maxIterator = std::ranges::max_element(counts, [](const auto& a, const auto& b) 
        {
            return a.second < b.second;
        });

    icu::UnicodeString mostCommonGrapheme = maxIterator->first;
    int maxCount = maxIterator->second;

    std::string resultStr;

    return { mostCommonGrapheme.toUTF8String(resultStr), maxCount };
}

std::vector<std::string> splitIntoTokens(const WordPosVec& wordPosVec, const std::string& text)
{
    std::vector<std::string> tokens;

    size_t searchPos = 0; // 在原始句子中搜索的起始位置
    for (const auto& wordPos : wordPosVec) {
        const auto& token = wordPos.front();
        // 从 searchPos 开始查找当前 token
        size_t tokenPos = text.find(token, searchPos);
        // 错误处理：如果在预期位置找不到 token，说明输入有问题
        if (tokenPos == std::string::npos) {
            throw std::runtime_error("Token '" + token + "' not found in the remainder of the original sentence.");
        }
        // 1. 提取并添加 token 前面的空白部分
        if (tokenPos > searchPos) {
            tokens.push_back(text.substr(searchPos, tokenPos - searchPos));
        }
        // 2. 更新下一次搜索的起始位置
        searchPos = tokenPos + token.length();
        // 3. 添加 token 本身
        tokens.push_back(std::move(token));
    }
    // 4. 处理最后一个 token 后面的尾随空白
    if (searchPos < text.length()) {
        tokens.push_back(text.substr(searchPos));
    }

    return tokens;
}

std::vector<std::string> splitIntoGraphemes(const std::string& sourceString) {
    std::vector<std::string> resultVector;

    if (sourceString.empty()) {
        return resultVector;
    }

    UErrorCode errorCode = U_ZERO_ERROR;
    icu::UnicodeString uString = icu::UnicodeString::fromUTF8(sourceString);

    std::unique_ptr<icu::BreakIterator> breakIterator(
        icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), errorCode)
    );

    if (U_FAILURE(errorCode)) {
        throw std::runtime_error(std::format("Failed to create a character break iterator: {}", u_errorName(errorCode)));
    }

    breakIterator->setText(uString);

    int32_t start = breakIterator->first();
    for (int32_t end = breakIterator->next(); end != icu::BreakIterator::DONE; start = end, end = breakIterator->next()) {
        icu::UnicodeString graphemeUString;
        uString.extract(start, end - start, graphemeUString);
        std::string graphemeStdString;
        resultVector.push_back(graphemeUString.toUTF8String(graphemeStdString));
    }

    return resultVector;
}

auto countGraphemesFunc = [](auto&& sourceString) -> size_t
    {
        if (sourceString.empty()) {
            return 0;
        }
        UErrorCode errorCode = U_ZERO_ERROR;
        icu::UnicodeString uString = icu::UnicodeString::fromUTF8(sourceString);
        std::unique_ptr<icu::BreakIterator> breakIterator(
            icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), errorCode)
        );
        if (U_FAILURE(errorCode)) {
            throw std::runtime_error(std::format("Failed to create a character break iterator: {}", u_errorName(errorCode)));
        }
        breakIterator->setText(uString);

        int32_t start = breakIterator->first();
        size_t count = 0;
        for (int32_t end = breakIterator->next(); end != icu::BreakIterator::DONE; end = breakIterator->next()) {
            ++count;
        }
        return count;
    };

size_t countGraphemes(std::string_view sourceString) {
    return countGraphemesFunc(sourceString);
}

size_t countGraphemes(const std::string& sourceString) {
    return countGraphemesFunc(sourceString);
}

// 计算子串出现次数
int countSubstring(const std::string& text, std::string_view sub) {
    return (int)std::ranges::distance(text | std::views::split(sub)) - 1;
}

// 计算的是子串在删去子串后的主串中出现的位置
std::vector<double> getSubstringPositions(const std::string& text, std::string_view sub) {
    if (text.empty() || sub.empty()) return {};
    std::vector<size_t> positions;
    std::vector<double> relpositions;

    for (size_t offset = text.find(sub); offset != std::string::npos; offset = text.find(sub, offset + sub.length())) {
        positions.push_back(offset);
    }
    size_t newTotalLength = text.length() - positions.size() * sub.length();
    if (newTotalLength == 0) {
        return {};
    }
    for (size_t i = 0; i < positions.size(); i++) {
        size_t newPos = positions[i] - i * sub.length();
        relpositions.push_back((double)newPos / newTotalLength);
    }
    return relpositions;
}

std::string& replaceStrInplace(std::string& str, std::string_view org, std::string_view rep) {
    str = str | std::views::split(org) | std::views::join_with(rep) | std::ranges::to<std::string>();
    return str;
}

std::string replaceStr(const std::string& str, std::string_view org, std::string_view rep) {
    std::string result = str | std::views::split(org) | std::views::join_with(rep) | std::ranges::to<std::string>();
    return result;
}

// 核心辅助函数
// 接受一个源字符串和一组目标脚本，返回所有匹配字符组成的UTF-8字符串
std::string extractCharactersByScripts(const std::string& sourceString, const std::vector<UScriptCode>& targetScripts) {

    icu::UnicodeString resultUString;
    icu::UnicodeString sourceUString = icu::UnicodeString::fromUTF8(sourceString);

    icu::StringCharacterIterator iter(sourceUString);
    UChar32 codePoint;
    UErrorCode errorCode = U_ZERO_ERROR;

    while (iter.hasNext()) {
        codePoint = iter.next32PostInc();
        UScriptCode script = uscript_getScript(codePoint, &errorCode);

        if (U_SUCCESS(errorCode)) {
            auto it = std::ranges::find(targetScripts, script);
            if (it != targetScripts.end()) {
                resultUString.append(codePoint);
            }
        }
    }

    std::string resultString;
    return resultUString.toUTF8String(resultString);
}

std::string extractKatakana(const std::string& sourceString) {
    return extractCharactersByScripts(sourceString, { USCRIPT_KATAKANA });
}

std::string extractKana(const std::string& sourceString) {
    return extractCharactersByScripts(sourceString, { USCRIPT_HIRAGANA, USCRIPT_KATAKANA });
}

std::string extractLatin(const std::string& sourceString) {
    return extractCharactersByScripts(sourceString, { USCRIPT_LATIN });
}

std::string extractHangul(const std::string& sourceString) {
    return extractCharactersByScripts(sourceString, { USCRIPT_HANGUL });
}

std::string extractCJK(const std::string& sourceString) {
    return extractCharactersByScripts(sourceString, { USCRIPT_HAN });
}

std::function<std::string(const std::string&)> getTraditionalChineseExtractor(std::shared_ptr<spdlog::logger>& logger)
{
    // 是否需要线程安全？(似乎是不需要)
    std::function<std::string(const std::string&)> result;
    try {
        std::shared_ptr<opencc::SimpleConverter> converter = std::make_shared<opencc::SimpleConverter>("BaseConfig/opencc/t2s.json");
        result = [=](const std::string& sourceString)
            {
                static const std::set<std::string> excludeList =
                {
                    "乾", "阪"
                };
                std::string resultStr;
                std::vector<std::string> graphemes = splitIntoGraphemes(sourceString);
                for (const auto& grapheme : graphemes | std::views::filter([&](const std::string& g) { return !excludeList.contains(g); })) {
                    std::string simplified = converter->Convert(grapheme);
                    if (simplified != grapheme) {
                        resultStr += grapheme;
                    }
                }
                return resultStr;
            };
        logger->info("OpenCC is usable for traditional Chinese conversion");
        return result;
    }
    catch (...) {
        logger->error("OpenCC is not usable, falling back to ICU-based traditional Chinese conversion");
        UErrorCode status = U_ZERO_ERROR;
        auto toSimplified = std::shared_ptr<icu::Transliterator>(icu::Transliterator::createInstance("Traditional-Simplified", UTRANS_FORWARD, status));
        if (U_FAILURE(status)) {
            throw std::runtime_error("ICU-based traditional Chinese conversion is not available");
        }
        auto toTraditional = std::shared_ptr<icu::Transliterator>(icu::Transliterator::createInstance("Simplified-Traditional", UTRANS_FORWARD, status));
        if (U_FAILURE(status)) {
            throw std::runtime_error("ICU-based simplified Chinese conversion is not available");
        }

        result = [=](const std::string& sourceString)
            {
                // 白名单/排除列表：用于解决简繁转换中的歧义问题。
                // "著" (U+8457) 是一个典型例子，它在简体中文里也是合法字符，但T->S的转换规则可能导致误判。
                static const std::set<UChar32> excludeList = {
                    U'著', U'乾', U'阪',
                };

                icu::UnicodeString uSource = icu::UnicodeString::fromUTF8(sourceString);
                std::set<UChar32> traditionalChars;

                icu::StringCharacterIterator iter(uSource);
                UChar32 charSource;
                while (iter.hasNext()) {
                    charSource = iter.next32PostInc();

                    // 1. 必须是汉字
                    UErrorCode scriptErr = U_ZERO_ERROR;
                    if (uscript_getScript(charSource, &scriptErr) != USCRIPT_HAN || U_FAILURE(scriptErr)) {
                        continue;
                    }

                    // 2. 检查是否在排除列表中
                    if (excludeList.contains(charSource)) {
                        continue;
                    }

                    icu::UnicodeString uCharSource(charSource);
                    icu::UnicodeString uSimplified = uCharSource;
                    toSimplified->transliterate(uSimplified);

                    // 3. 繁体转简体后必须有变化
                    if (uCharSource == uSimplified) {
                        continue;
                    }

                    // 4. 核心双向检查：简体转回繁体，必须能得到原始字符，确保转换是明确且可逆的
                    icu::UnicodeString uReTraditional = uSimplified;
                    toTraditional->transliterate(uReTraditional);

                    if (uReTraditional == uCharSource) {
                        traditionalChars.insert(charSource);
                    }
                }

                icu::UnicodeString resultUStr;
                for (UChar32 ch : traditionalChars) {
                    resultUStr.append(ch);
                }

                std::string resultStr;
                return resultUStr.toUTF8String(resultStr);
            };
        logger->info("ICU-based traditional Chinese conversion is available");
        return result;
    }
    return result;
}

std::unordered_map<std::string, std::vector<std::vector<std::string>>> loadTokenizeCache
(const fs::path& cachePath, std::shared_ptr<spdlog::logger> logger) {
    std::unordered_map<std::string, std::vector<std::vector<std::string>>> result;
    try {
        if (fs::exists(cachePath)) {
            std::ifstream ifs(cachePath);
            result = json::parse(ifs).get<decltype(result)>();
        }
        else {
            logger->debug("未找到分词缓存 {}", wide2Ascii(cachePath));
        }
    }
    catch (...) {
        logger->error("读取分词缓存 {} 失败", wide2Ascii(cachePath));
    }
    return result;
}
void saveTokenizeCache
(const std::unordered_map<std::string, std::vector<std::vector<std::string>>>& cache, const fs::path& cachePath, std::shared_ptr<spdlog::logger>& logger) {
    try {
        createParent(cachePath);
        json j = cache;
        std::ofstream ofs(cachePath);
        ofs << j.dump(2);
        ofs.close();
        logger->debug("分词缓存已保存到 {}", wide2Ascii(cachePath));
    }
    catch (...) {
        logger->error("分词缓存 {} 保存失败", wide2Ascii(cachePath));
    }
}

void extractFileFromZip(const fs::path& zipPath, const fs::path& outputDir, const std::string& fileName) {
    bit7z::Bit7zLibrary library{ "7z.dll" };
    bit7z::BitFileExtractor extractor{ library, bit7z::BitFormat::Auto };
    extractor.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    extractor.extractMatching(wide2Ascii(zipPath), fileName, wide2Ascii(outputDir));
}

void extractZip(const fs::path& zipPath, const fs::path& outputDir) {
    bit7z::Bit7zLibrary library{ "7z.dll" };
    bit7z::BitFileExtractor extractor{ library, bit7z::BitFormat::Auto };
    extractor.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    extractor.extract(wide2Ascii(zipPath), wide2Ascii(outputDir));
}

void extractZipExclude(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& excludePrefixes) {
    bit7z::Bit7zLibrary library{ "7z.dll" };
    std::vector<uint32_t> indices;

    bit7z::BitArchiveReader archive{ library, wide2Ascii(zipPath) };
    for (const auto& item : archive) {
        if (
            std::ranges::any_of(excludePrefixes, [&](const std::string& prefix) { return item.path().starts_with(prefix); })
            )
        {
            continue;
        }
        indices.push_back(item.index());
    }

    bit7z::BitFileExtractor extractor{ library, bit7z::BitFormat::Auto };
    extractor.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    extractor.extractItems(wide2Ascii(zipPath), indices, wide2Ascii(outputDir));
}

uint64_t calculateFileCRC64(const fs::path& filePath) {
    std::ifstream ifs(filePath, std::ios::binary);
    boost::crc_optimal<64, 0x42F0E1EBA9EA3693, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, true, true> crc;
    constexpr size_t BUFFER_SIZE = 1024 * 1024;
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    while (ifs) {
        ifs.read((char*)buffer.data(), BUFFER_SIZE);
        auto readCount = ifs.gcount();
        if (readCount > 0) {
            crc.process_bytes(buffer.data(), (size_t)readCount);
        }
    }
    return crc.checksum();
}

bool cmpVer(const std::string& latestVer, const std::string& currentVer, bool& isCompatible)
{
    bool isCurrentVerPre = false;
    auto removePostfix = [&](std::string v) -> std::string
        {
            while (true) {
                if (
                    !v.empty() &&
                    (v.back() < '0' || v.back() > '9')
                    ) {
                    v.pop_back();
                    isCurrentVerPre = true;
                }
                else {
                    break;
                }
            }
            return v;
        };

    std::string fixedCurrentVer = removePostfix(currentVer);
    std::string v1s = latestVer.find_last_of("v") == std::string::npos ? latestVer : latestVer.substr(latestVer.find_last_of("v") + 1);
    std::string v2s = fixedCurrentVer.find_last_of("v") == std::string::npos ? fixedCurrentVer : fixedCurrentVer.substr(fixedCurrentVer.find_last_of("v") + 1);

    std::vector<std::string> latestVerParts = splitString(v1s, '.');
    std::vector<std::string> currentVerParts = splitString(v2s, '.');

    size_t len = std::max(latestVerParts.size(), currentVerParts.size());

    for (size_t i = 0; i < len; i++) {
        int latestVerPart = i < latestVerParts.size() ? std::stoi(latestVerParts[i]) : 0;
        int currentVerPart = i < currentVerParts.size() ? std::stoi(currentVerParts[i]) : 0;
        if (i == 0) {
            isCompatible = latestVerPart <= currentVerPart;
        }

        if (latestVerPart > currentVerPart) {
            return true;
        }
        else if (latestVerPart < currentVerPart) {
            return false;
        }
    }

    if (isCurrentVerPre) {
        return true;
    }

    return false;
}


template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const std::string& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const int& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const double& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const bool& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const toml::array& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const toml::table& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const toml::value& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const toml::ordered_array& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const toml::ordered_table& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const toml::ordered_value& value);
