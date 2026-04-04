module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"

module PDFTool;

import Tool;
import PythonManager;

namespace fs = std::filesystem;
namespace py = pybind11;

std::tuple<bool, std::string> extractPDF(const fs::path& pdfPath, const fs::path& jsonPath, bool showProgress)
{
    bool success;
    std::string message;
    auto extractTaskFunc = [&]()
        {
            std::tie(success, message) = py::module_::import("PDFConverter").attr("api_extract")
                (wide2Ascii(pdfPath), wide2Ascii(jsonPath), showProgress).cast<std::tuple<bool, std::string>>();
        };
    PythonMainInterpreterManager::getInstance().submitTask(std::move(extractTaskFunc)).get();
    return std::make_tuple(success, message);
}


std::tuple<bool, std::string> rejectPDF(const fs::path& orgPDFPath, const fs::path& translatedJsonPath, const fs::path& outputPDFPath,
    bool noMono, bool noDual, bool showProgress)
{
    bool success;
    std::string message;
    auto rejectTaskFunc = [&]()
        {
            // 其实传 wstring 也可以，不过还是统一成传 U8 吧
            std::tie(success, message) = py::module_::import("PDFConverter").attr("api_apply")
                (wide2Ascii(orgPDFPath), wide2Ascii(translatedJsonPath), wide2Ascii(outputPDFPath),
                    noMono, noDual, showProgress).cast<std::tuple<bool, std::string>>();
        };
    PythonMainInterpreterManager::getInstance().submitTask(std::move(rejectTaskFunc)).get();
    return std::make_tuple(success, message);
}

void checkPDFDependency(const std::shared_ptr<spdlog::logger>& logger) {
    checkPythonDependencies({ "babeldoc" }, logger);
}
