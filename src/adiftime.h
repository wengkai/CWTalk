#ifndef ADIFTIME_H
#define ADIFTIME_H

#include <QDateTime>
#include <QTimeZone>
#include <QString>

namespace AdifTime {

QTimeZone stationTimeZone();
QDateTime wallClockNow();
QDateTime toUtc(const QDateTime &wallClock);
void adifUtcFields(const QDateTime &wallClock, QString *qsoDate, QString *timeHms);

} // namespace AdifTime

#endif
