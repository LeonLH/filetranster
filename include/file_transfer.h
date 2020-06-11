#ifndef _FILE_TRANSFR_LIB_H_ 
#define _FILE_TRANSFR_LIB_H_

#include <string>
#include <boost/shared_ptr.hpp>

namespace ft {

    class Transfer;
    typedef boost::shared_ptr<Transfer> TransferPtr;
    
    TransferPtr make_transfer_client(const std::string& host, const std::string& port, const std::string& recv_path ="./files/", const std::string& log_path = "./logs/");
    TransferPtr make_transfer_server(const std::string& addr, const std::string& port, const std::string& recv_path = "./files/", const std::string& log_path = "./logs/");
    void stop(TransferPtr& tran);
    void send(TransferPtr& tran, const std::string& file, const std::string& id, const std::string& type, const std::string& date, const std::string& tm_begin, const std::string& tm_end);
    void query(TransferPtr& tran, const std::string& id, const std::string& type, const std::string& date, const std::string& tm_begin, const std::string& tm_end);

}

#endif // 
