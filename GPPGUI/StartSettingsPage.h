// StartSettingsPage.h

#ifndef STARTSETTINGSPAGE_H
#define STARTSETTINGSPAGE_H

#include <QThread>
#include <toml.hpp>
#include <filesystem>
#include <QSystemTrayIcon>
#include "BasePage.h"
#include "TranslatorWorker.h"
#include "ExponentialMovingAverageEstimator.hpp"

namespace fs = std::filesystem;

class ElaPushButton;
class ElaProgressBar;
class ElaComboBox;
class ElaPlainTextEdit;
class ElaProgressRing;
class ElaLCDNumber;
class NJCfgPage;
class EpubCfgPage;
class PDFCfgPage;
class CustomFilePluginCfgPage;

class StartSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit StartSettingsPage(QWidget* mainWindow, fs::path& projectDir, toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~StartSettingsPage() override;
    virtual void apply2Config() override;

    void clearLog();

Q_SIGNALS:
    void startTranslating();  // 让projectSettings去保存配置
    void finishTranslatingSignal(const QString& transEngine, int exitCode); // 这两个向projectSettings页发送
    void startWork();
    void stopWork();  // 这两个向worker发送

private:

    fs::path& _projectDir;
    QThread* _workThread;
    TranslatorWorker* _worker;

    void _setupUI();
    toml::ordered_value& _projectConfig;
    toml::ordered_value& _globalConfig;
    QWidget* _mainWindow;

    ElaPushButton* _startTranslateButton;
    ElaPushButton* _stopTranslateButton;
    ElaProgressBar* _progressBar;

    ElaPlainTextEdit* _logOutput;

    ElaComboBox* _fileFormatComboBox;

    QString _transEngine;
    ElaProgressRing* _threadNumRing;
    ElaLCDNumber* _usedTimeLabel;
    ElaLCDNumber* _remainTimeLabel;
    QSystemTrayIcon* _trayIcon;
    ExponentialMovingAverageEstimator _estimator;
    std::chrono::high_resolution_clock::time_point _startTime;

private Q_SLOTS:

    void _onStartTranslatingClicked();
    void _onStopTranslatingClicked();
    void _workFinished(int exitCode); // worker结束了的信号
    void _onOutputSettingClicked();

private:
    // 文件格式输出配置页
    NJCfgPage* _njCfgPage;
    EpubCfgPage* _epubCfgPage;
    PDFCfgPage* _pdfCfgPage;
    CustomFilePluginCfgPage* _customFilePluginCfgPage;
};

#endif // STARTSETTINGSPAGE_H
