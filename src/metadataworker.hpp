#ifndef _FAD_META_DATA_WORKER_HPP
#define _FAD_META_DATA_WORKER_HPP

#include <sys/fanotify.h>

#include <string>
#include <filesystem>

#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/spawn.hpp>

class MetadataWorker
{
public:
    MetadataWorker(boost::asio::posix::stream_descriptor& notify_fd)
        : m_notify_fd(notify_fd)
    {}

    void operator()(const fanotify_event_metadata *metadata, ssize_t len, boost::asio::yield_context& yield);

private:
    void printAccessType(const fanotify_event_metadata *metadata);
    
    std::filesystem::path getRealFilePath(const fanotify_event_metadata *metadata);
    
    std::pair<std::string, std::string> parseProc(int pid, boost::asio::yield_context& yield);
   

private:
    boost::asio::posix::stream_descriptor& m_notify_fd;
};

#endif //_FAD_META_DATA_WORKER_HPP
