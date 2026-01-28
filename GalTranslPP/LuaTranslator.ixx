module;

#include "GPPMacros.hpp"
#include <spdlog/spdlog.h>
#include <sol/sol.hpp>
#include <ctpl_stl.h>

export module LuaTranslator;

import Tool;
import NormalJsonTranslator;
import EpubTranslator;
import PDFTranslator;
import LuaManager;

namespace fs = std::filesystem;

export {

	template<typename BaseTranslator>
	class LuaTranslator : public BaseTranslator {
	private:
		std::shared_ptr<LuaStateInstance> m_luaState;
		sol::function m_luaRunFunc;
		std::string m_scriptPath;
		std::string m_translatorName;
		bool m_needReboot = false;

	public:
		virtual void run() override
		{
			this->m_logger->info("开始运行 LuaTranslator...");
			try {
				m_luaRunFunc();
			}
			catch (const sol::error& e) {
				throw std::runtime_error(std::format("LuaTranslator 运行时异常: {}", e.what()));
			}
		}

		template <typename... Args>
		LuaTranslator(const std::string& scriptPath, Args&&... args) :
			BaseTranslator(std::forward<Args>(args)...), m_scriptPath(scriptPath)
		{
			m_translatorName = wide2Ascii(fs::path(ascii2Wide(m_scriptPath)).filename());
			// m_inputDir = L"cache" / projectDir.filename() / (ascii2Wide(m_translatorName) + L"_json_input");
			// m_outputDir = L"cache" / projectDir.filename() / (ascii2Wide(m_translatorName) + L"_json_output");
			std::optional<std::shared_ptr<LuaStateInstance>> luaStateOpt = this->m_luaManager.registerFunction(m_scriptPath, "init", m_needReboot);
			if (!luaStateOpt.has_value()) {
				throw std::runtime_error("LuaTranslator 获取 init 函数失败。");
			}
			luaStateOpt = this->m_luaManager.registerFunction(m_scriptPath, "run", m_needReboot);
			if (!luaStateOpt.has_value()) {
				throw std::runtime_error("LuaTranslator 获取 run 函数失败。");
			}
			m_luaState = luaStateOpt.value();
			m_luaRunFunc = m_luaState->functions["run"];
			this->m_luaManager.registerFunction(m_scriptPath, "unload", m_needReboot);

			sol::state& luaState = *(m_luaState->lua);

			luaState.new_usertype<LuaTranslator<BaseTranslator>>("LuaTranslator",
				sol::base_classes, sol::bases<ITranslator,
				std::conditional_t<std::is_base_of_v<NormalJsonTranslator, BaseTranslator>, NormalJsonTranslator, void>,
				std::conditional_t<std::is_base_of_v<EpubTranslator, BaseTranslator>, EpubTranslator, void>,
				std::conditional_t<std::is_base_of_v<PDFTranslator, BaseTranslator>, PDFTranslator, void>
				>()
			);
			luaState["luaTranslator"] = this;

			sol::function initFunc = m_luaState->functions["init"];
			try {
				initFunc();
			}
			catch (const sol::error& e) {
				throw std::runtime_error(std::format("初始化 LuaTranslator 失败：{}", e.what()));
			}

			if (m_needReboot) {
				throw std::runtime_error("LuaTranslator 需要重启程序");
			}
		}

		virtual ~LuaTranslator() override
		{
			if (auto unloadFunc = m_luaState->functions["unload"]; unloadFunc.valid()) {
				unloadFunc();
			}
			this->m_logger->info("所有任务已完成！LuaTranslator {} 结束。", m_translatorName);
		}
	};
}
