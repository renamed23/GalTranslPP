module;

module GPPDefines;

namespace fs = std::filesystem;

const std::string GPPVERSION = "2.2.4";
const std::string PYTHONVERSION = "1.0.0";
const std::string PROMPTVERSION = "2.0.0";
const std::string DICTVERSION = "1.0.2";
const std::string QTVERSION = "6.9.2";
const std::string ICUVERSION = "7.8.0";

const fs::path baseConfigPath = L"BaseConfig";
const fs::path globalConfigPath = baseConfigPath / L"globalConfig.toml";
const fs::path defaultPromptPath = baseConfigPath / L"Prompt.toml";
const fs::path defaultDictPath = baseConfigPath / L"Dict";
const fs::path defaultGptDictPath = defaultDictPath / L"gpt";
const fs::path defaultPreDictPath = defaultDictPath / L"pre";
const fs::path defaultPostDictPath = defaultDictPath / L"post";
const fs::path pluginConfigsPath = baseConfigPath / L"pluginConfigs";
const fs::path filePluginConfigPath = pluginConfigsPath / L"filePlugins";
const fs::path prePluginConfigPath = pluginConfigsPath / L"textPrePlugins";
const fs::path postPluginConfigPath = pluginConfigsPath / L"textPostPlugins";
const std::wstring transCacheDirName = L"transl_cache";
const std::wstring otherCacheDirName = L"other_cache";

const std::string defaultRegCompileModifier = "mnjS"; // m: 多行, n: unicode 支持, j: \xhh \uhhhh 语法支持, S: jit编译
const std::string defaultRegReplaceModifier = "gxE"; // g: gloabl, x: ${n:-replace}/${n:+trueText:falseText} 语法支持, E: 未匹配的引用返回空字符串代替
