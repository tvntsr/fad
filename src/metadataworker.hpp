#ifndef _FAD_META_DATA_WORKER_HPP
#define _FAD_META_DATA_WORKER_HPP

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

#include <sys/fanotify.h>
#include <sys/types.h>
#include <unistd.h>

#include <tuple>
#include <string>
#include <filesystem>

#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/spawn.hpp>

#include "fanotifyerror.hpp"
#include "autoclosefd.hpp"

struct FanMaskMapping
{
    int mask;
    bool input_allowed;
    std::string access;
    std::string comment;
};

extern std::array<FanMaskMapping, 17> mask_mapping;

class MetadataWorker
{
public:
    typedef std::tuple<std::pair<std::string, std::string>,
                       std::pair<std::string, std::string>,
                       std::pair<std::string, int>,
                       std::string,
                       std::string,
                       std::pair<std::string, std::string>>
    DATA_RESULT;

    
public:
    MetadataWorker(boost::asio::posix::stream_descriptor& notify_fd)
        : m_notify_fd(notify_fd)
        , m_pid(getpid())
    {

    }

    // uid real, uid effective, pid, access type, comment
    DATA_RESULT
    operator()(const fanotify_event_metadata *metadata, ssize_t len, boost::asio::yield_context& yield)
    {
        if (metadata->pid == m_pid)
            throw FanotifyNoDataError(); // skip myself

        return (metadata->event_len == FAN_EVENT_METADATA_LEN) ?
            metadataParser(metadata, len, yield) :
            fidMetadataParser(metadata, len, yield);
    }

private:
    std::pair<std::string, std::string> printAccessType(const fanotify_event_metadata *metadata);
    
    std::filesystem::path getRealFilePath(int fd);
    
    std::tuple<std::string, std::string, std::string>
    parseProc(int pid, boost::asio::yield_context& yield);

    DATA_RESULT
    metadataParser(const fanotify_event_metadata *metadata, ssize_t len, boost::asio::yield_context& yield);

    DATA_RESULT
    fidMetadataParser(const fanotify_event_metadata *metadata, ssize_t len, boost::asio::yield_context& yield);

    DATA_RESULT
    prepareDataResult(int pid,
                      AutoCloseFd&& auto_fd,
                      const std::pair<std::string, std::string>& type_and_comment,
                      boost::asio::yield_context& yield);

private:
    boost::asio::posix::stream_descriptor& m_notify_fd;
    int m_pid; // fanotify pid is int, so make it int as well
};

#endif //_FAD_META_DATA_WORKER_HPP
