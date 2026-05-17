#ifndef RECORD_H
#define RECORD_H

#include <map>
#include <set>
#include <string>
#include <time.h>

namespace adif {
class Record {
public:
    
    // load a record from a file, returns true if successful
    bool load(std::istream& in);
    // save a record to a file, returns true if successful
    bool save(std::ostream& out) const;

    // bool operator<(const Record& that) const {
    //     return date_time() < that.date_time();
    // }
    friend inline bool operator<(const Record& lhs, const Record& rhs);
    friend inline bool operator==(const Record& lhs, const Record& rhs);
    
    std::string to_string() const;

    time_t time_stamp() const;

    std::string date_time() const {
        if (fields.count("qso_date") > 0 && fields.count("time_on") > 0) {
            std::string ret= fields.at("qso_date") + fields.at("time_on");
            if ( ret.length() == 12 ) {
                ret += "00";
            }
            return ret;
        }
        return ""; // Handle the case when the required keys are not present
    }

    std::set<std::string> enum_fields(void) const;

    std::string get_field(const std::string& key) const {
        if (fields.count(key) > 0) {
            return fields.at(key);
        }
        return ""; // Handle the case when the key is not present
    }

    void set_field(const std::string& key, const std::string& value) {
        fields[key] = value;
    }

    bool equal(const Record& that);
    bool inband(const std::string& band);
    void merge(const Record& that);
    static std::string& least_key_str(void);
    std::string export_csv(std::set<std::string> fields) const;
    bool iscomplete() const;
private:
    std::map<std::string, std::string> fields;
    static const std::string least_keys[];
};

inline bool operator<(const Record& lhs, const Record& rhs) {
    if ( lhs.date_time() < rhs.date_time() )
        return true;
    if ( lhs.date_time() == rhs.date_time() )
        return lhs.get_field("call") < rhs.get_field("call");
    return false;
}

inline bool operator==(const Record& lhs, const Record& rhs) {
    return lhs.date_time() == rhs.date_time() &&
        lhs.get_field("call") == rhs.get_field("call");
}
}   //  namespace adif

#endif // RECORD_H