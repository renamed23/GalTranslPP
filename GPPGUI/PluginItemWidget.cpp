// PluginItemWidget.cpp

#include "PluginItemWidget.h"

#include <QHBoxLayout>
#include <QMap>
#include "ElaText.h"
#include "ElaToggleSwitch.h"
#include "ElaToolTip.h"
#include "ElaIconButton.h"
#include "ElaDoubleText.h"

PluginItemWidget::PluginItemWidget(const QString& pluginName, QWidget* parent)
    : ElaScrollPageArea(parent)
{
    static const QMap<QString, QString> toolTipMap =
    {
        { "SkipTrans", tr("滤过插件，默认开启为 run 阶段") },
        { "TextPostFull2Half", tr("全角半角转换插件，默认开启为 postRun 阶段") },
        { "TextLinebreakFix", tr("换行修复插件，默认开启为 run 阶段") },
    };
    // 主水平布局
    QHBoxLayout* mainLayout = new QHBoxLayout(this);

    // 插件名称
    _pluginNameLabel = new ElaDoubleText(this,
        pluginName, 16, toolTipMap[pluginName], 10, "");

    // 新增设置按钮
    _settingsButton = new ElaIconButton(ElaIconType::Gear, this);
    connect(_settingsButton, &ElaIconButton::clicked, this, [=]() {
        Q_EMIT settingsRequested(this);
        });

    // 启用/禁用开关
    _enableSwitch = new ElaToggleSwitch(this);
    _enableSwitch->setIsToggled(false);

    // 上移按钮
    _moveUpButton = new ElaIconButton(ElaIconType::AngleUp, this);
    connect(_moveUpButton, &ElaIconButton::clicked, this, [=]() {
        Q_EMIT moveUpRequested(this);
        });

    // 下移按钮
    _moveDownButton = new ElaIconButton(ElaIconType::AngleDown, this);
    connect(_moveDownButton, &ElaIconButton::clicked, this, [=]() {
        Q_EMIT moveDownRequested(this);
        });

    // 组合布局
    mainLayout->addWidget(_pluginNameLabel);
    mainLayout->addStretch();
    mainLayout->addWidget(_enableSwitch);
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
    return _pluginNameLabel->getFirstLineText();
}

bool PluginItemWidget::isToggled() const
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
