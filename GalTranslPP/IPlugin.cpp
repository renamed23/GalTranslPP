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

std::vector<pro::proxy<PPlugin>> registerPlugins(const std::vector<std::string>& pluginNames, const fs::path& projectDir,
    PythonManager& pythonManager, LuaManager& luaManager, std::shared_ptr<spdlog::logger> logger,
    const toml::value& projectConfig) {
    std::vector<pro::proxy<PPlugin>> plugins;

    for (const auto& pluginName : pluginNames) {
        std::string pluginNameLower = str2Lower(pluginName);
        if (pluginNameLower.ends_with(".lua")) {
            plugins.push_back(pro::make_proxy_shared<PPlugin, LuaTextPlugin>(projectDir, replaceStr(pluginName, "<PROJECT_DIR>", wide2Ascii(projectDir)),
                luaManager, logger));
        }
        else if (pluginNameLower.ends_with(".py")) {
            plugins.push_back(pro::make_proxy_shared<PPlugin, PythonTextPlugin>(projectDir, replaceStr(pluginName, "<PROJECT_DIR>", wide2Ascii(projectDir)),
                pythonManager, logger));
        }
        else if (pluginName == "TextPostFull2Half") {
            plugins.push_back(pro::make_proxy_shared<PPlugin, TextPostFull2Half>(projectConfig, logger));
        }
        else if (pluginName == "TextLinebreakFix") {
            plugins.push_back(pro::make_proxy_shared<PPlugin, TextLinebreakFix>(projectDir, projectConfig, logger));
        }
        else if (pluginName == "SkipTrans") {
            plugins.push_back(pro::make_proxy_shared<PPlugin, SkipTrans>(projectDir, projectConfig, pythonManager, luaManager, logger));
        }
    }

    return plugins;
}
