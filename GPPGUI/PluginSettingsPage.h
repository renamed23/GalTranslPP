// PluginSettingsPage.h

#ifndef PLUGINSETTINGSPAGE_H
#define PLUGINSETTINGSPAGE_H

#include <QList>
#include <toml.hpp>
#include "BasePage.h"

class QVBoxLayout;
class PluginItemWidget;
class TF2HCfgPage;
class TLFCfgPage;
class SkipTransCfgPage;

namespace fs = std::filesystem;

class PluginSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit PluginSettingsPage(QWidget* mainWindow, fs::path& projectDir, toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~PluginSettingsPage();
    virtual void apply2Config() override;

private Q_SLOTS:
    void _onItemMoveUp(PluginItemWidget* item);
    void _onItemMoveDown(PluginItemWidget* item);
    void _onItemSettings(PluginItemWidget* item);

private:
    void _setupUI();
    void _updateMoveButtonStates(); // 更新所有项的上下移动按钮状态

    QVBoxLayout* _pluginListLayout; // 容纳所有 PluginItemWidget 的布局
    QList<PluginItemWidget*> _pluginItems; // 按顺序存储所有项的指针

    fs::path& _projectDir;
    toml::ordered_value& _projectConfig;
    QWidget* _mainWindow;

private:
    // 以下为各个插件的设置页面
    TF2HCfgPage* _tf2hCfgPage;
    TLFCfgPage* _tlfCfgPage;
    SkipTransCfgPage* _skipTransCfgPage;
};

#endif // PLUGINSETTINGSPAGE_H
