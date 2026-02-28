#include "UpdateWidget.h"

#include <QVBoxLayout>
#include "ElaText.h"

import Tool;

UpdateWidget::UpdateWidget(QWidget* parent)
    : QWidget{parent}
{
    setMinimumSize(200, 260);
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSizeConstraint(QLayout::SetMaximumSize);
    mainLayout->setContentsMargins(5, 10, 5, 5);
    mainLayout->setSpacing(4);

    ElaText* updateTitle = new ElaText("v" + QString::fromStdString(GPPVERSION) + " 更新", 15, this);
    QStringList updateList = {
        "1. 新增模型层面的问题检测，可通过 Prompt 让模型在翻译可疑句子时在译文结果前加上\"(GPPCProblem:xxx)\"格式的句子来让模型自己添加问题",
		"2. TextFull2Half 新增设置项: 不转换的字符，并且在 postRun 版中会跳过 (Failed to translate) 标志",
    	    "3. 新增清除窗口日志的快捷键及右键菜单，默认为 Ctrl+L",
    	    "4. 新增 页面切换特效 设置",
		"5. 修复了几个调用 Python 脚本时错误的异常处理",
		"6. 提高了一些性能表现",
		"7. 重写了 GUI 日志输出的回看逻辑",


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
