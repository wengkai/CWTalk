#include "qsolog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace {

std::string serializeRecord(const adif::Record &record)
{
    std::ostringstream oss;
    record.save(oss);
    return oss.str();
}

qint64 findAdifRecordEnd(const QByteArray &blob, qint64 start)
{
    if (start < 0 || start >= blob.size())
        return -1;

    const int eorPos = blob.indexOf("<eor>", static_cast<int>(start));
    if (eorPos < 0)
        return -1;

    qint64 end = eorPos + 5;
    if (end < blob.size() && blob.at(static_cast<int>(end)) == '\r')
        ++end;
    if (end < blob.size() && blob.at(static_cast<int>(end)) == '\n')
        ++end;
    return end;
}

} // namespace

bool QsoLog::recordMatchesKey(const adif::Record &stored, const adif::Record &key)
{
    adif::Record probe = key;
    return stored.date_time() == probe.date_time()
        && stored.get_field("mode") == probe.get_field("mode")
        && stored.get_field("freq") == probe.get_field("freq")
        && stored.get_field("call") == probe.get_field("call");
}

bool QsoLog::writeBlobToDisk()
{
    if (m_path.isEmpty())
        return false;

    const QFileInfo info(m_path);
    if (!info.absolutePath().isEmpty())
        QDir().mkpath(info.absolutePath());

    QSaveFile out(m_path);
    if (!out.open(QIODevice::WriteOnly))
        return false;
    if (out.write(m_fileBlob) != m_fileBlob.size())
        return false;
    return out.commit();
}

bool QsoLog::writeInPlace(qint64 offset, const QByteArray &data)
{
    if (m_path.isEmpty() || data.isEmpty())
        return false;

    QFile file(m_path);
    if (!file.open(QIODevice::ReadWrite))
        return false;
    if (!file.seek(offset))
        return false;
    if (file.write(data) != data.size())
        return false;
    return true;
}

void QsoLog::parseBlobIntoMemory()
{
    m_file = adif::File{};
    m_slots.clear();

    qint64 pos = 0;
    while (pos < m_fileBlob.size()) {
        const qint64 start = pos;
        const qint64 end = findAdifRecordEnd(m_fileBlob, start);
        if (end < 0)
            break;

        const QByteArray slice =
            m_fileBlob.mid(static_cast<int>(start), static_cast<int>(end - start));
        std::istringstream in(slice.toStdString());
        adif::Record rec;
        if (rec.load(in) && rec.iscomplete()) {
            RecordSlot slot;
            slot.rec = rec;
            slot.offset = start;
            slot.length = end - start;
            m_slots.push_back(slot);
            m_file.add(rec);
        }
        pos = end;
    }
}

bool QsoLog::rebuildBlobFromMemory()
{
    std::ostringstream oss;
    if (m_file.save(oss) < 0)
        return false;

    m_fileBlob = QByteArray::fromStdString(oss.str());
    parseBlobIntoMemory();
    return writeBlobToDisk();
}

int QsoLog::findSlotIndex(const adif::Record &key) const
{
    for (int i = 0; i < static_cast<int>(m_slots.size()); ++i) {
        if (recordMatchesKey(m_slots[static_cast<size_t>(i)].rec, key))
            return i;
    }
    return -1;
}

bool QsoLog::load(const QString &path)
{
    m_path = path;
    m_file = adif::File{};
    m_slots.clear();
    m_fileBlob.clear();

    if (m_path.isEmpty())
        return false;

    const QFileInfo info(m_path);
    if (!info.absolutePath().isEmpty())
        QDir().mkpath(info.absolutePath());

    if (!QFile::exists(m_path))
        return true;

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    m_fileBlob = file.readAll();
    parseBlobIntoMemory();
    return true;
}

bool QsoLog::appendAndSave(const adif::Record &record)
{
    if (!record.iscomplete() || m_path.isEmpty())
        return false;

    adif::Record probe = record;
    if (m_file.find(probe) != nullptr)
        return false;

    const std::string bytes = serializeRecord(record);
    const QByteArray chunk = QByteArray::fromStdString(bytes);

    const QFileInfo info(m_path);
    if (!info.absolutePath().isEmpty())
        QDir().mkpath(info.absolutePath());

    std::ofstream out(m_path.toStdString(),
                      std::ios::binary | std::ios::app);
    if (!out.is_open())
        return false;

    if (!out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        return false;
    }
    out.flush();
    if (!out.good())
        return false;

    RecordSlot slot;
    slot.rec = record;
    slot.offset = m_fileBlob.size();
    slot.length = chunk.size();
    m_fileBlob.append(chunk);
    m_slots.push_back(slot);
    m_file.add(record);
    return true;
}

const adif::Record *QsoLog::findRecord(const adif::Record &key)
{
    adif::Record probe = key;
    return m_file.find(probe);
}

bool QsoLog::removeAndSave(const adif::Record &key)
{
    const adif::Record *existing = findRecord(key);
    if (!existing)
        return false;

    adif::Record stored = *existing;
    m_file.remove(stored);
    return rebuildBlobFromMemory();
}

bool QsoLog::updateAndSave(const adif::Record &oldKey, const adif::Record &newRecord)
{
    if (!newRecord.iscomplete() || m_path.isEmpty())
        return false;

    const adif::Record *existing = findRecord(oldKey);
    if (!existing)
        return false;

    adif::Record storedOld = *existing;
    adif::Record probe = newRecord;
    const adif::Record *dup = m_file.find(probe);
    if (dup && !recordMatchesKey(*dup, storedOld))
        return false;

    const std::string newBytes = serializeRecord(newRecord);
    const QByteArray chunk = QByteArray::fromStdString(newBytes);
    const int slotIdx = findSlotIndex(storedOld);

    if (slotIdx >= 0
        && static_cast<qint64>(newBytes.size()) == m_slots[static_cast<size_t>(slotIdx)].length) {
        const qint64 offset = m_slots[static_cast<size_t>(slotIdx)].offset;
        if (!writeInPlace(offset, chunk))
            return false;
        m_fileBlob.replace(static_cast<int>(offset), static_cast<int>(chunk.size()), chunk);
        m_slots[static_cast<size_t>(slotIdx)].rec = newRecord;
        m_file.update(storedOld, newRecord);
        return true;
    }

    m_file.update(storedOld, newRecord);
    return rebuildBlobFromMemory();
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
