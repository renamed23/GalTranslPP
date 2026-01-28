#include "AboutDialog.h"

#include <ElaIconButton.h>
#include <ElaToolButton.h>
#include <ElaToolTip.h>
#include <QIcon>
#include <QDesktopServices>
#include <QVBoxLayout>

#include "ElaImageCard.h"
#include "ElaText.h"

import Tool;

AboutDialog::AboutDialog(QWidget* parent)
    : ElaDialog(parent)
{
    setWindowTitle(tr("关于"));
    setWindowIcon(QIcon(":/GPPGUI/Resource/Image/webIcon.jpeg"));
    this->setIsFixedSize(true);
    setWindowModality(Qt::ApplicationModal);
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);

    ElaImageCard* pixCard = new ElaImageCard(this);
    pixCard->setFixedSize(60, 60);
    pixCard->setIsPreserveAspectCrop(false);
    pixCard->setCardImage(QImage(":/GPPGUI/Resource/Image/webIcon.jpeg"));

    QVBoxLayout* pixCardLayout = new QVBoxLayout();
    pixCardLayout->addWidget(pixCard);
    pixCardLayout->addStretch();

    ElaText* versionText = new ElaText("GalTransl++ GUI v" + QString::fromStdString(GPPVERSION), this);
    QFont versionTextFont = versionText->font();
    versionTextFont.setWeight(QFont::Bold);
    versionText->setFont(versionTextFont);
    versionText->setWordWrap(false);
    versionText->setTextPixelSize(18);

    ElaText* licenseText = new ElaText("Apache License 2.0", this);
    licenseText->setWordWrap(false);
    licenseText->setTextPixelSize(14);
    ElaText* copyrightText = new ElaText(tr("版权所有 © 2025-2026 julixian"), this);
    copyrightText->setWordWrap(false);
    copyrightText->setTextPixelSize(14);

    ElaIconButton* jumpToGithubButton = new ElaIconButton(ElaIconType::Warehouse, this);
    jumpToGithubButton->setFixedWidth(40);
    connect(jumpToGithubButton, &ElaIconButton::clicked, this, [=]()
        {
            QDesktopServices::openUrl(QUrl("https://github.com/julixian/GalTranslPP/releases"));
        });
    ElaToolTip* jumpToGithubToolTip = new ElaToolTip(jumpToGithubButton);
    jumpToGithubToolTip->setToolTip(tr("跳转到 Github 发布页"));

    ElaIconButton* checkUpdateButton = new ElaIconButton(ElaIconType::CheckToSlot, this);
    checkUpdateButton->setFixedWidth(40);
    connect(checkUpdateButton, &ElaIconButton::clicked, this, [=]()
        {
            Q_EMIT checkUpdateSignal();
        });
    ElaToolTip* checkUpdateToolTip = new ElaToolTip(checkUpdateButton);
    checkUpdateToolTip->setToolTip(tr("检查更新"));

    _downloadButton = new ElaIconButton(ElaIconType::Download, this);
    _downloadButton->setFixedWidth(40);
    _downloadButton->setEnabled(false);
    connect(_downloadButton, &ElaIconButton::clicked, this, [=]()
        {
            Q_EMIT downloadUpdateSignal();
        });
    ElaToolTip* downloadUpdateToolTip = new ElaToolTip(_downloadButton);
    downloadUpdateToolTip->setToolTip(tr("下载更新"));

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(jumpToGithubButton);
    buttonLayout->addWidget(checkUpdateButton);
    buttonLayout->addWidget(_downloadButton);

    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->setSpacing(15);
    textLayout->addWidget(versionText);
    textLayout->addWidget(licenseText);
    textLayout->addWidget(copyrightText);
    textLayout->addLayout(buttonLayout);
    textLayout->addStretch();

    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->addSpacing(30);
    contentLayout->addLayout(pixCardLayout);
    contentLayout->addSpacing(30);
    contentLayout->addLayout(textLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 25, 0, 0);
    mainLayout->addLayout(contentLayout);
}

void AboutDialog::setDownloadButtonEnabled(bool enabled)
{
    _downloadButton->setEnabled(enabled);
}

AboutDialog::~AboutDialog()
{
}
