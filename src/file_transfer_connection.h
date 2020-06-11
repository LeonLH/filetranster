#ifndef _FILE_TRANSFER_WIN32_TRANSFER_H_
#define _FILE_TRANSFER_WIN32_TRANSFER_H_

#include <string>
#include <fstream>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>

#ifdef __linux__
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#endif

#include "easylogging++.h"
#include "file_info.h"

namespace ft {

    class Transfer;
    using boost::asio::ip::tcp;

#if defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)
    using boost::asio::windows::overlapped_ptr;
    using boost::asio::windows::random_access_handle;
#endif

    class Transfer_connection
        : public boost::enable_shared_from_this<Transfer_connection>
    {
    public:
        typedef boost::shared_ptr<Transfer_connection> pointer;

        static pointer create(Transfer* transfer, boost::asio::io_service& io_service, const std::string& recv_path)
        {
            return pointer(new Transfer_connection(transfer, io_service, recv_path));
        }

        tcp::socket& socket()
        {
            return socket_;
        }

        const FileInfo& file_info() const
        {
            return file_info_;
        }

        void start_recv();
        void start_query(const FileInfo& info, const std::string& addr, const std::string& port);
        void start_send(const std::string& file_path, const FileInfo& info, const std::string& addr, const std::string& port);

    private:
        Transfer_connection(Transfer* transfer, boost::asio::io_service& io_service, const std::string& recv_path);

        bool connect(const std::string& addr, const std::string& port);

        void send_file(const std::string& file_path);

        std::string make_query_action() const;

        std::string make_reply_action(const std::string& info, int status) const;

        std::string make_send_action(const FileInfo& file_info);

        void send_action(const std::string& info, boost::system::error_code& ec);
        void handle_action(const boost::system::error_code& error, size_t bytes_transferred);
        void handle_file_sent(const boost::system::error_code& error, size_t bytes_transferred);

        void start_recv_file();
        void handle_file_data_recv(const boost::system::error_code& error, size_t bytes_transferred);

        void start_reply();

        bool make_directory(const std::string type);
        std::string to_file_path(const FileInfo& info) const;

        void shutdown();

        void parseOptions(const std::string& str);
        std::string getOption(const std::string& key) const;

    private:
        Transfer* transfer_;
        tcp::socket socket_;
        std::string filename_;
#if defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)
        random_access_handle file_;
#endif
        std::string action_;
        std::map <std::string, std::string> optios_;
        FileInfo file_info_;
        int file_size_;
        int recv_count_;
        const std::string recv_path_;

        boost::asio::streambuf buffer_;
        std::ofstream  out_file_;
    };


} // NS ft

#endif // 
