module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"
#include <spdlog/spdlog.h>
#include <toml.hpp>

export module PDFTranslator;

import Tool;
import PDFTool;
import NormalJsonTranslator;

namespace fs = std::filesystem;
using json = nlohmann::json;

export {

    class PDFTranslator : public NormalJsonTranslator {

        friend void pybind11_init_gpp_plugin_api(::pybind11::module_& m);
        friend class LuaManager;

    protected:
        fs::path m_pdfInputDir;
        fs::path m_pdfOutputDir;

        // PDF 处理相关的配置
        bool m_bilingualOutput;

        // 存储json文件相对路径到其所属PDF完整路径的映射
        std::map<fs::path, fs::path> m_jsonToPDFPathMap;

    public:

        void init();
        void beforeRun();

        virtual void run() override;

        PDFTranslator(const fs::path& projectDir, std::shared_ptr<IController> controller, std::shared_ptr<spdlog::logger> logger);

        virtual ~PDFTranslator() override
        {
            m_logger->info("所有任务已完成！PDFTranslator结束。");
        }
    };
}



module :private;

PDFTranslator::PDFTranslator(const fs::path& projectDir, std::shared_ptr<IController> controller, std::shared_ptr<spdlog::logger> logger) :
    NormalJsonTranslator(projectDir, controller, logger,
        // m_inputDir                                                m_inputCacheDir
        // m_outputDir                                               m_outputCacheDir
        L"cache" / projectDir.filename() / L"pdf_json_input", L"cache" / projectDir.filename() / L"gt_input_cache",
        L"cache" / projectDir.filename() / L"pdf_json_output", L"cache" / projectDir.filename() / L"gt_output_cache")
{
    m_logger->info("GalTransl++ PDFTranslator 启动...");
}

void PDFTranslator::init()
{
    NormalJsonTranslator::init();
    m_pdfInputDir = m_projectDir / L"gt_input";
    m_pdfOutputDir = m_projectDir / L"gt_output";

    try {
        const auto projectConfig = toml::uparse(m_projectDir / L"config.toml");
        const auto pluginConfig = toml::uparse(filePluginConfigPath / L"PDF.toml");

        m_bilingualOutput = parseToml<bool>(projectConfig, pluginConfig, "plugins.PDF.输出双语翻译文件");

        checkPDFDependency(m_logger);
    }
    catch (const toml::exception& e) {
        m_logger->critical("PDF 配置文件解析失败");
        throw std::runtime_error(e.what());
    }
}


void PDFTranslator::beforeRun()
{
    for (const auto& dir : { m_pdfInputDir, m_pdfOutputDir }) {
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
            m_logger->info("已创建目录: {}", wide2Ascii(dir));
        }
    }

    for (const auto& dir : { m_inputDir, m_outputDir }) {
        fs::remove_all(dir);
        fs::create_directories(dir);
    }

    std::vector<fs::path> pdfFilePaths;
    for (const auto& entry : fs::recursive_directory_iterator(m_pdfInputDir)) {
        if (entry.is_regular_file() && isSameExtension(entry.path(), L".pdf")) {
            pdfFilePaths.push_back(entry.path());
        }
    }
    if (pdfFilePaths.empty()) {
        throw std::runtime_error("未找到 PDF 文件");
    }
    
    for (const auto& pdfFilePath : pdfFilePaths) {
        if (m_controller->shouldStop()) {
            return;
        }
        fs::path relPDFPath = fs::relative(pdfFilePath, m_pdfInputDir);
        fs::path relJsonPath = fs::path(relPDFPath).replace_extension(".json");

        m_jsonToPDFPathMap[relJsonPath] = pdfFilePath;
        fs::path inputJsonFile = m_inputDir / relJsonPath;
        createParent(inputJsonFile);

        bool success = false;
        std::string message;
        m_logger->info("正在提取文件: {}", wide2Ascii(relPDFPath));
        std::tie(success, message) = extractPDF(pdfFilePath, inputJsonFile);
        if (success) {
            m_logger->info("成功提取元数据: {}", message);
        }
        else {
            throw std::runtime_error(std::format("提取元数据失败: {}", message));
        }
    }

    m_onFileProcessed = [this](fs::path relProcessedFile)
        {
            if (!m_jsonToPDFPathMap.contains(relProcessedFile)) {
                m_logger->warn("未找到与 {} 对应的元数据，跳过", wide2Ascii(relProcessedFile));
                return;
            }
            fs::path origPDFPath = m_jsonToPDFPathMap[relProcessedFile];
            fs::path relPDFPath = fs::relative(origPDFPath, m_pdfInputDir);
            fs::path outputPDFFile = m_pdfOutputDir / relPDFPath;
            createParent(outputPDFFile);

            bool success = false;
            std::string message;
            m_logger->info("正在回注文件: {}", wide2Ascii(relPDFPath));
            std::tie(success, message) = rejectPDF(origPDFPath, m_outputDir / relProcessedFile, outputPDFFile.parent_path(), false, !m_bilingualOutput);
            if (success) {
                m_logger->info("成功翻译文件: {}", message);
            }
            else {
                throw std::runtime_error(std::format("翻译文件失败: {}", message));
            }
        };

}

void PDFTranslator::run() {
    NormalJsonTranslator::init();
    PDFTranslator::init();
    PDFTranslator::beforeRun();
    std::optional<std::vector<fs::path>> relFilePathsOpt = NormalJsonTranslator::beforeRun();
    if (!relFilePathsOpt.has_value()) {
        return;
    }
    NormalJsonTranslator::process(std::move(relFilePathsOpt.value()));
    NormalJsonTranslator::afterRun();
}