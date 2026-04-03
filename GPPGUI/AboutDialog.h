#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <ElaDialog.h>

class ElaIconButton;

class AboutDialog : public ElaDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr);
    ~AboutDialog() override;
    void setDownloadButtonEnabled(bool enabled);

Q_SIGNALS:
    void checkUpdateSignal();
    void downloadUpdateSignal();

private:
    ElaIconButton* _downloadButton;
};

#endif // ABOUTDIALOG_H
