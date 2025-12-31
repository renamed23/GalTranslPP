// PluginSettingsPage.cpp

#include "PluginSettingsPage.h"
#include "PluginItemWidget.h" // 引入自定义控件

#include <QVBoxLayout>
#include "ElaText.h"
#include "ElaPushButton.h"
#include "ElaScrollPageArea.h"
#include "ElaPlainTextEdit.h"
#include "ElaMessageBar.h"
#include "ElaToolTip.h"

#include "TLFCfgPage.h"
#include "PostFull2HalfCfgPage.h"
#include "SkipTransCfgPage.h"

import Tool;

PluginSettingsPage::PluginSettingsPage(QWidget* mainWindow, toml::ordered_value& projectConfig, QWidget* parent) :
    BasePage(parent), _projectConfig(projectConfig), _mainWindow(mainWindow)
{
    setWindowTitle(tr("插件设置"));
    setTitleVisible(false);

    _setupUI();
}

PluginSettingsPage::~PluginSettingsPage()
{
}

void PluginSettingsPage::apply2Config()
{
    if (_applyFunc) {
        _applyFunc();
    }
}

void PluginSettingsPage::_setupUI()
{
    QWidget* mainWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

    // 前处理插件列表
    ElaText* preTitle = new ElaText(mainWidget);
    preTitle->setText(tr("预处理插件设置"));
    preTitle->setTextPixelSize(18);
    mainLayout->addWidget(preTitle);

    QWidget* preListContainer = new QWidget(mainWidget);
    _prePluginListLayout = new QVBoxLayout(preListContainer);

    // 插件名称列表
    QStringList prePluginNames = { "SkipTrans" };
    toml::ordered_array customPrePlugins;
    const auto& prePluginsArr = toml::find_or_default<toml::ordered_array>(_projectConfig, "plugins", "textPrePlugins");
    for (const auto& pluginNameStr : prePluginsArr) {
        if (!pluginNameStr.is_string()) {
            continue;
        }
        QString pluginName = QString::fromStdString(pluginNameStr.as_string());
        if (!prePluginNames.contains(pluginName)) {
            customPrePlugins.push_back(pluginNameStr);
            continue;
        }
        PluginItemWidget* item = new PluginItemWidget(pluginName, this);
        item->setIsToggled(true);
        _prePluginItems.append(item);
        _prePluginListLayout->addWidget(item);
        connect(item, &PluginItemWidget::moveUpRequested, this, &PluginSettingsPage::_onPreMoveUp);
        connect(item, &PluginItemWidget::moveDownRequested, this, &PluginSettingsPage::_onPreMoveDown);
        connect(item, &PluginItemWidget::settingsRequested, this, &PluginSettingsPage::_onPreSettings);
        // 防止重复添加
        prePluginNames.removeOne(pluginName);
    }

    mainLayout->addWidget(preListContainer);
    mainLayout->addSpacing(10);

    // 遍历剩下的名称列表，创建并添加 PluginItemWidget
    for (const QString& name : prePluginNames)
    {
        PluginItemWidget* item = new PluginItemWidget(name, this);
        _prePluginItems.append(item); // 添加到列表中
        _prePluginListLayout->addWidget(item); // 添加到布局中

        // 连接信号
        connect(item, &PluginItemWidget::moveUpRequested, this, &PluginSettingsPage::_onPreMoveUp);
        connect(item, &PluginItemWidget::moveDownRequested, this, &PluginSettingsPage::_onPreMoveDown);
        connect(item, &PluginItemWidget::settingsRequested, this, &PluginSettingsPage::_onPreSettings);
    }

    // 后处理插件列表
    ElaText* postTitle = new ElaText(mainWidget);
    postTitle->setText(tr("后处理插件设置"));
    postTitle->setTextPixelSize(18);
    mainLayout->addWidget(postTitle);

    // 创建一个容器用于放置列表
    QWidget* postListContainer = new QWidget(mainWidget);
    _postPluginListLayout = new QVBoxLayout(postListContainer);

    // 插件名称列表
    QStringList postPluginNames = { "TextPostFull2Half", "TextLinebreakFix" };
    toml::ordered_array customPostPlugins;
    // 先处理项目已经启用的插件
    const auto& postPluginsArr = toml::find_or_default<toml::ordered_array>(_projectConfig, "plugins", "textPostPlugins");
    for (const auto& pluginNameStr : postPluginsArr) {
        if (!pluginNameStr.is_string()) {
            continue;
        }
        QString pluginName = QString::fromStdString(pluginNameStr.as_string());
        if (!postPluginNames.contains(pluginName)) {
            customPostPlugins.push_back(pluginNameStr);
            continue;
        }
        PluginItemWidget* item = new PluginItemWidget(pluginName, this);
        item->setIsToggled(true);
        _postPluginItems.append(item);
        _postPluginListLayout->addWidget(item);
        connect(item, &PluginItemWidget::moveUpRequested, this, &PluginSettingsPage::_onPostMoveUp);
        connect(item, &PluginItemWidget::moveDownRequested, this, &PluginSettingsPage::_onPostMoveDown);
        connect(item, &PluginItemWidget::settingsRequested, this, &PluginSettingsPage::_onPostSettings);
        // 防止重复添加
        postPluginNames.removeOne(pluginName);
    }

    // 遍历剩下的名称列表，创建并添加 PluginItemWidget
    for (const QString& name : postPluginNames)
    {
        PluginItemWidget* item = new PluginItemWidget(name, this);
        _postPluginItems.append(item); // 添加到列表中
        _postPluginListLayout->addWidget(item); // 添加到布局中

        // 连接信号
        connect(item, &PluginItemWidget::moveUpRequested, this, &PluginSettingsPage::_onPostMoveUp);
        connect(item, &PluginItemWidget::moveDownRequested, this, &PluginSettingsPage::_onPostMoveDown);
        connect(item, &PluginItemWidget::settingsRequested, this, &PluginSettingsPage::_onPostSettings);
    }
    mainLayout->addWidget(postListContainer);

    // 初始化按钮状态
    _updatePreMoveButtonStates();
    _updatePostMoveButtonStates();

    auto createCustomPluginsPlainTextEditFunc = 
        [=](const QString& title, const std::string& configKey, const toml::ordered_array& customPlugins) -> std::function<void(toml::ordered_array&)>
        {
            ElaText* customPluginsTitle = new ElaText(title, 18, mainWidget);
            customPluginsTitle->setWordWrap(false);
            ElaToolTip* customPluginsTip = new ElaToolTip(customPluginsTitle);
            customPluginsTip->setToolTip("用来加载自定义的 Lua/Python 插件");
            mainLayout->addWidget(customPluginsTitle);
            ElaPlainTextEdit* customPluginsEdit = new ElaPlainTextEdit(mainWidget);
            customPluginsEdit->setMaximumHeight(100);
            QFont font = customPluginsEdit->font();
            font.setPixelSize(14);
            customPluginsEdit->setFont(font);
            customPluginsEdit->setPlainText(QString::fromStdString(toml::format(toml::ordered_value{ toml::ordered_table{{ configKey, customPlugins }} })));
            customPluginsEdit->moveCursor(QTextCursor::Start);
            mainLayout->addWidget(customPluginsEdit);

            std::function<void(toml::ordered_array&)> saveFunc = [=](toml::ordered_array& arr)
                {
                    try {
                        toml::ordered_value newCustomPluginsTbl = toml::parse_str<toml::ordered_type_config>(customPluginsEdit->toPlainText().toStdString());
                        auto& newCustomPluginsArr = newCustomPluginsTbl[configKey];
                        if (newCustomPluginsArr.is_array()) {
                            for (const auto& newCustomPlugin : newCustomPluginsArr.as_array() 
                                | std::views::filter([](const auto& plugin) { return plugin.is_string(); })) {
                                arr.push_back(newCustomPlugin);
                            }
                        }
                    }
                    catch (...) {
                        ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析错误"), QString::fromStdString(configKey) + tr(" 不符合 toml 规范"), 3000);
                    }
                };
            return saveFunc;
        };

    mainLayout->addSpacing(10);
    auto saveCustomPrePluginsFunc = createCustomPluginsPlainTextEditFunc(tr("自定义预处理插件"), "customTextPrePlugins", customPrePlugins);
    auto saveCustomPostPluginsFunc = createCustomPluginsPlainTextEditFunc(tr("自定义后处理插件"), "customTextPostPlugins", customPostPlugins);
    
    mainLayout->addStretch();

    addCentralWidget(mainWidget, true, true, 0);

    // 这里的顺序和_onPre/PostSettings 中的navigation索引对应
    _pf2hCfgPage = new PostFull2HalfCfgPage(_projectConfig, this);
    addCentralWidget(_pf2hCfgPage);
    _tlfCfgPage = new TLFCfgPage(_projectConfig, this);
    addCentralWidget(_tlfCfgPage, true, true, 0);
    _skipTransCfgPage = new SkipTransCfgPage(_projectConfig, this);
    addCentralWidget(_skipTransCfgPage, true, true, 0);



    _applyFunc = [=]()
        {
            _skipTransCfgPage->apply2Config();
            _tlfCfgPage->apply2Config();
            _pf2hCfgPage->apply2Config();

            toml::ordered_array prePlugins;
            for (PluginItemWidget* item : _prePluginItems) {
                if (!item->isToggled()) {
                    continue;
                }
                prePlugins.push_back(item->getPluginName().toStdString());
            }
            toml::ordered_array postPlugins;
            for (PluginItemWidget* item : _postPluginItems) {
                if (!item->isToggled()) {
                    continue;
                }
                postPlugins.push_back(item->getPluginName().toStdString());
            }
            saveCustomPrePluginsFunc(prePlugins);
            saveCustomPostPluginsFunc(postPlugins);
            insertToml(_projectConfig, "plugins.textPrePlugins", prePlugins);
            insertToml(_projectConfig, "plugins.textPostPlugins", postPlugins);
        };
}

void PluginSettingsPage::_onPostSettings(PluginItemWidget* item)
{
    if (!item) {
        return;
    }
    QString pluginName = item->getPluginName();

    if (pluginName == "TextPostFull2Half") {
        this->navigation(1);
    }
    else if (pluginName == "TextLinebreakFix") {
        this->navigation(2);
    }
}

void PluginSettingsPage::_onPreSettings(PluginItemWidget* item)
{
    if (!item) {
        return;
    }
    QString pluginName = item->getPluginName();

    if (pluginName == "SkipTrans") {
        this->navigation(3);
    }
}

// 下面不用看，没什么用

void PluginSettingsPage::_onPostMoveUp(PluginItemWidget* item)
{
    int index = _postPluginListLayout->indexOf(item);
    if (index > 0) // 确保不是第一个
    {
        // 从布局和列表中移除
        _postPluginListLayout->removeWidget(item);
        _postPluginItems.removeOne(item);

        // 插入到新位置
        _postPluginListLayout->insertWidget(index - 1, item);
        _postPluginItems.insert(index - 1, item);

        _updatePostMoveButtonStates();
    }
}

void PluginSettingsPage::_onPostMoveDown(PluginItemWidget* item)
{
    int index = _postPluginListLayout->indexOf(item);
    // 确保不是最后一个有效的item
    if (index < _postPluginItems.count() - 1)
    {
        _postPluginListLayout->removeWidget(item);
        _postPluginItems.removeOne(item);

        _postPluginListLayout->insertWidget(index + 1, item);
        _postPluginItems.insert(index + 1, item);

        _updatePostMoveButtonStates();
    }
}

void PluginSettingsPage::_onPreMoveUp(PluginItemWidget* item)
{
    int index = _prePluginListLayout->indexOf(item);
    if (index > 0) // 确保不是第一个
    {
        // 从布局和列表中移除
        _prePluginListLayout->removeWidget(item);
        _prePluginItems.removeOne(item);

        // 插入到新位置
        _prePluginListLayout->insertWidget(index - 1, item);
        _prePluginItems.insert(index - 1, item);

        _updatePreMoveButtonStates();
    }
}

void PluginSettingsPage::_onPreMoveDown(PluginItemWidget* item)
{
    int index = _prePluginListLayout->indexOf(item);
    // 确保不是最后一个有效的item
    if (index < _prePluginItems.count() - 1)
    {
        _prePluginListLayout->removeWidget(item);
        _prePluginItems.removeOne(item);

        _prePluginListLayout->insertWidget(index + 1, item);
        _prePluginItems.insert(index + 1, item);

        _updatePreMoveButtonStates();
    }
}

void PluginSettingsPage::_updatePostMoveButtonStates()
{
    for (int i = 0; i < _postPluginItems.count(); ++i)
    {
        PluginItemWidget* item = _postPluginItems.at(i);
        // 第一个不能上移
        item->setMoveUpButtonEnabled(i > 0);
        // 最后一个不能下移
        item->setMoveDownButtonEnabled(i < _postPluginItems.count() - 1);
    }
}

void PluginSettingsPage::_updatePreMoveButtonStates()
{
    for (int i = 0; i < _prePluginItems.count(); ++i)
    {
        PluginItemWidget* item = _prePluginItems.at(i);
        // 第一个不能上移
        item->setMoveUpButtonEnabled(i > 0);
        // 最后一个不能下移
        item->setMoveDownButtonEnabled(i < _prePluginItems.count() - 1);
    }
}
