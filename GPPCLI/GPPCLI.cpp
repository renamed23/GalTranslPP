#define PYBIND11_HEADERS
#include "../GalTranslPP/GPPMacros.hpp"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <spdlog/spdlog.h>
#include <toml.hpp>

import Tool;
import PythonManager;
import TerminalController;

namespace fs = std::filesystem;
namespace py = pybind11;

#pragma comment(lib, "GalTranslPP.lib")

int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::unique_ptr<py::gil_scoped_release> release;

    try {
        const auto globalConfig = toml::uparse(globalConfigPath);
        const std::string pyEnvPathStr = toml::find_or(globalConfig, "pyEnvPath", "BaseConfig/python-3.12.10-embed-amd64");

        const fs::path pyEnvPath = ascii2Wide(pyEnvPathStr);
        if (!startUpPythonEnv(pyEnvPath, release)) {
            spdlog::info("未设置 Python 环境，将无法使用需要 Python 环境的模块。");
        }
    }
    catch (...) {
        spdlog::critical("无法读取全局配置，请检查 BaseConfig/globalConfig.toml 是否存在。");
    }

    fs::path projectPath;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--project-path") {
            if (i + 1 >= argc) {
                spdlog::critical("参数 '--project-path' 缺少路径值。用法: GPPCLI --project-path <项目目录或config.toml路径>");
                shutDownPythonEnv(release);
                return 1;
            }
            projectPath = ascii2Wide(argv[++i]);
        }
        else if (arg.starts_with("--project-path=")) {
            projectPath = ascii2Wide(arg.substr(std::string("--project-path=").size()));
        }
        else {
            spdlog::critical("未知参数: {}。用法: GPPCLI --project-path <项目目录或config.toml路径>", arg);
            shutDownPythonEnv(release);
            return 1;
        }
    }

    if (projectPath.empty()) {
        spdlog::critical("请通过 '--project-path' 指定项目路径。用法: GPPCLI --project-path <项目目录或config.toml路径>");
        shutDownPythonEnv(release);
        return 1;
    }

    if (fs::is_regular_file(projectPath) && projectPath.has_parent_path()) {
        projectPath = projectPath.parent_path();
    }

    if (!fs::exists(projectPath) || !fs::is_directory(projectPath)) {
        spdlog::critical("路径 '{}' 不存在或不是有效的项目目录。", wide2Ascii(projectPath.wstring()));
        shutDownPythonEnv(release);
        return 1;
    }

    try {
        spdlog::info("开始处理项目: {}", wide2Ascii(projectPath.wstring()));

        {
            std::unique_ptr<ITranslator> translator = createTranslator(projectPath, std::make_shared<TerminalController>());
            if (!translator) {
                spdlog::error("创建翻译器实例失败，请检查项目配置。");
                shutDownPythonEnv(release);
                return 1;
            }

            translator->run();
        }

        std::cout << std::endl;
        spdlog::info("项目 '{}' 处理完成！", wide2Ascii(projectPath.wstring()));
    }
    catch (const std::invalid_argument& e) {
        spdlog::critical("[参数错误] {}", e.what());
    }
    catch (const std::runtime_error& e) {
        spdlog::critical("[运行时错误] {}", e.what());
    }
    catch (const std::exception& e) {
        spdlog::critical("[标准库错误] {}", e.what());
    }
    catch (...) {
        spdlog::critical("发生未知错误。");
    }

    shutDownPythonEnv(release);

    try {
        fs::remove(L"cache");
    }
    catch (...) { }

    spdlog::info("程序退出。");
    return 0;
}
