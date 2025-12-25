// CommonNormalDictPage.h

#ifndef COMMONNORMALDICTPAGE_H
#define COMMONNORMALDICTPAGE_H

#include <toml.hpp>
#include <string>
#include "BasePage.h"
#include "NormalTabEntry.h"

namespace fs = std::filesystem;

class CommonNormalDictPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonNormalDictPage(const std::string& mode, toml::ordered_value& globalConfig, QWidget* parent = nullptr);
    ~CommonNormalDictPage() override;

Q_SIGNALS:
    void commonDictsChanged();

private:

    void _setupUI();

    toml::ordered_value& _globalConfig;

    QList<NormalTabEntry> _normalTabEntries;
    QWidget* _mainWindow;

    std::string _mode;
    std::string _modeConfigKey;
    fs::path _modeDictDir;
};

#endif // COMMONNORMALDICTPAGE_H
