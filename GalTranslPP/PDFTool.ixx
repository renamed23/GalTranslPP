module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"
#include <spdlog/spdlog.h>

export module PDFTool;

import Tool;
import PythonManager;

namespace fs = std::filesystem;
namespace py = pybind11;

export {

    std::tuple<bool, std::string> extractPDF(const fs::path& pdfPath, const fs::path& jsonPath, bool showProgress = false);

    std::tuple<bool, std::string> rejectPDF(const fs::path& orgPDFPath, const fs::path& translatedJsonPath, const fs::path& outputPDFPath,
        bool noMono = false, bool noDual = false, bool showProgress = false);

    void checkPDFDependency(std::shared_ptr<spdlog::logger>& logger);
}



module :private;


std::tuple<bool, std::string> extractPDF(const fs::path& pdfPath, const fs::path& jsonPath, bool showProgress)
{
    bool success = false;
    std::string message;
    auto extractTaskFunc = [&]()
        {
            std::tie(success, message) = py::module_::import("PDFConverter").attr("api_extract")
                (pdfPath.wstring(), jsonPath.wstring(), showProgress).cast<std::tuple<bool, std::string>>();
        };
    PythonMainInterpreterManager::getInstance().submitTask(std::move(extractTaskFunc)).get();
    return std::make_tuple(success, message);
}


std::tuple<bool, std::string> rejectPDF(const fs::path& orgPDFPath, const fs::path& translatedJsonPath, const fs::path& outputPDFPath,
    bool noMono, bool noDual, bool showProgress)
{
    bool success = false;
    std::string message;
    auto rejectTaskFunc = [&]()
        {
            std::tie(success, message) = py::module_::import("PDFConverter").attr("api_apply")
                (orgPDFPath.wstring(), translatedJsonPath.wstring(), outputPDFPath.wstring(), noMono, noDual, showProgress).cast<std::tuple<bool, std::string>>();
        };
    PythonMainInterpreterManager::getInstance().submitTask(std::move(rejectTaskFunc)).get();
    return std::make_tuple(success, message);
}

void checkPDFDependency(std::shared_ptr<spdlog::logger>& logger) {
    checkPythonDependencies({ "babeldoc" }, logger);
}