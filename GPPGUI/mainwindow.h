#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#define PYBIND11_HEADERS
#include "../GalTranslPP/GPPMacros.hpp"
#include "ElaWindow.h"

#include <QMainWindow>
#include <toml.hpp>
#include <filesystem>

class HomePage;
class AboutDialog;
class DefaultPromptPage;
class CommonGptDictPage;
class CommonNormalDictPage;
class SettingPage;
class ProjectSettingsPage;
class ElaContentDialog;
class UpdateChecker;

namespace fs = std::filesystem;
namespace py = pybind11;

class MainWindow : public ElaWindow
{
    Q_OBJECT

public:
    MainWindow(std::unique_ptr<py::gil_scoped_release>& release, QWidget* parent = nullptr);
    ~MainWindow();

public Q_SLOTS:
    void checkUpdate();

protected:
    virtual void mouseReleaseEvent(QMouseEvent* event);

private Q_SLOTS:
    void _onNewProjectTriggered();
    void _onOpenProjectTriggered();
    void _onRemoveProjectTriggered();
    void _onDeleteProjectTriggered();
    void _onSaveProjectTriggered();
    void _onFinishTranslating(QString nodeKey);
    void _onCloseWindowClicked(bool restart);

private:

    void initWindow();
    void initEdgeLayout();
    void initContent();
    ProjectSettingsPage* _createProjectSettingsPage(const fs::path& projectDir);

    HomePage* _homePage{nullptr};
    AboutDialog* _aboutDialog{nullptr};
    DefaultPromptPage* _defaultPromptPage{nullptr};
    CommonNormalDictPage* _commonPreDictPage{nullptr};
    CommonGptDictPage* _commonGptDictPage{nullptr};
    CommonNormalDictPage* _commonPostDictPage{nullptr};
    SettingPage* _settingPage{nullptr};

    QString _commonDictExpanderKey;
    QString _projectExpanderKey;

    QString _aboutKey;
    QString _transIllustrationKey;
    QString _settingKey;

    QList<QSharedPointer<ProjectSettingsPage>> _projectPages;

    ElaContentDialog* _closeDialog{nullptr};
    UpdateChecker* _updateChecker{nullptr};

    toml::ordered_value _globalConfig;
    std::unique_ptr<py::gil_scoped_release>& _release;
};
#endif // MAINWINDOW_H
