#include "file.h"

using namespace std;
using namespace adif;

int adif::File::load(istream& in) 
{
    int ret = 0;
    while (true) {
        Record record;
        if ( record.load(in) == false ) {
            // cout << "ended at:" << record.to_string() << endl;
            break;
        }
        if ( record.iscomplete() == false ) {
            cout << "Incomplete record: " << record.date_time() << "," << record.get_field("call") << endl;
            continue;
        }
        // cout << record.to_string();
        // records.push_back(record);
        pair<set<adif::Record>::iterator,bool> r = records.insert(record);
        if ( !r.second ) {
            cout << "For: " << record.to_string() << endl;
            cout << "Record already exists: " << r.first->to_string() << endl;
        }
        ret++;
    }
    return ret;
}

int adif::File::save(ostream& out) const 
{
    int ret = 0;
    // for (vector<Record>::const_iterator it = records.begin(); it != records.end(); it++) {
    //     it->save(out);
    //     ret++;
    // }
    for ( auto record : records ) {
        record.save(out);
        ret++;
    }
    return ret;
}

bool adif::File::add(const adif::Record& record) 
{
    // bool ret = true;
    // if (find(records.begin(), records.end(), record) != records.end()) {
    //     ret = false;
    // } else {
    //     records.push_back(record);
    //     sort(records.begin(), records.end());
    // }
    pair<set<adif::Record>::iterator,bool> r = records.insert(record);
    return r.second;
}

void adif::File::print(ostream& out) const 
{
    for ( auto record : records ) {
        out << record.to_string() << endl;
    }
}

const Record* adif::File::find(const adif::Record& record)
{
    for ( set<Record>::iterator x = records.begin(); x != records.end(); x++ ) {
        if ( x->date_time() == record.date_time() && 
            x->get_field("mode") == record.get_field("mode") &&
		x->get_field("freq") == record.get_field("freq") && 
		x->get_field("call") == record.get_field("call") ) {
            return &(*x);
        }
    }
    return nullptr;
    //auto it = std::find(records.begin(), records.end(), record);
    //if (it != records.end()) {
    //    return &(*it);
    //}
    //return nullptr;
}

int adif::File::merge(const adif::File& file, MergeCallback opt_callback)
{
    int cnt = 0;
    for ( auto rec : file.records ) {
        const Record* p = find(rec);
        if (p) {
            if (rec.equal(*p)) continue;
            switch (opt_callback(*p, rec)) {
            case MERGE_ADD:
                add(rec);
                cnt +=1;
                break;
            case MERGE_REPLACE:
                records.erase(*p);
                add(rec);
                break;
            case MERGE_SKIP:
                continue;
            case MERGE_MERGE:
                rec.merge(*p);
                records.erase(*p);
                add(rec);
                break;
            case MERGE_QUIT:
                goto END;
            }
        } else {
            add(rec);
            cnt += 1;
        }
    }
END:
    return cnt;
}

void adif::File::filter_out(std::function<bool(const Record&)> filter)
{
    std::set<adif::Record> newrec;
    //  iterate records using iterator
    for ( auto x : records ) {
        if ( !filter(x) ) {
            newrec.insert(x);
        }
    }
    records = newrec;
    // for (auto it = records.begin(); it != records.end(); ++it) {
    //     if ( filter(*it) ) {
    //         cout << 1 << endl;
    //         records.erase(it);
    //         cout << 2 << endl;
    //     }
    // }
    // cout << 3 << endl;
}

vector<adif::Record> adif::File::search_records_by_call(const string& call) const
{
    vector<Record> ret;
    for ( auto record : records ) {
        if ( record.get_field("call") == call ) {
            ret.push_back(record);
        }
    }
    return ret;
}

static string upperAscii(const string& s)
{
    string u = s;
    for (char &c : u) {
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    }
    return u;
}

vector<adif::Record> adif::File::find_records_by_call(const string& call) const
{
    const string want = upperAscii(call);
    vector<Record> ret;
    for (const auto &record : records) {
        if (upperAscii(record.get_field("call")) == want)
            ret.push_back(record);
    }
    return ret;
}

vector<adif::Record> adif::File::search_records_by_mode_band(const string& mode, const string& band) const
{
    vector<Record> ret;
    for ( auto record : records ) {
        if (!mode.empty() && record.get_field("mode") != mode ) {
            continue;
        }
        if (!band.empty() ) {
            if ( !record.inband(band) ) {
                continue;
            }
        }
        ret.push_back(record);
    }
    return ret;
}

vector<adif::Record> adif::File::search_records_by_date(const string& sdate, const string& edate) const
{
    vector<Record> ret;
    for ( auto record : records ) {
        if ( record.get_field("qso_date") >= sdate && record.get_field("qso_date") <= edate ) {
            ret.push_back(record);
        }
    }
    return ret;
}

void adif::File::export_csv(ostream& out) const
{
    //  find all fields in all records
    set<string> fields;
    for ( auto record : records ) {
        set<string> keys = record.enum_fields();
        fields.insert(keys.begin(), keys.end());
    }
    //  print the least keys first
    string& least_keys = Record::least_key_str();
    out << least_keys;

    //  then the rest of the fields
    for ( auto field : fields ) {
        //  if the field is not in the least_keys
        if ( least_keys.find(field) == string::npos ) {
            out << field;
            //  if it is not the last field
            if ( field != *fields.rbegin() ) {
                out << ",";
            }
        }
    }

    out << endl;
    for ( auto record : records ) {
        out << record.export_csv(fields) << endl;
    }
}

void adif::File::remove(const adif::Record& record)
{
    records.erase(record);
}

void adif::File::update(const adif::Record& orecord, const adif::Record& nrecord)
{
    remove(orecord);
    add(nrecord);
}

#ifdef _FILE_TEST_
int main()
{
    adif::File file;
    file.load(cin);
    file.save(cout);
    // file.print(cout);
    return 0;
}
#endif
