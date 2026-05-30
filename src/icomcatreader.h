#ifndef ICOMCATREADER_H
#define ICOMCATREADER_H

#include "icatreader.h"
#include <QSerialPort>
#include <QTimer>
#include <functional>

class IcomCatReader : public ICatReader
{
    Q_OBJECT

public:
    explicit IcomCatReader(QObject *parent = nullptr);
    ~IcomCatReader() override;

    void setSerialPort(QSerialPort *port) override;
    void setRadioAddress(quint8 addr);

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
    QByteArray buildReadFreqCommand() const;
    bool readCiVFrame(QByteArray &frame, int timeoutMs);
    bool transactionReadFrequency(double &mhzOut);
    static bool parseFreqResponseFrame(const QByteArray &frame, quint8 radioAddr, double &mhzOut);
    static quint64 fromBcdHz(const unsigned char *data, int byteCount);
    void setFrequency(double mhz);

    QSerialPort *m_port = nullptr;
    QTimer m_pollTimer;
    quint8 m_radioAddr = 0x6E;
    bool m_radioAddrSet = false;
    bool m_frequencyQueryActive = false;
    bool m_pollSuspended = false;
    double m_freqMHz = 0.0;
    bool m_hasFreq = false;
    QString m_lastError;
    std::function<void()> m_beforeTransaction;
};

#endif
