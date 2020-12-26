#ifndef _FAD_FANOTIFY_GROUP_HPP
#define _FAD_FANOTIFY_GROUP_HPP

#include <string.h>
#include <array>

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

#include "log.hpp"
#include "fanotifyerror.hpp"
#include "report.hpp"

/// clas to work with fanotifi Linux interface, man 7 fanotify
class FanotifyGroup
{
public:
    /// asio's io_context is used as engine, event_flags flags passed to fanotify
    FanotifyGroup(boost::asio::io_context& context, unsigned int event_flags);

    FanotifyGroup(FanotifyGroup&&) = default;
    ~FanotifyGroup() = default;

    /// copying is not supported
    FanotifyGroup(const FanotifyGroup&) = delete;
    FanotifyGroup& operator=(const FanotifyGroup&) = delete;

    /// add watched dir, set the mark
    void addMark(const std::string& dir, int mask);
    /// will be removed from the mark mask
    void removeMark(const std::string& dir, int mask);
    /// Remove  either  all  marks for filesystems, all marks for mounts,
    /// or all marks for directories and files from the fanotify group
    void flushMark(const std::string& dir, int mask);

    /// main engine, runs report on recevied event
    template<class Worker>
    void asyncEvent(FadReport& report, boost::asio::yield_context& yield);
    
private:
    boost::asio::io_context& m_context;
    boost::asio::posix::stream_descriptor m_notify_fd;
};

template<class Worker>
void FanotifyGroup::asyncEvent(FadReport& report, boost::asio::yield_context& yield)
{
    const struct fanotify_event_metadata *metadata;
    struct fanotify_event_metadata buf[200];
    ssize_t len;

    Worker worker(m_notify_fd);

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

        // Loop over all events in the buffer
        while (FAN_EVENT_OK(metadata, len))
        {
            try
            {
                auto to_report = worker(metadata, len, yield);

                boost::asio::spawn(m_context, [&](boost::asio::yield_context yield)
                                              {
                                                  report.makeReport(yield, to_report);
                                              });
            }
            catch(FanotifyNoDataError& e)
            {
                //skip it, watch the next item
            }
            metadata = FAN_EVENT_NEXT(metadata, len);
        }

    }
};


#endif //_FAD_FANOTIFY_GROUP_HPP
