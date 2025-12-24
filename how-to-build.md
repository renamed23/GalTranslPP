# GalTranslPP 编译指南

## 1. 环境配置

在开始编译之前，请确保你的开发环境满足以下要求：

- **操作系统**: Windows 10 或 Windows 11
- **IDE**: [Visual Studio 2026](https://visualstudio.microsoft.com/insiders/?rwnlp=zh-hans) 和 

    VS2022(会用到 2022 的 ToolChain，如果你懂的话可以只下 VS2022 的 BuildTools 而不下其 IDE，不过依然需要勾选**使用C++的桌面开发**选项)
    ![BuildTools](img/BuildTools.png?raw=true)
  - **必需工作负载**: `使用 C++ 的桌面开发`
  - **必需工具集**: `MSVC v143` (VS 2022) 和 `MSVC v145`
- **Python 3.12.10**: 本仓库中的 `python-3.12.10-embed-amd64.zip`
- **版本控制工具**: [git](https://git-scm.com/)

## 2. 安装核心依赖

### 2.1 vcpkg 包管理器

vcpkg 用于管理项目所需的 C++ 库。

```cmd
# 1. 克隆 vcpkg 仓库到任意位置
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg

# 2. 执行引导脚本进行安装
.\bootstrap-vcpkg.bat

# 3. (重要) 将 vcpkg 的根目录路径添加到用户环境变量"Path"中或运行 vcpkg integreate install 来将 vcpkg 绑定到 Visual Studio
```

### 2.2 Qt 框架

- 1、  访问 [Qt 官方网站](https://www.qt.io/download-qt-installer-oss)下载并运行Qt社区开源版本(LGPL协议)的在线安装器 (需要注册 Qt 账户)。
- 2、  在安装器的组件选择页面，确保勾选以下组件:
  - `Qt` → `Qt 6.9.2(或更高，但不保证兼容性)` → `MSVC 2022 64-bit`

## 3. 获取项目源码

将 GalTranslPP 主仓库连同子模块依赖克隆至本地。

```cmd
git clone --recurse-submodules https://github.com/julixian/GalTranslPP.git
cd GalTranslPP
```

## 4. 编译依赖

### 4.1 配置 Visual Studio 与 Qt

- 1、  **安装 VS 插件**:
  - 启动 Visual Studio，在顶部菜单栏选择 `扩展` → `管理扩展`。
  - 搜索并安装 **"Qt Visual Studio Tools"** 插件。
  - 根据提示重启 Visual Studio 以完成安装。
- 2、  **关联 Qt 版本**:
  - 重启后，在菜单栏选择 `扩展` → `Qt VS Tools` → `Qt Versions`。
  - 点击 `Add New Qt Version`，将路径指向你安装的 Qt MSVC 目录 (例如: `C:\Qt\6.9.2\msvc2022_64`)，并将其设置为默认版本。

### 4.2 编译 ElaWidgetTools

- 1、  使用 Visual Studio 打开 `3rdParty\ElaWidgetTools` 文件夹。
- 2、  在顶部工具栏中，将生成配置从 `Qt-Debug` 切换为 **`Qt-Release`**。
- 3、  在菜单栏中选择 `生成` → `生成 ElaWidgetTools.dll(安装)`

      (如果使用Visual Studio且严格按照上述步骤执行，则无需更改CMakeLists中的QT_SDK_DIR)。
- 4、  **确认编译产物**:
  - 确保 `3rdParty\ElaWidgetTools\Install\ElaWidgetTools\include` 文件夹存在，程序会用到里面的头文件
  - 确保 `3rdParty\ElaWidgetTools\Install\ElaWidgetTools\lib\ElaWidgetTools.lib` 文件存在
  - 确保 `3rdParty\ElaWidgetTools\Install\ElaWidgetTools\bin\ElaWidgetTools.dll` 文件存在

### 4.4 编译 OpenCC

- 1、  使用 Visual Studio 打开 `3rdParty\OpenCC` 文件夹。
- 2、  如果编译选项没有 `x64-Release`，就先点到管理配置，点击绿色加号，选择 `x64-Release`，Ctrl + S 保存。
- 3、  选择 `x64-Release`，生成 opencc.dll(安装) (lib\opencc.dll)。
- 4、  **确认编译产物**
  - 确保 `3rdParty\OpenCC\out\install\x64-Release\include` 文件夹存在，程序会用到里面的头文件
  - 确保 `3rdParty\OpenCC\out\install\x64-Release\lib` 目录下存在文件 `marisa.lib` 和 `opencc.lib`
  - 确保 `3rdParty\OpenCC\out\install\x64-Release\bin\opencc.dll` 文件存在

## 5. 编译 GalTranslPP (主项目)

此步骤涉及一次临时的平台工具集切换，以解决特定依赖的兼容性问题。

- 1、  使用 Visual Studio 打开根目录下的 `GalTranslPP.sln` 解决方案文件。

- 2、  **步骤一：使用 v143 工具集编译依赖**
  - 在 **解决方案资源管理器** 中，右键点击 `GalTranslPP` 模块，选择 `属性`。
  - 在属性页中，导航至 `配置属性` → `常规`。
  - 将 **平台工具集** 选项更改为 **`Visual Studio 2022 (v143)`** **(Debug和Release都改过来)**。
  - 点击 `应用` 保存设置。
  - 在菜单栏选择 `生成` → `批生成`勾选`GalTranslPP|Release|x64`并点击生成。
    > **注意**: 此步骤专门用于编译 `mecab:x64-windows` 等依赖。

- 3、  **步骤二：使用 v145 工具集编译主程序**
  - 再次打开 `GalTranslPP` 项目的属性页。
  - 将 **平台工具集** 切换回 **`Visual Studio v18 (v145)`**。
  - 点击 `应用` 并关闭属性窗口。
  - 为确保所有模块都被正确编译，在`生成` → `批生成`编译目标模块时应选择`全部重新生成`(如果实在过不了编就直接开VS2022的IDE把工具集切回v143编译)。

## 6. 完成与运行

编译成功后，所有可执行文件将生成于 `Release\` 目录下。  

还需将一些文件复制到文件夹内程序才可正常运行。  

- 0、 先将项目根目录的`Example\BaseConfig`文件夹内的`python-3.12.10-embed-amd64.zip`文件解压到当前文件夹

### 6.1 GPPCLI

- 1、  将`Example`文件夹内的`BaseConfig`文件夹复制到`GalTranslPP\Release\GPPCLI`

### 6.2 GPPGUI

- 1、  将`BaseConfig`文件夹复制到`GalTranslPP\Release\GPPGUI`
- 2、  打开 Qt专属控制台，如 Qt 6.9.2(MSVC 2022 64-bit)，输入命令 

```cmd
windeployqt path/to/GalTranslPP_GUI.exe
```

### 6.3 私人部署（非必需）

若你想将GPPCLI和GPPGUI移动到其它位置运行如`D:\GALGAME\GALGAMETOOLS\AIGC\GPPCLI`  
和`D:\GALGAME\GALGAMETOOLS\AIGC\GPPGUI`则请在GalTranslPP\Release执行以下cmd命令创建软链接

#### GPPCLI

```cmd
mklink .\GPPCLI_PRIVATE "D:\GALGAME\GALGAMETOOLS\AIGC\GPPCLI"
```

#### GPPGUI

```cmd
mklink .\GPPGUI_PRIVATE "D:\GALGAME\GALGAMETOOLS\AIGC\GPPGUI"
```

这样每次编译都会将最核心的文件`GalTranslPP_CLI.exe`、`GalTranslPP_GUI.exe`和`Updater_new.exe`复制到相应目录。  

至此所有步骤均已完成。
