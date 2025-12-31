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
		sol::function m_luaPreRunFunc;
		sol::function m_luaPostRunFunc;
		sol::function m_luaUnloadFunc;
		std::string m_scriptPath;
		std::shared_ptr<spdlog::logger> m_logger;
		bool m_needReboot = false;

	public:
		LuaTextPlugin(const fs::path& projectDir, const std::string& scriptPath, LuaManager& luaManager, std::shared_ptr<spdlog::logger> logger);
		bool needReboot() const { return m_needReboot; }
		void run(Sentence* se);
		void preRun(Sentence* se);
		void postRun(Sentence* se);
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
	m_luaState = luaStateOpt.value();

	auto registerFunctionFunc = [&](const std::string& funcName, sol::function& func)
		{
			luaStateOpt = luaManager.registerFunction(m_scriptPath, funcName, m_needReboot);
			if (luaStateOpt.has_value()) {
				func = m_luaState->functions[funcName];
				m_logger->info("{} {}函数注册成功", m_scriptPath, funcName);
			}
		};
	registerFunctionFunc("run", m_luaRunFunc);
	registerFunctionFunc("preRun", m_luaPreRunFunc);
	registerFunctionFunc("postRun", m_luaPostRunFunc);
	registerFunctionFunc("unload", m_luaUnloadFunc);

	try {
		m_luaState->functions["init"](projectDir);
	}
	catch (const sol::error& e) {
		m_logger->error("{} init函数执行失败", m_scriptPath);
		throw std::runtime_error(e.what());
	}

	m_logger->info("{} 初始化成功", m_scriptPath);
}

LuaTextPlugin::~LuaTextPlugin() {
	if (!m_luaUnloadFunc.valid()) {
		return;
	}
	try {
		std::lock_guard<std::mutex> lock(m_luaState->executionMutex);
		m_luaUnloadFunc();
	}
	catch (const sol::error&) {
		m_logger->error("{} unload函数执行失败", m_scriptPath);
	}
}

void LuaTextPlugin::run(Sentence* se) {
	if (!m_luaRunFunc.valid()) {
		return;
	}
	std::lock_guard<std::mutex> lock(m_luaState->executionMutex);
	try {
		m_luaRunFunc(se);
	}
	catch (const sol::error& e) {
		m_logger->error("{} run函数执行失败", m_scriptPath);
		throw std::runtime_error(e.what());
	}
}

void LuaTextPlugin::preRun(Sentence* se) {
	if (!m_luaPreRunFunc.valid()) {
		return;
	}
	std::lock_guard<std::mutex> lock(m_luaState->executionMutex);
	try {
		m_luaPreRunFunc(se);
	}
	catch (const sol::error& e) {
		m_logger->error("{} preRun函数执行失败", m_scriptPath);
		throw std::runtime_error(e.what());
	}
}

void LuaTextPlugin::postRun(Sentence* se) {
	if (!m_luaPostRunFunc.valid()) {
		return;
	}
	std::lock_guard<std::mutex> lock(m_luaState->executionMutex);
	try {
		m_luaPostRunFunc(se);
	}
	catch (const sol::error& e) {
		m_logger->error("{} postRun函数执行失败", m_scriptPath);
		throw std::runtime_error(e.what());
	}
}
