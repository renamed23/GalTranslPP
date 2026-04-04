#include "ElaInputDialog.h"

#include <QVBoxLayout>
#include <QFormLayout>

#include "ElaScrollPageArea.h"
#include "ElaText.h"
#include "ElaLineEdit.h"


ElaInputDialog::ElaInputDialog(QWidget* parent, const QString& label, const QString& text, QString& result, bool* ok) :
	ElaContentDialog(parent), _result(result), _ok(ok)
{
	setWindowTitle("InputDialog");

	setLeftButtonText(tr("取消"));
	setMiddleButtonText(tr("重置"));
	setRightButtonText(tr("确定"));

	*_ok = false;

	// 创建一个中心部件和布局
	QWidget* centerWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);
	mainLayout->setContentsMargins(15, 25, 15, 10);

	ElaText* labelText = new ElaText(label, centerWidget);
	labelText->setTextStyle(ElaTextType::Title);
	labelText->setWordWrap(false);
	mainLayout->addWidget(labelText);
	mainLayout->addSpacing(2);

	_lineEdit = new ElaLineEdit(centerWidget);
	_lineEdit->setPlaceholderText(text);
	mainLayout->addStretch();
	mainLayout->addWidget(_lineEdit);

	setCentralWidget(centerWidget);
}

ElaInputDialog::~ElaInputDialog()
{

}

void ElaInputDialog::onRightButtonClicked()
{
	*_ok = true;
	_result = _lineEdit->text();
}

void ElaInputDialog::onMiddleButtonClicked()
{
	_lineEdit->clear();
}
