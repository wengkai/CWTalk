#ifndef CONFIG_H
#define CONFIG_H

#include <QString>
#include <QVariant>
#include <QSettings>
#include <QMutex>

class Config
{
public:
    // 获取单例实例
    static Config& instance();
    
    // 删除拷贝构造和赋值
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    // 读取配置（带默认值）
    QVariant get(const QString &key, const QVariant &defaultValue = QVariant()) const;
    QString getString(const QString &key, const QString &defaultValue = QString()) const;
    int getInt(const QString &key, int defaultValue = 0) const;
    bool getBool(const QString &key, bool defaultValue = false) const;
    double getDouble(const QString &key, double defaultValue = 0.0) const;
    
    // 写入配置（立即同步到文件）
    void set(const QString &key, const QVariant &value);
    
    // 批量写入（减少文件操作）
    void beginGroup(const QString &prefix);
    void endGroup();
    
    // 文件路径
    QString filePath() const;
    
    // 重新加载（如果外部修改了文件）
    void reload();
    
private:
    Config();  // 私有构造
    ~Config();
    
    void initDefaults();  // 初始化默认值
    void migrateLegacyStationKeys();  // 旧 Station 键名 → 与宏变量同名

    // 保存（通常不需要手动调用，set 已自动保存）
    void save();
    
    QSettings *m_settings;
    mutable QMutex m_mutex;  // 线程安全
};

// 便捷宏
#define theConfig Config::instance()

#endif