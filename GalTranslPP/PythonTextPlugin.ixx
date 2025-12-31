module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"
#include <toml.hpp>
#include <spdlog/spdlog.h>

export module PythonTextPlugin;

import Tool;
import PythonManager;
export import IPlugin;

namespace fs = std::filesystem;
namespace py = pybind11;

export {

    class PythonTextPlugin {
    private:
        std::shared_ptr<PythonInterpreterInstance> m_pythonInterpreter;
        py::object* m_pythonRunFunc;
        std::string m_modulePath;
        std::shared_ptr<spdlog::logger> m_logger;
        bool m_needReboot = false;

    public:
        PythonTextPlugin(const fs::path& projectDir, const std::string& modulePath, PythonManager& pythonManager, std::shared_ptr<spdlog::logger> logger);
        bool needReboot() const { return m_needReboot; }
        void run(Sentence* se);
        ~PythonTextPlugin();
    };
}


module :private;

PythonTextPlugin::PythonTextPlugin(const fs::path& projectDir, const std::string& modulePath, PythonManager& pythonManager, std::shared_ptr<spdlog::logger> logger)
    : m_logger(logger), m_modulePath(modulePath)
{
    m_logger->info("正在初始化 Python 插件 {}", m_modulePath);
    std::optional<std::shared_ptr<PythonInterpreterInstance>> pythonInterpreterOpt = pythonManager.registerFunction(m_modulePath, "init", m_needReboot);
    if (!pythonInterpreterOpt.has_value()) {
        throw std::runtime_error(std::format("{} init函数初始化失败", m_modulePath));
    }
    pythonInterpreterOpt = pythonManager.registerFunction(m_modulePath, "run", m_needReboot);
    if (!pythonInterpreterOpt.has_value()) {
        throw std::runtime_error(std::format("{} run函数初始化失败", m_modulePath));
    }
    m_pythonInterpreter = pythonInterpreterOpt.value();
    m_pythonRunFunc = m_pythonInterpreter->functions["run"].get();
    pythonManager.registerFunction(m_modulePath, "unload", m_needReboot);

    m_pythonInterpreter->submitTask([&]()
        {
            try {
                (*(m_pythonInterpreter->functions["init"]))(projectDir);
            }
            catch (const py::error_already_set& e) {
                throw std::runtime_error(std::format("{} init函数执行失败: {}", m_modulePath, e.what()));
            }
        }).get();

    m_logger->info("{} 初始化成功", m_modulePath);
}

PythonTextPlugin::~PythonTextPlugin()
{
    m_pythonInterpreter->submitTask([&]()
        {
            try {
                if (auto& unloadFuncPtr = m_pythonInterpreter->functions["unload"]; unloadFuncPtr.operator bool() && py::isinstance<py::function>(*unloadFuncPtr)) {
                    (*unloadFuncPtr)();
                }
            }
            catch (const py::error_already_set& e) {
                throw std::runtime_error(std::format("{} unload函数执行失败: {}", m_modulePath, e.what()));
            }
        }).get();
}

void PythonTextPlugin::run(Sentence* se) {
    m_pythonInterpreter->submitTask([&]()
        {
            try {
                (*m_pythonRunFunc)(se);
            }
            catch (const py::error_already_set& e) {
                throw std::runtime_error(std::format("{} run函数执行失败: {}", m_modulePath, e.what()));
            }
        }).get();
}
