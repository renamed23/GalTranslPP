module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"
#include <toml.hpp>
#include <spdlog/spdlog.h>

export module PythonManager;

import Tool;

namespace fs = std::filesystem;
namespace py = pybind11;

export {

    template <typename T>
    class SafeQueue {
    public:
        void push(T value) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(std::move(value));
            m_cond.notify_one();
        }

        std::optional<T> pop() {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cond.wait(lock, [this] { return !m_queue.empty() || m_stopped; });
            if (m_stopped && m_queue.empty()) {
                return std::nullopt;
            }
            T value = std::move(m_queue.front());
            m_queue.pop();
            return value;
        }

        void stop() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopped = true;
            m_cond.notify_all();
        }

    private:
        std::queue<T> m_queue;
        std::mutex m_mutex;
        std::condition_variable m_cond;
        bool m_stopped = false;
    };

    struct PythonTask {
        std::function<void()> taskFunc;
        std::promise<void> promise; // 用于返回结果
    };

    class PythonMainInterpreterManager {
    public:

        static PythonMainInterpreterManager& getInstance() {
            static PythonMainInterpreterManager instance;
            return instance;
        }
        ~PythonMainInterpreterManager(){}

        void stop() {
            m_taskQueue.stop();
            if (m_daemonThread.joinable()) {
                m_daemonThread.join();
            }
        }

        PythonMainInterpreterManager(PythonMainInterpreterManager&) = delete;
        PythonMainInterpreterManager(PythonMainInterpreterManager&&) = delete;

        std::future<void> submitTask(std::function<void()> taskFunc) {
            std::unique_ptr<PythonTask> task = std::make_unique<PythonTask>();
            task->taskFunc = std::move(taskFunc);
            auto future = task->promise.get_future();
            m_taskQueue.push(std::move(task));
            return future;
        }
        
        std::shared_ptr<py::object> registerNLPFunction
        (const std::string& moduleName, const std::string& modelName, std::shared_ptr<spdlog::logger>& logger, bool& needReboot);

    private:

        PythonMainInterpreterManager() {
            if (Py_IsInitialized()) {
                m_daemonThread = std::thread(&PythonMainInterpreterManager::daemonThreadFunc, this);
            }
            else {
                throw std::runtime_error("Python 环境未初始化");
            }
        }

        void daemonThreadFunc() {
            py::gil_scoped_acquire acquire;
            try {
                py::module_::import("gpp_plugin_api");
            }
            catch (const py::error_already_set& e) {
                throw std::runtime_error("import gpp_plugin_api 时出现异常: " + std::string(e.what()));
            }
            while (true) {
                auto taskOpt = m_taskQueue.pop();
                if (!taskOpt) {
                    break;
                }
                auto task = std::move(*taskOpt);
                try {
                    task->taskFunc();
                    task->promise.set_value();
                }
                catch (...) {
                    task->promise.set_exception(std::current_exception());
                }
            }
        }

        std::mutex m_mutex;
        std::map<std::string, std::map<std::string, std::weak_ptr<py::object>>> m_nlpModuleFunctions;
        std::thread m_daemonThread; // 守护线程
        SafeQueue<std::unique_ptr<PythonTask>> m_taskQueue;
    };

    template <typename T>
    void pythonMainInterpreterDeleter(T* ptr) {
        auto deleteTaskFunc = [ptr]()
            {
                delete ptr;
            };
        PythonMainInterpreterManager::getInstance().submitTask(std::move(deleteTaskFunc));
    }

    struct PythonInterpreterInstance {
        std::unique_ptr<py::subinterpreter> subInterpreter;
        // 这里的 functions 只允许用 weak_ptr 继承
        std::map<std::string, std::shared_ptr<py::object>> functions;
        std::thread daemonThread;
        SafeQueue<std::unique_ptr<PythonTask>> m_taskQueue;

        void daemonThreadFunc();

        std::future<void> submitTask(std::function<void()> taskFunc) {
            std::unique_ptr<PythonTask> task = std::make_unique<PythonTask>();
            task->taskFunc = std::move(taskFunc);
            auto future = task->promise.get_future();
            m_taskQueue.push(std::move(task));
            return future;
        }

        PythonInterpreterInstance();

        ~PythonInterpreterInstance();
    };

    class PythonManager {

    public:

        std::optional<std::shared_ptr<PythonInterpreterInstance>> registerFunction
        (const std::string& modulePath, const std::string& functionName, bool& needReboot);

        PythonManager(std::shared_ptr<spdlog::logger> logger) : m_logger(logger) {}

    private:

        void registerCustomTypes(const std::string& moduleName, bool& needReboot);

        std::map<fs::path, std::shared_ptr<PythonInterpreterInstance>> m_interpreters;

        std::shared_ptr<spdlog::logger> m_logger;
    };

    void checkPythonDependencies(const std::vector<std::string>& dependencies, std::shared_ptr<spdlog::logger>& logger);

}
