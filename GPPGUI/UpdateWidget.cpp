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
        "1. 修复了 分割缓存查找距离 选项不生效的 bug",
        "v2.2.1略过",
        "v2.2.0更新: ",
        "0. 【重要！】修改/增加了 GenDict 和 NameTrans 这两个模式的提示词解析方式，请注意为自己已有项目的提示词进行更新，避免解析失败",
        "1. 新增保留原 json 中的其它元数据",
        "2. 字典未使用检查所使用的分词也会被缓存了",
        "3. 给 background 设定了长度上限，避免 ai 持续抽风导致无法进行翻译",
        "4. 新增翻译模式 NameTrans",
        "5. 新增功能 导入翻译问题概览至缓存（在其它设置里）",
        "6. 优化 GUI log 输出颜色",
        "7. API 调用现在会检测系统代理，如果有设置则自动使用",
        "8. 新增设置项 分割缓存查找距离 以规范单文件分割翻译时读取缓存的贪婪程度",
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
