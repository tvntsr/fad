#ifndef _FAD_FANOTIFY_GROUP_HPP
#define _FAD_FANOTIFY_GROUP_HPP

#include <string.h>
#include <array>

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

#include "log.hpp"
#include "fanotifyerror.hpp"

class FanotifyGroup
{
public:
    FanotifyGroup(boost::asio::io_context& context, unsigned int event_flags);

    FanotifyGroup(FanotifyGroup&&) = default;
    ~FanotifyGroup() = default;
    
    FanotifyGroup(const FanotifyGroup&) = delete;
    FanotifyGroup& operator=(const FanotifyGroup&) = delete;
    
    void addMark(const std::string& dir, int mask);
    void removeMark(const std::string& dir, int mask);
    void flushMark(const std::string& dir, int mask);

    template<class Worker>
    void asyncEvent(boost::asio::yield_context& yield);
    
private:
    boost::asio::io_context& m_context;
    boost::asio::posix::stream_descriptor m_notify_fd;
};

template<class Worker>
void FanotifyGroup::asyncEvent(boost::asio::yield_context& yield)
{
    const struct fanotify_event_metadata *metadata;
    struct fanotify_event_metadata buf[200];
    ssize_t len;

    Worker worker(m_notify_fd);

    fprintf(stderr, "FAN_START\n");
    // do some fun
    for(;;)
    {
        // Read some events
        LogDebug("FAN_LOOP, fd " << m_notify_fd.native_handle());

        boost::system::error_code ec;
        len = m_notify_fd.async_read_some(boost::asio::buffer(buf), yield[ec]);
        
        if (len == -1 && errno != EAGAIN)
        {
            throw FanotifyGroupError("read error", errno);
        }
        // Check if end of available data reached
        if (len <= 0)
            return;//break;

        LogDebug("FAN_LOOP, got event");
        
        // Point to the first event in the buffer
        metadata = buf;
        
        worker(metadata, len, yield);
    }
};


#endif //_FAD_FANOTIFY_GROUP_HPP
