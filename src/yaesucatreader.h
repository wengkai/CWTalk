#ifndef YAESUCATREADER_H
#define YAESUCATREADER_H

#include "icatreader.h"
#include <QSerialPort>
#include <QTimer>
#include <functional>

// Yaesu 标准 ASCII CAT（FT-710 等）：FA/FB 读频，VS 选当前主 VFO
class YaesuCatReader : public ICatReader
{
    Q_OBJECT

public:
    explicit YaesuCatReader(QObject *parent = nullptr);
    ~YaesuCatReader() override;

    void setSerialPort(QSerialPort *port) override;

    bool open() override;
    void close() override;
    bool isOpen() const override;

    double frequencyMHz() const override { return m_freqMHz; }
    QString lastError() const override { return m_lastError; }

    void startPolling(int intervalMs = 800) override;
    void stopPolling() override;

    bool isFrequencyQueryActive() const override { return m_frequencyQueryActive; }

    void setBeforeTransaction(std::function<void()> hook) { m_beforeTransaction = std::move(hook); }

    bool readOnce() override;

private slots:
    void onPollTimer();

private:
    void beginFrequencyQuery();
    void endFrequencyQuery();
    bool transaction(const QByteArray &cmd, QByteArray &response, int timeoutMs = 0);
    static int minResponseBytesForCommand(const QByteArray &cmd);
    bool readOperatingFrequency();
    static bool parseFreqResponse(const QByteArray &response, double &mhzOut);
    void setFrequency(double mhz);

    QSerialPort *m_port = nullptr;
    QTimer m_pollTimer;
    bool m_frequencyQueryActive = false;
    bool m_pollSuspended = false;
    double m_freqMHz = 0.0;
    bool m_hasFreq = false;
    QString m_lastError;
    std::function<void()> m_beforeTransaction;
};

#endif
