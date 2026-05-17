#include "callsignprefixdb.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTextStream>

namespace {

bool isPrefixChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('/');
}

} // namespace

QString CallsignPrefixDatabase::normalizeCallsignBase(const QString &callsign)
{
    QString base = callsign.trimmed().toUpper();
    const int slash = base.indexOf(QLatin1Char('/'));
    if (slash >= 0)
        base = base.left(slash);

    QString out;
    out.reserve(base.size());
    for (const QChar c : base) {
        if (isPrefixChar(c))
            out.append(c);
    }
    return out;
}

QString CallsignPrefixDatabase::normalizePrefixToken(const QString &raw, bool *exactMatch)
{
    if (exactMatch)
        *exactMatch = false;

    QString token = raw.trimmed().toUpper();
    if (token.isEmpty())
        return {};

    if (token.startsWith(QLatin1Char('='))) {
        token = token.mid(1);
        if (exactMatch)
            *exactMatch = true;
    }

    if (token.startsWith(QLatin1Char('*')))
        token = token.mid(1);

    for (const QChar sep :
         {QLatin1Char('('), QLatin1Char('['), QLatin1Char('<'),
          QLatin1Char('{'), QLatin1Char('~')}) {
        const int i = token.indexOf(sep);
        if (i >= 0)
            token = token.left(i);
    }

    QString out;
    out.reserve(token.size());
    for (const QChar c : token) {
        if (isPrefixChar(c))
            out.append(c);
    }
    return out;
}

bool CallsignPrefixDatabase::isCountryHeaderLine(const QString &line)
{
    static const QRegularExpression re(
        QStringLiteral("^.+:\\s*\\d+\\s*:\\s*\\d+\\s*:"));
    return re.match(line).hasMatch();
}

bool CallsignPrefixDatabase::parseCountryHeader(const QString &line, CountryBlock *block)
{
    if (!block)
        return false;

    const QStringList fields = line.split(QLatin1Char(':'), Qt::KeepEmptyParts);
    if (fields.size() < 7)
        return false;

    block->info.country = fields.at(0).trimmed();
    block->info.cqZone = fields.at(1).trimmed().toInt();
    block->info.ituZone = fields.at(2).trimmed().toInt();
    block->info.continent = fields.at(3).trimmed();
    block->info.latitude = fields.at(4).trimmed().toDouble();
    block->info.longitude = fields.at(5).trimmed().toDouble();

    QString primary = fields.at(6).trimmed();
    if (primary.startsWith(QLatin1Char('*')))
        primary = primary.mid(1);
    block->primaryPrefix = normalizePrefixToken(primary, nullptr);
    return !block->info.country.isEmpty();
}

void CallsignPrefixDatabase::parseAliasLine(const QString &line, const CountryBlock &block,
                                            QHash<QString, PrefixInfo> *prefixes,
                                            QHash<QString, PrefixInfo> *exactMatch)
{
    if (!prefixes || !exactMatch)
        return;

    QString s = line.trimmed();
    if (s.endsWith(QLatin1Char(';')))
        s.chop(1);

    const QStringList tokens = s.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString raw : tokens) {
        bool exact = false;
        const QString pfx = normalizePrefixToken(raw, &exact);
        if (pfx.isEmpty())
            continue;

        if (exact) {
            if (!exactMatch->contains(pfx))
                exactMatch->insert(pfx, block.info);
        } else if (!prefixes->contains(pfx)) {
            prefixes->insert(pfx, block.info);
        }
    }
}

void CallsignPrefixDatabase::addPrefix(const QString &prefix, bool exact, const PrefixInfo &info)
{
    if (prefix.isEmpty())
        return;
    if (exact) {
        if (!m_exactMatch.contains(prefix))
            m_exactMatch.insert(prefix, info);
    } else if (!m_prefixes.contains(prefix)) {
        m_prefixes.insert(prefix, info);
    }
}

bool CallsignPrefixDatabase::loadFromFile(const QString &path)
{
    m_prefixes.clear();
    m_exactMatch.clear();
    m_lastError.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QStringLiteral("无法打开冠字表: %1").arg(path);
        return false;
    }

    CountryBlock block;
    bool haveBlock = false;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.trimmed().isEmpty())
            continue;

        if (isCountryHeaderLine(line)) {
            if (parseCountryHeader(line, &block)) {
                haveBlock = true;
                if (!block.primaryPrefix.isEmpty())
                    addPrefix(block.primaryPrefix, false, block.info);
            }
            continue;
        }

        if (haveBlock)
            parseAliasLine(line, block, &m_prefixes, &m_exactMatch);
    }

    if (m_prefixes.isEmpty() && m_exactMatch.isEmpty()) {
        m_lastError = QStringLiteral("冠字表未解析到任何前缀: %1").arg(path);
        return false;
    }
    return true;
}

PrefixLookupResult CallsignPrefixDatabase::lookup(const QString &callsign) const
{
    PrefixLookupResult result;
    const QString base = normalizeCallsignBase(callsign);
    if (base.isEmpty())
        return result;

    if (m_exactMatch.contains(base)) {
        const PrefixInfo &info = m_exactMatch.value(base);
        result.valid = true;
        result.country = info.country;
        result.continent = info.continent;
        result.cqZone = info.cqZone;
        result.ituZone = info.ituZone;
        result.latitude = info.latitude;
        result.longitude = info.longitude;
        result.matchedPrefix = base;
        return result;
    }

    int bestLen = 0;
    PrefixInfo best;

    const int maxTry = qMin(base.length(), 8);
    for (int len = maxTry; len >= 1; --len) {
        const QString candidate = base.left(len);
        const auto it = m_prefixes.constFind(candidate);
        if (it != m_prefixes.constEnd() && len > bestLen) {
            bestLen = len;
            best = it.value();
            result.matchedPrefix = candidate;
        }
    }

    if (bestLen <= 0)
        return result;

    result.valid = true;
    result.country = best.country;
    result.continent = best.continent;
    result.cqZone = best.cqZone;
    result.ituZone = best.ituZone;
    result.latitude = best.latitude;
    result.longitude = best.longitude;
    return result;
}
