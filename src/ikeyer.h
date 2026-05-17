#ifndef IKEYER_H
#define IKEYER_H

#include <QObject>
#include <QString>

// 发送状态回调接口
class IKeyerListener {
public:
    virtual ~IKeyerListener() = default;
    
    // Keyer 开始发送字符
    // virtual void onKeyerCharStart(int index, QChar ch) = 0;
    
    // Keyer 完成发送字符
    virtual void onKeyerCharComplete(int index, QChar ch) = 0;
    
    // Keyer 完成发送字符串
    // virtual void onKeyerComplete() = 0;
    
    // Keyer 被中断
    // virtual void onKeyerInterrupted() = 0;

    virtual void onKeyerBufferEmpty() = 0;  // 缓冲区发送完毕
};

class IKeyer  {
    // Q_OBJECT

public:
    // explicit IKeyer(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IKeyer() = default;

    // 设置回调监听
    void setListener(IKeyerListener *listener) { m_listener = listener; }
    
    // 核心接口
    virtual bool open() = 0;                    // 打开硬件
    virtual void close() = 0;                   // 关闭硬件
    virtual bool isOpen() const = 0;            // 是否已打开
    
    // virtual void sendChar(QChar ch) = 0;        // 发送单个字符
    // virtual void sendString(const QString &str) = 0;  // 发送字符串
    // virtual void interrupt() = 0;               // 中断当前发送
    virtual void clear() = 0;                    // 清空，停止
    virtual void update(const QString &text) = 0; // 智能更新缓冲区

    // 速度控制
    virtual void setWpm(int wpm) = 0;
    virtual int wpm() const = 0;
    
    // 当前状态
    virtual bool isSending() const = 0;
    // virtual QString buffer() const = 0;         // 当前缓冲区内容
    virtual int currentIndex() const = 0;       // 当前发送到的位置

    // 第一个允许用户编辑的字符下标（已发完及正在发点划的字符不可改）
    virtual int firstEditableIndex() const = 0;
    // 与 firstEditableIndex 对应、必须与输入框前缀一致的一段缓冲快照
    virtual QString requiredSendPrefix() const = 0;

protected:
    IKeyerListener *m_listener = nullptr;
    
    // 辅助函数：通知监听者
    // void notifyCharStart(int index, QChar ch) {
    //     if (m_listener) m_listener->onKeyerCharStart(index, ch);
    // }
    void notifyCharComplete(int index, QChar ch) {
        if (m_listener) m_listener->onKeyerCharComplete(index, ch);
    }
    // void notifyComplete() {
    //     if (m_listener) m_listener->onKeyerComplete();
    // }
    // void notifyInterrupted() {
    //     if (m_listener) m_listener->onKeyerInterrupted();
    // }
    void notifyBufferEmpty() {
        if (m_listener) m_listener->onKeyerBufferEmpty();
    }
};

#endif