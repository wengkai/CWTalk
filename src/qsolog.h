#ifndef QSOLOG_H
#define QSOLOG_H

#include <QString>
#include <vector>
#include "adif/file.h"
#include "adif/record.h"

// Qt 侧 ADIF 日志：启动时全量加载；记录时仅追加一条并 flush
class QsoLog
{
public:
    bool load(const QString &path);
    bool appendAndSave(const adif::Record &record);
    int count() const { return m_file.size(); }
    QString filePath() const { return m_path; }
    std::vector<adif::Record> lastRecordsForCall(const QString &call, int limit = 5) const;

private:
    adif::File m_file;
    QString m_path;
};

#endif
