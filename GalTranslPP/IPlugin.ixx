module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"
#include <spdlog/spdlog.h>
#include <sol/sol.hpp>
#include <toml.hpp>
#include <proxy/proxy_macros.h>

export module IPlugin;

export import std;
export import GPPDefines;
export import LuaManager;
export import PythonManager;
export import proxy.v4;

namespace fs = std::filesystem;

extern "C++" {
	PRO_DEF_MEM_DISPATCH(MemPreRun, preRun);
	PRO_DEF_MEM_DISPATCH(MemRun, run);
	PRO_DEF_MEM_DISPATCH(MemPostRun, postRun);
	PRO_DEF_MEM_DISPATCH(MemNeedReboot, needReboot);
}

export {

	template <>
	struct pro::weak_dispatch<MemNeedReboot> : MemNeedReboot {
		using MemNeedReboot::operator();
		template <class... Args>
		bool operator()(Args&&...) const
			requires(!std::is_invocable_v<MemNeedReboot, Args...>)
		{
			return false;
		}
	};

	template <>
	struct pro::weak_dispatch<MemPreRun> : MemPreRun {
		using MemPreRun::operator();
		template <class... Args>
		void operator()(Args&&...) const
			requires(!std::is_invocable_v<MemPreRun, Args...>)
		{ }
	};

	template <>
	struct pro::weak_dispatch<MemRun> : MemRun {
		using MemRun::operator();
		template <class... Args>
		void operator()(Args&&...) const
			requires(!std::is_invocable_v<MemRun, Args...>)
		{ }
	};

	template <>
	struct pro::weak_dispatch<MemPostRun> : MemPostRun {
		using MemPostRun::operator();
		template <class... Args>
		void operator()(Args&&...) const
			requires(!std::is_invocable_v<MemPostRun, Args...>)
		{ }
	};

	struct PPrePlugin : pro::facade_builder
		::add_convention<pro::weak_dispatch<MemPreRun>, void(Sentence*)>
		::add_convention<pro::weak_dispatch<MemRun>, void(Sentence*)>
		::add_convention<pro::weak_dispatch<MemNeedReboot>, bool() const> // 弱 proxy 约束的实现不是必须的，如果没有实现则默认返回 false
		::build { };

	struct PPostPlugin : pro::facade_builder
		::add_convention<pro::weak_dispatch<MemRun>, void(Sentence*)>
		::add_convention<pro::weak_dispatch<MemPostRun>, void(Sentence*)>
		::add_convention<pro::weak_dispatch<MemNeedReboot>, bool() const> 
		::build { };

	std::vector<pro::proxy<PPrePlugin>> registerPrePlugins(const std::vector<std::string>& pluginNames, const fs::path& projectDir, const fs::path& otherCacheDir,
		PythonManager& pythonManager, LuaManager& luaManager, std::shared_ptr<spdlog::logger> logger,
		const toml::value& projectConfig);

	std::vector<pro::proxy<PPostPlugin>> registerPostPlugins(const std::vector<std::string>& pluginNames, const fs::path& projectDir, const fs::path& otherCacheDir,
		PythonManager& pythonManager, LuaManager& luaManager, std::shared_ptr<spdlog::logger> logger,
		const toml::value& projectConfig);
}