#ifndef _FILE_TRANSFER_IMPL_H_
#define _FILE_TRANSFER_IMPL_H_

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>

#include "file_transfer_connection.h"

namespace ft
{
    typedef Transfer_connection Connection;

    using namespace boost::asio::ip;
    typedef boost::shared_ptr<tcp::acceptor> acceptor_ptr;

    class Transfer
    {
    public:
        enum Status
        {
            UNKNOWN,
            NOT_EXSISTS,
            QUERY,
            RECV,
            SEND,
            ABORT,
            READY,
        };
    public:
        explicit Transfer(const std::string& host, const std::string& port, const std::string& recv_path, bool is_server) : is_server_(is_server),
            ios_(),
            host_(host),
            port_(port),
            recv_path_(recv_path),
            acceptor_(),
            work_(new boost::asio::io_service::work(ios_))
        {
            if (is_server)
            {
                tcp::endpoint endpoint(address::from_string(host_), std::atoi(port.c_str()));
                acceptor_.reset(new tcp::acceptor(ios_, endpoint));

                start_accept();
            }
        }

        ~Transfer()
        {
            if (thread_.joinable())
                thread_.join();
        }

        void send(const std::string& file_path, const FileInfo& info)
        {
            LINFO << "start send " << file_path;

            boost::filesystem::path path(file_path);

            if (boost::filesystem::exists(path))
            {
                Status status = get_task_status(info);
                if (status == UNKNOWN)
                {
                    Connection::pointer conn = Connection::create(this, ios_, recv_path_);

                    add_task(conn, SEND);

                    conn->start_send(file_path, info, host_, port_);
                }
                else {
                    LWARNING << info.key() << " has pending task, status(" << status << ")";
                }
            }
            else {
                LERROR << "file not exists, " << boost::filesystem::system_complete(path).generic_string();
            }
        }

        Status query(const FileInfo& info)
        {
            LINFO << "query " << info.key();

            Status status = get_task_status(info);
            if (status != UNKNOWN)
                return status;

            /*std::string file = recv_path_ + info.type + "/" + info.key();
            boost::filesystem::path path(file);
            if (boost::filesystem::exists(path))
            {
                LINFO << "file " << info.key() << " exists";
                return READY;
            }*/

            if (!is_server_)
            {
                Connection::pointer conn = Connection::create(this, ios_, recv_path_);

                conn->start_query(info, host_, port_);

                return QUERY;
            }

            return UNKNOWN;
        }

        void start()
        {
            thread_ = boost::thread(boost::bind(&Transfer::run,
                this
            ));

        }

        void stop()
        {
            work_.reset();
            if (thread_.joinable())
                thread_.join();
        }

    private:
        friend class Transfer_connection;

        void start_accept()
        {
            Transfer_connection::pointer new_connection =
                Transfer_connection::create(this, acceptor_->get_io_service(), recv_path_);

            acceptor_->async_accept(new_connection->socket(),
                boost::bind(&Transfer::handle_accept, this, new_connection,
                    boost::asio::placeholders::error));
        }

        void handle_accept(Transfer_connection::pointer new_connection,
            const boost::system::error_code& error)
        {
            if (!error)
            {
                new_connection->start_recv();
            }

            start_accept();
        }

        void run()
        {
            ios_.run();
        }

        Status get_task_status(const FileInfo& info)
        {
            boost::lock_guard<boost::mutex> guard(mutex_);

            std::map<std::string, Task>::iterator it = pending_task_.find(info.key());
            if (it != pending_task_.end())
            {
                return it->second.status;
            }

            return UNKNOWN;
        }

        void add_task(Connection::pointer ptr, Status status)
        {
            boost::lock_guard<boost::mutex> guard(mutex_);

            std::map<std::string, Task>::value_type value(ptr->file_info().key(), Task(status, ptr));

            std::pair< std::map<std::string, Task>::iterator, bool> result = pending_task_.insert(value);
            if (!result.second)
            {
                LERROR << "duplicate task " << ptr->file_info().key();
            }
        }

        void remove_task(Connection::pointer ptr)
        {
            boost::lock_guard<boost::mutex> guard(mutex_);
            pending_task_.erase(ptr->file_info().key());
        }

    private:
        const bool is_server_;
        boost::asio::io_service ios_;
        boost::shared_ptr< boost::asio::io_service::work> work_;
        boost::thread thread_;
        const std::string host_;
        const std::string port_;
        const std::string recv_path_;

        acceptor_ptr acceptor_;

        boost::mutex mutex_;
        struct Task {
            Status status;
            Connection::pointer connection;

            Task() : status(UNKNOWN),
                connection()
            {}

            Task(Status s, Connection::pointer ptr) : status(s),
                connection(ptr)
            {}
        };

        std::map<std::string, Task> pending_task_;
    };
}

#endif
