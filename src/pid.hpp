#ifndef _FAD_PID_HPP
#define _FAD_PID_HPP

#include <string>

class Pid
{
public:
    Pid(const std::string& path);
    
    ~Pid();

    const std::string& filename() const noexcept
    {
        return m_path;
    }
    
private:
    std::string m_path;
    int m_pid_file;
};

#endif //_FAD_PID_HPP
