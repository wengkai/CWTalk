# CWTalk

日常 CW 通联极简辅助工具。绿色便携、完全离线，面向 Windows + Qt。

通过串口 DTR/RTS 键控发报，可选 Yaesu CAT 读频，ADIF 日志追加写入，内置 Big CTY 国际冠字表做呼号前缀解析。

## 功能概览

| 模块 | 说明 |
|------|------|
| **键控发报** | 发送框输入即发；5–40 WPM 可调（发送行左侧）；Esc 停止 |
| **快捷键 F1–F8** | 单击追加并发送；双击 CQ 循环；右键编辑宏 |
| **宏变量** | `<CALL>` `<RST_SENT>` `<RST_RCVD>` `<NAME>` `<QTH>` `<MY_CALL>` `<MY_NAME>` `<MY_QTH>` `<MY_RIG>` |
| **临时提速** | 宏或发送内容中用 `[` `]` 包裹的段落按 **1.5× WPM** 发送（括号本身不发码） |
| **CAT 读频** | Yaesu（FT-710 等，`FA`/`FB`）；可与键控同口；发射时暂停轮询 |
| **通联记录** | ADIF v3 追加写入；**记录** / `Ctrl+L`；`freq` 为 MHz |
| **呼号解析** | 启动加载 `data/cty.dat`，实时显示国家、CQ/ITU 区、大洲 |
| **历史查询** | 呼号失焦后显示该台最近 5 条 QSO（逗号分隔文本行） |

## 系统要求

- **操作系统**：Windows 10/11（当前仅维护 Windows 构建）
- **Qt**：6.x（开发环境示例：Qt 6.11 + MinGW 64-bit）
- **构建工具**：qmake、mingw32-make
- **硬件**：USB/串口；键控需 DTR 或 RTS 线控接口（外接隔离电路）

## 快速开始

### 1. 编译

在项目根目录执行（需已将 Qt/MinGW 的 `bin` 加入 `PATH`）：

```bat
build.bat
```

或手动：

```bat
cd build
qmake ..\src\cwtalk.pro
mingw32-make -j4
mkdir release\data 2>nul
copy /Y ..\data\cty.dat release\data\
cd ..
```

可执行文件：`build\release\CWTalk.exe`

### 2. 首次配置

1. 将 `cwtalk.ini.example` 复制为 `build\release\cwtalk.ini`（与 exe 同目录）。
2. 在 **选项**（信息栏右侧齿轮）或 ini 中设置：
   - `Hardware/Keying_Port`、`Hardware/CAT_Port`（可同口）
   - `Station/MY_CALL`、`Station/Grid` 等
3. 确认 `data\cty.dat` 与 exe 相对路径正确（默认 `Files/CTY_Path=data/cty.dat`）。

首次运行若无 ini，程序会按内置默认值生成一份。

### 3. 运行

```bat
build\release\CWTalk.exe
```

## 常用操作

| 操作 | 说明 |
|------|------|
| `Alt+K` | 聚焦摩尔斯发送框 |
| `Esc` | 清除当前 QSO / 停止循环 |
| `Ctrl+L` | 记录当前通联到 ADIF |
| `Tab`（RST 框） | 空 RST 自动填 `599` 并跳到下一项 |
| `F1`–`F8` | 触发对应快捷键宏 |
| 双击 F 键按钮 | 开始/停止该宏 CQ 循环 |

**Yaesu CAT 提示**：FT-710 等请使用 **Enhanced COM**，波特率与电台菜单 **CAT-1 RATE** 一致（常用 **38400**）。

## 目录结构

```
cwtalk/
├── src/                 # Qt/C++ 源码
│   ├── adif/            # ADIF 读写
│   ├── mainwindow.*     # 主界面
│   ├── pckeyer.*        # 摩尔斯键控
│   ├── yaesucatreader.* # Yaesu CAT
│   └── callsignprefixdb.* # CTY 冠字表查询
├── data/
│   └── cty.dat          # Big CTY 冠字表（运行必需，可定期更新）
├── doc/                 # 需求文档与路线图
├── build.bat            # Windows 构建脚本
├── cwtalk.ini.example   # 配置模板（不含个人数据）
└── README.md
```

## 配置与数据文件

| 文件 | 说明 |
|------|------|
| `cwtalk.ini` | 用户配置（**勿提交到 Git**），与 exe 同目录 |
| `log.adif` / `*.adi` | QSO 日志（**勿提交**） |
| `data/cty.dat` | 国际冠字表，随仓库分发；可从 [country-files.com/bigcty](https://www.country-files.com/bigcty/) 下载新版替换 |

更新冠字表后重启程序，或在 ini 中修改 `Files/CTY_Path`。

## 开发说明

- 工程文件：`src/cwtalk.pro`
- 详细需求见：`doc/CWTalk 需求文档（正式版 v1.3）.md`
- 开发摘要见：`doc/CWTalk 开发日志.md`
- 发布前请核对 `.gitignore`，确保未提交 `cwtalk.ini`、日志与 `build/` 产物

### 计划中的功能

- Icom CI-V CAT
- 基于 Grid 的方位角 / 距离显示
- WinKeyer、hamlib 等（见需求文档 v2.x）

## 免责声明

本软件按「原样」提供。键控与 CAT 涉及电台与串口硬件连接，请自行确保电路隔离与安全，作者不对设备损坏或违规操作负责。使用 CTY 数据请遵守 [country-files.com](https://www.country-files.com/) 相关分发说明。


