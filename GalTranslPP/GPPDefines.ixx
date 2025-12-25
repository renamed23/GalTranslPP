export module GPPDefines;

export import std;

namespace fs = std::filesystem;

export {

    extern const std::string GPPVERSION;
    extern const std::string PYTHONVERSION;
    extern const std::string PROMPTVERSION;
    extern const std::string DICTVERSION;
    extern const std::string QTVERSION;
    extern const std::string ICUVERSION;

    extern const fs::path baseConfigPath;
    extern const fs::path globalConfigPath;
    extern const fs::path defaultPromptPath;
    extern const fs::path defaultDictPath;
    extern const fs::path defaultGptDictPath;
    extern const fs::path defaultPreDictPath;
    extern const fs::path defaultPostDictPath;
    extern const fs::path pluginConfigsPath;
    extern const fs::path filePluginConfigPath;
    extern const fs::path prePluginConfigPath;
    extern const fs::path postPluginConfigPath;

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