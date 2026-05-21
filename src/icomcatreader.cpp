#include "icomcatreader.h"
#include "config.h"
#include <QSerialPort>
#include <QThread>
#include <QDateTime>

namespace {
constexpr unsigned char kPreamble = 0xFE;
constexpr unsigned char kEndMark = 0xFD;
constexpr unsigned char kControllerAddr = 0xE0;
constexpr unsigned char kReadFreqCmd = 0x03;
constexpr unsigned char kNak = 0xFA;
constexpr int kFreqBcdBytes = 5;
}

IcomCatReader::IcomCatReader(QObject *parent)
    : ICatReader(parent)
{
    m_pollTimer.setSingleShot(false);
    connect(&m_pollTimer, &QTimer::timeout, this, &IcomCatReader::onPollTimer);
}

IcomCatReader::~IcomCatReader()
{
    close();
}

void IcomCatReader::setSerialPort(QSerialPort *port)
{
    m_port = port;
}

bool IcomCatReader::open()
{
    stopPolling();
    m_lastError.clear();

    m_radioAddr = static_cast<quint8>(
        theConfig.getInt("Hardware/CAT_Address", 0x6E) & 0xFF);

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

void IcomCatReader::close()
{
    stopPolling();
    emit connectionStateChanged(false);
}

bool IcomCatReader::isOpen() const
{
    return m_port && m_port->isOpen();
}

void IcomCatReader::startPolling(int intervalMs)
{
    if (!isOpen())
        return;
    m_pollSuspended = false;
    const int ms = qMax(200, intervalMs);
    m_pollTimer.start(ms);
    onPollTimer();
}

void IcomCatReader::stopPolling()
{
    m_pollSuspended = true;
    if (m_pollTimer.isActive())
        m_pollTimer.stop();
}

QByteArray IcomCatReader::buildReadFreqCommand() const
{
    QByteArray frame;
    frame.reserve(6);
    frame.append(char(kPreamble));
    frame.append(char(kPreamble));
    frame.append(char(m_radioAddr));
    frame.append(char(kControllerAddr));
    frame.append(char(kReadFreqCmd));
    frame.append(char(kEndMark));
    return frame;
}

static int findLastCiVFrameStart(const QByteArray &acc, int endInclusive)
{
    for (int j = endInclusive - 1; j >= 1; --j) {
        if (static_cast<unsigned char>(acc.at(j - 1)) == 0xFE
            && static_cast<unsigned char>(acc.at(j)) == 0xFE) {
            return j - 1;
        }
    }
    return -1;
}

bool IcomCatReader::readCiVFrame(QByteArray &frame, int timeoutMs)
{
    frame.clear();
    if (!m_port)
        return false;

    QByteArray acc;
    int waited = 0;
    const int step = 50;

    auto tryExtract = [&]() -> bool {
        for (int end = acc.size() - 1; end >= 0; --end) {
            if (static_cast<unsigned char>(acc.at(end)) != kEndMark)
                continue;
            const int start = findLastCiVFrameStart(acc, end);
            if (start < 0)
                continue;
            frame = acc.mid(start, end - start + 1);
            return true;
        }
        return false;
    };

    while (waited < timeoutMs) {
        if (m_port->waitForReadyRead(step)) {
            acc += m_port->readAll();
            if (tryExtract())
                return true;
        }
        waited += step;
    }

    while (m_port->waitForReadyRead(30))
        acc += m_port->readAll();

    return tryExtract();
}

quint64 IcomCatReader::fromBcdHz(const unsigned char *data, int byteCount)
{
    quint64 hz = 0;
    for (int i = 0; i < byteCount; ++i) {
        const unsigned char b = data[i];
        const unsigned hi = b >> 4;
        const unsigned lo = b & 0x0f;
        if (hi > 9 || lo > 9)
            return 0;
        hz = hz * 10 + hi;
        hz = hz * 10 + lo;
    }
    return hz;
}

bool IcomCatReader::parseFreqResponseFrame(const QByteArray &frame, quint8 radioAddr,
                                           double &mhzOut)
{
    if (frame.size() < 11)
        return false;

    const auto u = [&](int i) -> unsigned char {
        return static_cast<unsigned char>(frame.at(i));
    };

    if (u(2) != kControllerAddr || u(3) != radioAddr || u(4) != kReadFreqCmd)
        return false;

    if (frame.size() >= 6 && u(frame.size() - 2) == kNak)
        return false;

    const int dataStart = 5;
    const int dataEnd = frame.size() - 1;
    if (dataEnd - dataStart < kFreqBcdBytes)
        return false;

    const quint64 hz = fromBcdHz(
        reinterpret_cast<const unsigned char *>(frame.constData() + dataStart),
        kFreqBcdBytes);
    if (hz == 0)
        return false;

    mhzOut = static_cast<double>(hz) / 1000000.0;
    return mhzOut > 0.0;
}

bool IcomCatReader::transactionReadFrequency(double &mhzOut)
{
    mhzOut = 0.0;
    if (!isOpen() || m_pollSuspended)
        return false;

    const int timeoutMs = theConfig.getInt("Cat/Timeout_Ms", 1000);
    const QByteArray cmd = buildReadFreqCommand();

    if (m_beforeTransaction)
        m_beforeTransaction();

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

    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
    while (QDateTime::currentMSecsSinceEpoch() < deadline) {
        const int remain = static_cast<int>(
            deadline - QDateTime::currentMSecsSinceEpoch());
        if (remain <= 0)
            break;

        QByteArray frame;
        if (!readCiVFrame(frame, qMax(50, remain)))
            continue;

        if (frame.size() >= 6
            && static_cast<unsigned char>(frame.at(2)) == m_radioAddr
            && static_cast<unsigned char>(frame.at(3)) == kControllerAddr
            && static_cast<unsigned char>(frame.at(4)) == kReadFreqCmd
            && frame == cmd) {
            continue;
        }

        if (frame.size() == 6
            && static_cast<unsigned char>(frame.at(4)) == kNak) {
            m_lastError = tr("电台拒绝 CI-V 命令（NAK），请检查 CI-V 地址与波特率");
            return false;
        }

        if (parseFreqResponseFrame(frame, m_radioAddr, mhzOut))
            return true;
    }

    m_lastError = tr(
        "CI-V 无有效频率响应（请确认：① 接 CI-V 口；② 波特率与菜单 CI-V Baud Rate 一致，"
        "IC-756PROIII 默认 19200；③ CI-V 地址默认 0x6E）");
    return false;
}

void IcomCatReader::beginFrequencyQuery()
{
    m_frequencyQueryActive = true;
}

void IcomCatReader::endFrequencyQuery()
{
    m_frequencyQueryActive = false;
    emit frequencyQueryFinished();
}

void IcomCatReader::setFrequency(double mhz)
{
    const double rounded = qRound(mhz * 1e6) / 1e6;
    if (m_hasFreq && qAbs(rounded - m_freqMHz) < 1e-9)
        return;
    m_freqMHz = rounded;
    m_hasFreq = true;
    emit frequencyChanged(m_freqMHz);
}

void IcomCatReader::onPollTimer()
{
    if (!isOpen() || m_pollSuspended)
        return;

    beginFrequencyQuery();
    double mhz = 0.0;
    if (transactionReadFrequency(mhz)) {
        setFrequency(mhz);
        m_lastError.clear();
    }
    endFrequencyQuery();
}
