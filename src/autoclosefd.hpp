#ifndef _FAD_AUTO_CLOSE_FD_HPP
#define _FAD_AUTO_CLOSE_FD_HPP

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

#include <unistd.h>

struct AutoCloseFd
{
    AutoCloseFd(int fd)
        : m_fd(fd)
    {    }

    AutoCloseFd(AutoCloseFd&& m)
    {
        m_fd = std::move(m.m_fd);
        m.m_fd = -1; // only one could own fd
    }

    AutoCloseFd(const AutoCloseFd& )           = delete;
    AutoCloseFd& operator=(const AutoCloseFd&) = delete;
    
    ~AutoCloseFd()
    {
        if (m_fd != -1)
            close(m_fd);
    }

    const int getFd() const noexcept
    {
        return m_fd;
    }
    
private:
    int m_fd;
};

#endif //_FAD_AUTO_CLOSE_FD_HPP
