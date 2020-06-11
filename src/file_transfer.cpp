#include <file_transfer.h>

#include <boost/shared_ptr.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

#include <easylogging++.h>
_INITIALIZE_EASYLOGGINGPP

#include "file_transfer_impl.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace ft {

    void initialize_logger(const std::string& path)
    {
        easyloggingpp::Configurations defaultConf;
        defaultConf.setAll(easyloggingpp::ConfigurationType::Filename, path + "file_transfer.log");

        easyloggingpp::Loggers::reconfigureAllLoggers(defaultConf);
    }

    TransferPtr make_transfer_client(const std::string& host, const std::string& port, const std::string& recv_path, const std::string& log_path)
    {
        initialize_logger(log_path);

        LINFO << "start file transfer client " << host << ":" << port;

        TransferPtr tran = boost::make_shared<Transfer>(host, port, recv_path, false);
        tran->start();

        return tran;
    }

    TransferPtr make_transfer_server(const std::string& addr, const std::string& port, const std::string& recv_path, const std::string& log_path)
    {
        initialize_logger(log_path);

        LINFO << "start file transfer server " << addr << ":" << port;

        TransferPtr tran = boost::make_shared<Transfer>(addr, port, recv_path, true);
        tran->start();

        return tran;
    }

    void stop(TransferPtr& tran)
    {
        if (tran)
        {
            LINFO << "shutdown file transfer";

            tran->stop();
        }
    }

    void send(TransferPtr& tran, const std::string& file, const std::string& id, const std::string& type, const std::string& date, const std::string& tm_begin, const std::string& tm_end)
    {
        if (tran)
        {
            FileInfo info;
            info.id = id;
            info.type = type;
            info.date = date;
            info.tm_begin = tm_begin;
            info.tm_end = tm_end;

            tran->send(file, info);
        }

    }

    void query(TransferPtr& tran, const std::string& id, const std::string& type, const std::string& date, const std::string& tm_begin, const std::string& tm_end)
    {
        if (tran)
        {
            FileInfo info;
            info.id = id;
            info.type = type;
            info.date = date;
            info.tm_begin = tm_begin;
            info.tm_end = tm_end;

            tran->query(info);
        }
    }

}