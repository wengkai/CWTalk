#include "qsolog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <fstream>

bool QsoLog::load(const QString &path)
{
    m_path = path;
    m_file = adif::File{};

    if (m_path.isEmpty())
        return false;

    const QFileInfo info(m_path);
    if (!info.absolutePath().isEmpty())
        QDir().mkpath(info.absolutePath());

    if (!QFile::exists(m_path))
        return true;

    std::ifstream in(m_path.toStdString(), std::ios::binary);
    if (!in.is_open())
        return false;

    m_file.load(in);
    return true;
}

bool QsoLog::appendAndSave(const adif::Record &record)
{
    if (!record.iscomplete() || m_path.isEmpty())
        return false;

    adif::Record probe = record;
    if (m_file.find(probe) != nullptr)
        return false;

    const QFileInfo info(m_path);
    if (!info.absolutePath().isEmpty())
        QDir().mkpath(info.absolutePath());

    std::ofstream out(m_path.toStdString(),
                      std::ios::binary | std::ios::app);
    if (!out.is_open())
        return false;

    if (!record.save(out)) {
        return false;
    }
    out.flush();
    if (!out.good())
        return false;

    m_file.add(record);
    return true;
}

std::vector<adif::Record> QsoLog::lastRecordsForCall(const QString &call, int limit) const
{
    if (limit <= 0 || call.trimmed().isEmpty())
        return {};

    std::vector<adif::Record> matches =
        m_file.find_records_by_call(call.trimmed().toUpper().toStdString());

    std::sort(matches.begin(), matches.end(),
              [](const adif::Record &a, const adif::Record &b) {
                  return a.date_time() > b.date_time();
              });

    if (static_cast<int>(matches.size()) > limit)
        matches.resize(static_cast<size_t>(limit));
    return matches;
}
