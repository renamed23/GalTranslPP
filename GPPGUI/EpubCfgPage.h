// EpubCfgPage.h

#ifndef EPUBCFGPAGE_H
#define EPUBCFGPAGE_H

#include <toml.hpp>
#include "BasePage.h"

class EpubCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit EpubCfgPage(toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~EpubCfgPage() override;

private:
    toml::ordered_value& _projectConfig;
};

#endif // EPUBCFGPAGE_H
