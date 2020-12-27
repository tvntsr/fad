// Copyright 2020 Volodymyr Tarasenko
// Author : Volodymyr Tarasenko
//
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.

//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.

//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
