#include "yaesucatreader.h"
#include "config.h"
#include <QSerialPort>
#include <QThread>

YaesuCatReader::YaesuCatReader(QObject *parent)
    : ICatReader(parent)
{
    m_pollTimer.setSingleShot(false);
    connect(&m_pollTimer, &QTimer::timeout, this, &YaesuCatReader::onPollTimer);
}

YaesuCatReader::~YaesuCatReader()
{
    close();
}

void YaesuCatReader::setSerialPort(QSerialPort *port)
{
    m_port = port;
}

bool YaesuCatReader::open()
{
    stopPolling();
    m_lastError.clear();

    if (!m_port) {
        m_lastError = tr("未设置 CAT 串口");
        emit connectionStateChanged(false);
        return false;
    }
    if (!m_port->isOpen()) {
        m_lastError = tr("CAT 串口未打开: %1").arg(m_port->portName());
        emit connectionStateChanged(false);
        return false;
    }

    emit connectionStateChanged(true);
    return true;
}

void YaesuCatReader::close()
{
    stopPolling();
    emit connectionStateChanged(false);
}

bool YaesuCatReader::isOpen() const
{
    return m_port && m_port->isOpen();
}

void YaesuCatReader::startPolling(int intervalMs)
{
    if (!isOpen())
        return;
    m_pollSuspended = false;
    const int ms = qMax(200, intervalMs);
    m_pollTimer.start(ms);
    onPollTimer();
}

void YaesuCatReader::stopPolling()
{
    m_pollSuspended = true;
    if (m_pollTimer.isActive())
        m_pollTimer.stop();
}

int YaesuCatReader::minResponseBytesForCommand(const QByteArray &cmd)
{
    const QByteArray c = cmd.trimmed().toUpper();
    if (c == "FA;" || c == "FB;")
        return 12; // FA + 9 digits + ;
    return 3;
}

bool YaesuCatReader::transaction(const QByteArray &cmd, QByteArray &response, int timeoutMs)
{
    response.clear();
    if (!isOpen() || m_pollSuspended)
        return false;

    if (timeoutMs <= 0)
        timeoutMs = theConfig.getInt("Cat/Timeout_Ms", 1000);

    if (m_beforeTransaction)
        m_beforeTransaction();

    const int minBytes = minResponseBytesForCommand(cmd);

    m_port->clear(QSerialPort::Input);
    if (m_port->write(cmd) != cmd.size()) {
        m_lastError = tr("CAT 写入失败");
        return false;
    }
    if (!m_port->waitForBytesWritten(300)) {
        m_lastError = tr("CAT 写入超时");
        return false;
    }

    QThread::msleep(30);

    int waited = 0;
    const int step = 50;
    while (waited < timeoutMs) {
        if (m_port->waitForReadyRead(step)) {
            response += m_port->readAll();
            if (response.indexOf(';') >= 0 && response.size() >= minBytes)
                break;
        }
        waited += step;
    }

    while (m_port->waitForReadyRead(30))
        response += m_port->readAll();

    if (response.isEmpty()) {
        m_lastError = tr("CAT 无响应（请确认：① 接的是电台 CAT 口而非仅 PTT 口；② 波特率与菜单 CAT-1/CAT-2 RATE 一致，FT-710 CAT-1 常用 38400）");
        return false;
    }
    if (response.indexOf(';') < 0) {
        m_lastError = tr("CAT 响应不完整");
        return false;
    }
    if (response.size() < minBytes) {
        m_lastError = tr("CAT 响应过短（可能仅有命令回显）: %1")
                          .arg(QString::fromLatin1(response));
        return false;
    }
    return true;
}

bool YaesuCatReader::parseFreqResponse(const QByteArray &response, double &mhzOut)
{
    const QByteArray r = response.trimmed().toUpper();
    int idx = r.indexOf("FA");
    if (idx < 0)
        idx = r.indexOf("FB");
    if (idx < 0)
        return false;

    const int digitStart = idx + 2;
    if (digitStart + 9 > r.size())
        return false;

    bool ok = false;
    const quint64 hz = r.mid(digitStart, 9).toULong(&ok);
    if (!ok)
        return false;

    mhzOut = static_cast<double>(hz) / 1000000.0;
    return mhzOut > 0.0;
}

void YaesuCatReader::beginFrequencyQuery()
{
    m_frequencyQueryActive = true;
}

void YaesuCatReader::endFrequencyQuery()
{
    m_frequencyQueryActive = false;
    emit frequencyQueryFinished();
}

bool YaesuCatReader::readOperatingFrequency()
{
    if (m_pollSuspended)
        return false;

    beginFrequencyQuery();

    QByteArray resp;
    char mainVfo = '0';

    if (transaction("VS;", resp)) {
        const QByteArray u = resp.toUpper();
        const int i = u.indexOf("VS");
        if (i >= 0 && i + 3 < u.size())
            mainVfo = u.at(i + 2);
    }

    if (m_pollSuspended) {
        endFrequencyQuery();
        return false;
    }

    const QByteArray cmd = (mainVfo == '1') ? QByteArray("FB;") : QByteArray("FA;");
    if (!transaction(cmd, resp)) {
        endFrequencyQuery();
        return false;
    }

    double mhz = 0.0;
    if (!parseFreqResponse(resp, mhz)) {
        m_lastError = tr("无法解析频率响应: %1").arg(QString::fromLatin1(resp));
        endFrequencyQuery();
        return false;
    }

    setFrequency(mhz);
    m_lastError.clear();
    endFrequencyQuery();
    return true;
}

void YaesuCatReader::setFrequency(double mhz)
{
    const double rounded = qRound(mhz * 1e6) / 1e6;
    if (m_hasFreq && qAbs(rounded - m_freqMHz) < 1e-9)
        return;
    m_freqMHz = rounded;
    m_hasFreq = true;
    emit frequencyChanged(m_freqMHz);
}

bool YaesuCatReader::readOnce()
{
    const bool wasSuspended = m_pollSuspended;
    m_pollSuspended = false;
    const bool ok = readOperatingFrequency();
    m_pollSuspended = wasSuspended;
    return ok;
}

void YaesuCatReader::onPollTimer()
{
    if (!isOpen() || m_pollSuspended)
        return;
    readOperatingFrequency();
}
