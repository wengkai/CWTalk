#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QTimer>
#include <QDateTime>
#include <QPushButton>
#include <QSpinBox>
#include <QFrame>
#include <QVBoxLayout>
#include <QList>
#include "ikeyer.h"

namespace adif {
class Record;
}

class ICatReader;
class QsoLog;
class CallsignPrefixDatabase;
class QSerialPort;
class YaesuCatReader;

class MainWindow : public QMainWindow, public IKeyerListener
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // IKeyerListener 实现
    // void onKeyerCharStart(int index, QChar ch) override;
    // void onKeyerCharComplete(int index, QChar ch) override;
    // void onKeyerComplete() override;
    // void onKeyerInterrupted() override;
    void onKeyerCharComplete(int index, QChar ch) override;
    void onKeyerBufferEmpty() override;
private slots:
    void onCallsignChanged(const QString &text);
    void onCallsignEditingFinished();
    void onClearClicked();
    void onLogQsoClicked();
    void onSendInputChanged();
    void onPostSendIdleClearTimeout();
    void onOpenSettings();
    void onCatFrequencyChanged(double mhz);
    void onCatConnectionChanged(bool connected);
    void onManualFrequencyEdited();
    void onCatFrequencyQueryFinished();
    void onWpmChanged(int wpm);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    
    void setupUI();
    void setupMacroButtons(QVBoxLayout *mainLayout); // 初始化快捷键按钮
    void refreshShortcutButtonLabels();
    void applySettingsFromConfig();
    void updateMacroButtonStyles(); // 更新按钮样式（循环状态）
    void updateSendDisplay();       // 更新显示样式（已发/待发分区）
    void clampSendCursor();         // 限制光标不得进入已发/正在发区域
    void initHardware();            // 打开串口并初始化 Keyer / CAT
    void shutdownHardware();
    QSerialPort *openSerialPort(const QString &portName, int baudRate);
    void initKeyer();
    void initCat();
    void reinitHardware();
    void updateFrequencyFieldMode(bool catConnected);
    void updateCatFreqPlaceholder();
    void clearQsoFieldsOnly();
    void resetQsoTiming();
    double parseFrequencyMHz(const QString &text) const;
    QString formatFrequencyMHz(double mhz) const;
    double currentFrequencyMHz() const;
    void triggerMacro(int index);   // 触发快捷键
    void editMacro(int index);      // 编辑快捷键
    QString expandMacro(const QString &macro); // 展开宏变量
    void startLoop(int index);      // 启动自动循环
    void stopLoop();                // 停止自动循环
    void toggleLoop(int index);     // 切换循环状态
    void cancelPostSendClearTimer();
    void syncCatPollingWithKeyer();       // 发射期间暂停 CAT 轮询
    void resumeCatPollingIfIdle();
    bool isCatFrequencyQueryActive() const;
    void applySendInputUpdate(bool allowDeferForCatQuery = true);
    void loadAdifLog();
    void loadPrefixDatabase();
    QString defaultCtyPath() const;
    adif::Record buildQsoRecordFromUi();
    void clearUiAfterQsoLogged();
    bool isCompleteCallsign(const QString &call) const;
    void hideQsoHistoryPanel();
    void showQsoHistoryForCall(const QString &call);
    void applyWpmToKeyer(int wpm);

    // 第一行：6个输入框
    QLineEdit *m_callsign;
    QLineEdit *m_rstSent;
    QLineEdit *m_rstRcvd;
    QLineEdit *m_name;
    QLineEdit *m_qth;
    QLineEdit *m_comment;
    QPushButton *m_clearBtn;
    QPushButton *m_logBtn = nullptr;
    QLineEdit *m_freqEdit = nullptr;
    QSpinBox *m_wpmSpin = nullptr;
    QsoLog *m_qsoLog = nullptr;
    CallsignPrefixDatabase *m_prefixDb = nullptr;

    // 第二行：解析信息
    QLabel *m_infoLabel;

    QFrame *m_qsoHistoryPanel = nullptr;
    QLabel *m_qsoHistoryTitle = nullptr;
    QLabel *m_qsoHistoryBody = nullptr;
    QString m_qsoHistoryShownCall;
    
    // 第三行：摩尔斯发送
    QTextEdit *m_sendInput;
    
    // Keyer / CAT / 串口（MainWindow 统一打开，同口共享）
    QList<QSerialPort *> m_serialPorts;
    QSerialPort *m_keyingSerial = nullptr;
    QSerialPort *m_catSerial = nullptr;
    bool m_serialPortsShared = false;
    bool m_catPollPausedForTx = false;

    IKeyer *m_keyer = nullptr;
    ICatReader *m_catReader = nullptr;
    bool m_catActive = false;
    double m_manualFreqMHz = 0.0;
    double m_lastCatFreqMHz = 0.0;
    QDateTime m_qsoTimeOn; // 首次在呼号框输入时记下，用于 ADIF time_on

    // 发送状态
    int m_sentIndex = 0;        // 已发送到的位置
    bool m_inUserEdit = false;  // 防止递归触发
    QString m_lastValidSendPlain; // 上一次与 keyer 一致的输入，用于非法编辑回滚
    bool m_sendUpdateDeferred = false; // 读频期间暂缓的 keyer 更新

    // 第四行：快捷键按钮
    QPushButton *m_macroButtons[8] = {};
    
    // 全部发完后延迟清空发送框（可配置秒数，0 关闭）
    QTimer *m_sendClearTimer = nullptr;

    // 自动循环
    QTimer *m_loopTimer = nullptr;
    bool m_isLooping = false;
    int m_loopingIndex = -1;
    
    // 单击/双击区分
    QTimer *m_clickTimer = nullptr;
    int m_pendingClickIndex = -1;
    bool m_ignoreNextRelease = false;
    
    // 样式颜色
    const QString COLOR_SENT = "#666666";      // 灰色：已发送
    const QString COLOR_SENDING = "#00aa00";   // 绿色：正在发送
    const QString COLOR_PENDING = "#ffffff";   // 白色：待发送
};

#endif