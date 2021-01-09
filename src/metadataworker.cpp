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

#include <fstream>
#include <vector>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

#include "metadataworker.hpp"
#include "log.hpp"

namespace io = boost::asio;
namespace fs = std::filesystem;

std::array<FanMaskMapping, 17>
mask_mapping = { {
    { FAN_ACCESS,         true,  "ACCESS",         "A file or a directory (but see BUGS) was accessed (read)."},
    { FAN_OPEN,           true,  "OPEN",           "A file or a directory was opened."},
    { FAN_OPEN_EXEC,      true,  "OPEN_EXEC",      "A file was opened with the intent to be executed."},
    { FAN_ATTRIB,         true,  "ATTRIB",         "A file or directory metadata was changed."},
    { FAN_CREATE,         true,  "CREATE",         "A child file or directory was created in a watched parent."},
    { FAN_DELETE,         true,  "DELETE",         "A child file or directory was deleted in a watched parent."},
    { FAN_DELETE_SELF,    true,  "DELETE_SELF",    "A watched file or directory was deleted."},
    { FAN_MOVED_FROM,     true,  "MOVED_FROM",     "A file or directory has been moved from a watched parent directory."},
    { FAN_MOVED_TO,       true,  "FAN_MOVED_TO",   "A file or directory has been moved to a watched parent directory."},
    { FAN_MOVE_SELF,      true,  "MOVE_SELF",      "A watched file or directory was moved."},
    { FAN_MODIFY,         true,  "MODIFY",         "A file was modified."},
    { FAN_CLOSE_WRITE,    true,  "CLOSE_WRITE",    "A file that was opened for writing (O_WRONLY or O_RDWR) was closed."},
    { FAN_CLOSE_NOWRITE,  true,  "CLOSE_NOWRITE",  "A file or directory that was opened read-only (O_RDONLY) was closed."},
    { FAN_Q_OVERFLOW,     false, "Q_OVERFLOW",     "The event queue exceeded the limit of entries."},
    { FAN_ACCESS_PERM,    false, "ACCESS_PERM",    "An application wants to read a file or directory, for example using read(2) or readdir(2)."},
    { FAN_OPEN_PERM,      false, "OPEN_PERM",      "An application wants to open a file or directory."},
    { FAN_OPEN_EXEC_PERM, false, "OPEN_EXEC_PERM", "An  application  wants  to  open a file for execution."}
    }
};

std::pair<std::string, std::string>
MetadataWorker::printAccessType(const fanotify_event_metadata *metadata)
{
    std::string access, comment;

    for (auto r: mask_mapping)
        if (metadata->mask & r.mask)
        {
            if (!access.empty()) access += "|";
            if (!comment.empty()) comment += "; ";
            
            access  += r.access;
            comment += r.comment;
            
            LogDebug(r.comment);
        }

    return std::make_pair(access, comment);
}
    
fs::path MetadataWorker::getRealFilePath(int fd)
{
    LogDebug("Reading file path, fd " << fd);
    fs::path p = "/proc/self/fd/";
    p /= std::to_string(fd);
    if (!fs::exists(p))
    {
        throw FanotifyNoDataError();
         //throw std::runtime_error("link does not exists");
    }

    return fs::read_symlink(p);
}
    
std::tuple<std::string, std::string, std::string> MetadataWorker::parseProc(int pid, io::yield_context& yield)
{
    LogDebug("Reading proc, pid " << pid);
    
    fs::path p = "/proc";
    p /= std::to_string(pid);
    p /= "status";

    LogDebug("Checking file " << p.string());
    if (!fs::exists(p))
    {
        throw FanotifyNoDataError();
        //throw std::runtime_error("File does not exists");
    }

    LogDebug("Go " << p);
    std::string line;
    std::string binary_name;
    boost::asio::streambuf input_buffer;

    boost::asio::posix::stream_descriptor file_reader(m_notify_fd.get_executor(),
                                                      open(p.string().c_str(), O_RDONLY));

    LogDebug("Reading " << p.string());
    size_t len;
    do
    {
        len = boost::asio::async_read_until(file_reader, input_buffer, '\n', yield);
        if (len > 0)
        {
            LogDebug("Read "<< len << " bytes");
            std::istream is(&input_buffer);
            for (std::string line; std::getline(is, line); )
            {
                LogDebug("Line ["<< line << "], len " << len);
                if (binary_name.empty() && boost::starts_with(line, "Name"))
                {
                    std::vector<std::string> parsed_record;
                    boost::split(parsed_record, line, boost::is_any_of(" \t"));

                    binary_name = parsed_record[1];
                }
                else if (boost::starts_with(line, "Uid"))
                {
                    std::vector<std::string> parsed_record;
                    boost::split(parsed_record, line, boost::is_any_of(" \t"));
                    return std::make_tuple(parsed_record[1], parsed_record[2], binary_name);
                }
            }
        }
    }
    while(len > 0);
    
    return std::make_tuple("-", "-", "-");
}

MetadataWorker::DATA_RESULT
MetadataWorker::metadataParser(const fanotify_event_metadata *metadata, ssize_t len, io::yield_context& yield)
{
    LogDebug("Start parsing metadata");

    AutoCloseFd auto_fd(metadata->fd);
    
    // Check that run-time and compile-time structures match
    if (metadata->vers != FANOTIFY_METADATA_VERSION)
    {
        throw FanotifyGroupError("mismatch of fanotify metadata version", errno);
    }

    /* man page:
       metadata->fd contains either FAN_NOFD, indicating a
       queue overflow, or a file descriptor (a nonnegative
       integer). Here, we simply ignore queue overflow. */
    
    if (metadata->fd < 0)
    {
        throw FanotifyNoDataError();
    }

    /* Handle open permission event */
    if (metadata->mask & FAN_OPEN_PERM)
    {
        LogDebug("FAN_OPEN_PERM ");
            
        struct fanotify_response response;
        // Allow file to be opened 
        response.fd = metadata->fd;
        response.response = FAN_ALLOW;
        (void)!write(m_notify_fd.native_handle(), &response,
                     sizeof(struct fanotify_response));
            
        // io::async_write(m_notify_fd,
        //             io::buffer(&response, sizeof(struct fanotify_response)),
        //             yield);
        //write(fd, &response,
        //      sizeof(struct fanotify_response));
    }

    return prepareDataResult(metadata->pid, std::move(auto_fd), printAccessType(metadata), yield);
}

MetadataWorker::DATA_RESULT
MetadataWorker::fidMetadataParser(const fanotify_event_metadata *metadata, ssize_t len, io::yield_context& yield)
{
    LogDebug("Start parsing metadata and fid structure");
   
    // Check that run-time and compile-time structures match
    if (metadata->vers != FANOTIFY_METADATA_VERSION)
    {
        throw FanotifyGroupError("mismatch of fanotify metadata version", errno);
    }

    // metadata->fd shoul contain FAN_NOFD in case of FID
    if (metadata->fd >= 0)
    {
        throw FanotifyNoDataError();
    }

    // C structure, C style 
    struct fanotify_event_info_fid* fid = (struct fanotify_event_info_fid *) (metadata + 1);
    struct file_handle* file_handle = (struct file_handle *) fid->handle;

    // Ensure that the event info is of the correct type
    if (fid->hdr.info_type != FAN_EVENT_INFO_TYPE_FID)
    {
        throw FanotifyGroupError("unexpected event info type", errno);
    }

    auto type_and_comment = printAccessType(metadata);
    
    /* according to man page:
       metadata->fd is set to FAN_NOFD when FAN_REPORT_FID is enabled.
       To obtain a file descriptor for the file object corresponding to
       an event you can use the struct file_handle that's provided
       within the fanotify_event_info_fid in conjunction with the
       open_by_handle_at(2) system call. A check for ESTALE is done
       to accommodate for the situation where the file handle for the
       object was deleted prior to this system call. */

    LogDebug("fsid.val[0]: " << fid->fsid.val[0] << ", fsid.val[1]: " << fid->fsid.val[1]);
    
    int event_fd = open_by_handle_at(AT_FDCWD, file_handle, O_RDONLY);
    if (event_fd == -1) {
        if (errno == ESTALE) {
            LogWarning("File pointed by file handle has been deleted");
            throw FanotifyNoDataError();
        } else {
            throw FanotifyGroupError("Error with open_by_handle_at", errno);
        }
    }
   
    return prepareDataResult(metadata->pid, AutoCloseFd(event_fd), type_and_comment, yield);
}

MetadataWorker::DATA_RESULT
MetadataWorker::prepareDataResult(int pid,
                                  AutoCloseFd&& auto_fd,
                                  const std::pair<std::string, std::string>& type_and_comment,
                                  io::yield_context& yield)
{
    AutoCloseFd fd(std::move(auto_fd));

    std::string uid_real_s, uid_effective_s, binary_name;

    try
    {
        std::tie(uid_real_s, uid_effective_s, binary_name) = parseProc(pid, yield);
        LogDebug("PID " << pid << "("<< binary_name << "), uids:" << uid_real_s << "/" << uid_effective_s);
    }
    catch(FanotifyNoDataError& err)
    {
        LogWarning("Cannot get uid, proc file already removed");
    }
    catch(FanotifyGroupError& err)
    {
        LogWarning("Cannot get uid, " << err.what());
    }
    
    auto path = getRealFilePath(fd.getFd());
    LogDebug("Path: " << path);

    return std::make_tuple(std::make_pair("UID real", uid_real_s)
                           , std::make_pair("UID effective", uid_effective_s)
                           , std::make_pair("PID", pid)
                           , binary_name
                           , path.string()
                           , type_and_comment
        );
}
