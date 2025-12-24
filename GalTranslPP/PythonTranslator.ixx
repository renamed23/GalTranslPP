module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"
#include <pybind11/embed.h>
#include <spdlog/spdlog.h>

export module PythonTranslator;

import Tool;
import NormalJsonTranslator;
import EpubTranslator;
import PDFTranslator;
import PythonManager;

namespace fs = std::filesystem;
namespace py = pybind11;

export {

	template<typename Base>
	class PythonTranslator : public Base {
	private:
		std::shared_ptr<PythonInterpreterInstance> m_pythonInterpreter;
		std::weak_ptr<py::object> m_pythonRunFunc;
		std::string m_modulePath;
		std::string m_translatorName;

	public:
		virtual void run() override
		{
			this->m_logger->info("开始运行 PythonTranslator...");
			m_pythonInterpreter->submitTask([&]()
				{
					try {
						if (auto runFuncLocked = m_pythonRunFunc.lock()) {
							(*runFuncLocked)();
						}
						else {
							throw std::runtime_error("PythonTranslator run 函数已释放");
						}
					}
					catch (const std::exception& e) {
						throw std::runtime_error("PythonTranslator 运行时异常: " + std::string(e.what()));
					}
				}).get();
		}

		template<typename... Args>
		PythonTranslator(const std::string& modulePath, Args&&... args) :
			Base(std::forward<Args>(args)...), m_modulePath(modulePath)
		{
			bool needRoot = false;
			this->m_pythonTranslator = true;
			m_translatorName = wide2Ascii(fs::path(ascii2Wide(m_modulePath)).stem());
			std::optional<std::shared_ptr<PythonInterpreterInstance>> pythonInterpreterOpt = this->m_pythonManager.registerFunction(m_modulePath, "init", needRoot);
			if (!pythonInterpreterOpt.has_value()) {
				throw std::runtime_error("PythonTranslator 获取 init 函数失败！");
			}
			pythonInterpreterOpt = this->m_pythonManager.registerFunction(m_modulePath, "run", needRoot);
			if (!pythonInterpreterOpt.has_value()) {
				throw std::runtime_error("PythonTranslator 获取 run 函数失败！");
			}
			m_pythonInterpreter = pythonInterpreterOpt.value();
			m_pythonRunFunc = m_pythonInterpreter->functions["run"];
			this->m_pythonManager.registerFunction(m_modulePath, "unload", needRoot);

			m_pythonInterpreter->submitTask([&]()
				{
					try {
						fs::path stdModulePath = fs::weakly_canonical(ascii2Wide(m_modulePath));
						std::string moduleName = wide2Ascii(stdModulePath.stem());
						py::module_ pythonTranslatorModule = py::module_::import(moduleName.c_str());
						pythonTranslatorModule.attr("pythonTranslator") = (Base*)this;
						(*(m_pythonInterpreter->functions["init"]))();
					}
					catch (const pybind11::error_already_set& e) {
						throw std::runtime_error("初始化 PythonTranslator 时发生错误: " + std::string(e.what()));
					}
				}).get();
			if (needRoot) {
				throw std::runtime_error("PythonTranslator 需要重启程序");
			}
			this->m_logger->info("PythonTranslator 已加载模块: {}", m_translatorName);
		}

		virtual ~PythonTranslator() override
		{
			m_pythonInterpreter->submitTask([&]()
				{
					try {
						if (auto unloadFuncPtr = m_pythonInterpreter->functions["unload"]; unloadFuncPtr.operator bool()) {
							(*unloadFuncPtr)();
						}
						this->m_onFileProcessed = {};
						this->m_onPerformApi = {};
					}
					catch (const py::error_already_set& e) {
						throw std::runtime_error("卸载 PythonTranslator 时发生错误: " + std::string(e.what()));
					}
				}).get();
			this->m_logger->info("所有任务已完成！PythonTranslator {} 结束。", m_translatorName);
		}

	};
}
