#include "adiftime.h"
#include "config.h"

namespace AdifTime {

QTimeZone stationTimeZone()
{
    const QString id = theConfig.getString("Operation/TimeZone", QStringLiteral("system")).trimmed();
    if (id.isEmpty() || id.compare(QStringLiteral("system"), Qt::CaseInsensitive) == 0)
        return QTimeZone::systemTimeZone();

    const QTimeZone tz(id.toUtf8());
    return tz.isValid() ? tz : QTimeZone::systemTimeZone();
}

QDateTime wallClockNow()
{
    return QDateTime::currentDateTimeUtc().toTimeZone(stationTimeZone());
}

QDateTime toUtc(const QDateTime &wallClock)
{
    if (!wallClock.isValid())
        return wallClock;

    if (wallClock.timeSpec() == Qt::UTC)
        return wallClock;

    if (wallClock.timeSpec() == Qt::TimeZone)
        return wallClock.toTimeZone(QTimeZone::utc());

    const QDateTime zoned(wallClock.date(), wallClock.time(), stationTimeZone());
    return zoned.toTimeZone(QTimeZone::utc());
}

void adifUtcFields(const QDateTime &wallClock, QString *qsoDate, QString *timeHms)
{
    const QDateTime utc = toUtc(wallClock);
    if (qsoDate)
        *qsoDate = utc.toString(QStringLiteral("yyyyMMdd"));
    if (timeHms)
        *timeHms = utc.toString(QStringLiteral("HHmmss"));
}

} // namespace AdifTime
