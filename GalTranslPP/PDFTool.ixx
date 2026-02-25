module;

#include "GPPMacros.hpp"
#include <spdlog/spdlog.h>

export module PDFTool;

import std;

namespace fs = std::filesystem;

export {

    std::tuple<bool, std::string> extractPDF(const fs::path& pdfPath, const fs::path& jsonPath, bool showProgress = false);

    std::tuple<bool, std::string> rejectPDF(const fs::path& orgPDFPath, const fs::path& translatedJsonPath, const fs::path& outputPDFPath,
        bool noMono = false, bool noDual = false, bool showProgress = false);

    void checkPDFDependency(const std::shared_ptr<spdlog::logger>& logger);
}
