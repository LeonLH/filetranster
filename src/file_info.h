#ifndef _TRANSFER_FILE_INFO_H
#define _TRANSFER_FILE_INFO_H

#include <string>

namespace ft {

    struct FileInfo
    {
        std::string id;
        std::string type;
        std::string date;
        std::string tm_begin;
        std::string tm_end;

        FileInfo() : id(),
            type(),
            date(),
            tm_begin(),
            tm_end()
        {}

        std::string key() const
        {
            std::string key;

            key.append(id).append("_")
                .append(type).append("_")
                .append(date).append("_")
                .append(tm_begin).append("_")
                .append(tm_end);

            return key;
        }

        std::string to_string() const
        {
            std::string str;
            str.append("ID=").append(id).append(",");
            str.append("TYPE=").append(type).append(",");
            str.append("DATE=").append(date).append(",");
            str.append("BEGIN=").append(tm_begin).append(",");
            str.append("END=").append(tm_end).append(",");

            return str;
        }
    };
}

#endif // 
