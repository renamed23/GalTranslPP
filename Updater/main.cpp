#include <toml.hpp>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QApplication>
#include <QProcess>
#include <QDir>
#include <QDebug>
#pragma comment(lib, "../lib/GalTranslPP.lib")


#ifdef Q_OS_WIN
#include <windows.h>
#pragma comment(lib, "User32.lib")
#endif

import Tool;
namespace fs = std::filesystem;

template<typename FnCastTo>
FnCastTo FnCast(uint64_t fnToCast, FnCastTo) {
    return (FnCastTo)fnToCast;
}

void waitForProcessToExit(qint64 pid) {
#ifdef Q_OS_WIN
    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (hProcess != NULL) {
        WaitForSingleObject(hProcess, INFINITE);
        CloseHandle(hProcess);
    }
#else

#endif
}

void killProcess(qint64 pid) {
#ifdef Q_OS_WIN
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess != NULL) {
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
    }
#else

#endif
}


int main(int argc, char* argv[]) {

#ifdef Q_OS_WIN
    HMODULE hUser32 = LoadLibraryW(L"User32.dll");
    if (hUser32) {
        uint64_t orgSetProcessDpiAwarenessContext = (uint64_t)(GetProcAddress(hUser32, "SetProcessDpiAwarenessContext"));
        if (orgSetProcessDpiAwarenessContext) {
            FnCast(orgSetProcessDpiAwarenessContext, SetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
        FreeLibrary(hUser32);
    }
#endif

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("GalTransl++ Updater");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({ {"p", "pid"}, "Process ID of the main application.", "pid" });
    parser.addOption({ {"s", "source"}, "Source path of the update package.", "source" });
    parser.addOption({ {"t", "target"}, "Target directory for installation.", "target" });
    parser.addOption({ {"r", "restart"}, "Restart the main application after update is installed.", "restart" });
    parser.addOption({ {"n", "newActionFlag"}, "Flag to indicate a new action.", "newActionFlag" });
    parser.addOption({ QStringList{"gppVersion"}, "Version of GalTranslPP.", "gppVersion" });
    parser.addOption({ QStringList{"pythonVersion"}, "Version of Python.", "pythonVersion" });
    parser.addOption({ QStringList{"promptVersion"}, "Version of Prompt.", "promptVersion" });
    parser.addOption({ QStringList{"dictVersion"}, "Version of dictionary.", "dictVersion" });
    parser.addOption({ QStringList{"qtVersion"}, "Version of QT.", "qtVersion" });
    parser.process(a);

    qint64 pid = parser.value("pid").toLongLong();
    QString sourceZip = parser.value("source");
    QString targetDir = parser.value("target");

    if (pid == 0 || sourceZip.isEmpty() || targetDir.isEmpty()) {
#ifdef Q_OS_WIN
        MessageBoxW(NULL, L"Invalid arguments provided.", L"GalTransl++ Updater", MB_ICONERROR | MB_TOPMOST);
#endif
        return -1;
    }

    if (parser.isSet("newActionFlag")) {
        bool isCompatible;
        std::string orgGppVersion = parser.isSet("gppVersion") ? parser.value("gppVersion").toStdString() : "1.0.0";
        std::string orgPythonVersion = parser.isSet("pythonVersion") ? parser.value("pythonVersion").toStdString() : "1.0.0";
        std::string orgPromptVersion = parser.isSet("promptVersion") ? parser.value("promptVersion").toStdString() : "1.0.0";
        std::string orgDictVersion = parser.isSet("dictVersion") ? parser.value("dictVersion").toStdString() : "1.0.0";
        std::string orgQtVersion = parser.isSet("qtVersion") ? parser.value("qtVersion").toStdString() : "6.9.2";
        // 向后兼容，在下个大版本中删除
        if (cmpVer("2.1.1", orgGppVersion, isCompatible)) {
            killProcess(pid);
        }
        else {
            waitForProcessToExit(pid);
        }
        try {
            std::set<std::string> excludePreFixes =
            {
                "BaseConfig/python-3.12.10-embed-amd64", "BaseConfig/pyScripts", "BaseConfig/Prompt.toml", 
                "BaseConfig/Dict", 
            };
            // 向后兼容
            if (cmpVer("2.1.1", orgGppVersion, isCompatible)) {
                excludePreFixes.insert("Qt6Core.dll");
                excludePreFixes.insert("7z.dll");
                excludePreFixes.insert("Updater_new.exe");
            }
            if (cmpVer(PYTHONVERSION, orgPythonVersion, isCompatible)) {
                excludePreFixes.erase("BaseConfig/python-3.12.10-embed-amd64");
                excludePreFixes.erase("BaseConfig/pyScripts");
            }
            if (cmpVer(PROMPTVERSION, orgPromptVersion, isCompatible)) {
#ifdef Q_OS_WIN
                int ret = MessageBoxW(NULL, L"检测到新版本的 Prompt，是否更新 Prompt (会覆盖当前的默认提示词)？", L"GalTransl++ Updater", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
                if (ret == IDYES) {
                    excludePreFixes.erase("BaseConfig/Prompt.toml");
                }
#endif
            }
            if (cmpVer(DICTVERSION, orgDictVersion, isCompatible)) {
#ifdef Q_OS_WIN
                int ret = MessageBoxW(NULL, L"检测到新版本的 GPT 字典，是否更新字典（会覆盖当前的默认字典）？", L"GalTransl++ Updater", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
                if (ret == IDYES) {
                    excludePreFixes.erase("BaseConfig/Dict");
                }
#endif
            }
            extractZipExclude(sourceZip.toStdWString(), targetDir.toStdWString(), excludePreFixes);

#ifdef Q_OS_WIN
            MessageBoxW(NULL, L"GalTransl++ 更新成功", L"成功", MB_OK | MB_TOPMOST);
#endif
            fs::remove(sourceZip.toStdWString());
            if (parser.isSet("restart")) {
                QStringList args;
                args << "--pid" << QString::number(QApplication::applicationPid());
                QProcess::startDetached(parser.value("restart"), args, targetDir);
            }
        }
        catch (const std::exception&) {
#ifdef Q_OS_WIN
            MessageBoxW(NULL, L"Failed to extract update package.", L"GalTransl++ Updater", MB_ICONERROR | MB_TOPMOST);
#endif
            return -1;
        }
        return 0;
    }

    // 1. 等待主程序退出
    waitForProcessToExit(pid);

    // 2. 解压并覆盖文件
    try {
        fs::create_directories(targetDir.toStdWString() + L"/new");
        extractFileFromZip(sourceZip.toStdWString(), targetDir.toStdWString() + L"/new", "Updater_new.exe");
        extractFileFromZip(sourceZip.toStdWString(), targetDir.toStdWString() + L"/new", "Qt6Core.dll");
        extractFileFromZip(sourceZip.toStdWString(), targetDir.toStdWString() + L"/new", "7z.dll");
        if (fs::exists(L"new/Updater_new.exe")) {
            QStringList arguments;
            arguments << "--newActionFlag" << QString::number(QApplication::applicationPid());
            arguments << "--pid" << QString::number(QApplication::applicationPid());
            arguments << "--source" << sourceZip << "--target" << targetDir;
            arguments << "--gppVersion" << QString::fromStdString(GPPVERSION);
            arguments << "--pythonVersion" << QString::fromStdString(PYTHONVERSION);
            arguments << "--promptVersion" << QString::fromStdString(PROMPTVERSION);
            arguments << "--dictVersion" << QString::fromStdString(DICTVERSION);
            arguments << "--qtVersion" << QString::fromStdString(QTVERSION);
            if (parser.isSet("restart")) {
                arguments << "--restart" << parser.value("restart");
            }
            QProcess::startDetached("new/Updater_new.exe", arguments, QString(targetDir.toStdWString() + L"/new"));
        }
    }
    catch (const std::exception&) {
#ifdef Q_OS_WIN
        MessageBoxW(NULL, L"Failed to extract Updater_new.exe.", L"GalTransl++ Updater", MB_ICONERROR | MB_TOPMOST);
#endif
        return -1;
    }

    return 0;
}
