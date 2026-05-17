#ifndef PCKEYER_H
#define PCKEYER_H

#include "ikeyer.h"
#include <QSerialPort>
#include <QTimer>
#include <QMap>

class PCKeyer : public QObject, public IKeyer {
    Q_OBJECT

signals:
    void transmissionActiveChanged(bool active);

public:
    explicit PCKeyer(QObject *parent = nullptr);
    ~PCKeyer() override;

    // 配置（必须在 open 前设置）
    void setSerialPort(QSerialPort *port) { m_serial = port; }
    void setOwnsSerialPort(bool owns) { m_ownsSerial = owns; }
    void setPortName(const QString &port) { m_portName = port; }
    void setKeyingLine(const QString &line) { m_keyingLine = line; }  // "DTR" or "RTS"
    void setActiveLow(bool activeLow) { m_activeLow = activeLow; }

    // IKeyer 实现
    bool open() override;
    void close() override;
    bool isOpen() const override;
    
    // void sendChar(QChar ch) override;
    // void sendString(const QString &str) override;
    // void interrupt() override;

    void clear() override;
    void update(const QString &text) override;
    
    void setWpm(int wpm) override;
    int wpm() const override { return m_wpm; }
    
    bool isSending() const override { return m_isSending; }

    // 同口 CAT 读频前释放 PTT/键控线，避免占用半双工串口
    void releaseKeyingLines();
    // QString buffer() const override { return m_buffer; }
    int currentIndex() const override { return m_currentIndex; }
    int firstEditableIndex() const override;
    QString requiredSendPrefix() const override;

private slots:
    void onTimerTimeout();  // 完成当前元素（点/划/间隔）

private:
    // 摩尔斯编码
    struct MorseCode {
        QString pattern;  // "." 和 "-" 组成的字符串
    };
    static const QMap<QChar, MorseCode> s_morseTable;
    
    // 发送控制
    void startElement();      // 开始发送一个点或划
    void stopElement();       // 结束当前点或划（进入间隔）
    void nextElement();       // 移动到下一个元素
    void nextChar();          // 移动到下一个字符
    
    int elementDuration(bool isDash) const;  // 计算点/划时长
    bool speedBoostActiveAt(int index) const;
    int effectiveWpmAt(int index) const;
    bool isSpeedControlChar(QChar ch) const;
    
    // 硬件控制
    void setKey(bool on);     // 控制 DTR/RTS 电平
    void endTransmission();
    
    // 配置
    QString m_portName = "COM4";
    QString m_keyingLine = "DTR";  // "DTR" or "RTS"
    bool m_activeLow = false;        // true=低电平有效
    
    // 硬件
    QSerialPort *m_serial = nullptr;
    bool m_ownsSerial = false;
    
    // 发送状态
    bool m_isSending = false;
    QString m_buffer;         // 完整字符串缓冲区
    int m_currentIndex = 0;   // 当前字符索引
    QString m_currentPattern; // 当前字符的摩尔斯模式（如 ".-.")
    int m_patternIndex = 0;     // 当前模式中的位置（点或划）
    bool m_isGap = false;       // 当前是否在间隔期
    
    int m_wpm = 20;
    QTimer *m_timer = nullptr;
    static constexpr double kSpeedBoostFactor = 1.5;

    enum State { Idle, SendingElement, ElementGap, CharGap, WordGap };
    State m_state = Idle;
};

#endif