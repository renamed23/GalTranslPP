#ifndef SETTINGPAGE_H
#define SETTINGPAGE_H

#include <toml.hpp>
#include "BasePage.h"

class SettingPage : public BasePage
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit SettingPage(toml::ordered_value& globalConfig, QWidget* parent = nullptr);
    ~SettingPage() override;

Q_SIGNALS:
    void restartPythonEnvSignal(QString path);

private:

    toml::ordered_value& _globalConfig;
};

#endif // SETTINGPAGE_H
