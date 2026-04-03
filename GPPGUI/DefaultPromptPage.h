#ifndef DEFAULTPROMPTPAGE_H
#define DEFAULTPROMPTPAGE_H

#include <toml.hpp>
#include <QStackedWidget>
#include "BasePage.h"

class DefaultPromptPage : public BasePage
{
    Q_OBJECT
public:

    explicit DefaultPromptPage(QWidget* parent = nullptr);
    ~DefaultPromptPage() override;

private:

    toml::ordered_value _promptConfig;

    void _setupUI();
};

#endif // DEFAULTPROMPTPAGE_H
