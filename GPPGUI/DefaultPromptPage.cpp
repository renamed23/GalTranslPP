#include "DefaultPromptPage.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

#include "ElaPushButton.h"
#include "ElaFlowLayout.h"
#include "ElaPlainTextEdit.h"
#include "ElaMenu.h"
#include "ElaMessageBar.h"
#include "ElaTabWidget.h"

import Tool;
namespace fs = std::filesystem;

DefaultPromptPage::DefaultPromptPage(QWidget* parent)
    : BasePage(parent)
{
    setWindowTitle(tr("默认提示词管理"));
    setTitleVisible(false);

	if (fs::exists(defaultPromptPath)) {
		try {
			_promptConfig = toml::uoparse(defaultPromptPath);
		}
		catch (...) {
			ElaMessageBar::error(ElaMessageBarType::TopRight, tr("解析失败"), tr("默认提示词配置文件不符合 toml 规范"), 3000);
		}
	}

    _setupUI();
}

DefaultPromptPage::~DefaultPromptPage()
{
}

void DefaultPromptPage::_setupUI()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(10, 20, 10, 0);

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
			ElaPushButton* promptSaveAllButton = new ElaPushButton(promptWidget);
			promptSaveAllButton->setText(tr("全部保存"));
			ElaPushButton* promptSaveButton = new ElaPushButton(promptWidget);
			promptSaveButton->setText(tr("保存"));
			promptButtonLayout->addWidget(promptUserModeButtom);
			promptButtonLayout->addWidget(promptSystemModeButtom);
			promptButtonLayout->addStretch();
			promptButtonLayout->addWidget(promptSaveAllButton);
			promptButtonLayout->addWidget(promptSaveButton);
			promptLayout->addLayout(promptButtonLayout);

			QStackedWidget* promptStackedWidget = new QStackedWidget(promptWidget);
			// 用户提示词
			ElaPlainTextEdit* promptUserModeEdit = new ElaPlainTextEdit(promptStackedWidget);
			QFont plainTextFont = promptUserModeEdit->font();
			plainTextFont.setPixelSize(15);
			promptUserModeEdit->setFont(plainTextFont);
			promptUserModeEdit->setPlainText(
				QString::fromStdString(toml::find_or(_promptConfig, userPromptKey, std::string{}))
			);
			promptStackedWidget->addWidget(promptUserModeEdit);
			// 系统提示词
			ElaPlainTextEdit* promptSystemModeEdit = new ElaPlainTextEdit(promptStackedWidget);
			promptSystemModeEdit->setFont(plainTextFont);
			promptSystemModeEdit->setPlainText(
				QString::fromStdString(toml::find_or(_promptConfig, systemPromptKey, std::string{}))
			);
			promptStackedWidget->addWidget(promptSystemModeEdit);
			promptStackedWidget->setCurrentIndex(0);
			promptLayout->addWidget(promptStackedWidget, 1);
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
			connect(promptSaveAllButton, &ElaPushButton::clicked, this, [=]()
				{
					this->apply2Config();
					ElaMessageBar::success(ElaMessageBarType::TopRight, tr("保存成功"), tr("所有默认提示词配置已保存。"), 3000);
				});

			auto resultApply2ConfigFunc = [=]()
				{
					toml::ordered_value userPromptVal = promptUserModeEdit->toPlainText().toStdString();
					toml::ordered_value systemPromptVal = promptSystemModeEdit->toPlainText().toStdString();
					userPromptVal.as_string_fmt().fmt = toml::string_format::multiline_basic;
					systemPromptVal.as_string_fmt().fmt = toml::string_format::multiline_basic;
					insertToml(_promptConfig, userPromptKey, userPromptVal);
					insertToml(_promptConfig, systemPromptKey, systemPromptVal);
				};

			connect(promptSaveButton, &ElaPushButton::clicked, this, [=]()
				{
					resultApply2ConfigFunc();
					std::ofstream ofs(defaultPromptPath, std::ios::binary);
					ofs << _promptConfig;
					ofs.close();
					ElaMessageBar::success(ElaMessageBarType::TopRight, tr("保存成功"), tr("默认 ") + promptName + tr(" 提示词配置已保存。"), 3000);
				});
			tabWidget->addTab(promptWidget, promptName);

			return resultApply2ConfigFunc;
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
			std::ofstream ofs(defaultPromptPath, std::ios::binary);
			ofs << _promptConfig;
			ofs.close();
		};

	mainLayout->addWidget(tabWidget);
    addCentralWidget(mainWidget, true, false, 0);
}
