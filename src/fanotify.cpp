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

#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>

#include <sys/fanotify.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

#include "fanotify.hpp"
#include "log.hpp"
#include "fanotifyerror.hpp"

namespace io = boost::asio;
namespace fs = std::filesystem;

FanotifyGroup::FanotifyGroup(io::io_context& context, unsigned int event_flags)
    : m_context(context)
    , m_notify_fd(context)
{
    int fd = fanotify_init(event_flags, O_RDONLY | O_LARGEFILE);
    if (fd == -1)
    {
        throw FanotifyGroupError("fanotify_init", errno);
    }

    m_notify_fd.assign(fd);
}

void
FanotifyGroup::addMark(const std::string& dir, int mask)
{
    if (fanotify_mark(m_notify_fd.native_handle(), FAN_MARK_ADD | FAN_MARK_ONLYDIR,
                      mask,
                      AT_FDCWD,
                      dir.c_str()) == -1)
    {
        throw FanotifyGroupError("fanotify_mark", errno);
    }
}


void FanotifyGroup::removeMark(const std::string& dir, int mask)
{
    if (fanotify_mark(m_notify_fd.native_handle(), FAN_MARK_REMOVE | FAN_MARK_MOUNT,
                      mask,
                      AT_FDCWD,
                      dir.c_str()) == -1)
    {
        throw FanotifyGroupError("fanotify_mark delete", errno);
    }

}
void FanotifyGroup::flushMark(const std::string& dir, int mask)
{
    if (fanotify_mark(m_notify_fd.native_handle(), FAN_MARK_FLUSH | FAN_MARK_MOUNT,
                      mask,
                      AT_FDCWD,
                      dir.c_str()) == -1)
    {
        throw FanotifyGroupError("fanotify_mark flush", errno);
    }
}

