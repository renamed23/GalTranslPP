module;

#include <spdlog/spdlog.h>
#include <sol/sol.hpp>

export module LuaTextPlugin;

import LuaManager;
export import IPlugin;

namespace fs = std::filesystem;

export {

	class LuaTextPlugin {
	private:
		std::shared_ptr<LuaStateInstance> m_luaState;
		sol::function m_luaRunFunc;
		std::string m_scriptPath;
		std::shared_ptr<spdlog::logger> m_logger;
		bool m_needReboot = false;

	public:
		LuaTextPlugin(const fs::path& projectDir, const std::string& scriptPath, LuaManager& luaManager, std::shared_ptr<spdlog::logger> logger);
		bool needReboot() const { return m_needReboot; }
		void run(Sentence* se);
		~LuaTextPlugin();
	};
}


module :private;


LuaTextPlugin::LuaTextPlugin(const fs::path& projectDir, const std::string& scriptPath, LuaManager& luaManager, std::shared_ptr<spdlog::logger> logger)
	: m_logger(logger), m_scriptPath(scriptPath)
{
	m_logger->info("正在初始化 Lua 插件 {}", m_scriptPath);
	std::optional<std::shared_ptr<LuaStateInstance>> luaStateOpt = luaManager.registerFunction(m_scriptPath, "init", m_needReboot);
	if (!luaStateOpt.has_value()) {
		throw std::runtime_error(std::format("{} init函数初始化失败", m_scriptPath));
	}
	luaStateOpt = luaManager.registerFunction(m_scriptPath, "run", m_needReboot);
	if (!luaStateOpt.has_value()) {
		throw std::runtime_error(std::format("{} run函数初始化失败", m_scriptPath));
	}
	m_luaState = luaStateOpt.value();
	m_luaRunFunc = m_luaState->functions["run"];
	luaManager.registerFunction(m_scriptPath, "unload", m_needReboot);

	try {
		m_luaState->functions["init"](projectDir);
	}
	catch (const sol::error& e) {
		m_logger->error("{} init函数执行失败", m_scriptPath);
		throw std::runtime_error(e.what());
	}

	m_logger->info("{} 初始化成功", m_scriptPath);
}

LuaTextPlugin::~LuaTextPlugin()
{
	sol::function unloadFunc = m_luaState->functions["unload"];
	if (unloadFunc.valid()) {
		try {
			std::lock_guard<std::mutex> lock(m_luaState->executionMutex);
			unloadFunc();
		}
		catch (const sol::error& e) {
			m_logger->error("{} unload函数执行失败", m_scriptPath);
		}
	}
}

void LuaTextPlugin::run(Sentence* se) {
	std::lock_guard<std::mutex> lock(m_luaState->executionMutex);
	try {
		m_luaRunFunc(se);
	}
	catch (const sol::error& e) {
		m_logger->error("{} run函数执行失败", m_scriptPath);
		throw std::runtime_error(e.what());
	}
}
