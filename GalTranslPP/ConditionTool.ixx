module;

#define PYBIND11_HEADERS
#define PCRE2_HEADERS
#include "GPPMacros.hpp"
#include <toml.hpp>
#include <spdlog/spdlog.h>
#include <sol/sol.hpp>

export module ConditionTool;

import Tool;
import PythonManager;
import LuaManager;

namespace fs = std::filesystem;
namespace py = pybind11;

export {

    template<typename T>
    concept IsMapLike = requires {
        typename T::key_type;
        typename T::mapped_type;
    };

    struct GppConditionPattern {
        CachePart conditionTarget = CachePart::None;
        int sentenceOffset = 0;
        jpc::Regex conditionReg;
    };
    using GPPCondition = std::vector<GppConditionPattern>;

    bool checkString(const jpc::Regex& conditionReg, const std::string& str);
    bool checkGppCondition(const GPPCondition& gppCondition, const Sentence* se);

    template<typename TC>
    GPPCondition createGppCondition(const toml::basic_value<TC>& conditionPatterns) {
        GPPCondition patterns;

        auto appendPatternFunc = [&](const auto& conditionTbl)
            {
                GppConditionPattern pattern;
                std::string conditionTargetStr = conditionTbl.at("conditionTarget").as_string();
                while (conditionTargetStr.starts_with("prev_")) {
                    pattern.sentenceOffset--;
                    conditionTargetStr = conditionTargetStr.substr(5);
                }
                while (conditionTargetStr.starts_with("next_")) {
                    pattern.sentenceOffset++;
                    conditionTargetStr = conditionTargetStr.substr(5);
                }
                pattern.conditionTarget = chooseCachePart(conditionTargetStr);
                const std::string& conditionRegStr = conditionTbl.at("conditionReg").as_string();
                if (conditionRegStr.empty()) {
                    return;
                }
                const std::string modifier = toml::find_or(conditionTbl, "compile_modifier", defaultRegCompileModifier);
                pattern.conditionReg.setPattern(conditionRegStr).setModifier(modifier).compile();
                if (!pattern.conditionReg) {
                    throw std::runtime_error(std::format("Failed to compile regex pattern: [{}]", conditionRegStr));
                }
                patterns.push_back(std::move(pattern));
            };
        if (conditionPatterns.is_array()) {
            for (const auto& condition : conditionPatterns.as_array()
                | std::views::filter([](const auto& condition) { return condition.is_table(); }))
            {
                appendPatternFunc(condition);
            }
        }
        else if (conditionPatterns.is_table()) {
            appendPatternFunc(conditionPatterns);
            if (conditionPatterns.contains("additionalPatterns") && conditionPatterns.at("additionalPatterns").is_array()) {
                for (const auto& condition : conditionPatterns.at("additionalPatterns").as_array()
                    | std::views::filter([](const auto& condition) { return condition.is_table(); }))
                {
                    appendPatternFunc(condition);
                }
            }
        }

        return patterns;
    }

    template<typename TC>
    ConditionType getConditionType(const toml::basic_value<TC>& tbl) {
        if (tbl.contains("conditionTarget") && !(tbl.at("conditionTarget").as_string().empty())
            && tbl.contains("conditionReg") && !(tbl.at("conditionReg").as_string().empty())) {
            return ConditionType::Gpp;
        }
        if (tbl.contains("conditionScript") && !tbl.at("conditionScript").as_string().empty()
            && tbl.contains("conditionFunc") && !tbl.at("conditionFunc").as_string().empty()) {
            std::string conditionScriptStr = str2Lower(tbl.at("conditionScript").as_string());
            if (conditionScriptStr.ends_with(".lua")) {
                return ConditionType::Lua;
            }
            else if (conditionScriptStr.ends_with(".py")) {
                return ConditionType::Python;
            }
        }
        return ConditionType::None;
    }

    template<typename TC>
    CheckSeCondFunc getCheckSeCondFunc(const toml::basic_value<TC>& condElem, const fs::path& projectDir,
        PythonManager& pythonManager, LuaManager& luaManager, std::shared_ptr<spdlog::logger>& logger, bool& needReboot) {
        std::vector<CheckSeCondFunc> funcs;

        auto appendFunctionFunc = [&](const auto& tbl)
            {
                ConditionType condType = getConditionType(tbl);
                switch (condType)
                {
                case ConditionType::Gpp:
                {
                    GPPCondition gppCondition = createGppCondition(tbl);
                    CheckSeCondFunc checkFunc = [condr = std::move(gppCondition)](const Sentence* se) -> bool
                        {
                            return checkGppCondition(condr, se);
                        };
                    funcs.push_back(std::move(checkFunc));
                }
                break;

                case ConditionType::Lua:
                {
                    std::string conditionLuaStr = tbl.at("conditionScript").as_string();
                    const std::string& conditionFuncStr = tbl.at("conditionFunc").as_string();
                    std::optional<std::shared_ptr<LuaStateInstance>> luaStateOpt = luaManager.registerFunction(
                        replaceStrInplace(conditionLuaStr, "<PROJECT_DIR>", wide2Ascii(projectDir)), conditionFuncStr, needReboot);
                    if (luaStateOpt) {
                        std::shared_ptr<LuaStateInstance> luaState = *luaStateOpt;
                        sol::function conditionFunc = luaState->functions[conditionFuncStr];
                        CheckSeCondFunc checkFunc = [luaState, conditionFunc, conditionFuncStr](const Sentence* se) -> bool
                            {
                                std::lock_guard<std::mutex> lock(luaState->executionMutex);
                                bool result;
                                try {
                                    result = conditionFunc(se).get<bool>();
                                }
                                catch (const sol::error& e) {
                                    throw std::runtime_error(std::format("执行Lua条件函数 {} 时发生错误: {}", conditionFuncStr, e.what()));
                                }
                                return result;
                            };
                        funcs.push_back(std::move(checkFunc));
                    }
                    else {
                        throw std::runtime_error(std::format("注册Lua脚本 {} 中的条件函数 {} 失败", conditionLuaStr, conditionFuncStr));
                    }
                }
                break;

                case ConditionType::Python:
                {
                    std::string conditionPythonStr = tbl.at("conditionScript").as_string();
                    const std::string& conditionFuncStr = tbl.at("conditionFunc").as_string();
                    std::optional<std::shared_ptr<PythonInterpreterInstance>> pythonInterpreterOpt = pythonManager.registerFunction(
                        replaceStrInplace(conditionPythonStr, "<PROJECT_DIR>", wide2Ascii(projectDir)), conditionFuncStr, needReboot);
                    if (pythonInterpreterOpt) {
                        std::shared_ptr<PythonInterpreterInstance> pythonInterpreter = *pythonInterpreterOpt;
                        std::reference_wrapper<py::object> conditionFunc = *(pythonInterpreter->functions[conditionFuncStr]);
                        CheckSeCondFunc checkFunc = [pythonInterpreter, conditionFunc, conditionFuncStr](const Sentence* se) -> bool
                            {
                                bool result;
                                pythonInterpreter->submitTask([&]()
                                    {
                                        try {
                                            result = conditionFunc.get()(se).cast<bool>();
                                        }
                                        catch (const py::error_already_set& e) {
                                            throw std::runtime_error(std::format("执行Python条件函数 {} 时发生错误: {}", conditionFuncStr, e.what()));
                                        }
                                    }).get();
                                return result;
                            };
                        funcs.push_back(std::move(checkFunc));
                    }
                    else {
                        throw std::runtime_error(std::format("注册Python脚本 {} 中的条件函数 {} 失败", conditionPythonStr, conditionFuncStr));
                    }
                }
                break;

                default:
                    throw std::runtime_error("未知的条件类型");
                }
            };

        if (condElem.is_array()) {
            for (const auto& condition : condElem.as_array()
                | std::views::filter([](const auto& condition) { return condition.is_table(); }))
            {
                appendFunctionFunc(condition);
            }
        }
        else if (condElem.is_table()) {
            appendFunctionFunc(condElem);
        }

        CheckSeCondFunc resultFunc;
        if (funcs.empty()) {
            resultFunc = [](const Sentence*) -> bool
                {
                    return true;
                };
        }
        else {
            resultFunc = [=](const Sentence* se) -> bool
                {
                    return std::ranges::all_of(funcs, [&](const CheckSeCondFunc& func)
                        {
                            return func(se);
                        });
                };
        }
        return resultFunc;
    }
}



module :private;

bool checkString(const jpc::Regex& conditionReg, const std::string& str) {
    jpc::RegexMatch rm(&conditionReg);
    return rm.setSubject(&str).match() > 0;
}

bool checkGppCondition(const GPPCondition& gppCondition, const Sentence* se) {
    return std::ranges::all_of(gppCondition, [&](const GppConditionPattern& pattern)
        {
            const Sentence* sentenceToCheck = se;
            if (pattern.sentenceOffset > 0) {
                for (int i = 0; i < pattern.sentenceOffset; i++) {
                    sentenceToCheck = sentenceToCheck->next;
                    if (sentenceToCheck == nullptr) {
                        return false;
                    }
                }
            }
            else if (pattern.sentenceOffset < 0) {
                for (int i = 0; i > pattern.sentenceOffset; i--) {
                    sentenceToCheck = sentenceToCheck->prev;
                    if (sentenceToCheck == nullptr) {
                        return false;
                    }
                }
            }
            auto checkAnyOf = [&]<typename ContainerType>(const ContainerType& container) -> bool
            {
                return std::ranges::any_of(container, [&](const auto& item)
                    {
                        if constexpr (IsMapLike<ContainerType>) {
                            return checkString(pattern.conditionReg, item.first) || checkString(pattern.conditionReg, item.second);
                        }
                        else {
                            return checkString(pattern.conditionReg, item);
                        }
                    });
            };
            switch (pattern.conditionTarget) {
            case CachePart::Names:
                return checkAnyOf(sentenceToCheck->names);
            case CachePart::NamesPreview:
                return checkAnyOf(sentenceToCheck->names_preview);
            case CachePart::Problems:
                return checkAnyOf(sentenceToCheck->problems);
            case CachePart::OtherInfo:
                return checkAnyOf(sentenceToCheck->other_info);
            default:
                return checkString(pattern.conditionReg, chooseStringRef(sentenceToCheck, pattern.conditionTarget));
            }
            return false;
        });
}
