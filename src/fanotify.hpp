#ifndef _FAD_FANOTIFY_GROUP_HPP
#define _FAD_FANOTIFY_GROUP_HPP

#include <string.h>
#include <array>

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>


class FanotifyGroupError : public std::exception
{
    std::string m_message;
public:
    FanotifyGroupError(const std::string& message, int err)
        : m_message(message)
    {
        makeErrorMessage(err);
    }

    FanotifyGroupError(std::string&& message, int err)
        : m_message(message)
    {
        makeErrorMessage(err);
    }
    
    ~FanotifyGroupError() noexcept
    {}

    const char* what() const noexcept
    {
        return m_message.c_str();
    }
private:
    void makeErrorMessage(int err)
    {
        m_message += ", error ";
        m_message += std::to_string(err);
        // m_message += ",";
        
        // std::array<char, 1024> buff;
        // int r = strerror_r(err, buff.data(), 1024);
        // printf("Err %d, errno %d \n", r, errno);
        
        // if (r>0)
        // {
        //     std::array<char, 4*1024> buff;
        //     int r = strerror_r(err, buff.data(), 1024);
        //     m_message += buff.data();
        // }
        // else
        //     m_message += buff.data();
    }
};

class FanotifyGroup
{
public:
    FanotifyGroup(boost::asio::io_context& context, unsigned int event_flags);

    //~FanotifyGroup();
    
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
