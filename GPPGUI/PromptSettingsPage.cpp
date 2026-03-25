#include "PromptSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStackedWidget>

#include "ElaLineEdit.h"
#include "ElaScrollPageArea.h"
#include "ElaTabWidget.h"
#include "ElaPushButton.h"
#include "ElaMessageBar.h"
#include "ElaPlainTextEdit.h"

import Tool;

PromptSettingsPage::PromptSettingsPage(fs::path& projectDir, toml::ordered_value& projectConfig, QWidget* parent) :
	BasePage(parent), _projectConfig(projectConfig), _projectDir(projectDir)
{
	setWindowTitle(tr("项目提示词设置"));
	setTitleVisible(false);

	if (fs::exists(_projectDir / L"Prompt.toml")) {
		try {
			_promptConfig = toml::uoparse(_projectDir / L"Prompt.toml");
		}
		catch (...) {
			ElaMessageBar::error(ElaMessageBarType::TopRight, tr("解析失败"), tr("项目 ") +
				QString(_projectDir.filename().wstring()) + tr(" 的提示词配置文件不符合标准。"), 3000);
		}
	}
	else if (fs::exists(defaultPromptPath)) {
		try {
			_promptConfig = toml::uoparse(_projectDir / L"Prompt.toml");
		}
		catch (...) {
			ElaMessageBar::error(ElaMessageBarType::TopRight, tr("解析失败"), tr("默认提示词文件不符合 toml 规范"), 3000);
		}
	}
	else {
		ElaMessageBar::error(ElaMessageBarType::TopRight, tr("解析失败"), tr("找不到提示词文件"), 3000);
	}
	
	_setupUI();
}

PromptSettingsPage::~PromptSettingsPage()
{

}


void PromptSettingsPage::_setupUI()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(10, 10, 10, 0);

	ElaTabWidget* tabWidget = new ElaTabWidget(mainWidget);
	tabWidget->setTabsClosable(false);
	tabWidget->setIsTabTransparent(true);


	auto createPromptWidgetFunc =
		[=](const QString& promptName, const std::string& userPromptKey, const std::string& systemPromptKey) -> std::function<void()>
		{
			QWidget* promptWidget = new QWidget(mainWidget);
			QVBoxLayout* promptLayout = new QVBoxLayout(promptWidget);
			promptLayout->setContentsMargins(0, 0, 0, 0);
			
			QHBoxLayout* promptButtonLayout = new QHBoxLayout(promptWidget);
			ElaPushButton* promptUserModeButtom = new ElaPushButton(promptWidget);
			promptUserModeButtom->setText(tr("用户提示词"));
			promptUserModeButtom->setEnabled(false);
			ElaPushButton* promptSystemModeButtom = new ElaPushButton(promptWidget);
			promptSystemModeButtom->setText(tr("系统提示词"));
			promptSystemModeButtom->setEnabled(true);
			promptButtonLayout->addWidget(promptUserModeButtom);
			promptButtonLayout->addWidget(promptSystemModeButtom);
			promptButtonLayout->addStretch();
			promptLayout->addLayout(promptButtonLayout);

			QStackedWidget* promptStackedWidget = new QStackedWidget(promptWidget);
			// 用户提示词
			ElaPlainTextEdit* promptUserModeEdit = new ElaPlainTextEdit(promptStackedWidget);
			QFont plainTextFont = promptUserModeEdit->font();
			plainTextFont.setPixelSize(15);
			promptUserModeEdit->setFont(plainTextFont);
			promptUserModeEdit->setPlainText(
				QString::fromStdString(toml::find_or(_promptConfig, userPromptKey, ""))
			);
			promptStackedWidget->addWidget(promptUserModeEdit);
			// 系统提示词
			ElaPlainTextEdit* forgalJsonSystemModeEdit = new ElaPlainTextEdit(promptStackedWidget);
			forgalJsonSystemModeEdit->setFont(plainTextFont);
			forgalJsonSystemModeEdit->setPlainText(
				QString::fromStdString(toml::find_or(_promptConfig, systemPromptKey, ""))
			);
			promptStackedWidget->addWidget(forgalJsonSystemModeEdit);
			promptStackedWidget->setCurrentIndex(0);
			promptLayout->addWidget(promptStackedWidget);
			connect(promptUserModeButtom, &ElaPushButton::clicked, this, [=]()
				{
					promptStackedWidget->setCurrentIndex(0);
					promptUserModeButtom->setEnabled(false);
					promptSystemModeButtom->setEnabled(true);
				});
			connect(promptSystemModeButtom, &ElaPushButton::clicked, this, [=]()
				{
					promptStackedWidget->setCurrentIndex(1);
					promptUserModeButtom->setEnabled(true);
					promptSystemModeButtom->setEnabled(false);
				});
			tabWidget->addTab(promptWidget, promptName);

			auto result = [=]()
				{
					toml::ordered_value userPromptVal = promptUserModeEdit->toPlainText().toStdString();
					toml::ordered_value systemPromptVal = forgalJsonSystemModeEdit->toPlainText().toStdString();
					userPromptVal.as_string_fmt().fmt = toml::string_format::multiline_basic;
					systemPromptVal.as_string_fmt().fmt = toml::string_format::multiline_basic;
					insertToml(_promptConfig, userPromptKey, userPromptVal);
					insertToml(_promptConfig, systemPromptKey, systemPromptVal);
				};
			return result;
		};

	auto forgalJsonApplyFunc = createPromptWidgetFunc("ForGalJson", "FORGALJSON_TRANS_PROMPT_EN", "FORGALJSON_SYSTEM");
	auto forgalTsvApplyFunc = createPromptWidgetFunc("ForGalTsv", "FORGALTSV_TRANS_PROMPT_EN", "FORGALTSV_SYSTEM");
	auto forNovelTsvApplyFunc = createPromptWidgetFunc("ForNovelTsv", "FORNOVELTSV_TRANS_PROMPT_EN", "FORNOVELTSV_SYSTEM");
	auto sakuraApplyFunc = createPromptWidgetFunc("Sakura", "SAKURA_TRANS_PROMPT", "SAKURA_SYSTEM_PROMPT");
	auto gendictApplyFunc = createPromptWidgetFunc("GenDict", "GENDIC_PROMPT", "GENDIC_SYSTEM");
	auto nametransApplyFunc = createPromptWidgetFunc("NameTrans", "NAMETRANS_PROMPT", "NAMETRANS_SYSTEM");

	_applyFunc = [=]()
		{
			forgalJsonApplyFunc();
			forgalTsvApplyFunc();
			forNovelTsvApplyFunc();
			sakuraApplyFunc();
			gendictApplyFunc();
			nametransApplyFunc();
			std::ofstream ofs(_projectDir / L"Prompt.toml", std::ios::binary);
			ofs << _promptConfig;
			ofs.close();
		};

	mainLayout->addWidget(tabWidget);
	addCentralWidget(mainWidget, true, false, 0);
}
