#ifndef CALLSIGNPREFIXDB_H
#define CALLSIGNPREFIXDB_H

#include <QString>
#include <QHash>

struct PrefixLookupResult {
    bool valid = false;
    QString country;
    QString continent;
    int cqZone = 0;
    int ituZone = 0;
    double latitude = 0.0;
    double longitude = 0.0;
    QString matchedPrefix;
};

class CallsignPrefixDatabase
{
public:
    bool loadFromFile(const QString &path);
    PrefixLookupResult lookup(const QString &callsign) const;

    QString lastError() const { return m_lastError; }
    int prefixCount() const { return m_prefixes.size() + m_exactMatch.size(); }

private:
    struct PrefixInfo {
        QString country;
        QString continent;
        int cqZone = 0;
        int ituZone = 0;
        double latitude = 0.0;
        double longitude = 0.0;
    };

    struct CountryBlock {
        PrefixInfo info;
        QString primaryPrefix;
    };

    static QString normalizeCallsignBase(const QString &callsign);
    static QString normalizePrefixToken(const QString &raw, bool *exactMatch);
    static bool isCountryHeaderLine(const QString &line);
    static bool parseCountryHeader(const QString &line, CountryBlock *block);
    static void parseAliasLine(const QString &line, const CountryBlock &block,
                               QHash<QString, PrefixInfo> *prefixes,
                               QHash<QString, PrefixInfo> *exactMatch);

    void addPrefix(const QString &prefix, bool exact, const PrefixInfo &info);

    QHash<QString, PrefixInfo> m_prefixes;
    QHash<QString, PrefixInfo> m_exactMatch;
    QString m_lastError;
};

#endif
