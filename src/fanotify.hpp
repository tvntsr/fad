#ifndef _FAD_FANOTIFY_GROUP_HPP
#define _FAD_FANOTIFY_GROUP_HPP

#include <string.h>
#include <array>

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

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
    
    void asyncEvent(boost::asio::yield_context& yield);
    
private:
    boost::asio::io_context& m_context;
    boost::asio::posix::stream_descriptor m_notify_fd;
};


#endif //_FAD_FANOTIFY_GROUP_HPP
