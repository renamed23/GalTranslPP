#include "UpdateWidget.h"

#include <QVBoxLayout>
#include "ElaText.h"

import Tool;

UpdateWidget::UpdateWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(200, 260);
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSizeConstraint(QLayout::SetMaximumSize);
    mainLayout->setContentsMargins(5, 10, 5, 5);
    mainLayout->setSpacing(4);

    ElaText* updateTitle = new ElaText("v" + QString::fromStdString(GPPVERSION) + " 更新", 15, this);
    QStringList updateList = {
        "1. 修正『翻译为空』问题的触发条件",
		"2. 添加翻译设置: 解析不完整时重翻整段而非仅重翻漏掉的部分，增加模型因串行而导致解析失败时的容错，默认关闭以节省token/防止因模型截断造成无限循环",
		"3. [GUI] 添加日志设置: log 文件大小限制 && log 文件滚动数量上限",
		"4. [GUI] 优化界面布局",

    };

    mainLayout->addWidget(updateTitle);
    for (const auto& str : updateList) {
        ElaText* updateItem = new ElaText(str, 13, this);
        updateItem->setIsWrapAnywhere(true);
        mainLayout->addWidget(updateItem);
    }
    
    mainLayout->addStretch();
}

UpdateWidget::~UpdateWidget()
{

}
