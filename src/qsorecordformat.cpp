#include "qsorecordformat.h"
#include "adif/record.h"

#include <QLocale>

namespace {

QString adifField(const adif::Record &rec, const char *key)
{
    return QString::fromStdString(rec.get_field(key)).trimmed();
}

QString formatAdifDate(QString d)
{
    d = d.trimmed();
    if (d.length() == 8)
        return d.left(4) + QLatin1Char('-') + d.mid(4, 2) + QLatin1Char('-') + d.mid(6, 2);
    return d;
}

QString formatAdifTimeCompact(QString t)
{
    t = t.trimmed();
    t.remove(QLatin1Char(':'));
    if (t.length() >= 6)
        return t.left(6);
    return t;
}

QString formatAdifFreqMHz(const QString &raw)
{
    bool ok = false;
    const double mhz = QLocale::c().toDouble(raw, &ok);
    if (ok && mhz > 0.0)
        return QLocale::c().toString(mhz, 'f', 3);
    return raw;
}

} // namespace

QString formatSessionLogLine(const adif::Record &rec)
{
    QStringList parts;
    parts << formatAdifDate(adifField(rec, "qso_date"))
          << formatAdifTimeCompact(adifField(rec, "time_on"))
          << formatAdifFreqMHz(adifField(rec, "freq"))
          << adifField(rec, "call")
          << adifField(rec, "rst_sent")
          << adifField(rec, "rst_rcvd")
          << adifField(rec, "name")
          << adifField(rec, "qth")
          << adifField(rec, "comment");
    return parts.join(QStringLiteral(", "));
}
