// ElaDoubleText.h

#ifndef ELADOUBLETEXT_H
#define ELADOUBLETEXT_H

#include "ElaText.h"
#include "ElaToolTip.h"

class ElaLineEdit;

class ElaDoubleText : public QWidget
{
    Q_OBJECT

public:
    explicit ElaDoubleText(QWidget* parent, const QString& firstLine, int firstLinePixelSize, const QString& secondLine, int secondLinePixelSize, const QString& toolTip);
    ~ElaDoubleText() override;

    QString getFirstLineText() const;

private:
    ElaText* _firstLine{ nullptr };
    ElaText* _secondLine{ nullptr };
    ElaToolTip* _toolTip{ nullptr };
};

#endif // ELADOUBLETEXT_H