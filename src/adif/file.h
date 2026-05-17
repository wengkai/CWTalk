#ifndef ADIF_FILE_H
#define ADIF_FILE_H

#include <vector>
#include <set>
#include <iostream>
#include "record.h"
#include <functional>

enum MERGE_OPT {
    MERGE_ADD = 1,
    MERGE_REPLACE,
    MERGE_MERGE,
    MERGE_SKIP,
    MERGE_QUIT
};

namespace adif {

class File {
public:
    int load(std::istream& in);
    int save(std::ostream& out) const;
    bool add(const adif::Record& record);
    void print(std::ostream& out) const;
    int size(void) const { return static_cast<int>(records.size()); }
    const Record* find(const adif::Record& record);
    void remove(const adif::Record& record);
    void update(const adif::Record& orecord,const adif::Record& nrecord);
    void export_csv(std::ostream& out) const;
    void filter_out(std::function<bool(const Record&)> filter);
    std::vector<adif::Record> search_records_by_call(const std::string& call) const;
    std::vector<adif::Record> find_records_by_call(const std::string& call) const;
    std::vector<adif::Record> search_records_by_date(const std::string& sdate, const std::string& edate) const;
    std::vector<adif::Record> search_records_by_mode_band(const std::string& mode, const std::string& band) const;

    using MergeCallback = MERGE_OPT (*)(const adif::Record& orig, const adif::Record& cur);
    int merge(const adif::File& file, MergeCallback opt_callback);
private:
    std::set<adif::Record> records;
};

}

#endif
