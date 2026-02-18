module;

#include <toml.hpp>
#include <spdlog/spdlog.h>
#include <sol/sol.hpp>

export module LuaManager;

import Tool;

namespace fs = std::filesystem;

export {

	struct LuaStateInstance {
		std::unique_ptr<sol::state> lua;
		std::map<std::string, std::unique_ptr<sol::function>> functions;
		std::mutex executionMutex; // 用于保护这个特定 lua 实例的执行
		LuaStateInstance() : lua(std::make_unique<sol::state>()) {}
	};

	class LuaManager {

	public:
		//
		std::optional<std::shared_ptr<LuaStateInstance>> registerFunction
		(const std::string& scriptPath, const std::string& functionName, bool& needReboot);

		LuaManager(std::shared_ptr<spdlog::logger> logger) : m_logger(logger) {}

	private:

		std::map<fs::path, std::shared_ptr<LuaStateInstance>> m_scriptStates;

		std::shared_ptr<spdlog::logger> m_logger;

		void registerCustomTypes(std::shared_ptr<LuaStateInstance> luaStateInstance, const std::string& scriptPath, bool& needReboot);
	};
}

