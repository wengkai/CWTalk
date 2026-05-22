#ifndef QSOLOG_H
#define QSOLOG_H

#include <QByteArray>
#include <QString>
#include <vector>
#include "adif/file.h"
#include "adif/record.h"

// Qt 侧 ADIF 日志：启动时全量加载；新记录追加；编辑/删除时尽量原地写或内存拼接后一次落盘
class QsoLog
{
public:
    bool load(const QString &path);
    bool appendAndSave(const adif::Record &record);
    bool removeAndSave(const adif::Record &key);
    bool updateAndSave(const adif::Record &oldKey, const adif::Record &newRecord);
    const adif::Record *findRecord(const adif::Record &key);
    int count() const { return m_file.size(); }
    QString filePath() const { return m_path; }
    std::vector<adif::Record> lastRecordsForCall(const QString &call, int limit = 5) const;

private:
    struct RecordSlot {
        adif::Record rec;
        qint64 offset = 0;
        qint64 length = 0;
    };

    bool writeBlobToDisk();
    bool rebuildBlobFromMemory();
    bool writeInPlace(qint64 offset, const QByteArray &data);
    void parseBlobIntoMemory();
    int findSlotIndex(const adif::Record &key) const;
    static bool recordMatchesKey(const adif::Record &stored, const adif::Record &key);

    adif::File m_file;
    QString m_path;
    QByteArray m_fileBlob;
    std::vector<RecordSlot> m_slots;
};

#endif
