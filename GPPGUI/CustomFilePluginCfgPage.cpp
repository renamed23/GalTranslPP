#include "CustomFilePluginCfgPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDesktopServices>

#include "ElaScrollPageArea.h"
#include "ElaComboBox.h"
#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"

import Tool;

CustomFilePluginCfgPage::CustomFilePluginCfgPage(toml::ordered_value& projectConfig, toml::ordered_value& globalConfig, QWidget* parent) 
	: BasePage(parent), _globalConfig(globalConfig), _projectConfig(projectConfig)
{
	setWindowTitle(tr("自定义文件处理插件配置"));
	setContentsMargins(10, 0, 10, 0);

	// 创建一个中心部件和布局
	QWidget* centerWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

	// 创建自定义文件处理插件路径设置
	const std::string& customFilePluginPath = toml::find_or(_projectConfig, "plugins", "customFilePlugin", "Lua/MySampleFilePlugin.lua");
	ElaScrollPageArea* filePluginArea = new ElaScrollPageArea(this);
	QHBoxLayout* filePluginLayout = new QHBoxLayout(filePluginArea);
	ElaText* filePluginText = new ElaText(tr("自定义文件处理插件路径"), 16, filePluginArea);
	filePluginText->setWordWrap(false);
	filePluginLayout->addWidget(filePluginText);
	ElaLineEdit* filePluginEdit = new ElaLineEdit(filePluginArea);
	filePluginEdit->setText(QString::fromStdString(customFilePluginPath));
	filePluginLayout->addWidget(filePluginEdit);
	ElaPushButton* filePluginBtn = new ElaPushButton(tr("浏览"), filePluginArea);
	connect(filePluginBtn, &ElaPushButton::clicked, this, [=]()
		{
			QString path = QFileDialog::getOpenFileName(this, tr("选择自定义文件处理插件"), 
				QString::fromStdString(toml::find_or(_globalConfig, "lastPluginPath", "./")), 
				"custom script (*.lua *.py)");
			if (!path.isEmpty())
			{
				filePluginEdit->setText(path);
				insertToml(_globalConfig, "lastPluginPath", path.toStdString());
			}
		});
	filePluginLayout->addWidget(filePluginBtn);
	mainLayout->addWidget(filePluginArea);

	// 继承的基类 # NormalJson, Epub, PDF
	const std::string& inherit = toml::find_or(_projectConfig, "plugins", "baseClassName", "NormalJson");
	ElaScrollPageArea* inheritArea = new ElaScrollPageArea(this);
	QHBoxLayout* inheritLayout = new QHBoxLayout(inheritArea);
	ElaText* inheritText = new ElaText(tr("基类继承"), 16, inheritArea);
	inheritLayout->addWidget(inheritText);
	ElaComboBox* inheritCombo = new ElaComboBox(inheritArea);
	inheritCombo->setFixedWidth(150);
	inheritCombo->addItem("NormalJson");
	inheritCombo->addItem("Epub");
	inheritCombo->addItem("PDF");
	if (!inherit.empty()) {
		QString inheritStr = QString::fromStdString(inherit);
		int index = inheritCombo->findText(inheritStr);
		if (index >= 0) {
			inheritCombo->setCurrentIndex(index);
		}
	}
	inheritLayout->addWidget(inheritCombo);
	mainLayout->addWidget(inheritArea);

	_applyFunc = [=]()
		{
			insertToml(_projectConfig, "plugins.customFilePlugin", filePluginEdit->text().toStdString());
			insertToml(_projectConfig, "plugins.baseClassName", inheritCombo->currentText().toStdString());
		};

	mainLayout->addStretch();
	centerWidget->setWindowTitle(tr("自定义文件处理插件配置"));
	addCentralWidget(centerWidget, true, true, 0);
}

CustomFilePluginCfgPage::~CustomFilePluginCfgPage()
{

}
