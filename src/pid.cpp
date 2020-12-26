#include <sys/file.h>
#include <unistd.h>

#include <stdexcept>
#include <sstream>

#include "pid.hpp"

Pid::Pid(const std::string& path)
    : m_path(path)
    , m_pid_file(-1)
{
    m_pid_file = open(m_path.c_str(), O_CREAT | O_RDWR, 0644);
    int rc = flock(m_pid_file, LOCK_EX | LOCK_NB);
    if (rc)
    {
        if (EWOULDBLOCK == errno)
            throw std::runtime_error("pid file already exists");
    }

    pid_t my = getpid();
    std::ostringstream ost;
    ost << my;

    int r = write(m_pid_file, ost.str().c_str(), ost.str().size());
    r = r;
}

Pid::~Pid()
{
    flock(m_pid_file, LOCK_UN);
    close(m_pid_file);
    unlink(m_path.c_str());
}
