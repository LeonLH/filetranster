#include "file_transfer_impl.h"
#include <boost/filesystem.hpp>

using namespace ft;

#if defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)

    using boost::asio::windows::overlapped_ptr;
    using boost::asio::windows::random_access_handle;


    // A wrapper for the TransmitFile overlapped I/O operation.
    template <typename Handler>
    void transmit_file(tcp::socket& socket, random_access_handle& file, Handler handler)
    {
        // Construct an OVERLAPPED-derived object to contain the handler.
        overlapped_ptr overlapped(socket.get_io_service(), handler);

        // Initiate the TransmitFile operation.
        BOOL ok = ::TransmitFile(socket.native_handle(),
            file.native_handle(), 0, 0, overlapped.get(), 0, 0);
        DWORD last_error = ::GetLastError();

        // Check if the operation completed immediately.
        if (!ok && last_error != ERROR_IO_PENDING)
        {
            // The operation completed immediately, so a completion notification needs
            // to be posted. When complete() is called, ownership of the OVERLAPPED-
            // derived object passes to the io_service.
            boost::system::error_code ec(last_error,
                boost::asio::error::get_system_category());
            overlapped.complete(ec, 0);
        }
        else
        {
            // The operation was successfully initiated, so ownership of the
            // OVERLAPPED-derived object has passed to the io_service.
            overlapped.release();
        }
    }
#endif

void Transfer_connection::start_query(const FileInfo& info, const std::string& addr, const std::string& port)
{
    LINFO << "start query file " << info.key();

    if (!connect(addr, port))
        return;

    file_info_ = info;

    boost::system::error_code ec;
    std::string query = make_query_action();
    LINFO << "query: " << query;

    send_action(query, ec);
    if (ec)
    {
        LERROR << ec.message();
    }

    start_recv();
}

void Transfer_connection::start_recv()
{
    tcp::socket::endpoint_type remote = socket_.remote_endpoint();
    LINFO << "start recv for " << remote.address().to_string() << ":" << remote.port();

    boost::asio::async_read_until(socket_,
        buffer_,
        ';',
        boost::bind(&Transfer_connection::handle_action,
            shared_from_this(),
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
}

bool Transfer_connection::connect(const std::string& addr, const std::string& port)
{
    LINFO << "start connect " << addr << ":" << port;

    boost::system::error_code ec;

    tcp::resolver resolver(socket_.get_io_service());
    tcp::resolver::query query(addr, port);
    tcp::resolver::iterator iterator = resolver.resolve(query, ec);
    if (ec)
    {
        LERROR << ec.message();
        shutdown();
        return false;
    }

    boost::asio::connect(socket_, iterator, ec);
    if (ec)
    {
        LERROR << ec.message();
        shutdown();
        return false;
    }

    return true;
}

void Transfer_connection::start_send(const std::string& file_path, const FileInfo& info, const std::string& addr, const std::string& port)
{
    LINFO << "start send " << file_path << " to " << addr << ":" << port;

    file_info_ = info;

    if (!transfer_->is_server_ && !connect(addr, port))
    {
        shutdown();
        return;
    }

    send_file(file_path);
}

#ifdef WIN32
void Transfer_connection::send_file(const std::string& file_path)
{
    LINFO << "send file " << file_path;

    filename_ = file_path;

    boost::system::error_code ec;
    file_.assign(::CreateFile(filename_.c_str(), GENERIC_READ, 0, 0,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, 0), ec);
    if (file_.is_open())
    {
        std::string action = make_send_action(file_info_);
        send_action(action, ec);

        if (ec)
        {
            LERROR << ec.message();
            shutdown();
            return;
        }

        transmit_file(socket_, file_,
            boost::bind(&Transfer_connection::handle_file_sent,
                shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));

        return;
    }

    if (ec)
    {
        LERROR << "open " << filename_ << ": " << ec.message();
    }

    shutdown();
}

#endif

#ifdef __linux__
void Transfer_connection::send_file(const std::string& file_path)
{
    LINFO << "send file " << file_path;

    filename_ = file_path;

    int fd = ::open(filename_.c_str(), O_RDONLY);
    if (-1 == fd)
    {
        LERROR << "open file " << filename_ << " error(" << errno << ")";
        shutdown();
        return;
    }

    boost::system::error_code ec;
    std::string action = make_send_action(file_info_);
    send_action(action, ec);

    if (ec)
    {
        LERROR << ec.message();
        shutdown();
        ::close(fd);
        return;
    }

    //if (socket_.non_blocking())  // asio ÓÐbug 
    {
        socket_.non_blocking(false, ec);
        if (ec)
        {
            LERROR << "(" << ec.value() << ")" << ec.message();
            shutdown();
            return;
        }
    }

    int sock = socket_.native();
    off_t start = 0;
    while (start < file_size_)
    {
        size_t end = start + 1 * 1024 * 1024;
        if (start > file_size_)
            end = file_size_;

        ssize_t result = ::sendfile(sock, fd, &start, end - start );
        if (result == -1)
        {
            ec.assign(errno, boost::system::system_category());
            LERROR <<"(" << ec.value() << ")" << ec.message();
            break;
        }
        else if (result == 0)
        {
            ec.assign(errno, boost::system::system_category());
            LERROR << ec.message();
            break;
        }
        else {
            LINFO <<file_info_.key() << " send bytes: " << start << " rest: " << file_size_ - start << " percentage:" << double(start) /file_size_;
            if (start == file_size_)
                break;
        }
    }

    ::close(fd);
    shutdown();
}
#endif

Transfer_connection::Transfer_connection(Transfer* transfer, boost::asio::io_service& io_service, const std::string& recv_path)
    : transfer_(transfer),
    socket_(io_service),
    filename_(),
#if defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)
    file_(io_service),
#endif // DEBUG
    file_size_(0),
    recv_count_(0),
    recv_path_(recv_path)
{
}

std::string Transfer_connection::make_query_action() const
{
    std::stringstream os;
    os << "CMD=QUERY,";
    os << file_info_.to_string();
    os << ";";

    return os.str();
}

std::string Transfer_connection::make_reply_action(const std::string& info, int status) const
{
    std::stringstream os;
    os << "CMD=REPLY,";
    os << "INFO=" << info <<",";
    os << "STATUS=" << status << ",";
    os << ";";

    return os.str();
}

std::string Transfer_connection::make_send_action(const FileInfo& file_info)
{
#ifdef WIN32 
    DWORD size = ::GetFileSize(file_.native_handle(), NULL);
#endif

#ifdef __linux__
    struct stat statbuf;
    int result = stat(filename_.c_str(), &statbuf);
    assert(result != -1);
    int size = statbuf.st_size;
    file_size_ = size;
#endif

    std::stringstream os;
    os << "CMD=SEND,";
    os << file_info.to_string();
    os << "SIZE=" << size << ",";
    os << ";";

    return os.str();
}

void Transfer_connection::send_action(const std::string& info, boost::system::error_code& ec)
{
    LINFO << "send: " << info;

    boost::asio::write(socket_,
        boost::asio::buffer(info),
        boost::asio::transfer_exactly(info.size()),
        ec);
}

void Transfer_connection::handle_action(const boost::system::error_code& error,
    size_t bytes_transferred)
{
    if (error)
    {
        LERROR << error.message();
        shutdown();
        return;
    }

    boost::asio::streambuf::const_buffers_type data = buffer_.data();
    action_ = std::string(boost::asio::buffer_cast<const char*>(data), bytes_transferred);
    buffer_.consume(bytes_transferred);

    LINFO << "handle action: " << action_;

    parseOptions(action_);

    const std::string cmd = getOption("CMD");

    if (cmd == "SEND")
    {
        transfer_->add_task(shared_from_this(), Transfer::RECV);

        start_recv_file();
    }
    else if (cmd == "QUERY")
    {
        start_reply();
    }
    else if (cmd == "REPLY")
    {
        std::string result = getOption("RESULT");

        shutdown();
    }
}

void Transfer_connection::start_recv_file()
{
    boost::asio::streambuf::mutable_buffers_type mbuf = buffer_.prepare(4 * 1024);
    socket_.async_read_some(boost::asio::buffer(mbuf),
        boost::bind(&Transfer_connection::handle_file_data_recv,
            shared_from_this(),
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred)
    );
}

void Transfer_connection::handle_file_data_recv(const boost::system::error_code& error,
    size_t bytes_transferred)
{
    if (!error)
    {
        if (bytes_transferred == 0)
        {
            LINFO << "recv completed, " << file_info_.key();

            shutdown();
            return;
        }

        if (!out_file_.is_open())
        {
            std::string type = getOption("TYPE");

            if (!make_directory(type))
                return;

            std::string file = recv_path_ + type + "/" + file_info_.key();
            out_file_.open(file.c_str(), std::ios::binary);
            if (!out_file_)
            {
                LERROR << "open file fail: " << file;
                shutdown();
                return;
            }

            LINFO << "recv " << file;
        }

        boost::asio::streambuf::const_buffers_type data = buffer_.data();
        out_file_.write(boost::asio::buffer_cast<const char*>(data), bytes_transferred);

        buffer_.consume(bytes_transferred);

        size_t M = recv_count_ / (1 * 1024 * 1024);
        recv_count_ += bytes_transferred;

        if ((recv_count_ / (1 * 1024 * 1024)) > M)
        {
            LINFO << file_info_.key() << " recieve bytes: " << recv_count_ << " rest: " << file_size_ - recv_count_ << " percentage:" << double(recv_count_) / file_size_;
        }

        start_recv_file();
    }
    else
    {
        if (recv_count_ == file_size_)
        {
            LINFO << "recv completed " << file_info_.key();
        }
        else {
            LERROR << "recv file data: " << error.message();
        }
        shutdown();
    }
}

bool Transfer_connection::make_directory(const std::string type)
{
    boost::filesystem::path path(recv_path_);
    if (!boost::filesystem::exists(path))
    {
        boost::system::error_code ec;
        boost::filesystem::create_directory(path, ec);
        if (ec)
        {
            LERROR << ec.message() << " " << path.generic_string();
            shutdown();
            return false;
        }
    }

    path /= type;
    if (!boost::filesystem::exists(path))
    {
        boost::system::error_code ec;
        boost::filesystem::create_directory(path, ec);
        if (ec)
        {
            LERROR << ec.message() << " " << path.generic_string();
            shutdown();
            return false;
        }
    }

    return true;
}

std::string Transfer_connection::to_file_path(const FileInfo& info) const
{
    return recv_path_ + info.type + "/" + file_info_.key();
}

void Transfer_connection::handle_file_sent(const boost::system::error_code& error,
    size_t bytes_transferred)
{
    if (error)
    {
        LERROR << "send " << filename_ << ": " << error.message();
    }
    LINFO << filename_ << " send completed";

    shutdown();
}

void Transfer_connection::shutdown()
{
    boost::system::error_code ignored_ec;
    socket_.shutdown(tcp::socket::shutdown_both, ignored_ec);

    transfer_->remove_task(shared_from_this());

    LINFO << "connection shutdown: " << file_info_.key();
}

void Transfer_connection::parseOptions(const std::string& str)
{
    optios_.clear();

    std::vector<std::string> option;
    boost::split(option, str, boost::is_any_of(","));

    std::vector<std::string>::const_iterator it = option.begin();
    while (it != option.end())
    {
        std::vector<std::string> kv;
        boost::split(kv, *it, boost::is_any_of("="));

        if (kv.size() == 2)
        {
            boost::trim(kv[0]);
            boost::trim(kv[1]);

            optios_[kv[0]] = kv[1];
        }
        else {
            if (*it == ";")
                break;

            LERROR << "invalid option: " << *it;
        }

        ++it;
    }

    file_info_.id = getOption("ID");
    file_info_.type = getOption("TYPE");
    file_info_.date = getOption("DATE");
    file_info_.tm_begin = getOption("BEGIN");
    file_info_.tm_end = getOption("END");

    std::string size = getOption("SIZE").c_str();
    if (!size.empty())
    {
        file_size_ = std::atoi(size.c_str());
    }
    else {
        file_size_ = 0;
    }
}

std::string Transfer_connection::getOption(const std::string& key) const
{
    std::map <std::string, std::string>::const_iterator it = optios_.find(key);
    if (optios_.end() != it)
    {
        return it->second;
    }

    return "";
}

void Transfer_connection::start_reply()
{
    Transfer::Status status = transfer_->get_task_status(file_info_);
    std::string info = "not ready";
    if (status == Transfer::UNKNOWN)
    {
        std::string file = to_file_path(file_info_);
        boost::filesystem::path path(file);
        if (boost::filesystem::exists(path))
        {
            LINFO << "reply file " << file_info_.key();

            send_file(file);
            return;
        }

        info = "file not exists";
    }
        
    std::string action = make_reply_action(info, status);
    boost::system::error_code ignore;
    send_action(action, ignore);

    shutdown();
    return;
}
