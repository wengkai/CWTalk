#include "mainwindow.h"
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QShortcut>
#include <QTextCursor>
#include <QDialog>
#include <QMouseEvent>
#include <QKeyEvent>
#include "config.h"
#include "pckeyer.h"
#include "settingsdialog.h"
#include "catreaderfactory.h"
#include "icatreader.h"
#include "yaesucatreader.h"
#include "icomcatreader.h"
#include "qsolog.h"
#include "callsignprefixdb.h"
#include "adif/record.h"

#include <QMessageBox>
#include <QDateTime>
#include <QRegularExpressionValidator>
#include <QToolButton>
#include <QIcon>
#include <QSizePolicy>
#include <QLocale>
#include <QtMath>
#include <QSerialPort>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QStatusBar>
#include <QFrame>
#include <QRegularExpression>

namespace {

const char kQsoFieldStyleNormal[] =
    "QLineEdit {"
    "  font-size: 14px;"
    "  font-family: Consolas, 'Courier New', monospace;"
    "  font-weight: bold;"
    "  padding: 6px;"
    "  border: 2px solid #555;"
    "  border-radius: 4px;"
    "  background-color: #2b2b2b;"
    "  color: #fff;"
    "}"
    "QLineEdit:focus {"
    "  border-color: #4a9eff;"
    "  background-color: #1e3a5f;"
    "}";

const char kQsoFieldStyleFaded[] =
    "QLineEdit {"
    "  font-size: 14px;"
    "  font-family: Consolas, 'Courier New', monospace;"
    "  font-weight: bold;"
    "  padding: 6px;"
    "  border: 2px solid #444;"
    "  border-radius: 4px;"
    "  background-color: #2b2b2b;"
    "  color: #666;"
    "}"
    "QLineEdit:focus {"
    "  border-color: #555;"
    "  background-color: #2b2b2b;"
    "}";

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    m_qsoLog = new QsoLog;
    m_prefixDb = new CallsignPrefixDatabase;
    setupUI();
    loadPrefixDatabase();
    loadAdifLog();
    initHardware();
    setWindowTitle("CWTalk - 日常 CW 通联");
    resize(1020, 180);
}

MainWindow::~MainWindow()
{
    shutdownHardware();
    delete m_prefixDb;
    delete m_qsoLog;
}

QSerialPort *MainWindow::openSerialPort(const QString &portName, int baudRate)
{
    if (portName.isEmpty())
        return nullptr;

    auto *serial = new QSerialPort(this);
    serial->setPortName(portName);
    serial->setBaudRate(baudRate);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!serial->open(QIODevice::ReadWrite)) {
        qDebug() << "Failed to open port" << portName << serial->errorString();
        delete serial;
        return nullptr;
    }

    m_serialPorts.append(serial);
    qDebug() << "Serial opened:" << portName << baudRate;
    return serial;
}

void MainWindow::shutdownHardware()
{
    if (m_catReader) {
        m_catReader->stopPolling();
        m_catReader->close();
        m_catReader->deleteLater();
        m_catReader = nullptr;
    }

    if (m_keyer) {
        if (auto *pcKeyer = dynamic_cast<PCKeyer *>(m_keyer))
            pcKeyer->close();
        delete m_keyer;
        m_keyer = nullptr;
    }

    for (QSerialPort *port : std::as_const(m_serialPorts)) {
        if (port->isOpen())
            port->close();
        delete port;
    }
    m_serialPorts.clear();
    m_keyingSerial = nullptr;
    m_catSerial = nullptr;
    m_serialPortsShared = false;
    m_catPollPausedForTx = false;
    m_sendUpdateDeferred = false;
    m_catActive = false;
}

void MainWindow::initHardware()
{
    shutdownHardware();

    const QString keyingPort = theConfig.getString("Hardware/Keying_Port", "COM8");
    const QString catPort = theConfig.getString("Hardware/CAT_Port", "COM3");
    const bool catEnabled = theConfig.getBool("Cat/Enabled", true);
    const int keyingBaud = 9600;
    const int catBaud = theConfig.getInt("Hardware/CAT_Baud", 38400);

    const bool samePort = catEnabled
        && !keyingPort.isEmpty()
        && !catPort.isEmpty()
        && (QString::compare(keyingPort, catPort, Qt::CaseInsensitive) == 0);

    if (samePort) {
        const int sharedBaud = catBaud;
        m_keyingSerial = openSerialPort(keyingPort, sharedBaud);
        m_catSerial = m_keyingSerial;
        m_serialPortsShared = (m_keyingSerial != nullptr);
    } else {
        if (!keyingPort.isEmpty())
            m_keyingSerial = openSerialPort(keyingPort, keyingBaud);
        if (catEnabled && !catPort.isEmpty())
            m_catSerial = openSerialPort(catPort, catBaud);
    }

    qDebug() << "[HW] keyingSerial:" << (m_keyingSerial ? m_keyingSerial->portName() : QString())
             << "open:" << (m_keyingSerial && m_keyingSerial->isOpen())
             << "catSerial:" << (m_catSerial ? m_catSerial->portName() : QString())
             << "shared:" << m_serialPortsShared;

    initKeyer();
    initCat();
}

void MainWindow::initKeyer()
{
    if (!m_keyingSerial)
        return;

    auto *pcKeyer = new PCKeyer(this);
    pcKeyer->setSerialPort(m_keyingSerial);
    pcKeyer->setOwnsSerialPort(false);
    pcKeyer->setPortName(theConfig.getString("Hardware/Keying_Port", "COM8"));
    pcKeyer->setKeyingLine(theConfig.getString("Hardware/Keying_Line", "RTS"));
    pcKeyer->setActiveLow(theConfig.getBool("Hardware/Keying_ActiveLow", false));
    const int wpm = m_wpmSpin ? m_wpmSpin->value()
                              : theConfig.getInt("Operation/Default_WPM", 22);
    pcKeyer->setWpm(wpm);

    if (!pcKeyer->open())
        qDebug() << "Error: Keyer failed to attach serial";
    else
        qDebug() << "Keyer attached on" << m_keyingSerial->portName();

    pcKeyer->setListener(this);
    connect(pcKeyer, &PCKeyer::transmissionActiveChanged,
            this, [this](bool) { syncCatPollingWithKeyer(); });
    m_keyer = pcKeyer;
}

void MainWindow::reinitHardware()
{
    m_lastCatFreqMHz = 0.0;
    initHardware();
    if (!m_catActive)
        updateFrequencyFieldMode(false);
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // ========== 第一行：6个输入框 ==========
    QHBoxLayout *inputRow = new QHBoxLayout();
    inputRow->setSpacing(10);
    
    auto createField = [](const QString &label, int width, QLineEdit *&field) -> QVBoxLayout* {
        QVBoxLayout *layout = new QVBoxLayout();
        layout->setSpacing(2);
        
        QLabel *lbl = new QLabel(label);
        lbl->setStyleSheet("font-size: 10px; color: #888;");
        
        field = new QLineEdit();
        field->setFixedWidth(width);
        field->setStyleSheet(
            "QLineEdit {"
            "  font-size: 14px;"
            "  font-family: Consolas, 'Courier New', monospace;"
            "  font-weight: bold;"
            "  padding: 6px;"
            "  border: 2px solid #555;"
            "  border-radius: 4px;"
            "  background-color: #2b2b2b;"
            "  color: #fff;"
            "}"
            "QLineEdit:focus {"
            "  border-color: #4a9eff;"
            "  background-color: #1e3a5f;"
            "}"
        );
        
        layout->addWidget(lbl);
        layout->addWidget(field);
        return layout;
    };
    
    inputRow->addLayout(createField("呼号 Call", 110, m_callsign));
    inputRow->addLayout(createField("发送 RST", 70, m_rstSent));
    inputRow->addLayout(createField("接收 RST", 70, m_rstRcvd));
    inputRow->addLayout(createField("姓名 Name", 90, m_name));
    inputRow->addLayout(createField("地址 QTH", 130, m_qth));
    inputRow->addLayout(createField("备注 Note", 200, m_comment));
    inputRow->addLayout(createField("频率 MHz", 100, m_freqEdit));
    m_freqEdit->setPlaceholderText(tr("手动 MHz"));

    auto *rstValidator = new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^\\d{0,3}$")), this);
    m_rstSent->setMaxLength(3);
    m_rstRcvd->setMaxLength(3);
    m_rstSent->setValidator(rstValidator);
    m_rstRcvd->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^\\d{0,3}$")), this));
    m_rstSent->installEventFilter(this);
    m_rstRcvd->installEventFilter(this);
    connect(m_freqEdit, &QLineEdit::editingFinished,
            this, &MainWindow::onManualFrequencyEdited);

    // 清除按钮
    m_clearBtn = new QPushButton(tr("清除"));
    m_clearBtn->setFixedSize(60, 50);
    m_clearBtn->setStyleSheet(
        "QPushButton {"
        "  font-size: 11px;"
        "  background-color: #8b0000;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 4px;"
        "}"
        "QPushButton:hover { background-color: #a50000; }"
        "QPushButton:pressed { background-color: #5c0000; }"
    );
    connect(m_clearBtn, &QPushButton::clicked, this, &MainWindow::onClearClicked);
    inputRow->addWidget(m_clearBtn);

    m_logBtn = new QPushButton(tr("记录\nCtrl+L"));
    m_logBtn->setFixedSize(60, 50);
    m_logBtn->setStyleSheet(
        "QPushButton {"
        "  font-size: 11px;"
        "  background-color: #1a5f1a;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 4px;"
        "}"
        "QPushButton:hover { background-color: #227722; }"
        "QPushButton:pressed { background-color: #0d3d0d; }"
    );
    connect(m_logBtn, &QPushButton::clicked, this, &MainWindow::onLogQsoClicked);
    inputRow->addWidget(m_logBtn);
    
    inputRow->addStretch();
    
    // ========== 第二行：呼号解析信息 + 选项 ==========
    auto *infoRow = new QHBoxLayout();
    infoRow->setSpacing(4);

    m_infoLabel = new QLabel("输入呼号查看前缀信息");
    m_infoLabel->setStyleSheet(
        "font-size: 12px;"
        "color: #666;"
        "padding: 8px;"
        "background-color: #252525;"
        "border-radius: 4px;"
        "font-family: Consolas, monospace;"
    );
    m_infoLabel->setFixedHeight(36);
    m_infoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *settingsBtn = new QToolButton();
    settingsBtn->setFixedSize(32, 36);
    settingsBtn->setToolTip(tr("选项..."));
    settingsBtn->setCursor(Qt::PointingHandCursor);
    settingsBtn->setAutoRaise(true);
    const QIcon gearIcon = QIcon::fromTheme(QStringLiteral("preferences-system"));
    if (!gearIcon.isNull()) {
        settingsBtn->setIcon(gearIcon);
        settingsBtn->setIconSize(QSize(18, 18));
    } else {
        settingsBtn->setText(QStringLiteral("\u2699"));
    }
    settingsBtn->setStyleSheet(
        "QToolButton {"
        "  border: none;"
        "  border-radius: 4px;"
        "  background-color: #252525;"
        "  color: #999;"
        "  font-size: 17px;"
        "}"
        "QToolButton:hover { background-color: #3a3a3a; color: #ddd; }"
        "QToolButton:pressed { background-color: #1a1a1a; }"
    );
    connect(settingsBtn, &QToolButton::clicked, this, &MainWindow::onOpenSettings);

    infoRow->addWidget(m_infoLabel, 1);
    infoRow->addWidget(settingsBtn);
    
    // ========== 第三行：WPM + 摩尔斯发送输入框 ==========
    auto *sendRow = new QHBoxLayout();
    sendRow->setSpacing(8);

    auto *wpmLay = new QVBoxLayout();
    wpmLay->setSpacing(2);
    auto *wpmLbl = new QLabel(tr("WPM"));
    wpmLbl->setStyleSheet("font-size: 10px; color: #888;");
    m_wpmSpin = new QSpinBox();
    m_wpmSpin->setRange(5, 40);
    m_wpmSpin->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    m_wpmSpin->setFixedWidth(72);
    m_wpmSpin->setFixedHeight(38);
    m_wpmSpin->setAlignment(Qt::AlignCenter);
    m_wpmSpin->setKeyboardTracking(false);
    m_wpmSpin->setValue(theConfig.getInt("Operation/Default_WPM", 22));
    m_wpmSpin->setStyleSheet(
        "QSpinBox {"
        "  font-size: 14px;"
        "  font-family: Consolas, 'Courier New', monospace;"
        "  font-weight: bold;"
        "  padding: 4px;"
        "  border: 2px solid #555;"
        "  border-radius: 4px;"
        "  background-color: #2b2b2b;"
        "  color: #fff;"
        "}"
        "QSpinBox:focus { border-color: #4a9eff; background-color: #1e3a5f; }"
        "QSpinBox::up-button, QSpinBox::down-button {"
        "  width: 18px; background-color: #3a3a3a; border: none;"
        "}"
        "QSpinBox::up-button:hover, QSpinBox::down-button:hover {"
        "  background-color: #4a4a4a;"
        "}"
        "QSpinBox::up-arrow { image: none; border-left: 4px solid transparent;"
        "  border-right: 4px solid transparent; border-bottom: 6px solid #ccc; width: 0; height: 0; }"
        "QSpinBox::down-arrow { image: none; border-left: 4px solid transparent;"
        "  border-right: 4px solid transparent; border-top: 6px solid #ccc; width: 0; height: 0; }"
    );
    wpmLay->addWidget(wpmLbl);
    wpmLay->addWidget(m_wpmSpin);
    sendRow->addLayout(wpmLay);
    connect(m_wpmSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onWpmChanged);

    m_sendInput = new QTextEdit();
    m_sendInput->setPlaceholderText("Alt+K 聚焦，输入即发送，可回退编辑未发字符...");
    m_sendInput->setFixedHeight(50);
    m_sendInput->setStyleSheet(
        "QTextEdit {"
        "  font-size: 16px;"
        "  font-family: Consolas, 'Courier New', monospace;"
        "  font-weight: bold;"
        "  padding: 8px;"
        "  border: 2px solid #555;"
        "  border-radius: 4px;"
        "  background-color: #1a1a1a;"
        "  color: #fff;"
        "}"
    );
    sendRow->addWidget(m_sendInput, 1);

    // 发送定时器
    // m_sendTimer = new QTimer(this);
    // m_sendTimer->setSingleShot(true);
    // connect(m_sendTimer, &QTimer::timeout, this, &MainWindow::sendNextChar);
    
    // 连接输入变化信号
    // connect(m_sendInput, &QTextEdit::textChanged, 
    //         this, &MainWindow::onTextChanged);
    
    // 组装
    mainLayout->addLayout(inputRow);
    mainLayout->addLayout(infoRow);
    mainLayout->addLayout(sendRow);
    
    // 第四行：快捷键按钮
    setupMacroButtons(mainLayout);

    m_qsoHistoryPanel = new QFrame(centralWidget);
    m_qsoHistoryPanel->setVisible(false);
    m_qsoHistoryPanel->setStyleSheet(
        "QFrame { background-color: #2a2a2a; border: 1px solid #444; border-radius: 4px; }");
    auto *historyLay = new QVBoxLayout(m_qsoHistoryPanel);
    historyLay->setContentsMargins(8, 6, 8, 6);
    historyLay->setSpacing(4);
    m_qsoHistoryTitle = new QLabel(m_qsoHistoryPanel);
    m_qsoHistoryTitle->setStyleSheet(
        "color: #9ad; font-size: 11px; font-weight: bold; font-family: Consolas, monospace;");
    m_qsoHistoryBody = new QLabel(m_qsoHistoryPanel);
    m_qsoHistoryBody->setWordWrap(true);
    m_qsoHistoryBody->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_qsoHistoryBody->setStyleSheet(
        "color: #ccc; font-size: 11px; font-family: Consolas, 'Courier New', monospace;"
        " line-height: 130%;");
    historyLay->addWidget(m_qsoHistoryTitle);
    historyLay->addWidget(m_qsoHistoryBody);
    mainLayout->addWidget(m_qsoHistoryPanel);
    
    // 信号连接（淡色通联区：键入时先释放再处理呼号逻辑）
    const auto hookQsoFadeRelease = [this](QLineEdit *field) {
        connect(field, &QLineEdit::textChanged,
                this, &MainWindow::onQsoFieldEditedAfterLog);
    };
    hookQsoFadeRelease(m_callsign);
    hookQsoFadeRelease(m_rstSent);
    hookQsoFadeRelease(m_rstRcvd);
    hookQsoFadeRelease(m_name);
    hookQsoFadeRelease(m_qth);
    hookQsoFadeRelease(m_comment);

    connect(m_callsign, &QLineEdit::textChanged,
            this, &MainWindow::onCallsignChanged);
    connect(m_callsign, &QLineEdit::editingFinished,
            this, &MainWindow::onCallsignEditingFinished);
    
            // 修改：输入框变化时传递给 Keyer
    connect(m_sendInput, &QTextEdit::textChanged, 
            this, &MainWindow::onSendInputChanged);
    m_sendInput->installEventFilter(this);
    m_sendInput->viewport()->installEventFilter(this);

    m_sendClearTimer = new QTimer(this);
    m_sendClearTimer->setSingleShot(true);
    connect(m_sendClearTimer, &QTimer::timeout, this, &MainWindow::onPostSendIdleClearTimeout);

    // Tab 顺序
    setTabOrder(m_callsign, m_rstSent);
    setTabOrder(m_rstSent, m_rstRcvd);
    setTabOrder(m_rstRcvd, m_name);
    setTabOrder(m_name, m_qth);
    setTabOrder(m_qth, m_comment);
    setTabOrder(m_comment, m_freqEdit);
    setTabOrder(m_freqEdit, m_sendInput);
    
    // Alt+K 聚焦；Alt+C/N/Q/M 追加通联区或本台呼号到发送框
    QShortcut *altK = new QShortcut(QKeySequence("Alt+K"), this);
    connect(altK, &QShortcut::activated, [this]() {
        m_sendInput->setFocus();
    });

    const auto appendFieldShortcut = [this](const QKeySequence &seq, QLineEdit *field) {
        auto *sc = new QShortcut(seq, this);
        connect(sc, &QShortcut::activated, [this, field]() {
            if (field->text().trimmed().isEmpty())
                return;
            appendToSendInput(field->text());
            m_sendInput->setFocus();
        });
    };
    appendFieldShortcut(QKeySequence(QStringLiteral("Alt+C")), m_callsign);
    appendFieldShortcut(QKeySequence(QStringLiteral("Alt+N")), m_name);
    appendFieldShortcut(QKeySequence(QStringLiteral("Alt+Q")), m_qth);

    auto *altM = new QShortcut(QKeySequence(QStringLiteral("Alt+M")), this);
    connect(altM, &QShortcut::activated, [this]() {
        const QString myCall =
            theConfig.getString(QStringLiteral("Station/MY_CALL"), QString()).trimmed();
        if (myCall.isEmpty())
            return;
        appendToSendInput(myCall);
        m_sendInput->setFocus();
    });

    QShortcut *esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(esc, &QShortcut::activated, this, &MainWindow::onAbortSend);

    QShortcut *logShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this);
    connect(logShortcut, &QShortcut::activated, this, &MainWindow::onLogQsoClicked);
    
    // 深色主题
    centralWidget->setStyleSheet("background-color: #333;");
}

void MainWindow::onOpenSettings()
{
    SettingsDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    applySettingsFromConfig();
}

void MainWindow::refreshShortcutButtonLabels()
{
    for (int i = 0; i < 8; ++i) {
        if (!m_macroButtons[i])
            continue;
        const QString title = theConfig.getString(
            QString("Shortcuts/F%1_Title").arg(i + 1), QString("F%1").arg(i + 1));
        m_macroButtons[i]->setText(title);
    }
}

bool MainWindow::isCatFrequencyQueryActive() const
{
    if (!m_serialPortsShared || !m_catReader)
        return false;
    return m_catReader->isFrequencyQueryActive();
}

void MainWindow::resumeCatPollingIfIdle()
{
    if (!m_catReader || !m_catActive)
        return;
    if (m_keyer && m_keyer->isSending())
        return;
    if (!m_catPollPausedForTx)
        return;

    m_catPollPausedForTx = false;
    const int ms = theConfig.getInt("Cat/Poll_Interval_Ms", 800);
    m_catReader->startPolling(ms);
}

void MainWindow::syncCatPollingWithKeyer()
{
    if (!m_catReader || !m_catActive)
        return;

    if (m_keyer && m_keyer->isSending()) {
        if (!m_catPollPausedForTx) {
            m_catReader->stopPolling();
            m_catPollPausedForTx = true;
        }
    } else {
        resumeCatPollingIfIdle();
    }
}

void MainWindow::applyWpmToKeyer(int wpm)
{
    const int clamped = qBound(5, wpm, 40);
    theConfig.set("Operation/Default_WPM", clamped);
    if (m_keyer)
        m_keyer->setWpm(clamped);
}

void MainWindow::onWpmChanged(int wpm)
{
    applyWpmToKeyer(wpm);
}

void MainWindow::applySettingsFromConfig()
{
    loadAdifLog();
    loadPrefixDatabase();
    refreshShortcutButtonLabels();
    if (m_wpmSpin) {
        m_wpmSpin->blockSignals(true);
        m_wpmSpin->setValue(theConfig.getInt("Operation/Default_WPM", 22));
        m_wpmSpin->blockSignals(false);
    }
    if (m_keyer) {
        applyWpmToKeyer(theConfig.getInt("Operation/Default_WPM", 22));
        if (auto *pcKeyer = dynamic_cast<PCKeyer *>(m_keyer)) {
            pcKeyer->setKeyingLine(theConfig.getString("Hardware/Keying_Line", "RTS"));
            pcKeyer->setActiveLow(theConfig.getBool("Hardware/Keying_ActiveLow", false));
        }
    }
    reinitHardware();
    QMessageBox::information(
        this, tr("选项"),
        tr("配置已保存。\n串口与 CAT/键控相关项已重新应用。"));
}

void MainWindow::initCat()
{
    m_catActive = false;

    if (!theConfig.getBool("Cat/Enabled", true)) {
        updateFrequencyFieldMode(false);
        return;
    }

    m_catReader = CatReaderFactory::create(this);
    if (!m_catReader) {
        updateFrequencyFieldMode(false);
        return;
    }

    if (auto *yaesu = dynamic_cast<YaesuCatReader *>(m_catReader)) {
        yaesu->setSerialPort(m_catSerial);
        if (m_serialPortsShared) {
            yaesu->setBeforeTransaction([this]() {
                if (auto *pcKeyer = dynamic_cast<PCKeyer *>(m_keyer))
                    pcKeyer->releaseKeyingLines();
            });
        }
    } else if (auto *icom = dynamic_cast<IcomCatReader *>(m_catReader)) {
        icom->setSerialPort(m_catSerial);
        if (m_serialPortsShared) {
            icom->setBeforeTransaction([this]() {
                if (auto *pcKeyer = dynamic_cast<PCKeyer *>(m_keyer))
                    pcKeyer->releaseKeyingLines();
            });
        }
    }

    connect(m_catReader, &ICatReader::frequencyChanged,
            this, &MainWindow::onCatFrequencyChanged);
    connect(m_catReader, &ICatReader::connectionStateChanged,
            this, &MainWindow::onCatConnectionChanged);
    connect(m_catReader, &ICatReader::frequencyQueryFinished,
            this, &MainWindow::onCatFrequencyQueryFinished);

    if (!m_catSerial) {
        updateFrequencyFieldMode(false);
        m_freqEdit->setToolTip(tr("CAT 串口打开失败，请检查配置"));
        return;
    }

    if (m_catReader->open()) {
        m_catActive = true;
        updateFrequencyFieldMode(true);
        const int ms = theConfig.getInt("Cat/Poll_Interval_Ms", 800);
        m_catReader->startPolling(ms);
        syncCatPollingWithKeyer();
        updateCatFreqPlaceholder();
    } else {
        m_catActive = false;
        updateFrequencyFieldMode(false);
        m_freqEdit->setToolTip(m_catReader->lastError());
    }
}

void MainWindow::updateFrequencyFieldMode(bool catConnected)
{
    if (!m_freqEdit)
        return;

    m_freqEdit->setReadOnly(catConnected);
    if (catConnected) {
        m_freqEdit->setPlaceholderText(tr("CAT"));
        m_freqEdit->setStyleSheet(
            "QLineEdit {"
            "  font-size: 14px;"
            "  font-family: Consolas, 'Courier New', monospace;"
            "  font-weight: bold;"
            "  padding: 6px;"
            "  border: 2px solid #3a6a3a;"
            "  border-radius: 4px;"
            "  background-color: #1e2e1e;"
            "  color: #aaffaa;"
            "}"
        );
    } else {
        m_freqEdit->setPlaceholderText(tr("手动 MHz"));
        m_freqEdit->setStyleSheet(
            "QLineEdit {"
            "  font-size: 14px;"
            "  font-family: Consolas, 'Courier New', monospace;"
            "  font-weight: bold;"
            "  padding: 6px;"
            "  border: 2px solid #555;"
            "  border-radius: 4px;"
            "  background-color: #2b2b2b;"
            "  color: #fff;"
            "}"
            "QLineEdit:focus {"
            "  border-color: #4a9eff;"
            "  background-color: #1e3a5f;"
            "}"
        );
    }
}

void MainWindow::onCatConnectionChanged(bool connected)
{
    if (!connected) {
        m_catActive = false;
        updateFrequencyFieldMode(false);
    }
}

void MainWindow::updateCatFreqPlaceholder()
{
    if (!m_freqEdit || !m_catActive)
        return;
    if (m_lastCatFreqMHz > 0.0) {
        m_freqEdit->setPlaceholderText(tr("CAT"));
        return;
    }
    const QString err = m_catReader ? m_catReader->lastError() : QString();
    m_freqEdit->setPlaceholderText(err.isEmpty() ? tr("CAT 读频中…") : err);
}

void MainWindow::onCatFrequencyChanged(double mhz)
{
    if (!m_freqEdit)
        return;

    if (m_catActive && m_lastCatFreqMHz > 0.0
        && qAbs(mhz - m_lastCatFreqMHz) > 0.0005) {
        clearQsoFieldsOnly();
    }
    m_lastCatFreqMHz = mhz;

    m_freqEdit->blockSignals(true);
    m_freqEdit->setText(formatFrequencyMHz(mhz));
    m_freqEdit->blockSignals(false);
    updateCatFreqPlaceholder();
}

void MainWindow::onManualFrequencyEdited()
{
    if (m_catActive || !m_freqEdit)
        return;
    const double mhz = parseFrequencyMHz(m_freqEdit->text());
    if (mhz > 0.0) {
        if (m_manualFreqMHz > 0.0 && qAbs(mhz - m_manualFreqMHz) > 0.0005)
            clearQsoFieldsOnly();
        m_manualFreqMHz = mhz;
        m_freqEdit->setText(formatFrequencyMHz(mhz));
    }
}

void MainWindow::resetQsoTiming()
{
    m_qsoTimeOn = QDateTime();
}

void MainWindow::hideQsoHistoryPanel()
{
    if (!m_qsoHistoryPanel)
        return;
    m_qsoHistoryPanel->setVisible(false);
    m_qsoHistoryShownCall.clear();
}

bool MainWindow::isCompleteCallsign(const QString &call) const
{
    const QString c = call.trimmed().toUpper();
    if (c.length() < 3)
        return false;
    static const QRegularExpression valid(QStringLiteral("^[A-Z0-9/]+$"));
    if (!valid.match(c).hasMatch())
        return false;
    return c.contains(QRegularExpression(QStringLiteral("[A-Z]")));
}

namespace {

QString adifField(const adif::Record &rec, const char *key)
{
    return QString::fromStdString(rec.get_field(key)).trimmed();
}

QString formatAdifDate(QString d)
{
    d = d.trimmed();
    if (d.length() == 8)
        return d.left(4) + QLatin1Char('-') + d.mid(4, 2) + QLatin1Char('-') + d.mid(6, 2);
    return d;
}

QString formatAdifTimeCompact(QString t)
{
    t = t.trimmed();
    t.remove(QLatin1Char(':'));
    if (t.length() >= 6)
        return t.left(6);
    return t;
}

QString formatAdifFreqMHz(const QString &raw)
{
    bool ok = false;
    const double mhz = QLocale::c().toDouble(raw, &ok);
    if (ok && mhz > 0.0)
        return QLocale::c().toString(mhz, 'f', 3);
    return raw;
}

QString formatHistoryLine(const adif::Record &rec)
{
    QStringList parts;
    parts << formatAdifDate(adifField(rec, "qso_date"))
          << formatAdifTimeCompact(adifField(rec, "time_on"))
          << formatAdifFreqMHz(adifField(rec, "freq"))
          << adifField(rec, "mode");

    const auto appendIfSet = [&](const char *key) {
        const QString v = adifField(rec, key);
        if (!v.isEmpty())
            parts.append(v);
    };
    appendIfSet("name");
    appendIfSet("qth");
    appendIfSet("comment");

    return parts.join(QStringLiteral(", "));
}

} // namespace

void MainWindow::showQsoHistoryForCall(const QString &call)
{
    if (!m_qsoLog || !m_qsoHistoryPanel || !m_qsoHistoryBody)
        return;

    const QString c = call.trimmed().toUpper();
    const std::vector<adif::Record> rows = m_qsoLog->lastRecordsForCall(c, 5);
    if (rows.empty()) {
        hideQsoHistoryPanel();
        return;
    }

    QStringList lines;
    lines.reserve(static_cast<int>(rows.size()));
    for (const adif::Record &rec : rows)
        lines.append(formatHistoryLine(rec));

    m_qsoHistoryTitle->setText(c);
    m_qsoHistoryBody->setText(lines.join(QLatin1Char('\n')));
    m_qsoHistoryShownCall = c;
    m_qsoHistoryPanel->setVisible(true);
    adjustSize();
}

void MainWindow::onCallsignEditingFinished()
{
    const QString c = m_callsign->text().trimmed();
    if (!isCompleteCallsign(c)) {
        hideQsoHistoryPanel();
        return;
    }
    showQsoHistoryForCall(c);
}

void MainWindow::setQsoFieldsFaded(bool faded)
{
    m_qsoFieldsFaded = faded;
    m_qsoFadedSnapshots.clear();
    if (faded) {
        for (QLineEdit *field :
             {m_callsign, m_rstSent, m_rstRcvd, m_name, m_qth, m_comment}) {
            m_qsoFadedSnapshots.insert(field, field->text());
        }
    }
    const QString style = faded ? QString::fromUtf8(kQsoFieldStyleFaded)
                                : QString::fromUtf8(kQsoFieldStyleNormal);
    for (QLineEdit *field :
         {m_callsign, m_rstSent, m_rstRcvd, m_name, m_qth, m_comment}) {
        field->setStyleSheet(style);
    }
}

QString MainWindow::newInputAfterFadedSnapshot(QLineEdit *field) const
{
    if (!field)
        return QString();
    const QString baseline = m_qsoFadedSnapshots.value(field);
    const QString current = field->text();
    if (current.startsWith(baseline))
        return current.mid(baseline.length());
    return current;
}

void MainWindow::onQsoFieldEditedAfterLog()
{
    if (!m_qsoFieldsFaded || m_suppressQsoFieldChange)
        return;

    auto *edit = qobject_cast<QLineEdit *>(sender());
    if (!edit)
        return;

    const QString pending = newInputAfterFadedSnapshot(edit);

    m_qsoFieldsFaded = false;
    m_suppressQsoFieldChange = true;
    setQsoFieldsFaded(false);

    for (QLineEdit *field :
         {m_callsign, m_rstSent, m_rstRcvd, m_name, m_qth, m_comment}) {
        field->blockSignals(true);
        field->clear();
        field->blockSignals(false);
    }
    hideQsoHistoryPanel();
    resetQsoTiming();

    edit->setText(pending);
    edit->setCursorPosition(pending.length());
    m_suppressQsoFieldChange = false;
}

void MainWindow::clearQsoFieldsOnly()
{
    hideQsoHistoryPanel();
    m_suppressQsoFieldChange = true;
    if (m_qsoFieldsFaded)
        setQsoFieldsFaded(false);
    m_qsoFieldsFaded = false;
    m_qsoFadedSnapshots.clear();
    m_callsign->clear();
    m_rstSent->clear();
    m_rstRcvd->clear();
    m_name->clear();
    m_qth->clear();
    m_comment->clear();
    m_suppressQsoFieldChange = false;
    resetQsoTiming();
}

QString MainWindow::defaultCtyPath() const
{
    const QString besideExe =
        QCoreApplication::applicationDirPath() + QStringLiteral("/data/cty.dat");
    if (QFile::exists(besideExe))
        return besideExe;

    const QString devTree = QCoreApplication::applicationDirPath()
        + QStringLiteral("/../data/cty.dat");
    if (QFile::exists(devTree))
        return QFileInfo(devTree).absoluteFilePath();

    return besideExe;
}

void MainWindow::loadPrefixDatabase()
{
    if (!m_prefixDb)
        return;

    const QString path = theConfig.getString("Files/CTY_Path", defaultCtyPath());
    if (!m_prefixDb->loadFromFile(path)) {
        qDebug() << "CTY load failed:" << m_prefixDb->lastError();
        return;
    }
    qDebug() << "CTY loaded:" << path << "prefixes:" << m_prefixDb->prefixCount();
}

void MainWindow::loadAdifLog()
{
    if (!m_qsoLog)
        return;

    const QString path = theConfig.getString(
        "Files/ADIF_Path",
        QCoreApplication::applicationDirPath() + "/log.adif");

    if (!m_qsoLog->load(path)) {
        qDebug() << "ADIF log load failed:" << path;
        return;
    }
    qDebug() << "ADIF log loaded:" << path << "records:" << m_qsoLog->count();
}

adif::Record MainWindow::buildQsoRecordFromUi()
{
    adif::Record rec;
    const QDateTime timeOff = QDateTime::currentDateTime();
    const QDateTime timeOn = m_qsoTimeOn.isValid() ? m_qsoTimeOn : timeOff;

    rec.set_field("qso_date", timeOn.toString("yyyyMMdd").toStdString());
    rec.set_field("time_on", timeOn.toString("HHmmss").toStdString());
    rec.set_field("time_off", timeOff.toString("HHmmss").toStdString());

    const double mhz = currentFrequencyMHz();
    if (mhz > 0.0)
        rec.set_field("freq", formatFrequencyMHz(mhz).toStdString());

    rec.set_field("mode", "CW");
    rec.set_field("call", m_callsign->text().trimmed().toUpper().toStdString());

    auto rstField = [](const QLineEdit *edit) -> std::string {
        QString t = edit->text().trimmed();
        if (t.isEmpty())
            t = QStringLiteral("599");
        return t.toStdString();
    };
    rec.set_field("rst_sent", rstField(m_rstSent));
    rec.set_field("rst_rcvd", rstField(m_rstRcvd));

    const QString name = m_name->text().trimmed();
    if (!name.isEmpty())
        rec.set_field("name", name.toStdString());

    const QString qth = m_qth->text().trimmed();
    if (!qth.isEmpty())
        rec.set_field("qth", qth.toStdString());

    const QString comment = m_comment->text().trimmed();
    if (!comment.isEmpty())
        rec.set_field("comment", comment.toStdString());

    const QString myRig = theConfig.getString("Station/MY_RIG", "").trimmed();
    if (!myRig.isEmpty())
        rec.set_field("my_rig", myRig.toStdString());

    return rec;
}

void MainWindow::clearUiAfterQsoLogged()
{
    stopLoop();
    cancelPostSendClearTimer();

    if (m_keyer)
        m_keyer->clear();
    m_sentIndex = 0;
    m_lastValidSendPlain.clear();

    setQsoFieldsFaded(true);
    m_sendInput->clear();
    syncCatPollingWithKeyer();
    m_callsign->setFocus();
}

void MainWindow::onLogQsoClicked()
{
    if (!m_qsoLog)
        return;

    const QString call = m_callsign->text().trimmed();
    if (call.isEmpty()) {
        QMessageBox::warning(this, tr("记录日志"), tr("请先填写对方呼号。"));
        m_callsign->setFocus();
        return;
    }

    const adif::Record rec = buildQsoRecordFromUi();
    if (!rec.iscomplete()) {
        QMessageBox::warning(this, tr("记录日志"), tr("日志字段不完整，无法写入。"));
        return;
    }

    if (!m_qsoLog->appendAndSave(rec)) {
        QMessageBox::warning(
            this, tr("记录日志"),
            tr("无法写入日志文件：\n%1").arg(m_qsoLog->filePath()));
        return;
    }

    hideQsoHistoryPanel();
    clearUiAfterQsoLogged();
    statusBar()->showMessage(
        tr("已记录 %1，日志共 %2 条 → %3")
            .arg(call.toUpper())
            .arg(m_qsoLog->count())
            .arg(m_qsoLog->filePath()),
        4000);
}

QString MainWindow::formatFrequencyMHz(double mhz) const
{
    return QLocale::c().toString(mhz, 'f', 3);
}

double MainWindow::parseFrequencyMHz(const QString &text) const
{
    bool ok = false;
    QString t = text.trimmed();
    t.remove(QLatin1Char(' '));
    const double v = QLocale::c().toDouble(t, &ok);
    return (ok && v > 0.0) ? v : 0.0;
}

double MainWindow::currentFrequencyMHz() const
{
    if (m_catActive && m_catReader)
        return m_catReader->frequencyMHz();
    return m_manualFreqMHz;
}

void MainWindow::cancelPostSendClearTimer()
{
    if (m_sendClearTimer)
        m_sendClearTimer->stop();
}

void MainWindow::onPostSendIdleClearTimeout()
{
    if (!m_sendInput)
        return;
    m_inUserEdit = true;
    m_sendInput->blockSignals(true);
    m_sendInput->clear();
    m_sendInput->blockSignals(false);
    m_lastValidSendPlain.clear();
    if (m_keyer)
        m_keyer->clear();
    m_inUserEdit = false;
}

void MainWindow::updateSendDisplay()
{
    QString text = m_sendInput->toPlainText().toUpper();
    if (text.isEmpty()) return;
    
    int currentIndex = m_keyer->currentIndex();
    bool isSending = m_keyer->isSending();
    int minCursor = (isSending ? m_keyer->firstEditableIndex() : 0);
    
    m_inUserEdit = true;
    
    // 保存光标
    QTextCursor cursor = m_sendInput->textCursor();
    int cursorPos = cursor.position();
    
    // 清空并重新设置带颜色的文本
    m_sendInput->clear();
    
    for (int i = 0; i < text.length(); i++) {
        QTextCharFormat fmt;
        if (i < currentIndex) {
            fmt.setForeground(QColor("#666666"));  // 已发送：灰
        } else if (i == currentIndex && isSending) {
            fmt.setForeground(QColor("#00aa00"));  // 发送中：绿
        } else {
            fmt.setForeground(QColor("#ffffff"));  // 待发送：白
        }
        QFont font("Consolas", 14, QFont::Bold);
        fmt.setFont(font);

        // fmt.setFontFamily("Consolas");
        // fmt.setFontPointSize(14);
        // fmt.setFontWeight(QFont::Bold);
        
        cursor.insertText(QString(text[i]), fmt);
    }
    
    // 恢复光标到编辑位置（不得进入已发/正在发区域）
    cursor.setPosition(qMax(cursorPos, minCursor));
    m_sendInput->setTextCursor(cursor);
    
    m_inUserEdit = false;
}

void MainWindow::clampSendCursor()
{
    if (!m_keyer || !m_keyer->isSending())
        return;
    const int fe = m_keyer->firstEditableIndex();
    QTextCursor c = m_sendInput->textCursor();
    const int p = c.position();
    const int a = c.anchor();
    const int lo = qMin(p, a);
    const int hi = qMax(p, a);
    if (lo >= fe)
        return;
    if (!c.hasSelection()) {
        c.setPosition(fe);
        m_sendInput->setTextCursor(c);
        return;
    }
    if (hi < fe) {
        c.setPosition(fe);
        c.clearSelection();
    } else {
        c.setPosition(fe);
        c.setPosition(hi, QTextCursor::KeepAnchor);
    }
    m_sendInput->setTextCursor(c);
}

void MainWindow::appendToSendInput(const QString &text)
{
    const QString t = text.trimmed();
    if (t.isEmpty() || !m_sendInput)
        return;

    cancelPostSendClearTimer();
    m_inUserEdit = true;
    m_sendInput->moveCursor(QTextCursor::End);
    m_sendInput->insertPlainText(t.toUpper());
    m_inUserEdit = false;
    applySendInputUpdate(true);

    QTextCursor cursor = m_sendInput->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_sendInput->setTextCursor(cursor);
}

void MainWindow::applySendInputUpdate(bool allowDeferForCatQuery)
{
    if (m_inUserEdit || !m_keyer || !m_sendInput)
        return;

    cancelPostSendClearTimer();

    const QString text = m_sendInput->toPlainText().toUpper();

    if (m_keyer->isSending()) {
        const QString req = m_keyer->requiredSendPrefix();
        if (text.length() < req.length() || !text.startsWith(req)) {
            m_inUserEdit = true;
            m_sendInput->blockSignals(true);
            m_sendInput->setPlainText(m_lastValidSendPlain);
            QTextCursor cur = m_sendInput->textCursor();
            cur.movePosition(QTextCursor::End);
            m_sendInput->setTextCursor(cur);
            m_sendInput->blockSignals(false);
            m_inUserEdit = false;
            m_sendUpdateDeferred = false;
            return;
        }
    }

    if (text.isEmpty()) {
        m_sendUpdateDeferred = false;
        m_lastValidSendPlain.clear();
        m_keyer->clear();
        updateSendDisplay();
        syncCatPollingWithKeyer();
        return;
    }

    if (allowDeferForCatQuery && m_serialPortsShared && isCatFrequencyQueryActive()) {
        m_sendUpdateDeferred = true;
        return;
    }

    m_sendUpdateDeferred = false;
    m_keyer->update(text);
    updateSendDisplay();
    m_lastValidSendPlain = text;
    syncCatPollingWithKeyer();
}

void MainWindow::onSendInputChanged()
{
    applySendInputUpdate(true);
}

void MainWindow::onCatFrequencyQueryFinished()
{
    updateCatFreqPlaceholder();
    if (!m_sendUpdateDeferred)
        return;
    applySendInputUpdate(false);
}

// ========== IKeyerListener 回调实现 ==========

void MainWindow::onKeyerCharComplete(int index, QChar ch)
{
    Q_UNUSED(ch)
    m_sentIndex = index + 1;  // 移动到下一个
    
    // // 检查用户是否已输入更多内容
    // QString currentText = m_sendInput->toPlainText().toUpper();
    // if (currentText.length() > m_sendBuffer.length()) {
    //     // 用户追加了内容，扩展发送
    //     m_sendBuffer = currentText;
    // }
    
    updateSendDisplay();
}

void MainWindow::onKeyerBufferEmpty()
{
    if (m_keyer)
        m_sentIndex = m_keyer->currentIndex();
    updateSendDisplay();
    syncCatPollingWithKeyer();

    const int delaySec = theConfig.getInt("Operation/Send_Clear_Delay_Sec", 2);
    if (!m_isLooping && m_sendClearTimer && delaySec > 0 && m_sendInput) {
        const QString plain = m_sendInput->toPlainText();
        if (!plain.isEmpty())
            m_sendClearTimer->start(delaySec * 1000);
    }
    
    if (m_isLooping && m_loopTimer) {
        int interval = theConfig.getInt("Operation/CQ_Interval", 4);
        m_loopTimer->start(interval * 1000);
    }

    syncCatPollingWithKeyer();
}

void MainWindow::onAbortSend()
{
    stopLoop();
    cancelPostSendClearTimer();

    if (m_keyer)
        m_keyer->clear();
    m_sentIndex = 0;
    m_lastValidSendPlain.clear();

    if (m_sendInput) {
        m_inUserEdit = true;
        m_sendInput->blockSignals(true);
        m_sendInput->clear();
        m_sendInput->blockSignals(false);
        m_sendInput->setFocus();
    }

    m_inUserEdit = false;
    syncCatPollingWithKeyer();
}

void MainWindow::onClearClicked()
{
    clearQsoFieldsOnly();
    m_callsign->setFocus();
}

void MainWindow::setupMacroButtons(QVBoxLayout *mainLayout)
{
    // 单击/双击区分定时器
    m_clickTimer = new QTimer(this);
    m_clickTimer->setSingleShot(true);
    m_clickTimer->setInterval(250);
    connect(m_clickTimer, &QTimer::timeout, [this]() {
        if (m_pendingClickIndex >= 0) {
            triggerMacro(m_pendingClickIndex);
            m_pendingClickIndex = -1;
        }
    });
    
    // 循环间隔定时器
    m_loopTimer = new QTimer(this);
    m_loopTimer->setSingleShot(true);
    connect(m_loopTimer, &QTimer::timeout, [this]() {
        if (m_isLooping && m_loopingIndex >= 0) {
            triggerMacro(m_loopingIndex);
        }
    });
    
    QHBoxLayout *macroRow = new QHBoxLayout();
    macroRow->setSpacing(6);
    
    for (int i = 0; i < 8; ++i) {
        QString title = theConfig.getString(QString("Shortcuts/F%1_Title").arg(i+1), QString("F%1").arg(i+1));
        m_macroButtons[i] = new QPushButton(title, this);
        m_macroButtons[i]->setFixedHeight(36);
        m_macroButtons[i]->setContextMenuPolicy(Qt::CustomContextMenu);
        m_macroButtons[i]->installEventFilter(this);
        
        // 右键菜单
        connect(m_macroButtons[i], &QPushButton::customContextMenuRequested, [this, i](const QPoint&) {
            editMacro(i);
        });
        
        macroRow->addWidget(m_macroButtons[i]);
    }
    
    updateMacroButtonStyles();
    mainLayout->addLayout(macroRow);
    
    // F1-F8 快捷键绑定（键盘仍视为单次发送）
    for (int i = 0; i < 8; ++i) {
        QShortcut *shortcut = new QShortcut(QKeySequence(QString("F%1").arg(i+1)), this);
        connect(shortcut, &QShortcut::activated, [this, i]() {
            triggerMacro(i);
        });
    }
}

void MainWindow::updateMacroButtonStyles()
{
    for (int i = 0; i < 8; ++i) {
        bool isLooping = (m_isLooping && m_loopingIndex == i);
        QString style = isLooping
            ? QString(
                "QPushButton {"
                "  font-size: 12px;"
                "  font-family: Consolas, monospace;"
                "  font-weight: bold;"
                "  background-color: #1a5f1a;"
                "  color: #aaffaa;"
                "  border: 2px solid #4aff4a;"
                "  border-radius: 4px;"
                "  padding: 4px 8px;"
                "}"
                "QPushButton:hover { background-color: #2a7f2a; border-color: #6aff6a; }"
                "QPushButton:pressed { background-color: #0a3f0a; }"
              )
            : QString(
                "QPushButton {"
                "  font-size: 12px;"
                "  font-family: Consolas, monospace;"
                "  font-weight: bold;"
                "  background-color: #2b2b2b;"
                "  color: #4a9eff;"
                "  border: 2px solid #555;"
                "  border-radius: 4px;"
                "  padding: 4px 8px;"
                "}"
                "QPushButton:hover { background-color: #3a3a3a; border-color: #4a9eff; }"
                "QPushButton:pressed { background-color: #1a3a5f; }"
              );
        m_macroButtons[i]->setStyleSheet(style);
    }
}

void MainWindow::toggleLoop(int index)
{
    if (m_isLooping && m_loopingIndex == index) {
        stopLoop();
    } else {
        if (m_isLooping) stopLoop();
        startLoop(index);
    }
}

void MainWindow::startLoop(int index)
{
    m_isLooping = true;
    m_loopingIndex = index;
    updateMacroButtonStyles();
    triggerMacro(index);
}

void MainWindow::stopLoop()
{
    m_isLooping = false;
    m_loopingIndex = -1;
    if (m_loopTimer) m_loopTimer->stop();
    updateMacroButtonStyles();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Tab && ke->modifiers() == Qt::NoModifier) {
            if (watched == m_rstSent && m_rstSent->text().trimmed().isEmpty()) {
                m_rstSent->setText(QStringLiteral("599"));
                m_rstRcvd->setFocus();
                return true;
            }
            if (watched == m_rstRcvd && m_rstRcvd->text().trimmed().isEmpty()) {
                m_rstRcvd->setText(QStringLiteral("599"));
                m_name->setFocus();
                return true;
            }
        }
    }

    if (m_sendInput) {
        if (watched == m_sendInput->viewport() && event->type() == QEvent::MouseButtonRelease) {
            QTimer::singleShot(0, this, &MainWindow::clampSendCursor);
        }

        if (watched == m_sendInput && event->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent *>(event);
            const int fe = (m_keyer ? m_keyer->firstEditableIndex() : 0);

            if (ke->key() == Qt::Key_Home && !(ke->modifiers() & Qt::ControlModifier)) {
                QTextCursor c = m_sendInput->textCursor();
                c.setPosition(fe);
                m_sendInput->setTextCursor(c);
                return true;
            }

            if (m_keyer && m_keyer->isSending()) {
                QTextCursor c = m_sendInput->textCursor();
                const int pos = c.position();
                const int anchor = c.anchor();
                const int selMin = qMin(pos, anchor);

                if (ke->key() == Qt::Key_Backspace) {
                    if (c.hasSelection()) {
                        if (selMin < fe)
                            return true;
                    } else if (pos <= fe && pos > 0) {
                        return true;
                    }
                }
                if (ke->key() == Qt::Key_Delete) {
                    if (c.hasSelection()) {
                        if (selMin < fe)
                            return true;
                    } else if (pos < fe) {
                        return true;
                    }
                }
                if (ke->matches(QKeySequence::Cut)) {
                    if (c.hasSelection() && selMin < fe)
                        return true;
                }
                if (!ke->text().isEmpty() && ke->text().at(0).isPrint()) {
                    if (c.hasSelection()) {
                        if (selMin < fe)
                            return true;
                    } else if (pos < fe) {
                        return true;
                    }
                }
            }
        }
    }

    for (int i = 0; i < 8; ++i) {
        if (watched == m_macroButtons[i]) {
            if (event->type() == QEvent::MouseButtonRelease) {
                QMouseEvent *me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    if (m_ignoreNextRelease) {
                        m_ignoreNextRelease = false;
                        return false;
                    }
                    m_pendingClickIndex = i;
                    m_clickTimer->start();
                }
            } else if (event->type() == QEvent::MouseButtonDblClick) {
                QMouseEvent *me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    m_clickTimer->stop();
                    m_pendingClickIndex = -1;
                    m_ignoreNextRelease = true;
                    toggleLoop(i);
                }
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::triggerMacro(int index)
{
    cancelPostSendClearTimer();

    // 如果点击了其他宏按钮，停止当前循环
    if (m_isLooping && m_loopingIndex != index) {
        stopLoop();
    }
    
    QString content = theConfig.getString(QString("Shortcuts/F%1_Content").arg(index+1), "");
    if (content.isEmpty()) return;
    
    QString expanded = expandMacro(content);
    if (expanded.isEmpty()) return;
    
    m_inUserEdit = true;
    
    if (m_isLooping && m_loopingIndex == index) {
        // 循环模式：清空后重新设置内容，避免累积
        m_sendInput->setPlainText(expanded.toUpper());
    } else {
        // 普通模式：追加到输入框末尾
        m_sendInput->moveCursor(QTextCursor::End);
        m_sendInput->insertPlainText(expanded.toUpper());
    }
    
    m_inUserEdit = false;

    applySendInputUpdate(true);

    // 非循环模式下，确保光标在末尾方便用户继续输入
    if (!m_isLooping) {
        QTextCursor cursor = m_sendInput->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_sendInput->setTextCursor(cursor);
    }
}

QString MainWindow::expandMacro(const QString &macro)
{
    QString result = macro;
    // 先替换较长的 MY_* 标记，避免与 <CALL>、<NAME>、<QTH> 等子串混淆
    result.replace("<MY_CALL>", theConfig.getString("Station/MY_CALL", ""), Qt::CaseInsensitive);
    result.replace("<MY_NAME>", theConfig.getString("Station/MY_NAME", ""), Qt::CaseInsensitive);
    result.replace("<MY_QTH>", theConfig.getString("Station/MY_QTH", ""), Qt::CaseInsensitive);
    result.replace("<MY_RIG>", theConfig.getString("Station/MY_RIG", ""), Qt::CaseInsensitive);
    result.replace("<RST_SENT>", m_rstSent->text(), Qt::CaseInsensitive);
    result.replace("<RST_RCVD>", m_rstRcvd->text(), Qt::CaseInsensitive);
    result.replace("<CALL>", m_callsign->text(), Qt::CaseInsensitive);
    result.replace("<NAME>", m_name->text(), Qt::CaseInsensitive);
    result.replace("<QTH>", m_qth->text(), Qt::CaseInsensitive);
    return result;
}

void MainWindow::editMacro(int index)
{
    QString keyTitle = QString("Shortcuts/F%1_Title").arg(index+1);
    QString keyContent = QString("Shortcuts/F%1_Content").arg(index+1);
    
    QString currentTitle = theConfig.getString(keyTitle, QString("F%1").arg(index+1));
    QString currentContent = theConfig.getString(keyContent, "");
    
    QDialog dialog(this);
    dialog.setWindowTitle(QString("编辑快捷键 F%1").arg(index+1));
    dialog.setMinimumWidth(400);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QLabel *lblTitle = new QLabel("标题:", &dialog);
    QLineEdit *titleEdit = new QLineEdit(currentTitle, &dialog);
    
    QLabel *lblContent = new QLabel("内容:", &dialog);
    QLineEdit *contentEdit = new QLineEdit(currentContent, &dialog);
    contentEdit->setPlaceholderText(
        "变量: <CALL> <RST_SENT> <RST_RCVD> <NAME> <QTH> "
        "<MY_CALL> <MY_NAME> <MY_QTH> <MY_RIG>；[...] 内提速 50% 发送");
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *okBtn = new QPushButton("确定", &dialog);
    QPushButton *cancelBtn = new QPushButton("取消", &dialog);
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    
    layout->addWidget(lblTitle);
    layout->addWidget(titleEdit);
    layout->addWidget(lblContent);
    layout->addWidget(contentEdit);
    layout->addLayout(btnLayout);
    
    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        theConfig.set(keyTitle, titleEdit->text());
        theConfig.set(keyContent, contentEdit->text());
        refreshShortcutButtonLabels();
    }
}

void MainWindow::onCallsignChanged(const QString &text)
{
    const QString upper = text.toUpper();
    if (upper != text) {
        const int pos = m_callsign->cursorPosition();
        m_callsign->blockSignals(true);
        m_callsign->setText(upper);
        m_callsign->setCursorPosition(pos);
        m_callsign->blockSignals(false);
        return;
    }

    if (m_qsoHistoryPanel && m_qsoHistoryPanel->isVisible()
        && upper.trimmed().toUpper() != m_qsoHistoryShownCall) {
        hideQsoHistoryPanel();
    }

    if (m_isLooping && !upper.isEmpty())
        stopLoop();

    if (upper.trimmed().isEmpty()) {
        hideQsoHistoryPanel();
        resetQsoTiming();
        m_infoLabel->setText("输入呼号查看前缀信息");
        m_infoLabel->setStyleSheet(
            "font-size: 12px; color: #666; padding: 8px; "
            "background-color: #252525; border-radius: 4px; "
            "font-family: Consolas, monospace;"
        );
        return;
    }

    if (!m_qsoTimeOn.isValid())
        m_qsoTimeOn = QDateTime::currentDateTime();

    QString country = tr("未知");
    int cqZone = 0;
    int ituZone = 0;
    QString continent;

    if (m_prefixDb) {
        const PrefixLookupResult hit = m_prefixDb->lookup(upper);
        if (hit.valid) {
            country = hit.country;
            cqZone = hit.cqZone;
            ituZone = hit.ituZone;
            continent = hit.continent;
        }
    }

    QString infoText = QString("📍 %1  |  CQ: %2  |  ITU: %3  |  方位: ---°  |  ---- km")
                           .arg(country, -18)
                           .arg(cqZone, 2)
                           .arg(ituZone, 2);
    if (!continent.isEmpty())
        infoText += QStringLiteral("  |  ") + continent;

    m_infoLabel->setText(infoText);
    m_infoLabel->setStyleSheet(
        "font-size: 12px; color: #4a9eff; padding: 8px; "
        "background-color: #1a2f4a; border-radius: 4px; "
        "font-family: Consolas, monospace; font-weight: bold;"
    );
}