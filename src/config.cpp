#include "config.h"
#include <QCoreApplication>
#include <QDir>
#include <QDebug>

Config::Config()
{
    // 配置文件路径：可执行文件同目录下的 CWTalk.ini
    QString iniPath = QCoreApplication::applicationDirPath() + "/cwtalk.ini";
    
    m_settings = new QSettings(iniPath, QSettings::IniFormat);
    m_settings->setFallbacksEnabled(false);  // 不用注册表回退
    
    // 首次运行，初始化默认值
    if (!QFile::exists(iniPath)) {
        initDefaults();
        save();
    } else {
        migrateLegacyStationKeys();
    }
    
    qDebug() << "Config loaded from:" << iniPath;
}

Config::~Config()
{
    save();  // 确保退出前保存
    delete m_settings;
}

Config& Config::instance()
{
    static Config instance;  // C++11 保证线程安全
    return instance;
}

void Config::initDefaults()
{
    // 硬件设置
    set("Hardware/CAT_Port", "COM3");
    set("Hardware/CAT_Baud", 38400);  // Yaesu FT-710 CAT-1 默认 38400
    set("Hardware/CAT_Address", 0x94);  // Icom CI-V 预留

    set("Cat/Enabled", true);
    set("Cat/Backend", "yaesu");
    set("Cat/Poll_Interval_Ms", 800);
    set("Cat/Timeout_Ms", 1000);
    set("Hardware/Keying_Port", "COM8");
    set("Hardware/Keying_Line", "RTS");  // DTR 或 RTS
    set("Hardware/Keying_ActiveLow", false);  // 高电平有效
    
    // 己方台站（键名与宏变量 MY_* 一致）
    set("Station/MY_CALL", "BH4XXX");
    set("Station/MY_NAME", "");
    set("Station/MY_QTH", "");
    set("Station/MY_RIG", "IC-7300");
    set("Station/Grid", "PM01bm");  // 6位 Grid
    set("Station/Power", 100);
    
    // 操作设置
    set("Operation/Default_WPM", 22);
    // set("Operation/CQ_Message", "CQ CQ CQ DE {MYCALL} K");
    set("Operation/CQ_Interval", 4);  // 秒
    set("Operation/Send_Clear_Delay_Sec", 2);  // 全部发完后延迟清空发送框；0=不自动清空
    
    // 文件路径
    set("Files/ADIF_Path", QCoreApplication::applicationDirPath() + "/log.adif");
    set("Files/CTY_Path", QCoreApplication::applicationDirPath() + "/data/cty.dat");

    for (int i = 1; i <= 8; ++i) {
        set(QString("Shortcuts/F%1_Title").arg(i), QString("F%1").arg(i));
        set(QString("Shortcuts/F%1_Content").arg(i), QString());
    }
}

QVariant Config::get(const QString &key, const QVariant &defaultValue) const
{
    QMutexLocker locker(&m_mutex);
    return m_settings->value(key, defaultValue);
}

QString Config::getString(const QString &key, const QString &defaultValue) const
{
    return get(key, defaultValue).toString();
}

int Config::getInt(const QString &key, int defaultValue) const
{
    return get(key, defaultValue).toInt();
}

bool Config::getBool(const QString &key, bool defaultValue) const
{
    return get(key, defaultValue).toBool();
}

double Config::getDouble(const QString &key, double defaultValue) const
{
    return get(key, defaultValue).toDouble();
}

void Config::set(const QString &key, const QVariant &value)
{
    QMutexLocker locker(&m_mutex);
    m_settings->setValue(key, value);
    m_settings->sync();  // 立即同步到文件
}

void Config::beginGroup(const QString &prefix)
{
    QMutexLocker locker(&m_mutex);
    m_settings->beginGroup(prefix);
}

void Config::endGroup()
{
    QMutexLocker locker(&m_mutex);
    m_settings->endGroup();
}

QString Config::filePath() const
{
    return m_settings->fileName();
}

void Config::reload()
{
    QMutexLocker locker(&m_mutex);
    m_settings->sync();  // 从文件重新加载
}

void Config::save()
{
    QMutexLocker locker(&m_mutex);
    m_settings->sync();
}

void Config::migrateLegacyStationKeys()
{
    QMutexLocker locker(&m_mutex);
    const struct { const char *oldKey; const char *newKey; } pairs[] = {
        {"Callsign", "MY_CALL"},
        {"Name", "MY_NAME"},
        {"QTH", "MY_QTH"},
        {"Rig", "MY_RIG"},
    };
    for (const auto &p : pairs) {
        const QString oldFull = QStringLiteral("Station/") + p.oldKey;
        const QString newFull = QStringLiteral("Station/") + p.newKey;
        if (!m_settings->contains(newFull) && m_settings->contains(oldFull)) {
            m_settings->setValue(newFull, m_settings->value(oldFull));
        }
    }
    m_settings->sync();
}