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
        py::object* m_pythonDPreRunFunc = nullptr;
        py::object* m_pythonPreRunFunc = nullptr;
        py::object* m_pythonPostRunFunc = nullptr;
        py::object* m_pythonDPostRunFunc = nullptr;
        py::object* m_pythonUnloadFunc = nullptr;
        std::string m_modulePath;
        std::shared_ptr<spdlog::logger> m_logger;
        bool m_needReboot = false;

    public:
        PythonTextPlugin(const fs::path& projectDir, const std::string& modulePath, PythonManager& pythonManager, const std::shared_ptr<spdlog::logger>& logger);
        bool needReboot() const { return m_needReboot; }
        void dPreRun(Sentence* se);
        void preRun(Sentence* se);
        void postRun(Sentence* se);
        void dPostRun(Sentence* se);
        ~PythonTextPlugin();
    };
}


module :private;

PythonTextPlugin::PythonTextPlugin(const fs::path& projectDir, const std::string& modulePath, PythonManager& pythonManager, const std::shared_ptr<spdlog::logger>& logger)
    : m_logger(logger), m_modulePath(modulePath)
{
    m_logger->info("正在初始化 Python 插件 {}", m_modulePath);
    std::optional<std::shared_ptr<PythonInterpreterInstance>> pythonInterpreterOpt = pythonManager.registerFunction(m_modulePath, "init", m_needReboot);
    if (!pythonInterpreterOpt.has_value()) {
        throw std::runtime_error(std::format("{} init函数初始化失败", m_modulePath));
    }
    m_pythonInterpreter = pythonInterpreterOpt.value();

    auto registerFunctionFunc = [&](const std::string& funcName, py::object*& func)
        {
            pythonInterpreterOpt = pythonManager.registerFunction(m_modulePath, funcName, m_needReboot);
            if (pythonInterpreterOpt.has_value()) {
                func = m_pythonInterpreter->functions[funcName].get();
                m_logger->info("{} {}函数注册成功", m_modulePath, funcName);
            }
        };
    registerFunctionFunc("dPreRun", m_pythonDPreRunFunc);
    registerFunctionFunc("preRun", m_pythonPreRunFunc);
    registerFunctionFunc("postRun", m_pythonPostRunFunc);
    registerFunctionFunc("dPostRun", m_pythonDPostRunFunc);
    registerFunctionFunc("unload", m_pythonUnloadFunc);

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
    if (!m_pythonUnloadFunc) {
        return;
    }
    m_pythonInterpreter->submitTask([&]()
        {
            try {
                (*m_pythonUnloadFunc)();
            }
            catch (const py::error_already_set& e) {
                m_logger->error("{} unload函数执行失败: {}", m_modulePath, e.what());
            }
        }).get();
}

void PythonTextPlugin::dPreRun(Sentence* se) {
    if (!m_pythonDPreRunFunc) {
        return;
    }
    m_pythonInterpreter->submitTask([&]()
        {
            try {
                (*m_pythonDPreRunFunc)(se);
            }
            catch (const py::error_already_set& e) {
                m_logger->error("{} dPreRun函数执行失败", m_modulePath);
                throw std::runtime_error(e.what());
            }
        }).get();
}

void PythonTextPlugin::preRun(Sentence* se) {
    if (!m_pythonPreRunFunc) {
        return;
    }
    m_pythonInterpreter->submitTask([&]()
        {
            try {
                (*m_pythonPreRunFunc)(se);
            }
            catch (const py::error_already_set& e) {
                m_logger->error("{} preRun函数执行失败", m_modulePath);
                throw std::runtime_error(e.what());
            }
        }).get();
}

void PythonTextPlugin::postRun(Sentence* se) {
    if (!m_pythonPostRunFunc) {
        return;
    }
    m_pythonInterpreter->submitTask([&]()
        {
            try {
                (*m_pythonPostRunFunc)(se);
            }
            catch (const py::error_already_set& e) {
                m_logger->error("{} postRun函数执行失败", m_modulePath);
                throw std::runtime_error(e.what());
            }
        }).get();
}

void PythonTextPlugin::dPostRun(Sentence* se) {
    if (!m_pythonDPostRunFunc) {
        return;
    }
    m_pythonInterpreter->submitTask([&]()
        {
            try {
                (*m_pythonDPostRunFunc)(se);
            }
            catch (const py::error_already_set& e) {
                m_logger->error("{} postRun函数执行失败", m_modulePath);
                throw std::runtime_error(e.what());
            }
        }).get();
}
