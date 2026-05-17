#include "settingsdialog.h"
#include "config.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFileDialog>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QSerialPortInfo>

namespace {

QWidget *wrapTab(QWidget *content)
{
    auto *outer = new QWidget;
    auto *lay = new QVBoxLayout(outer);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->addWidget(content);
    lay->addStretch();
    return outer;
}

} // namespace

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("CWTalk 选项"));
    setMinimumSize(560, 480);
    resize(600, 520);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(makeHardwareTab(), tr("硬件"));
    tabs->addTab(makeStationTab(), tr("台站"));
    tabs->addTab(makeOperationTab(), tr("操作"));
    tabs->addTab(makeFilesTab(), tr("文件"));
    tabs->addTab(makeShortcutsTab(), tr("快捷键 F1–F8"));

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *root = new QVBoxLayout(this);
    root->addWidget(tabs);
    root->addWidget(buttons);

    loadFromConfig();
}

void SettingsDialog::refreshSerialPortLists()
{
    populateSerialPortCombo(m_catPort, serialPortFromCombo(m_catPort));
    populateSerialPortCombo(m_keyingPort, serialPortFromCombo(m_keyingPort));
}

void SettingsDialog::populateSerialPortCombo(QComboBox *combo, const QString &selectedPort)
{
    if (!combo)
        return;

    const QString keep = selectedPort.trimmed();
    combo->clear();

    QStringList seen;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        const QString name = info.portName();
        if (name.isEmpty() || seen.contains(name, Qt::CaseInsensitive))
            continue;
        seen.append(name);

        QString label = name;
        const QString desc = info.description().trimmed();
        if (!desc.isEmpty())
            label += QStringLiteral(" — ") + desc;
        combo->addItem(label, name);
    }

    int idx = -1;
    for (int i = 0; i < combo->count(); ++i) {
        if (combo->itemData(i).toString().compare(keep, Qt::CaseInsensitive) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0 && !keep.isEmpty()) {
        combo->addItem(tr("%1 (当前未检测到)").arg(keep), keep);
        idx = combo->count() - 1;
    }
    combo->setCurrentIndex(idx >= 0 ? idx : (combo->count() > 0 ? 0 : -1));
}

QString SettingsDialog::serialPortFromCombo(const QComboBox *combo) const
{
    if (!combo || combo->currentIndex() < 0)
        return QString();
    return combo->currentData().toString();
}

void SettingsDialog::updateCatControlsEnabled()
{
    const bool on = m_catEnabled && m_catEnabled->isChecked();
    if (m_catPort)
        m_catPort->setEnabled(on);
    if (m_catBackend)
        m_catBackend->setEnabled(on);
    if (m_catBaud)
        m_catBaud->setEnabled(on);
    if (m_catPollMs)
        m_catPollMs->setEnabled(on);
    if (m_catAddress)
        m_catAddress->setEnabled(on);
}

QWidget *SettingsDialog::makeHardwareTab()
{
    auto *catBox = new QGroupBox(tr("CAT 电台控制"));
    auto *catForm = new QFormLayout(catBox);
    m_catPort = new QComboBox;
    m_catPort->setMinimumWidth(280);
    m_catBaud = new QComboBox;
    m_catBaud->addItems({QStringLiteral("9600"), QStringLiteral("19200"),
                         QStringLiteral("38400"), QStringLiteral("57600"),
                         QStringLiteral("115200")});
    m_catBaud->setEditable(true);
    m_catAddress = new QSpinBox;
    m_catAddress->setRange(0, 255);
    m_catAddress->setDisplayIntegerBase(16);
    m_catAddress->setPrefix(QStringLiteral("0x"));
    m_catEnabled = new QCheckBox(tr("启用 CAT 读频"));
    m_catBackend = new QComboBox;
    m_catBackend->addItem(tr("Yaesu (FT-710 等)"), QStringLiteral("yaesu"));
    m_catPollMs = new QSpinBox;
    m_catPollMs->setRange(200, 5000);
    m_catPollMs->setSingleStep(100);
    m_catPollMs->setSuffix(tr(" ms"));

    catForm->addRow(QString(), m_catEnabled);
    catForm->addRow(tr("协议:"), m_catBackend);
    catForm->addRow(tr("串口:"), m_catPort);
    catForm->addRow(tr("波特率:"), m_catBaud);
    catForm->addRow(tr("轮询间隔:"), m_catPollMs);
    catForm->addRow(tr("CI-V 地址:"), m_catAddress);

    connect(m_catEnabled, &QCheckBox::toggled, this, [this](bool) {
        updateCatControlsEnabled();
    });

    auto *refreshPortsBtn = new QPushButton(tr("刷新串口列表"));
    connect(refreshPortsBtn, &QPushButton::clicked, this, [this]() {
        refreshSerialPortLists();
    });
    catForm->addRow(QString(), refreshPortsBtn);

    auto *catHint = new QLabel(
        tr("FT-710 请使用 USB「Enhanced COM Port (CAT-1)」，菜单中 CAT-1 波特率与此处一致（默认 38400）。"));
    catHint->setWordWrap(true);
    catHint->setStyleSheet(QStringLiteral("color: #666; font-size: 11px;"));

    auto *keyBox = new QGroupBox(tr("键控 Keying"));
    auto *keyForm = new QFormLayout(keyBox);
    m_keyingPort = new QComboBox;
    m_keyingPort->setMinimumWidth(280);
    m_keyingLine = new QComboBox;
    m_keyingLine->addItems({QStringLiteral("RTS"), QStringLiteral("DTR")});
    m_keyingActiveLow = new QCheckBox(tr("低电平有效 (Active Low)"));
    keyForm->addRow(tr("串口:"), m_keyingPort);
    keyForm->addRow(tr("控制线:"), m_keyingLine);
    keyForm->addRow(QString(), m_keyingActiveLow);

    auto *panel = new QWidget;
    auto *lay = new QVBoxLayout(panel);
    lay->addWidget(catBox);
    lay->addWidget(catHint);
    lay->addWidget(keyBox);
    return wrapTab(panel);
}

QWidget *SettingsDialog::makeStationTab()
{
    auto *form = new QFormLayout;
    m_myCall = new QLineEdit;
    m_myName = new QLineEdit;
    m_myQth = new QLineEdit;
    m_myRig = new QLineEdit;
    m_grid = new QLineEdit;
    m_power = new QSpinBox;
    m_power->setRange(0, 9999);
    m_power->setSuffix(tr(" W"));

    form->addRow(tr("己方呼号 MY_CALL:"), m_myCall);
    form->addRow(tr("己方姓名 MY_NAME:"), m_myName);
    form->addRow(tr("己方 QTH MY_QTH:"), m_myQth);
    form->addRow(tr("设备 MY_RIG:"), m_myRig);
    form->addRow(tr("网格 Grid:"), m_grid);
    form->addRow(tr("功率 Power:"), m_power);

    auto *hint = new QLabel(tr("以上 MY_* 项对应快捷键宏变量，写入 [Station] 段。"));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #666; font-size: 11px;"));

    auto *panel = new QWidget;
    auto *lay = new QVBoxLayout(panel);
    lay->addLayout(form);
    lay->addWidget(hint);
    return wrapTab(panel);
}

QWidget *SettingsDialog::makeOperationTab()
{
    auto *form = new QFormLayout;
    m_defaultWpm = new QSpinBox;
    m_defaultWpm->setRange(5, 40);
    // m_cqMessage = new QLineEdit;
    m_cqInterval = new QSpinBox;
    m_cqInterval->setRange(2, 60);
    m_cqInterval->setSuffix(tr(" 秒"));
    m_sendClearDelay = new QSpinBox;
    m_sendClearDelay->setRange(0, 120);
    m_sendClearDelay->setSuffix(tr(" 秒"));
    m_sendClearDelay->setSpecialValueText(tr("关闭"));

    form->addRow(tr("默认速度 WPM:"), m_defaultWpm);
    // form->addRow(tr("CQ 报文模板:"), m_cqMessage);
    form->addRow(tr("CQ 循环间隔:"), m_cqInterval);
    form->addRow(tr("发完清空延迟:"), m_sendClearDelay);

    auto *hint = new QLabel(
        tr("发完清空：全部字符发送完成后，经过该时间自动清空发送框；0 为关闭。\n"
           "CQ 报文模板为预留项，快捷键内容请使用宏变量。"));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #666; font-size: 11px;"));

    auto *panel = new QWidget;
    auto *lay = new QVBoxLayout(panel);
    lay->addLayout(form);
    lay->addWidget(hint);
    return wrapTab(panel);
}

QWidget *SettingsDialog::makeFilesTab()
{
    auto *form = new QFormLayout;
    m_adifPath = new QLineEdit;
    auto *browse = new QPushButton(tr("浏览…"));
    connect(browse, &QPushButton::clicked, this, &SettingsDialog::onBrowseAdifPath);

    auto *pathRow = new QWidget;
    auto *pathLay = new QHBoxLayout(pathRow);
    pathLay->setContentsMargins(0, 0, 0, 0);
    pathLay->addWidget(m_adifPath, 1);
    pathLay->addWidget(browse);

    form->addRow(tr("ADIF 日志路径:"), pathRow);

    auto *panel = new QWidget;
    auto *lay = new QVBoxLayout(panel);
    lay->addLayout(form);
    return wrapTab(panel);
}

QWidget *SettingsDialog::makeShortcutsTab()
{
    m_shortcutTable = new QTableWidget(8, 3);
    m_shortcutTable->setHorizontalHeaderLabels(
        {tr("键"), tr("按钮标题"), tr("发送内容")});
    m_shortcutTable->verticalHeader()->setVisible(false);
    m_shortcutTable->horizontalHeader()->setStretchLastSection(true);
    m_shortcutTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_shortcutTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_shortcutTable->setAlternatingRowColors(true);

    for (int i = 0; i < 8; ++i) {
        auto *keyItem = new QTableWidgetItem(QStringLiteral("F%1").arg(i + 1));
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        m_shortcutTable->setItem(i, 0, keyItem);
        m_shortcutTable->setItem(i, 1, new QTableWidgetItem);
        m_shortcutTable->setItem(i, 2, new QTableWidgetItem);
    }

    auto *hint = new QLabel(
        tr("宏变量（不区分大小写）：\n"
           "<CALL> <RST_SENT> <RST_RCVD> <NAME> <QTH> — 主界面输入框\n"
           "<MY_CALL> <MY_NAME> <MY_QTH> <MY_RIG> — 台站配置"));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #555; font-size: 11px;"));

    auto *panel = new QWidget;
    auto *lay = new QVBoxLayout(panel);
    lay->addWidget(m_shortcutTable, 1);
    lay->addWidget(hint);
    return panel;
}

void SettingsDialog::loadFromConfig()
{
    const auto &cfg = theConfig;

    m_catEnabled->setChecked(cfg.getBool("Cat/Enabled", true));
    const QString backend = cfg.getString("Cat/Backend", "yaesu");
    const int backendIdx = m_catBackend->findData(backend);
    m_catBackend->setCurrentIndex(backendIdx >= 0 ? backendIdx : 0);
    m_catPollMs->setValue(cfg.getInt("Cat/Poll_Interval_Ms", 800));

    const QString catPort = cfg.getString("Hardware/CAT_Port", "COM3");
    const QString keyingPort = cfg.getString("Hardware/Keying_Port", "COM8");
    populateSerialPortCombo(m_catPort, catPort);
    populateSerialPortCombo(m_keyingPort, keyingPort);

    m_catBaud->setCurrentText(QString::number(cfg.getInt("Hardware/CAT_Baud", 38400)));
    m_catAddress->setValue(cfg.getInt("Hardware/CAT_Address", 0x94));
    const QString line = cfg.getString("Hardware/Keying_Line", "RTS").toUpper();
    m_keyingLine->setCurrentIndex(m_keyingLine->findText(line, Qt::MatchFixedString));
    m_keyingActiveLow->setChecked(cfg.getBool("Hardware/Keying_ActiveLow", false));

    updateCatControlsEnabled();

    m_myCall->setText(cfg.getString("Station/MY_CALL", ""));
    m_myName->setText(cfg.getString("Station/MY_NAME", ""));
    m_myQth->setText(cfg.getString("Station/MY_QTH", ""));
    m_myRig->setText(cfg.getString("Station/MY_RIG", ""));
    m_grid->setText(cfg.getString("Station/Grid", ""));
    m_power->setValue(cfg.getInt("Station/Power", 100));

    m_defaultWpm->setValue(cfg.getInt("Operation/Default_WPM", 22));
    // m_cqMessage->setText(cfg.getString("Operation/CQ_Message", ""));
    m_cqInterval->setValue(cfg.getInt("Operation/CQ_Interval", 4));
    m_sendClearDelay->setValue(cfg.getInt("Operation/Send_Clear_Delay_Sec", 2));

    m_adifPath->setText(cfg.getString("Files/ADIF_Path", ""));

    for (int i = 0; i < 8; ++i) {
        const QString n = QString::number(i + 1);
        m_shortcutTable->item(i, 1)->setText(
            cfg.getString(QString("Shortcuts/F%1_Title").arg(n), QString("F%1").arg(n)));
        m_shortcutTable->item(i, 2)->setText(
            cfg.getString(QString("Shortcuts/F%1_Content").arg(n), ""));
    }
}

void SettingsDialog::saveToConfig()
{
    auto &cfg = theConfig;

    cfg.set("Cat/Enabled", m_catEnabled->isChecked());
    cfg.set("Cat/Backend", m_catBackend->currentData().toString());
    cfg.set("Cat/Poll_Interval_Ms", m_catPollMs->value());

    cfg.set("Hardware/CAT_Port", serialPortFromCombo(m_catPort));
    cfg.set("Hardware/CAT_Baud", m_catBaud->currentText().toInt());
    cfg.set("Hardware/CAT_Address", m_catAddress->value());
    cfg.set("Hardware/Keying_Port", serialPortFromCombo(m_keyingPort));
    cfg.set("Hardware/Keying_Line", m_keyingLine->currentText().trimmed().toUpper());
    cfg.set("Hardware/Keying_ActiveLow", m_keyingActiveLow->isChecked());

    cfg.set("Station/MY_CALL", m_myCall->text().trimmed().toUpper());
    cfg.set("Station/MY_NAME", m_myName->text().trimmed());
    cfg.set("Station/MY_QTH", m_myQth->text().trimmed());
    cfg.set("Station/MY_RIG", m_myRig->text().trimmed());
    cfg.set("Station/Grid", m_grid->text().trimmed().toUpper());
    cfg.set("Station/Power", m_power->value());

    cfg.set("Operation/Default_WPM", m_defaultWpm->value());
    // cfg.set("Operation/CQ_Message", m_cqMessage->text().trimmed());
    cfg.set("Operation/CQ_Interval", m_cqInterval->value());
    cfg.set("Operation/Send_Clear_Delay_Sec", m_sendClearDelay->value());

    cfg.set("Files/ADIF_Path", m_adifPath->text().trimmed());

    for (int i = 0; i < 8; ++i) {
        const QString n = QString::number(i + 1);
        cfg.set(QString("Shortcuts/F%1_Title").arg(n),
                m_shortcutTable->item(i, 1)->text().trimmed());
        cfg.set(QString("Shortcuts/F%1_Content").arg(n),
                m_shortcutTable->item(i, 2)->text().trimmed());
    }
}

void SettingsDialog::onBrowseAdifPath()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("选择 ADIF 日志文件"), m_adifPath->text(),
        tr("ADIF 文件 (*.adi *.adif);;所有文件 (*)"));
    if (!path.isEmpty())
        m_adifPath->setText(path);
}

void SettingsDialog::onAccepted()
{
    saveToConfig();
    accept();
}
