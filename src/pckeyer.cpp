#include "pckeyer.h"
#include <QDebug>

// 摩尔斯编码表（简化版，实际需要完整表）
const QMap<QChar, PCKeyer::MorseCode> PCKeyer::s_morseTable = {
    {'A', {".-"}}, {'B', {"-..."}}, {'C', {"-.-."}}, {'D', {"-.."}},
    {'E', {"."}}, {'F', {"..-."}}, {'G', {"--."}}, {'H', {"...."}},
    {'I', {".."}}, {'J', {".---"}}, {'K', {"-.-"}}, {'L', {".-.."}},
    {'M', {"--"}}, {'N', {"-."}}, {'O', {"---"}}, {'P', {".--."}},
    {'Q', {"--.-"}}, {'R', {".-."}}, {'S', {"..."}}, {'T', {"-"}},
    {'U', {"..-"}}, {'V', {"...-"}}, {'W', {".--"}}, {'X', {"-..-"}},
    {'Y', {"-.--"}}, {'Z', {"--.."}},
    {'1', {".----"}}, {'2', {"..---"}}, {'3', {"...--"}},
    {'4', {"....-"}}, {'5', {"....."}}, {'6', {"-...."}},
    {'7', {"--..."}}, {'8', {"---.."}}, {'9', {"----."}}, {'0', {"-----"}},
    {'.', {".-.-.-"}}, {',', {"--..--"}}, {'?', {"..--.."}},
    {'\'', {".----."}}, {'!', {"-.-.--"}}, {'/', {"-..-."}},
    {'(', {"-.--."}}, {')', {"-.--.-"}}, {'&', {".-..."}},
    {':', {"---..."}}, {';', {"-.-.-."}}, {'=', {"-...-"}},
    {'+', {".-.-."}}, {'-', {"-....-"}}, {'_', {"..--.-"}},
    {'"', {".-..-."}}, {'$', {"...-..-"}},
    {' ', {" "}}  // 单词间隔
};

PCKeyer::PCKeyer(QObject *parent) :QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &PCKeyer::onTimerTimeout);
}

PCKeyer::~PCKeyer()
{
    close();
}

bool PCKeyer::open()
{
    if (!m_serial) {
        // m_serial = new QSerialPort(this);
        // m_serial->setPortName(m_portName);
        // m_serial->setBaudRate(QSerialPort::Baud9600);
        // m_serial->setDataBits(QSerialPort::Data8);
        // m_serial->setParity(QSerialPort::NoParity);
        // m_serial->setStopBits(QSerialPort::OneStop);
        // m_serial->setFlowControl(QSerialPort::NoFlowControl);

        // if (!m_serial->open(QIODevice::ReadWrite)) {
        //     qDebug() << "Failed to open port:" << m_portName;
        //     delete m_serial;
        //     m_serial = nullptr;
        //     return false;
        // }
        return false;  // 没有串口对象，无法打开
    }

    // 关键：设置 DTR/RTS 初始状态
    m_serial->setDataTerminalReady(false);
    m_serial->setRequestToSend(false);
    
    qDebug() << "Keyer opened on" << m_portName << "using" << m_keyingLine;
    
    return true;
}

void PCKeyer::close()
{
    if (!m_serial)
        return;
    setKey(false);
    if (m_ownsSerial) {
        m_serial->close();
        delete m_serial;
    }
    m_serial = nullptr;
}

bool PCKeyer::isOpen() const
{
    return m_serial && m_serial->isOpen();
}

void PCKeyer::endTransmission()
{
    const bool wasSending = m_isSending;
    m_timer->stop();
    setKey(false);
    m_isSending = false;
    if (wasSending)
        emit transmissionActiveChanged(false);
}

void PCKeyer::clear()
{
    endTransmission();
    m_buffer.clear();
    m_currentIndex = 0;
    m_currentPattern.clear();
    m_patternIndex = 0;
    m_state = Idle;
}

bool PCKeyer::isSpeedControlChar(QChar ch) const
{
    return ch == QLatin1Char('[') || ch == QLatin1Char(']');
}

bool PCKeyer::speedBoostActiveAt(int index) const
{
    int depth = 0;
    for (int i = 0; i < index && i < m_buffer.length(); ++i) {
        const QChar c = m_buffer.at(i);
        if (c == QLatin1Char('['))
            ++depth;
        else if (c == QLatin1Char(']') && depth > 0)
            --depth;
    }
    return depth > 0;
}

int PCKeyer::effectiveWpmAt(int index) const
{
    if (speedBoostActiveAt(index)) {
        const int boosted = qRound(m_wpm * kSpeedBoostFactor);
        return qBound(5, boosted, 60);
    }
    return m_wpm;
}

int PCKeyer::firstEditableIndex() const
{
    if (!m_isSending)
        return 0;
    if (m_state == SendingElement || m_state == ElementGap)
        return m_currentIndex + 1;
    return m_currentIndex;
}

QString PCKeyer::requiredSendPrefix() const
{
    return m_buffer.left(firstEditableIndex());
}

void PCKeyer::update(const QString &text)
{
    QString newText = text.toUpper();
    
    if (!m_isSending) {
        if (newText.isEmpty())
            return;

        const QString sentPrefix = m_buffer.left(qMin(m_currentIndex, m_buffer.size()));
        const bool canResume = !sentPrefix.isEmpty()
            && m_currentIndex > 0
            && newText.length() > m_currentIndex
            && newText.left(m_currentIndex) == sentPrefix;

        if (canResume) {
            m_buffer = newText;
            m_currentPattern.clear();
            m_patternIndex = 0;
            m_isSending = true;
            m_state = Idle;
            emit transmissionActiveChanged(true);
            nextChar();
            return;
        }

        m_buffer = newText;
        m_currentIndex = 0;
        m_currentPattern.clear();
        m_patternIndex = 0;
        m_isSending = true;
        m_state = Idle;
        emit transmissionActiveChanged(true);
        nextChar();
        return;
    }

    // 已发完前缀 = m_buffer.left(m_currentIndex)，其后整段用 newText 替换
    if (newText.length() < m_currentIndex)
        return;

    if (newText.length() == m_currentIndex) {
        if (newText != m_buffer.left(m_currentIndex))
            return;
        if (m_state == SendingElement || m_state == ElementGap)
            return;
        m_buffer = newText;
        m_currentPattern.clear();
        m_patternIndex = 0;
        m_state = Idle;
        endTransmission();
        notifyBufferEmpty();
        return;
    }

    QString prefix = m_buffer.left(m_currentIndex);
    QString suffix = newText.mid(m_currentIndex);
    m_buffer = prefix + suffix;
    qDebug() << "Updated buffer:" << m_buffer << "currentIndex:" << m_currentIndex;
}

void PCKeyer::setWpm(int wpm)
{
    const int newWpm = qBound(5, wpm, 40);
    if (newWpm == m_wpm)
        return;
    m_wpm = newWpm;

    if (!m_isSending || !m_timer->isActive())
        return;

    switch (m_state) {
    case ElementGap:
        m_timer->start(elementDuration(false));
        break;
    case CharGap:
        m_timer->start(elementDuration(false) * 3);
        break;
    case WordGap:
        m_timer->start(elementDuration(false) * 7);
        break;
    default:
        break;
    }
}

void PCKeyer::releaseKeyingLines()
{
    m_timer->stop();
    setKey(false);
}

void PCKeyer::setKey(bool on)
{
    if (!m_serial || !m_serial->isOpen())
        return;

    bool actual = m_activeLow ? !on : on;  // 电平反转

    if (m_keyingLine == "DTR") {
        m_serial->setDataTerminalReady(actual);
    } else {
        m_serial->setRequestToSend(actual);
    }
}

int PCKeyer::elementDuration(bool isDash) const
{
    const int wpm = qMax(5, effectiveWpmAt(m_currentIndex));
    const int dotMs = 1200 / wpm;
    return isDash ? dotMs * 3 : dotMs;
}

void PCKeyer::startElement()
{
    if (m_patternIndex >= m_currentPattern.length()) {
        // 字符完成
        // notifyCharComplete(m_currentIndex, m_buffer[m_currentIndex]);
        m_currentIndex++;
        
        if (m_currentIndex >= m_buffer.length()) {
            m_state = Idle;
            endTransmission();
            notifyBufferEmpty();
            return;
        }
        
        // 字符间隔
        m_state = CharGap;
        m_timer->start(elementDuration(false) * 3);
        return;
    }
    
    // 发送点或划
    bool isDash = (m_currentPattern[m_patternIndex] == '-');
    setKey(true);
    m_state = SendingElement;
    m_isGap = false;
    
    m_timer->start(elementDuration(isDash));
}

void PCKeyer::onTimerTimeout()
{
    if (!m_isSending) return;
    
    // qDebug() << "Timer: state=" << m_state 
    //          << "index=" << m_currentIndex 
    //          << "patternIdx=" << m_patternIndex;

    switch (m_state) {
    case Idle:
        // 不应该在 Idle 时触发定时器，但保险起见
        qDebug() << "ERROR: Timer in Idle state";
        break;
    case SendingElement:  // 正在发点划，现在结束它
        setKey(false);
        m_patternIndex++;
        
        if (m_patternIndex >= m_currentPattern.length()) {
            // qDebug() << "Char complete:" << m_buffer[m_currentIndex] 
            //      << "advancing to index:" << m_currentIndex + 1;
            // 字符完成
            notifyCharComplete(m_currentIndex, m_buffer[m_currentIndex]);
            m_currentIndex++;
            
            if (m_currentIndex >= m_buffer.length()) {
                endTransmission();
                notifyBufferEmpty();
                return;
            }
            
            // 进入字符间隔
            m_state = CharGap;
            m_timer->start(elementDuration(false) * 3);
        } else {
            // 元素间隔，继续同一字符
            m_state = ElementGap;
            m_timer->start(elementDuration(false));
        }
        break;
        
    case ElementGap:  // 元素间隔结束
        m_state = SendingElement;
        startElement();  // 发送下一个点划
        break;
        
    case CharGap:  // 字符间隔结束
        m_state = SendingElement;
        nextChar();  // 启动下一个字符
        break;
    case WordGap:
        // 单词间隔结束，处理下一个字符
        // m_currentIndex++;
        m_state = SendingElement;
        nextChar();
        break;
    }
}

void PCKeyer::nextChar()
{
    if (m_currentIndex >= m_buffer.length()) {
        m_state = Idle;
        endTransmission();
        notifyBufferEmpty();
        return;
    }
    
    QChar ch = m_buffer[m_currentIndex];

    if (isSpeedControlChar(ch)) {
        notifyCharComplete(m_currentIndex, ch);
        m_currentIndex++;
        nextChar();
        return;
    }

    if (ch == ' ') {
        qDebug() << "Word gap";
        m_currentIndex++;
        if (m_currentIndex >= m_buffer.length()) {
            m_state = Idle;
            endTransmission();
            notifyBufferEmpty();
            return;
        }
        m_state = WordGap;
        m_timer->start(elementDuration(false) * 5);  // 5个单位
        return;
    }
    
    auto it = s_morseTable.find(ch);
    if (it == s_morseTable.end()) {
        qDebug() << "Unknown char:" << ch;
        m_currentIndex++;
        nextChar();
        return;
    }
    
    m_currentPattern = it->pattern;
    m_patternIndex = 0;
    
    // 关键：启动第一个元素
    startElement();  // 里面会设置 m_state = SendingElement
}