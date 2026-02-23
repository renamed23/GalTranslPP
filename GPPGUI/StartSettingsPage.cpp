#include "StartSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QFileDialog>
#include <QScrollBar>
#include <QTextBlock>
#include <QDesktopServices>

#include "ElaText.h"
#include "ElaScrollPageArea.h"
#include "ElaToolTip.h"
#include "ElaPlainTextEdit.h"
#include "ElaPushButton.h"
#include "ElaProgressRing.h"
#include "NoWheelComboBox.h"
#include "ElaMessageBar.h"
#include "ElaLCDNumber.h"
#include "ElaProgressBar.h"

#include "NJCfgPage.h"
#include "EpubCfgPage.h"
#include "PDFCfgPage.h"
#include "CustomFilePluginCfgPage.h"

import Tool;

StartSettingsPage::StartSettingsPage(QWidget* mainWindow, fs::path& projectDir, toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent) :
	BasePage(parent), _projectConfig(projectConfig), _globalConfig(globalConfig), _projectDir(projectDir), _mainWindow(mainWindow)
{
	setWindowTitle(tr("启动设置"));
	setTitleVisible(false);

	_trayIcon = new QSystemTrayIcon(this);
	_trayIcon->setIcon(QIcon(":/GPPGUI/Resource/Image/julixian_s.ico"));
	connect(_trayIcon, &QSystemTrayIcon::messageClicked, this, [=]()
		{
			QUrl dirUrl = QUrl::fromLocalFile(QString(_projectDir.wstring()));
			QDesktopServices::openUrl(dirUrl);
		});

	_setupUI();
}

StartSettingsPage::~StartSettingsPage()
{
	_trayIcon = nullptr;
	if (_workThread && _workThread->isRunning()) {
		_worker->stopTranslation();
		_workThread->quit();
		_workThread->wait();
	}
}

void StartSettingsPage::apply2Config()
{
	_njCfgPage->apply2Config();
	_epubCfgPage->apply2Config();
	_pdfCfgPage->apply2Config();
	_customFilePluginCfgPage->apply2Config();
	if (_applyFunc) {
		_applyFunc();
	}
}

void StartSettingsPage::clearLog() {
	_logOutput->clear();
}

void StartSettingsPage::_setupUI()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

	QWidget* topWidget = new QWidget(mainWidget);
	QHBoxLayout* topLayout = new QHBoxLayout(topWidget);

	// 日志输出
	_logOutput = new ElaPlainTextEdit(topWidget);
	_logOutput->setReadOnly(true);
	QFont font = _logOutput->font();
	font.setPixelSize(14);
	_logOutput->setFont(font);
	_logOutput->setPlaceholderText(tr("日志输出"));
	topLayout->addWidget(_logOutput);


	ElaScrollPageArea* buttonArea = new ElaScrollPageArea(mainWidget);
	buttonArea->setFixedWidth(200);
	buttonArea->setMaximumHeight(600);
	QVBoxLayout* buttonLayout = new QVBoxLayout(buttonArea);

	// 文件格式
	std::string filePlugin = toml::find_or(_projectConfig, "plugins", "filePlugin", "NormalJson");
	QString filePluginStr = QString::fromStdString(filePlugin);
	ElaText* fileFormatLabel = new ElaText(buttonArea);
	fileFormatLabel->setTextPixelSize(16);
	fileFormatLabel->setText(tr("文件格式:"));
	buttonLayout->addWidget(fileFormatLabel);
	_fileFormatComboBox = new NoWheelComboBox(buttonArea);
	_fileFormatComboBox->addItem("NormalJson");
	_fileFormatComboBox->addItem("Epub");
	_fileFormatComboBox->addItem("PDF");
	_fileFormatComboBox->addItem("Custom");
	if (!filePluginStr.isEmpty()) {
		if (filePluginStr.toLower().endsWith(".lua") || filePluginStr.toLower().endsWith(".py")) {
			_fileFormatComboBox->setCurrentIndex(3);
		}
		else {
			int index = _fileFormatComboBox->findText(filePluginStr);
			if (index >= 0) {
				_fileFormatComboBox->setCurrentIndex(index);
			}
		}
	}
	buttonLayout->addWidget(_fileFormatComboBox);

	// 针对文件格式的输出设置
	ElaPushButton* outputSetting = new ElaPushButton(buttonArea);
	outputSetting->setText(tr("文件处理器设置"));
	buttonLayout->addWidget(outputSetting);
	connect(outputSetting, &ElaPushButton::clicked, this, &StartSettingsPage::_onOutputSettingClicked);

	// 线程数
	ElaText* threadNumLabel = new ElaText(buttonArea);
	threadNumLabel->setTextPixelSize(16);
	threadNumLabel->setText(tr("工作线程数:"));
	buttonLayout->addWidget(threadNumLabel);
	QWidget* threadNumWidget = new QWidget(buttonArea);
	QHBoxLayout* threadNumLayout = new QHBoxLayout(threadNumWidget);
	_threadNumRing = new ElaProgressRing(buttonArea);
	threadNumLayout->addWidget(_threadNumRing);
	ElaText* speedLabel = new ElaText(buttonArea);
	speedLabel->setTextPixelSize(12);
	speedLabel->setText("0 lines/s");
	threadNumLayout->addWidget(speedLabel);
	buttonLayout->addWidget(threadNumWidget);

	// 已用时间
	ElaText* usedTimeLabelText = new ElaText(buttonArea);
	usedTimeLabelText->setTextPixelSize(14);
	usedTimeLabelText->setText(tr("已用时间:"));
	buttonLayout->addWidget(usedTimeLabelText);
	_usedTimeLabel = new ElaLCDNumber(buttonArea);
	_usedTimeLabel->display("00:00:00");
	buttonLayout->addWidget(_usedTimeLabel);

	// 剩余时间
	ElaText* remainTimeLabelText = new ElaText(buttonArea);
	remainTimeLabelText->setTextPixelSize(14);
	remainTimeLabelText->setText(tr("剩余时间:"));
	buttonLayout->addWidget(remainTimeLabelText);
	_remainTimeLabel = new ElaLCDNumber(buttonArea);
	_remainTimeLabel->display("00:00:00");
	buttonLayout->addWidget(_remainTimeLabel);

	// 翻译模式
	std::string transEngine = toml::find_or(_projectConfig, "plugins", "transEngine", "ForGalJson");
	QString transEngineStr = QString::fromStdString(transEngine);
	ElaText* translateModeLabel = new ElaText(buttonArea);
	translateModeLabel->setTextPixelSize(16);
	translateModeLabel->setText(tr("翻译模式:"));
	buttonLayout->addWidget(translateModeLabel);
	ElaComboBox* translateMode = new NoWheelComboBox(buttonArea);
	// 不要被鼠标滚轮的滚动改变选项
	translateMode->addItem("ForGalJson");
	translateMode->addItem("ForGalTsv");
	translateMode->addItem("ForNovelTsv");
	translateMode->addItem("Sakura");
	translateMode->addItem("DumpName");
	translateMode->addItem("NameTrans");
	translateMode->addItem("GenDict");
	translateMode->addItem("Rebuild");
	translateMode->addItem("ShowNormal");
	if (!transEngineStr.isEmpty()) {
		int index = translateMode->findText(transEngineStr);
		if (index >= 0) {
			translateMode->setCurrentIndex(index);
		}
	}
	buttonLayout->addWidget(translateMode);

	// 开始翻译
	_startTranslateButton = new ElaPushButton(buttonArea);
	_startTranslateButton->setText(tr("开始翻译"));
	connect(_startTranslateButton, &ElaPushButton::clicked, this, &StartSettingsPage::_onStartTranslatingClicked);
	connect(_startTranslateButton, &ElaPushButton::clicked, this, [=]()
		{
			QScrollBar* scrollBar = _logOutput->verticalScrollBar();
			bool scrollIsAtBottom = (scrollBar->value() == scrollBar->maximum());
			QTextCursor tempCursor(_logOutput->document());
			tempCursor.movePosition(QTextCursor::End);
			tempCursor.insertText("\n\n");
			if (scrollIsAtBottom) {
				scrollBar->setValue(scrollBar->maximum());
			}
		});
	buttonLayout->addWidget(_startTranslateButton);

	// 停止翻译
	_stopTranslateButton = new ElaPushButton(buttonArea);
	_stopTranslateButton->setText(tr("停止翻译"));
	_stopTranslateButton->setEnabled(false);
	connect(_stopTranslateButton, &ElaPushButton::clicked, this, &StartSettingsPage::_onStopTranslatingClicked);
	buttonLayout->addWidget(_stopTranslateButton);
	topLayout->addWidget(buttonArea);

	mainLayout->addWidget(topWidget);


	// 进度条
	_progressBar = new ElaProgressBar(mainWidget);
	_progressBar->setRange(0, 100);
	_progressBar->setValue(0);
	mainLayout->addWidget(_progressBar);


	// 翻译线程
	_workThread = new QThread(this);
	_worker = new TranslatorWorker(_projectDir);
	_worker->moveToThread(_workThread);
	connect(_workThread, &QThread::finished, _worker, &TranslatorWorker::deleteLater);
	connect(this, &StartSettingsPage::startWork, _worker, &TranslatorWorker::doTranslation);
	connect(_worker, &TranslatorWorker::translationFinished, this, &StartSettingsPage::_workFinished);

	connect(_worker, &TranslatorWorker::makeBarSignal, this, [=](int totalSentences, int totalThreads)
		{
			_progressBar->setRange(0, totalSentences);
			_progressBar->setValue(0);
			_threadNumRing->setRange(0, totalThreads);
			_threadNumRing->setValue(0);
			_progressBar->setFormat("%v/%m lines [%p%]");
			_startTime = std::chrono::high_resolution_clock::now();
			_usedTimeLabel->display("00:00:00");
			_remainTimeLabel->display("--:--");
			_estimator.reset();
			
		});
	connect(_worker, &TranslatorWorker::writeLogSignal, this, [=](const QString& log)
		{
			constexpr int MAX_LOG_LINE_COUNT = 10000;

			// 禁用更新可以防止在进行大量操作时出现闪烁，并提高性能
			_logOutput->setUpdatesEnabled(false);

			// 1. 智能滚动判断: 记住修改前的位置
			QScrollBar* scrollBar = _logOutput->verticalScrollBar();
			bool scrollIsAtBottom = (scrollBar->value() >= scrollBar->maximum() - 4); // 减去一个小的容差

			// 如果用户没有滚动到底部，我们就需要“锚定”他当前看到的视图
			int firstVisibleBlockNumber = -1;
			if (!scrollIsAtBottom) {
				// 获取视口左上角位置的文本光标，从而得到当前可见的第一个文本块
				QTextCursor firstVisibleCursor = _logOutput->cursorForPosition(QPoint(0, 0));
				firstVisibleBlockNumber = firstVisibleCursor.blockNumber();
			}

			// 2. 追加日志 (使用一个临时的“影子”光标在后台进行操作)
			QTextCursor tempCursor(_logOutput->document());
			tempCursor.movePosition(QTextCursor::End);
			tempCursor.setCharFormat(QTextCharFormat());


			// --- 高亮代码逻辑开始 ---
			auto processLogFunc = [&](const QString& l)
				{
					QStringList lines = l.split('\n');
					for (int i = 0; i < lines.size(); ++i) {
						QString& line = lines[i];
						line = line.trimmed();
						// 如果是最后一行且为空（通常是因为 log 以 \n 结尾），则跳过，避免多余空行
						if (i == lines.size() - 1 && line.isEmpty()) break;
						QTextCharFormat fmt;
						// 根据 spdlog 的格式 [timestamp level] 进行判断
						// 使用 " level]" 包含右中括号和前置空格，防止匹配到日志内容中的单词
						if (line.contains(" error]")) {
							fmt.setForeground(Qt::red); // 错误：红色
						}
						else if (line.contains(" critical]")) {
							fmt.setForeground(Qt::darkRed); // 严重：深红
							fmt.setFontWeight(QFont::Bold); // 加粗
						}
						else if (line.contains(" warning]")) {
							// 警告：深橙色
							fmt.setForeground(QColor(255, 140, 0));
						}
						else if (line.contains(" debug]")) {
							fmt.setForeground(QColor(Qt::darkBlue));
						}
						else if (line.contains(" trace]")) {
							fmt.setForeground(QColor(Qt::darkGreen));
						}
						else {
							fmt.setForeground(QColor(Qt::black)); // 显式设为黑色或保持默认
						}
						// 应用格式并插入文本
						tempCursor.setCharFormat(fmt);
						tempCursor.insertText(line);
						// 补回换行符 (因为 split 去掉了换行符)
						// 如果不是最后一行，则插入换行
						if (i < lines.size() - 1) {
							tempCursor.insertText("\n");
						}
					}
				};
			if (log.contains("```\n问题概览:")) {
				QString logCopy = log;
				QString pre, overview, post;
				int index = log.indexOf("```\n问题概览:");
				pre = logCopy.left(index);
				logCopy = logCopy.mid(index);
				index = logCopy.indexOf("问题概览结束\n```");
				overview = logCopy.left(index + 10);
				logCopy = logCopy.mid(index + 10);
				post = std::move(logCopy);
				processLogFunc(pre);
				QTextCharFormat format;
				format.setForeground(QColor(255, 0, 0));
				tempCursor.setCharFormat(format);
				tempCursor.insertText(overview);
				processLogFunc(post);
			}
			else {
				if (log.length() > 512 * 3 || log.count('\n') > 30) {
					tempCursor.insertText(log);
				}
				else {
					processLogFunc(log);
				}
			}
			// --- 高亮代码逻辑结束 ---


			// 3. 清理旧日志
			int toRemoveLineCount = 0;
			int currentLineCount = _logOutput->document()->lineCount();
			if (currentLineCount > MAX_LOG_LINE_COUNT) {
				toRemoveLineCount = currentLineCount - MAX_LOG_LINE_COUNT;

				QTextCursor deleteCursor(_logOutput->document());
				deleteCursor.movePosition(QTextCursor::Start);
				// 选中要删除的行数
				deleteCursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, toRemoveLineCount);
				deleteCursor.removeSelectedText();
			}

			// 4. 恢复视口: 这才是关键！
			if (scrollIsAtBottom) {
				// 如果原来就在底部，操作完成后依然滚动到底部
				scrollBar->setValue(scrollBar->maximum());
			}
			else if (toRemoveLineCount > 0 && firstVisibleBlockNumber != -1) {
				// 如果我们删除了旧日志，并且之前已经锚定了视口
				// 计算出我们锚定的那个文本块现在的新行号
				int newTargetBlockNumber = firstVisibleBlockNumber - toRemoveLineCount;
				if (newTargetBlockNumber < 0) newTargetBlockNumber = 0;

				// 找到这个新的文本块
				QTextBlock targetBlock = _logOutput->document()->findBlockByNumber(newTargetBlockNumber);
				if (targetBlock.isValid()) {
					// 创建一个光标并移动到这个块的开头
					QTextCursor newCursor(targetBlock);
					_logOutput->setTextCursor(newCursor); // 将编辑器的光标设置到这里
					// 这一步是可选的，但可以确保光标行完全可见
					_logOutput->ensureCursorVisible(); 
				}
			}

			// 所有操作完成后，重新启用更新，让界面一次性刷新
			_logOutput->setUpdatesEnabled(true);
		});
	connect(_worker, &TranslatorWorker::addThreadNumSignal, this, [=]()
		{
			_threadNumRing->setValue(_threadNumRing->getValue() + 1);
		});
	connect(_worker, &TranslatorWorker::reduceThreadNumSignal, this, [=]()
		{
			_threadNumRing->setValue(_threadNumRing->getValue() - 1);
		});
	connect(_worker, &TranslatorWorker::updateBarSignal, this, [=](int ticks)
		{
			_progressBar->setValue(_progressBar->value() + ticks);
			std::chrono::seconds elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>
				(std::chrono::high_resolution_clock::now() - _startTime);
			_usedTimeLabel->display(QString::fromStdString(
				std::format("{:%T}", elapsedSeconds)
			));
			if (ticks <= 0) {
				return;
			}
			auto etaWithSpeed = _estimator.updateAndGetSpeedWithEta(_progressBar->value(), _progressBar->maximum());
			const double& speed = etaWithSpeed.first;
			const Duration& eta = etaWithSpeed.second;
			speedLabel->setText(QString::fromStdString(
				std::format("{:.2f} lines/s", speed == 0.0 ? (double)_progressBar->maximum() / (elapsedSeconds.count() + 1) : speed)
			));
			if (eta.count() == std::numeric_limits<double>::infinity() || std::isnan(eta.count())) {
				_remainTimeLabel->display("--:--");
				return;
			}
			_remainTimeLabel->display(QString::fromStdString(
				std::format("{:%T}", eta)
			));
		});

	_workThread->start();
	addCentralWidget(mainWidget, true, true, 0);

	_applyFunc = [=]()
		{
			if (_fileFormatComboBox->currentText() != "Custom") {
				insertToml(_projectConfig, "plugins.filePlugin", _fileFormatComboBox->currentText().toStdString());
			}
			else {
				const std::string& customFilePluginStr = toml::find_or(_projectConfig, "plugins", "customFilePlugin", "Lua/MySampleFilePlugin.lua");
				fs::path customFilePluginPath = ascii2Wide(customFilePluginStr);
				if (
					!isSameExtension(customFilePluginPath, L".lua") &&
					!isSameExtension(customFilePluginPath, L".py")
					) {
					ElaMessageBar::error(ElaMessageBarType::BottomRight, tr("文件格式错误"), tr("自定义文件插件的格式必须是 .lua 或 .py 格式。"), 3000);
				}
				insertToml(_projectConfig, "plugins.filePlugin", customFilePluginStr);
			}
			insertToml(_projectConfig, "plugins.transEngine", translateMode->currentText().toStdString());
		};

	// 顺序和_onOutputSettingClicked里的索引一致
	_njCfgPage = new NJCfgPage(_projectConfig, this);
	addCentralWidget(_njCfgPage, true, true, 0);
	_epubCfgPage = new EpubCfgPage(_projectConfig, this);
	addCentralWidget(_epubCfgPage, true, true, 0);
	_pdfCfgPage = new PDFCfgPage(_projectConfig, this);
	addCentralWidget(_pdfCfgPage, true, true, 0);
	_customFilePluginCfgPage = new CustomFilePluginCfgPage(_projectConfig, _globalConfig, this);
	addCentralWidget(_customFilePluginCfgPage, true, true, 0);
}

void StartSettingsPage::_onOutputSettingClicked()
{
	QString fileFormat = _fileFormatComboBox->currentText();
	if (fileFormat == "NormalJson") {
		this->navigation(1);
	}
	else if (fileFormat == "Epub") {
		this->navigation(2);
	}
	else if (fileFormat == "PDF") {
		this->navigation(3);
	}
	else if (fileFormat == "Custom") {
		this->navigation(4);
	}
}


// 底下的可以不用看

void StartSettingsPage::_onStartTranslatingClicked()
{
	Q_EMIT startTranslating();

	_startTime = std::chrono::high_resolution_clock::now();
	_usedTimeLabel->display("00:00:00");
	_startTranslateButton->setEnabled(false);
	_progressBar->setValue(0);

	Q_EMIT startWork();
	_transEngine = QString::fromStdString(toml::find_or(_projectConfig, "plugins", "transEngine", ""));
	_stopTranslateButton->setEnabled(true);
}

void StartSettingsPage::_onStopTranslatingClicked()
{
	_stopTranslateButton->setEnabled(false);
	_worker->stopTranslation();
	ElaMessageBar::information(ElaMessageBarType::BottomRight, tr("停止中"), tr("正在等待最后一批翻译完成，请稍候..."), 3000);
}

void StartSettingsPage::_workFinished(int exitCode)
{
	_threadNumRing->setValue(0);
	_remainTimeLabel->display("00:00:00");
	_trayIcon->show();

	switch (exitCode)
	{
	case -2:
		ElaMessageBar::error(ElaMessageBarType::BottomRight, tr("翻译失败"), tr("项目 ") + 
			QString(_projectDir.filename().wstring()) + tr(" 的翻译任务失败，请检查日志输出。"), 3000);

		// 显示通知消息
		_trayIcon->showMessage(
			tr("翻译失败"),                  // 标题
				tr("项目 ") + QString(_projectDir.filename().wstring()) + tr(" 的翻译任务失败，请检查日志输出。"),      // 内容
			QSystemTrayIcon::Critical, // 图标类型 (Information, Warning, Critical)
			5000                          // 显示时长 (毫秒)
		);
		break;
	case -1:
		ElaMessageBar::error(ElaMessageBarType::BottomRight, tr("翻译失败"), tr("项目 ") + 
			QString(_projectDir.filename().wstring()) + tr(" 连工厂函数都失败了，玩毛啊"), 3000);
		break;
	case 0:
		if (_transEngine == "DumpName" || _transEngine == "GenDict") {
			ElaMessageBar::success(ElaMessageBarType::BottomRight, tr("生成完成"), tr("项目 ") + 
				QString(_projectDir.filename().wstring()) + tr(" 的生成任务已完成。"), 3000);
			_trayIcon->showMessage(
				tr("生成完成"),                  // 标题
					tr("项目 ") + QString(_projectDir.filename().wstring()) + tr(" 的生成任务已完成。"),      // 内容
				QSystemTrayIcon::Information, // 图标类型 (Information, Warning, Critical)
				5000                          // 显示时长 (毫秒)
			);
		}
		else if (_transEngine == "ShowNormal") {
			ElaMessageBar::success(ElaMessageBarType::BottomRight, tr("生成完成"), 
				tr("请在 show_normal 文件夹中查收项目 ") + QString(_projectDir.filename().wstring()) + tr(" 的预处理结果。"), 3000);
			_trayIcon->showMessage(
				tr("生成完成"),                  // 标题
					tr("请在 show_normal 文件夹中查收项目 ") + QString(_projectDir.filename().wstring()) + tr(" 的预处理结果。"),      // 内容
				QSystemTrayIcon::Information, // 图标类型 (Information, Warning, Critical)
				5000                          // 显示时长 (毫秒)
			);
		}
		else {
			ElaMessageBar::success(ElaMessageBarType::BottomRight, tr("翻译完成"), 
				tr("请在 gt_output 文件夹中查收项目 ") + QString(_projectDir.filename().wstring()) + " 的翻译结果。", 3000);
			_trayIcon->showMessage(
				tr("翻译完成"),                  // 标题
					tr("请在 gt_output 文件夹中查收项目 ") + QString(_projectDir.filename().wstring()) + tr(" 的翻译结果。"),      // 内容
				QSystemTrayIcon::Information, // 图标类型 (Information, Warning, Critical)
				5000                          // 显示时长 (毫秒)
			);
		}
		break;
	case 1:
		_trayIcon->showMessage(
			tr("翻译停止"),                  // 标题
			tr("项目 ") + QString(_projectDir.filename().wstring()) + tr(" 的翻译任务停止成功。"),      // 内容
			QSystemTrayIcon::Information, // 图标类型 (Information, Warning, Critical)
			5000                          // 显示时长 (毫秒)
		);
		ElaMessageBar::information(ElaMessageBarType::BottomRight, tr("停止成功"), tr("项目 ") + 
			QString(_projectDir.filename().wstring()) + tr(" 的翻译任务已终止"), 3000);
		break;
	default:
		break;
	}
	std::thread([this]()
		{
			std::this_thread::sleep_for(std::chrono::seconds(5));
			if (_trayIcon) {
				_trayIcon->hide();
			}
		}).detach();

	Q_EMIT finishTranslatingSignal(_transEngine, exitCode);
	_startTranslateButton->setEnabled(true);
	_stopTranslateButton->setEnabled(false);
}
