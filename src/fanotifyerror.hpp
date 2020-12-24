#ifndef _FAD_FANOTIFY_GROUP_ERROR_HPP
#define _FAD_FANOTIFY_GROUP_ERROR_HPP

#include <string>
#include <exception>

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

#endif //_FAD_FANOTIFY_GROUP_ERROR_HPP
