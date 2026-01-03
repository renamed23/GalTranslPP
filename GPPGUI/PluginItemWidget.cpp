// PluginItemWidget.cpp

#include "PluginItemWidget.h"

#include <QHBoxLayout>
#include <QMap>
#include "ElaText.h"
#include "ElaToggleSwitch.h"
#include "ElaToolTip.h"
#include "ElaIconButton.h"
#include "ElaDoubleText.h"
#include "ElaComboBox.h"

PluginItemWidget::PluginItemWidget(const QString& pluginName, const QString& runTimeStr, QWidget* parent)
    : ElaScrollPageArea(parent)
{
    const QMap<QString, QString> toolTipMap =
    {
        { "SkipTrans", tr("滤过插件") },
        { "TextFull2Half", tr("全角半角转换插件") },
        { "TextLinebreakFix", tr("换行修复插件") },
    };
    const QMap<QString, QStringList> boxItemMap =
    {
        { "SkipTrans", { "dprerun", "prerun" } },
        { "TextFull2Half", { "dprerun", "prerun", "postrun", "dpostrun" } },
        { "TextLinebreakFix", { "dpostrun" } },
    };
    // 主水平布局
    QHBoxLayout* mainLayout = new QHBoxLayout(this);

    // 插件名称
    _pluginNameLabel = new ElaDoubleText(this,
        pluginName, 16, toolTipMap[pluginName], 10, "");

    _pluginRunTimeBox = new ElaComboBox(this);
    _pluginRunTimeBox->setFixedWidth(150);
    _pluginRunTimeBox->addItems(boxItemMap[pluginName]);
    _pluginRunTimeBox->setCurrentText(runTimeStr);

    // 新增设置按钮
    _settingsButton = new ElaIconButton(ElaIconType::Gear, this);
    connect(_settingsButton, &ElaIconButton::clicked, this, [=]() 
        {
            Q_EMIT settingsRequested(this);
        });

    // 启用/禁用开关
    _enableSwitch = new ElaToggleSwitch(this);
    _enableSwitch->setIsToggled(false);

    // 上移按钮
    _moveUpButton = new ElaIconButton(ElaIconType::AngleUp, this);
    connect(_moveUpButton, &ElaIconButton::clicked, this, [=]() 
        {
            Q_EMIT moveUpRequested(this);
        });

    // 下移按钮
    _moveDownButton = new ElaIconButton(ElaIconType::AngleDown, this);
    connect(_moveDownButton, &ElaIconButton::clicked, this, [=]() 
        {
            Q_EMIT moveDownRequested(this);
        });

    // 组合布局
    mainLayout->addWidget(_pluginNameLabel);
    mainLayout->addStretch();
    mainLayout->addWidget(_enableSwitch);
    mainLayout->addWidget(_pluginRunTimeBox);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(_settingsButton);
    mainLayout->addWidget(_moveUpButton);
    mainLayout->addWidget(_moveDownButton);
}

PluginItemWidget::~PluginItemWidget()
{
}

QString PluginItemWidget::getPluginName() const
{
    return _pluginRunTimeBox->currentText() + ":" + _pluginNameLabel->getFirstLineText();
}

bool PluginItemWidget::getIsToggled() const
{
    return _enableSwitch->getIsToggled();
}

void PluginItemWidget::setIsToggled(bool enabled)
{
    _enableSwitch->setIsToggled(enabled);
}

void PluginItemWidget::setMoveUpButtonEnabled(bool enabled)
{
    _moveUpButton->setEnabled(enabled);
}

void PluginItemWidget::setMoveDownButtonEnabled(bool enabled)
{
    _moveDownButton->setEnabled(enabled);
}
