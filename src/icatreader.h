#ifndef ICATREADER_H
#define ICATREADER_H

#include <QObject>
#include <QString>

class QSerialPort;

// CAT 只读接口：轮询频率，通过信号通知 UI（串口由 MainWindow 打开并注入）
class ICatReader : public QObject
{
    Q_OBJECT

public:
    explicit ICatReader(QObject *parent = nullptr) : QObject(parent) {}
    ~ICatReader() override = default;

    virtual void setSerialPort(QSerialPort *port) = 0;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual double frequencyMHz() const = 0;
    virtual QString lastError() const = 0;

    virtual void startPolling(int intervalMs = 800) = 0;
    virtual void stopPolling() = 0;

    virtual bool readOnce() { return false; }

    // 同口共享时：正在执行读频事务（VS/FA），上层应暂缓启动新的发射
    virtual bool isFrequencyQueryActive() const { return false; }

signals:
    void frequencyChanged(double mhz);
    void connectionStateChanged(bool connected);
    // 一次 VS/FA 读频事务结束（成功或失败），用于补发读频期间缓存的输入
    void frequencyQueryFinished();
};

#endif
