cd /d %~dp0

robocopy Example\BaseConfig\Dict\ Release\GPPCLI\BaseConfig\Dict\ /E
robocopy Example\BaseConfig\Dict\ Release\GPPGUI\BaseConfig\Dict\ /E
robocopy Example\BaseConfig\Dict\ Release\GUICORE\BaseConfig\Dict\ /E

robocopy Example\BaseConfig\illustration\ Release\GPPCLI\BaseConfig\illustration\ /E
robocopy Example\BaseConfig\illustration\ Release\GPPGUI\BaseConfig\illustration\ /E
robocopy Example\BaseConfig\illustration\ Release\GUICORE\BaseConfig\illustration\ /E

robycopy Example\BaseConfig\opencc\ Release\GPPCLI\BaseConfig\opencc\ /E
robycopy Example\BaseConfig\opencc\ Release\GPPGUI\BaseConfig\opencc\ /E
robycopy Example\BaseConfig\opencc\ Release\GUICORE\BaseConfig\opencc\ /E

robocopy Example\BaseConfig\pluginConfigs\ Release\GPPCLI\BaseConfig\pluginConfigs\ /E
robocopy Example\BaseConfig\pluginConfigs\ Release\GPPGUI\BaseConfig\pluginConfigs\ /E
robocopy Example\BaseConfig\pluginConfigs\ Release\GUICORE\BaseConfig\pluginConfigs\ /E

robocopy Example\BaseConfig\pyScripts\ Release\GPPCLI\BaseConfig\pyScripts\ /E
robocopy Example\BaseConfig\pyScripts\ Release\GPPGUI\BaseConfig\pyScripts\ /E
robocopy Example\BaseConfig\pyScripts\ Release\GUICORE\BaseConfig\pyScripts\ /E

robocopy Example\BaseConfig\ Release\GPPCLI\BaseConfig\ *.toml
robocopy Example\BaseConfig\ Release\GPPGUI\BaseConfig\ *.toml
robocopy Example\BaseConfig\ Release\GUICORE\BaseConfig\ Prompt.toml

robocopy Example\BaseConfig\ Release\GPPCLI\BaseConfig\ *.txt
robocopy Example\BaseConfig\ Release\GPPGUI\BaseConfig\ *.txt
robocopy Example\BaseConfig\ Release\GUICORE\BaseConfig\ *.txt

robycopy Example\sampleProject\ Release\GPPCLI\sampleProject\ /E

pause
