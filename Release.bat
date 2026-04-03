cd /d %~dp0

robocopy "vcpkg_installed\gpp-x64-windows-release\gpp-x64-windows-release\share\opencc" "Example\BaseConfig\opencc" /E

robocopy "Example\BaseConfig" "Release\GPPCLI\BaseConfig" /E /XF "python-3.12.10-embed-amd64.zip"
robocopy "Example\BaseConfig" "Release\GPPGUI\BaseConfig" /E /XF "python-3.12.10-embed-amd64.zip"
robocopy "Example\BaseConfig" "Release\GUICORE\BaseConfig" /E ^
/XF "python-3.12.10-embed-amd64.zip" "globalConfig.toml" ^
/XD "mecabDict" "python-3.12.10-embed-amd64"

robocopy "3rdParty" "Release\GPPCLI" 7z.dll
robocopy "3rdParty" "Release\GPPGUI" 7z.dll
robocopy "3rdParty" "Release\GUICORE" 7z.dll

robocopy "Example\sampleProject" "Release\GPPCLI\sampleProject" /E

pause
