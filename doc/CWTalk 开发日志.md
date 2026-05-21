# CWTalk 开发日志

按日期记录实现进展与重要决策。详细需求以《CWTalk 需求文档（正式版 v1.4）》为准。

---

## 2026-05-21 — 通联区淡色、快捷键与发报修复

### 已完成

| 项 | 说明 |
|----|------|
| 记录后通联区 | `Ctrl+L` /「记录」成功后：呼号/RST/姓名/QTH/备注**保留文字、灰色淡色**；频率与发送框仍按原逻辑（频率保留、发送框清空） |
| 淡色后键入 | 在任一通联框输入时：**先清空六框**，仅保留相对淡色快照的**新输入**（避免呼号追加在旧呼号后）；恢复正常样式 |
| 「清除」按钮 | 仅 `clearQsoFieldsOnly()`（通联六框 + 历史面板），**不**清发送框、**不**停键控 |
| **Esc** | `onAbortSend()`：停循环/键控、**仅清空发送框**；**不清**通联区 |
| 发送快捷键 | **Alt+C/N/Q**：对方呼号/姓名/QTH（空则跳过）；**Alt+M**：`Station/MY_CALL`；**Alt+K**：聚焦发送框 |
| 发完后追加 | `PCKeyer::update()`：已发前缀未变时从 `currentIndex` **续发**，避免整段重发 |
| `.gitignore` | 增加 `*.zip` |

### 主要修改文件

`src/mainwindow.cpp` / `.h`，`src/pckeyer.cpp`

---

## 2026-05-21 — Icom CI-V（IC-756PROIII）

### 已完成

- 新增 `IcomCatReader`：CI-V `0x03` 读当前 VFO 频率（5 字节 BCD，Hz）
- 设置中协议可选 **Icom IC-756PROIII**（`Cat/Backend=icom_ic756pro3`）
- 默认 CI-V 地址 **0x6E**、建议波特率 **19200**（与机内菜单一致）
- 与 Yaesu 相同：轮询、发射时暂停、CAT/键控同口时 `releaseKeyingLines`

### 配置提示

| 项 | IC-756PROIII |
|----|----------------|
| 协议 | Icom IC-756PROIII |
| 波特率 | 19200（菜单 CI-V Baud Rate） |
| CI-V 地址 | 0x6E（菜单默认） |


## 2026-05-16 — v1.2 / 开源准备日

### 当日目标

完善通联辅助体验、替换临时代码、整理仓库以便发布 GitHub 开源。

### 已完成功能

#### 界面与交互

| 项 | 说明 |
|----|------|
| QSO 历史面板 | 由表格改为最多 5 行纯文本；逗号分隔，无列名；最新在上 |
| 历史行格式 | 固定：`日期, time_on(6位), freq, mode`；可选 `name/qth/comment`（空则省略） |
| WPM 控件 | 与摩尔斯发送框同行左侧；`QSpinBox` 5–40，± 步进；变更立即作用于 Keyer |
| 选项入口 | 移除仅含一项的菜单栏；信息行右侧齿轮按钮打开设置 |
| 发送提速 | 宏/发送内容中 `[` `]` 内按 **1.5× WPM** 发码，括号不发摩尔斯 |

#### CAT / 键控

| 项 | 说明 |
|----|------|
| 调试输出 | 移除全部 `[CAT]` 类 `qDebug`，错误仍走 `lastError` 与界面提示 |
| 发射协同 | 键控发送期间暂停 CAT 轮询（此前已实现，文档对齐） |

#### 呼号前缀（替代 if-else 临时代码）

| 项 | 说明 |
|----|------|
| `CallsignPrefixDatabase` | 启动加载 Big CTY（`data/cty.dat`），最长前缀匹配 |
| 查询结果 | 国家名、CQ 区、ITU 区、大洲；经纬度已解析供后续方位/距离 |
| 配置 | `Files/CTY_Path`，默认 `data/cty.dat` |

#### 文档与仓库

| 项 | 说明 |
|----|------|
| 需求文档 | 更新至 v1.3，对齐上述实现 |
| `README.md` | 功能说明、编译运行、目录结构、发布注意事项 |
| `.gitignore` | 排除 `build/`、`cwtalk.ini`、`*.adi`/`*.adif`、个人 IDE 配置等 |
| `cwtalk.ini.example` | 无个人数据的配置模板 |

### 此前数日已并入 v1.2 的要点（本日文档化）

- Yaesu CAT 读频（FT-710 / Enhanced COM / 38400）；`Cat/Enabled`、串口下拉
- ADIF 追加日志、「记录」/`Ctrl+L`；`freq` MHz；`time_on`/`time_off`
- 发射时暂停 CAT；CAT/键控同口共享与 `releaseKeyingLines`
- 呼号大写、RST 三位与 Tab 填 599、频率 MHz 三位小数

### 未做 / 留待后续

- 基于 Grid 的方位角、距离显示（界面仍为 `---°`、`---- km`）
- 大国家国内分区细化（CTY 已覆盖实体，未做 W1/VE 等二级展示）
- ~~Icom CI-V CAT~~（同日已实现，见上节 Icom）
- `BAND`、`MY_POWER` 写入 ADIF
- 历史记录点击自动填充 QSO 字段
- 开源 **LICENSE** 文件（README 中已说明待补）

### 主要新增/修改文件

```
src/callsignprefixdb.h
src/callsignprefixdb.cpp
src/mainwindow.cpp / .h
src/pckeyer.cpp / .h
data/cty.dat
README.md
.gitignore
cwtalk.ini.example
doc/CWTalk 需求文档（正式版 v1.4）.md
doc/CWTalk 开发日志.md
```

### 备注

- `build.bat` 在 `cd build` 后复制 `cty.dat` 的路径宜改为 `..\data\cty.dat` → `release\data\`（若复制失败请按 README 手动复制）。
- 发布 GitHub：需先在远端建空仓库，再本地 `git push`；`cwtalk.ini` 与日志切勿入库。

---

## 模板（后续日期可复制）

### YYYY-MM-DD — 标题

**已完成：**

**进行中：**

**问题/决策：**
