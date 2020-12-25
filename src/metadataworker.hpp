#ifndef _FAD_META_DATA_WORKER_HPP
#define _FAD_META_DATA_WORKER_HPP

#include <sys/fanotify.h>

#include <tuple>
#include <string>
#include <filesystem>

#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/spawn.hpp>

class MetadataWorker
{
public:
    typedef std::tuple<std::pair<std::string, std::string>,
                       std::pair<std::string, std::string>,
                       std::pair<std::string, int>,
                       std::string,
                       std::pair<std::string, std::string>>
    DATA_RESULT;

    
public:
    MetadataWorker(boost::asio::posix::stream_descriptor& notify_fd)
        : m_notify_fd(notify_fd)
    {}

    // uid real, uid effective, pid, access type, comment
    DATA_RESULT
    operator()(const fanotify_event_metadata *metadata, ssize_t len, boost::asio::yield_context& yield);

private:
    std::pair<std::string, std::string> printAccessType(const fanotify_event_metadata *metadata);
    
    std::filesystem::path getRealFilePath(int fd);
    
    std::pair<std::string, std::string> parseProc(int pid, boost::asio::yield_context& yield);
   

private:
    boost::asio::posix::stream_descriptor& m_notify_fd;
};

#endif //_FAD_META_DATA_WORKER_HPP
