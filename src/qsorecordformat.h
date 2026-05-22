#ifndef QSORECORDFORMAT_H
#define QSORECORDFORMAT_H

#include <QString>

namespace adif {
class Record;
}

QString formatSessionLogLine(const adif::Record &rec);

#endif
