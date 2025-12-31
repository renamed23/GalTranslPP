module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"
#include <spdlog/spdlog.h>
#include <sol/sol.hpp>
#include <toml.hpp>

module IPlugin;

import Tool;
import TextPostFull2Half;
import TextLinebreakFix;
import SkipTrans;
import LuaTextPlugin;
import PythonTextPlugin;

namespace fs = std::filesystem;

std::vector<pro::proxy<PPrePlugin>> registerPrePlugins(const std::vector<std::string>& pluginNames, const fs::path& projectDir, const fs::path& otherCacheDir,
    PythonManager& pythonManager, LuaManager& luaManager, std::shared_ptr<spdlog::logger> logger,
    const toml::value& projectConfig) {
    std::vector<pro::proxy<PPrePlugin>> plugins;

    for (const auto& pluginName : pluginNames) {
        std::string pluginNameLower = str2Lower(pluginName);
        bool hasRun = pluginName.contains("run:");
        bool hasPreRun = pluginName.contains("preRun:");
        if (pluginNameLower.ends_with(".lua")) {
            plugins.push_back(pro::make_proxy<PPrePlugin, LuaTextPlugin>(projectDir, replaceStr(pluginName, "<PROJECT_DIR>", wide2Ascii(projectDir)),
                luaManager, logger));
        }
        else if (pluginNameLower.ends_with(".py")) {
            plugins.push_back(pro::make_proxy<PPrePlugin, PythonTextPlugin>(projectDir, replaceStr(pluginName, "<PROJECT_DIR>", wide2Ascii(projectDir)),
                pythonManager, logger));
        }
        else if (pluginName.contains("SkipTrans")) {
            if (!hasRun && !hasPreRun) {
                hasRun = true;
            }
            plugins.push_back(pro::make_proxy<PPrePlugin, SkipTrans>(projectDir, projectConfig, pythonManager, luaManager, logger, hasRun, hasPreRun));
        }
    }

    return plugins;
}

std::vector<pro::proxy<PPostPlugin>> registerPostPlugins(const std::vector<std::string>& pluginNames, const fs::path& projectDir, const fs::path& otherCacheDir,
    PythonManager& pythonManager, LuaManager& luaManager, std::shared_ptr<spdlog::logger> logger,
    const toml::value& projectConfig) {
    std::vector<pro::proxy<PPostPlugin>> plugins;

    for (const auto& pluginName : pluginNames) {
        std::string pluginNameLower = str2Lower(pluginName);
        bool hasRun = pluginName.contains("run:");
        bool hasPostRun = pluginName.contains("postRun:");
        if (pluginNameLower.ends_with(".lua")) {
            plugins.push_back(pro::make_proxy<PPostPlugin, LuaTextPlugin>(projectDir, replaceStr(pluginName, "<PROJECT_DIR>", wide2Ascii(projectDir)),
                luaManager, logger));
        }
        else if (pluginNameLower.ends_with(".py")) {
            plugins.push_back(pro::make_proxy<PPostPlugin, PythonTextPlugin>(projectDir, replaceStr(pluginName, "<PROJECT_DIR>", wide2Ascii(projectDir)),
                pythonManager, logger));
        }
        else if (pluginName.contains("TextPostFull2Half")) {
            if (!hasRun && !hasPostRun) {
                hasPostRun = true;
            }
            plugins.push_back(pro::make_proxy<PPostPlugin, TextPostFull2Half>(projectConfig, logger, hasRun, hasPostRun));
        }
        else if (pluginName.contains("TextLinebreakFix")) {
            if (!hasRun && !hasPostRun) {
                hasRun = true;
            }
            plugins.push_back(pro::make_proxy<PPostPlugin, TextLinebreakFix>(otherCacheDir, projectConfig, logger, hasRun, hasPostRun));
        }
    }

    return plugins;
}
