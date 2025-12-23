export module GPPDefines;

export import std;

namespace fs = std::filesystem;

export {

    const std::string GPPVERSION = "2.2.0";
    const std::string PYTHONVERSION = "1.0.0";
    const std::string PROMPTVERSION = "2.0.0";
    const std::string DICTVERSION = "1.0.2";
    const std::string QTVERSION = "6.9.2";

    const fs::path baseConfigPath = fs::current_path() / L"BaseConfig";
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

    enum class NameType
    {
        None, Single, Multiple
    };

    struct Sentence {
        int index;
        std::string name;
        std::vector<std::string> names;
        std::string name_preview;
        std::vector<std::string> names_preview;
        std::string original_text;
        std::string pre_processed_text;
        std::string pre_translated_text;
        std::vector<std::string> problems;
        std::string translated_by;
        std::string translated_preview;
        std::string originalLinebreak;
        std::map<std::string, std::string> other_info;

        NameType nameType = NameType::None;
        Sentence* prev = nullptr;
        Sentence* next = nullptr;
        bool complete = false;
        bool notAnalyzeProblem = false;

        std::optional<std::string> problems_get_by_index(int index) {
            if (index < 0 || index >= problems.size()) {
                return std::nullopt;
            }
            return problems[index];
        }

        bool problems_set_by_index(int index, const std::string& problem) {
            if (index < 0 || index >= problems.size()) {
                return false;
            }
            problems[index] = problem;
            return true;
        }
    };

    enum class TransEngine
    {
        None, ForGalJson, ForGalTsv, ForNovelTsv, DeepseekJson, Sakura, DumpName, NameTrans, GenDict, Rebuild, ShowNormal
    };

    enum class CachePart 
    { 
        None, Name, NamePreview, Names, NamesPreview, OrigText, PreprocText, PretransText, Problems, OtherInfo, TranslatedBy, TransPreview 
    };

    enum class ConditionType
    {
        None, Gpp, Lua, Python
    };


    using WordPosVec = std::vector<std::vector<std::string>>;
    using EntityVec = std::vector<std::vector<std::string>>;
    using NLPResult = std::tuple<WordPosVec, EntityVec>;

    using CheckSeCondFunc = std::function<bool(const Sentence*)>;

}