module;

#include <spdlog/spdlog.h>
#include <unicode/regex.h>
#include <unicode/unistr.h>
#include <zip.h>
#include <gumbo.h>
#include <toml.hpp>

export module EpubTranslator;

import Tool;
import NormalJsonTranslator;

namespace fs = std::filesystem;
using json = nlohmann::json;

export {

    struct EpubTextNodeInfo {
        size_t offset; // 节点在原始文件中的字节偏移量
        size_t length; // 节点内容的字节长度
    };

    struct CallbackPattern {
        std::shared_ptr<icu::RegexPattern> org;
        icu::UnicodeString rep;
    };

    struct RegexPattern {
        std::shared_ptr<icu::RegexPattern> org;
        icu::UnicodeString rep;

        bool isCallback;
        std::multimap<int, CallbackPattern> callbackPatterns;
    };

    struct JsonInfo {
        std::vector<EpubTextNodeInfo> metadata;
        // 存储json文件相对路径到原始 HTML 完整路径的映射
        fs::path htmlPath;
        // 存储json文件相对路径到其所属 epub 完整路径的映射
        fs::path epubPath;
        // 存储json文件相对路径到 normal_post 完整路径的映射
        fs::path normalPostPath;
    };

    class EpubTranslator : public NormalJsonTranslator {

    public:

        fs::path m_epubInputDir;
        fs::path m_epubOutputDir;
        fs::path m_tempUnpackDir;
        fs::path m_tempRebuildDir;

        // EPUB 处理相关的配置
        bool m_bilingualOutput;
        std::string m_originalTextColor;
        std::string m_originalTextScale;

        // 预处理正则和后处理正则
        std::vector<RegexPattern> m_preRegexPatterns;
        std::vector<RegexPattern> m_postRegexPatterns;


        // 存储json文件相对路径到各种元数据的映射
        std::map<fs::path, JsonInfo> m_jsonToInfoMap;

        // 每个epub完整路径对应的多个json文件相对路径以及有没有处理完毕
        std::map<fs::path, std::map<fs::path, bool>> m_epubToJsonsMap;

    public:

        void init();
        void beforeRun();

        virtual void run() override;

        EpubTranslator(const fs::path& projectDir, std::shared_ptr<IController> controller, std::shared_ptr<spdlog::logger> logger);

        virtual ~EpubTranslator() override
        {
            m_logger->info("所有任务已完成！EpubTranslator结束。");
        }
    };
}



module :private;

// 递归遍历 Gumbo 树以提取文本节点
void extractTextNodes(GumboNode* node, std::vector<std::pair<std::string, EpubTextNodeInfo>>& sentences) {
    if (node->type == GUMBO_NODE_TEXT) {
        std::string text = node->v.text.text;
        if (text.empty() || text.find_first_not_of(" \t\n\r") == std::string::npos) {
            return;
        }
        EpubTextNodeInfo info;
        info.offset = node->v.text.start_pos.offset;
        info.length = text.length();
        sentences.push_back({ text, info });
        return;
    }

    if (node->type != GUMBO_NODE_ELEMENT || node->v.element.tag == GUMBO_TAG_SCRIPT || node->v.element.tag == GUMBO_TAG_STYLE) {
        return;
    }

    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        extractTextNodes(static_cast<GumboNode*>(children->data[i]), sentences);
    }
}

EpubTranslator::EpubTranslator(const fs::path& projectDir, std::shared_ptr<IController> controller, std::shared_ptr<spdlog::logger> logger) :
    NormalJsonTranslator(projectDir, controller, logger,
        // m_inputDir                                                m_inputCacheDir
        // m_outputDir                                               m_outputCacheDir
        L"cache" / projectDir.filename() / L"epub_json_input", L"cache" / projectDir.filename() / L"gt_input_cache",
        L"cache" / projectDir.filename() / L"epub_json_output", L"cache" / projectDir.filename() / L"gt_output_cache")
{
    m_logger->info("GalTransl++ EpubTranslator 启动...");
}

void EpubTranslator::init()
{
    m_epubInputDir = m_projectDir / L"gt_input";
    m_epubOutputDir = m_projectDir / L"gt_output";
    m_tempUnpackDir = L"cache" / m_projectDir.filename() / L"epub_unpacked";
    m_tempRebuildDir = L"cache" / m_projectDir.filename() / L"epub_rebuild";

    std::ifstream ifs;

    try {
        const auto projectConfig = toml::parse(m_projectDir / L"config.toml");
        const auto pluginConfig = toml::parse(filePluginConfigPath / L"Epub.toml");

        m_bilingualOutput = parseToml<bool>(projectConfig, pluginConfig, "plugins.Epub.双语显示");
        m_originalTextColor = parseToml<std::string>(projectConfig, pluginConfig, "plugins.Epub.原文颜色");
        m_originalTextScale = std::to_string(parseToml<double>(projectConfig, pluginConfig, "plugins.Epub.缩小比例"));

        auto readRegexArr = [](const toml::array& regexArr, std::vector<RegexPattern>& patterns)
            {
                for (const auto& regexTbl : regexArr) {
                    std::string regexOrg = regexTbl.contains("org") ? toml::find_or(regexTbl, "org", "") :
                        toml::find_or(regexTbl, "searchStr", "");
                    if (regexOrg.empty()) {
                        continue;
                    }

                    RegexPattern regexPattern;
                    icu::UnicodeString regexUStr = icu::UnicodeString::fromUTF8(regexOrg);
                    UErrorCode status = U_ZERO_ERROR;
                    regexPattern.org = std::shared_ptr<icu::RegexPattern>(icu::RegexPattern::compile(regexUStr, 0, status));
                    if (U_FAILURE(status)) {
                        throw std::runtime_error("预处理正则编译失败: " + regexOrg);
                    }
                    regexPattern.isCallback = regexTbl.contains("callback");

                    if (regexPattern.isCallback) {
                        const auto& callbackArr = toml::get<toml::array>(regexTbl.at("callback"));
                        for (const auto& callbackTbl : callbackArr) {
                            if (!callbackTbl.is_table()) {
                                continue;
                            }
                            CallbackPattern callbackPattern;
                            int group = toml::find_or(callbackTbl, "group", 0);
                            if (group == 0) {
                                continue;
                            }
                            const std::string& callbackOrg = callbackTbl.contains("org") ? toml::find_or(callbackTbl, "org", "") :
                                toml::find_or(callbackTbl, "searchStr", "");
                            if (callbackOrg.empty()) {
                                continue;
                            }
                            const std::string& callbackRep = callbackTbl.contains("rep") ? toml::find_or(callbackTbl, "rep", "") :
                                toml::find_or(callbackTbl, "replaceStr", "");
                            icu::UnicodeString callbackUStr = icu::UnicodeString::fromUTF8(callbackOrg);
                            callbackPattern.org = std::shared_ptr<icu::RegexPattern>(icu::RegexPattern::compile(callbackUStr, 0, status));
                            if (U_FAILURE(status)) {
                                throw std::runtime_error("预处理正则回调正则编译失败: " + callbackOrg);
                            }
                            callbackPattern.rep = icu::UnicodeString::fromUTF8(callbackRep);
                            regexPattern.callbackPatterns.insert(std::make_pair(group, callbackPattern));
                        }
                    }
                    else {
                        const std::string& regexRep = regexTbl.contains("rep") ? toml::find_or(regexTbl, "rep", "") :
                            toml::find_or(regexTbl, "replaceStr", "");
                        icu::UnicodeString repUStr = icu::UnicodeString::fromUTF8(regexRep);
                        regexPattern.rep = repUStr;
                    }

                    patterns.push_back(regexPattern);
                }
            };

        const auto& preRegexArr = parseToml<toml::array>(projectConfig, pluginConfig, "plugins.Epub.preprocRegex");
        readRegexArr(preRegexArr, m_preRegexPatterns);
        const auto& postRegexArr = parseToml<toml::array>(projectConfig, pluginConfig, "plugins.Epub.postprocRegex");
        readRegexArr(postRegexArr, m_postRegexPatterns);
    }
    catch (const toml::exception& e) {
        m_logger->critical("Epub 配置文件解析失败");
        throw std::runtime_error(e.what());
    }
}


void EpubTranslator::beforeRun()
{
    for (const auto& dir : { m_epubInputDir, m_epubOutputDir }) {
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
            m_logger->info("已创建目录: {}", wide2Ascii(dir));
        }
    }

    for (const auto& dir : { m_tempUnpackDir, m_tempRebuildDir, m_inputDir, m_outputDir }) {
        fs::remove_all(dir);
        fs::create_directories(dir);
    }

    std::vector<fs::path> epubFiles;
    for (const auto& entry : fs::recursive_directory_iterator(m_epubInputDir)) {
        if (entry.is_regular_file() && isSameExtension(entry.path(), L".epub")) {
            epubFiles.push_back(entry.path());
        }
    }
    if (epubFiles.empty()) {
        throw std::runtime_error("未找到 EPUB 文件");
    }

    // 正则替换
    auto regexReplace = [this](const std::vector<RegexPattern>& regexPatterns, std::string& content)
        {
            icu::UnicodeString contentUStr = icu::UnicodeString::fromUTF8(content);
            for (const auto& reg : regexPatterns) {
                UErrorCode status = U_ZERO_ERROR;
                std::unique_ptr<icu::RegexMatcher> matcher(reg.org->matcher(contentUStr, status));
                if (U_FAILURE(status)) {
                    m_logger->error("预处理正则匹配失败");
                    continue;
                }
                if (reg.isCallback) {
                    icu::UnicodeString newContentUstr;
                    while (matcher->find() && U_SUCCESS(status)) {
                        icu::UnicodeString patternUstr;
                        for (int32_t i = 1; i < matcher->groupCount() + 1; i++) {
                            icu::UnicodeString groupUstr = matcher->group(i, status);
                            auto equalRange = reg.callbackPatterns.equal_range(i);
                            for (auto it = equalRange.first; it != equalRange.second; ++it) {
                                std::unique_ptr<icu::RegexMatcher> callbackMatcher(it->second.org->matcher(groupUstr, status));
                                if (U_FAILURE(status)) {
                                    m_logger->error("预处理正则回调匹配失败");
                                    continue;
                                }
                                groupUstr = callbackMatcher->replaceAll(it->second.rep, status);
                                if (U_FAILURE(status)) {
                                    m_logger->error("预处理正则回调替换失败");
                                    continue;
                                }
                            }
                            patternUstr.append(groupUstr);
                        }
                        matcher->appendReplacement(newContentUstr, patternUstr, status);
                    }
                    matcher->appendTail(newContentUstr);
                    contentUStr = std::move(newContentUstr);
                }
                else {
                    contentUStr = matcher->replaceAll(reg.rep, status);
                    if (U_FAILURE(status)) {
                        m_logger->error("预处理正则替换失败");
                    }
                }
            }
            content.clear();
            contentUStr.toUTF8String(content);
        };


    for (const auto& epubPath : epubFiles) {
        fs::path relEpubPath = fs::relative(epubPath, m_epubInputDir); // dir1/book1.epub
        fs::path bookUnpackPath = m_tempUnpackDir / relEpubPath.parent_path() / relEpubPath.stem(); // cache/myproject/epub_unpacked/dir1/book1
        fs::path bookRebuildPath = m_tempRebuildDir / relEpubPath.parent_path() / relEpubPath.stem(); // cache/myproject/epub_rebuild/dir1/book1

        // 解压 EPUB 文件
        m_logger->debug("解压 {} 到 {}", wide2Ascii(epubPath), wide2Ascii(bookUnpackPath));
        fs::create_directories(bookUnpackPath);
        extractZip(epubPath, bookUnpackPath);

        // 从html中提取json和元数据
        createParent(bookRebuildPath);
        fs::copy(bookUnpackPath, bookRebuildPath, fs::copy_options::recursive);
        fs::path relBookDir = relEpubPath.parent_path() / relEpubPath.stem(); // dir1/book1
        for (const auto& htmlEntry : fs::recursive_directory_iterator(bookUnpackPath)) {
            if (htmlEntry.is_regular_file() && (isSameExtension(htmlEntry.path(), L".html") || isSameExtension(htmlEntry.path(), L".xhtml"))) {

                std::ifstream ifs(htmlEntry.path(), std::ios::binary);
                std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

                regexReplace(m_preRegexPatterns, content);

                GumboOutput* output = gumbo_parse(content.c_str());
                std::vector<std::pair<std::string, EpubTextNodeInfo>> sentences;
                extractTextNodes(output->root, sentences);
                gumbo_destroy_output(&kGumboDefaultOptions, output);

                if (sentences.empty()) continue;

                // 创建json相对路径
                fs::path relativePath = fs::relative(htmlEntry.path(), bookUnpackPath); // OEBPS/chapter1.html
                fs::path showNormalHtmlPath = m_projectDir / L"epub_show_normal" / relBookDir / relativePath;
                fs::path showNormalPostHtmlPath = m_projectDir / L"epub_show_normal_post" / relBookDir / relativePath;
                fs::path relJsonPath = relBookDir / relativePath.replace_extension(".json"); // dir1/book1/OEBPS/chapter1.json


                // 存储映射关系
                JsonInfo& info = m_jsonToInfoMap[relJsonPath];
                info.htmlPath = htmlEntry.path();
                info.epubPath = epubPath;
                info.normalPostPath = showNormalPostHtmlPath;
                m_epubToJsonsMap[epubPath].insert(std::make_pair(relJsonPath, false));

                // 存储元数据
                std::ranges::sort(sentences, [](const auto& a, const auto& b)
                    {
                        return a.second.offset < b.second.offset;
                    });
                std::vector<EpubTextNodeInfo> metadata;
                json j = json::array();
                for (const auto& p : sentences) {
                    // TODO: 添加可选正则区分 name 和 message
                    j.push_back({ {"message", p.first} });
                    metadata.push_back(p.second);
                }
                info.metadata = std::move(metadata);

                createParent(m_inputDir / relJsonPath); // cache/myproject/epub_json_input/dir1/book1/OEBPS/chapter1.json
                std::ofstream ofs;
                ofs.open(m_inputDir / relJsonPath);
                ofs << j.dump(2);
                ofs.close();

                createParent(showNormalHtmlPath);
                ofs.open(showNormalHtmlPath, std::ios::binary);
                ofs << content;
                ofs.close();
            }
        }
    }

    m_onFileProcessed = [this, regexReplace](fs::path relProcessedFile)
        {
            if (!m_jsonToInfoMap.contains(relProcessedFile)) {
                m_logger->warn("未找到与 {} 对应的元数据，跳过", wide2Ascii(relProcessedFile));
                return;
            }
            const JsonInfo& info = m_jsonToInfoMap[relProcessedFile];
            const fs::path& epubPath = info.epubPath;
            std::map<fs::path, bool>& jsonsMap = m_epubToJsonsMap[epubPath];
            jsonsMap[relProcessedFile] = true;
            if (
                std::ranges::any_of(jsonsMap, [](const auto& p)
                    {
                        return !p.second;
                    })
                ) 
            {
                return;
            }

            // 这本epub的所有文件都翻译完毕，可以开始重组
            for (const auto& relJsonPath : jsonsMap | std::views::keys) {

                const JsonInfo& jsonInfo = m_jsonToInfoMap[relJsonPath];
                const fs::path& originalHtmlPath = jsonInfo.htmlPath;
                const fs::path rebuiltHtmlPath = m_tempRebuildDir / fs::relative(originalHtmlPath, m_tempUnpackDir);
                const auto& metadata = jsonInfo.metadata;

                // 替换 HTML 内容的逻辑
                std::ifstream ifs(rebuiltHtmlPath, std::ios::binary);
                std::string originalContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                ifs.close();

                // 预处理正则替换
                regexReplace(m_preRegexPatterns, originalContent);

                ifs.open(m_outputDir / relJsonPath);
                json translatedData = json::parse(ifs);
                ifs.close();

                if (metadata.size() != translatedData.size()) {
                    throw std::runtime_error(std::format("元数据和翻译数据数量不匹配，无法重组: {}meta / {}trans,文件: [{}]", metadata.size(), translatedData.size(), wide2Ascii(rebuiltHtmlPath)));
                }

                std::string newContent;
                newContent.reserve(originalContent.length() * 2);
                size_t lastPos = 0;

                for (size_t i = 0; i < metadata.size(); ++i) {
                    std::string translatedText = translatedData[i].value("message", "");
                    std::string replacement = m_bilingualOutput ?
                        (translatedText + "<br/><span style=\"color:" + m_originalTextColor + "; font-size:" + m_originalTextScale +
                            "em;\">" + originalContent.substr(metadata[i].offset, metadata[i].length) + "</span>")
                        : translatedText;
                    newContent.append(originalContent.substr(lastPos, metadata[i].offset - lastPos));
                    newContent.append(replacement);
                    lastPos = metadata[i].offset + metadata[i].length;
                }
                if (lastPos < originalContent.length()) {
                    newContent.append(originalContent.substr(lastPos));
                }

                // 后处理正则替换
                regexReplace(m_postRegexPatterns, newContent);

                std::ofstream ofs;
                ofs.open(rebuiltHtmlPath, std::ios::binary);
                ofs << newContent;
                ofs.close();

                const fs::path& showNormalPostHtmlPath = jsonInfo.normalPostPath;
                createParent(showNormalPostHtmlPath);
                ofs.open(showNormalPostHtmlPath, std::ios::binary);
                ofs << newContent;
                ofs.close();
            }

            fs::path relEpubPath = fs::relative(epubPath, m_epubInputDir);
            fs::path bookRebuildPath = m_tempRebuildDir / relEpubPath.parent_path() / relEpubPath.stem();

            fs::path outputEpubPath = m_epubOutputDir / relEpubPath;
            createParent(outputEpubPath);
            m_logger->debug("正在打包 {}", wide2Ascii(outputEpubPath));

            int error = 0;
            zip* za = zip_open(wide2Ascii(outputEpubPath).c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
            if (!za) {
                throw std::runtime_error("无法创建 EPUB (zip) 文件: " + std::to_string(error));
            }

            // --- 步骤一：优先处理 mimetype 文件，且不压缩 ---
            fs::path mimetypePath = bookRebuildPath / "mimetype";
            if (fs::exists(mimetypePath)) {
                zip_source_t* s = zip_source_file(za, wide2Ascii(mimetypePath).c_str(), 0, 0);
                if (!s) {
                    zip_close(za);
                    throw std::runtime_error("无法为 mimetype 创建 zip_source_file");
                }

                zip_int64_t idx = zip_file_add(za, "mimetype", s, ZIP_FL_ENC_UTF_8);
                if (idx < 0) {
                    zip_source_free(s);
                    zip_close(za);
                    throw std::runtime_error("无法将 mimetype 添加到 zip");
                }

                if (zip_set_file_compression(za, idx, ZIP_CM_STORE, 0) < 0) {
                    zip_source_free(s);
                    zip_close(za);
                    throw std::runtime_error("无法将 mimetype 设置为不压缩模式。");
                }
            }
            else {
                m_logger->warn("在源目录 {} 中未找到 mimetype 文件，生成的 EPUB 可能无效。", wide2Ascii(bookRebuildPath));
            }

            // --- 步骤二：处理其他所有文件和目录 ---
            for (const auto& entry : fs::recursive_directory_iterator(bookRebuildPath)) {
                fs::path relativePath = fs::relative(entry.path(), bookRebuildPath);
                std::string entryName = wide2Ascii(relativePath);
                std::ranges::replace(entryName, '\\', '/');

                if (entryName == "mimetype") {
                    continue;
                }

                if (fs::is_directory(entry.path())) {
                    zip_dir_add(za, entryName.c_str(), ZIP_FL_ENC_UTF_8);
                }
                else {
                    zip_source_t* s = zip_source_file(za, wide2Ascii(entry.path()).c_str(), 0, 0);
                    if (!s) {
                        zip_close(za);
                        throw std::runtime_error(std::format("无法为文件 {} 创建 zip_source_file", entryName));
                    }
                    if (zip_file_add(za, entryName.c_str(), s, ZIP_FL_ENC_UTF_8) < 0) {
                        zip_source_free(s);
                        zip_close(za);
                        throw std::runtime_error(std::format("无法将文件 {} 添加到 zip", entryName));
                    }
                }
            }

            // 所有 source 都在 zip_close 中被 libzip 自动管理和释放
            if (zip_close(za) < 0) {
                throw std::runtime_error("关闭 zip 存档时出错: " + std::string(zip_strerror(za)));
            }

            m_logger->info("已重建 EPUB 文件: {}", wide2Ascii(outputEpubPath));
        };
}

void EpubTranslator::run() {
    NormalJsonTranslator::init();
    EpubTranslator::init();
    EpubTranslator::beforeRun();
    std::optional<std::vector<fs::path>> relFilePathsOpt = NormalJsonTranslator::beforeRun();
    if (!relFilePathsOpt.has_value()) {
        return;
    }
    NormalJsonTranslator::process(std::move(relFilePathsOpt.value()));
    NormalJsonTranslator::afterRun();
}