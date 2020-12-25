//#include <limits.h>
//#include <unistd.h>
//#include <stdlib.h>
//#include <errno.h>
//#include <fcntl.h>
//#include <stdio.h>
//#include <poll.h>



//#include <filesystem>
#include <fstream>
//#include <string>
#include <vector>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

#include "log.hpp"
#include "metadataworker.hpp"
#include "fanotifyerror.hpp"

namespace io = boost::asio;
namespace fs = std::filesystem;

struct AutoCloseFd
{
    AutoCloseFd(int fd)
        : m_fd(fd)
    {}

    ~AutoCloseFd()
    {
        close(m_fd);
    }
    
private:
    int m_fd;
};

struct FanMaskMapping
{
    int mask;
    std::string access;
    std::string comment;
} mask_mapping[] = {
    { FAN_ACCESS,         "ACCESS",         "A file or a directory (but see BUGS) was accessed (read)."},
    { FAN_OPEN,           "OPEN",           "A file or a directory was opened."},
    { FAN_OPEN_EXEC,      "OPEN_EXEC",      "A file was opened with the intent to be executed."},
    { FAN_ATTRIB,         "ATTRIB",         "A file or directory metadata was changed."},
    { FAN_CREATE,         "CREATE",         "A child file or directory was created in a watched parent."},
    { FAN_DELETE,         "DELETE",         "A child file or directory was deleted in a watched parent."},
    { FAN_DELETE_SELF,    "DELETE_SELF",    "A watched file or directory was deleted."},
    { FAN_MOVED_FROM,     "MOVED_FROM",     "A file or directory has been moved from a watched parent directory."},
    { FAN_MOVED_TO,       "FAN_MOVED_TO",   "A file or directory has been moved to a watched parent directory."},
    { FAN_MOVE_SELF,      "MOVE_SELF",      "A watched file or directory was moved."},
    { FAN_MODIFY,         "MODIFY",         "A file was modified."},
    { FAN_CLOSE_WRITE,    "CLOSE_WRITE",    "A file that was opened for writing (O_WRONLY or O_RDWR) was closed."},
    { FAN_CLOSE_NOWRITE,  "CLOSE_NOWRITE",  "A file or directory that was opened read-only (O_RDONLY) was closed."},
    { FAN_Q_OVERFLOW,     "Q_OVERFLOW",     "The event queue exceeded the limit of entries."},
    { FAN_ACCESS_PERM,    "ACCESS_PERM",    "An application wants to read a file or directory, for example using read(2) or readdir(2)."},
    { FAN_OPEN_PERM,      "OPEN_PERM",      "An application wants to open a file or directory."},
    { FAN_OPEN_EXEC_PERM, "OPEN_EXEC_PERM", "An  application  wants  to  open a file for execution."}
};

MetadataWorker::DATA_RESULT
MetadataWorker::operator()(const fanotify_event_metadata *metadata, ssize_t len, io::yield_context& yield)
{
    std::string uid_real_s
        , uid_effective_s;
    
    LogDebug("Start operator()");

    AutoCloseFd auto_fd(metadata->fd);
    
    // Check that run-time and compile-time structures match
    if (metadata->vers != FANOTIFY_METADATA_VERSION)
    {
        throw FanotifyGroupError("mismatch of fanotify metadata version", errno);
    }

    /* metadata->fd contains either FAN_NOFD, indicating a
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
        write(m_notify_fd.native_handle(), &response,
              sizeof(struct fanotify_response));
            
        // io::async_write(m_notify_fd,
        //             io::buffer(&response, sizeof(struct fanotify_response)),
        //             yield);
        //write(fd, &response,
        //      sizeof(struct fanotify_response));
    }
    std::tie(uid_real_s, uid_effective_s) = parseProc(metadata->pid, yield);
    LogDebug("PID " << metadata->pid << ", uids:" << uid_real_s << "/" << uid_effective_s);
    
    auto path = getRealFilePath(metadata->fd);
    LogDebug("Path: " << path);
    auto type_and_comment = printAccessType(metadata);
    // Handle closing of writable file event

    if (metadata->mask & FAN_CLOSE_WRITE)
        LogDebug("FAN_CLOSE_WRITE ");

    // Close the file descriptor of the event
    //close(metadata->fd);

    return std::make_tuple(std::make_pair("UID real", uid_real_s)
                           , std::make_pair("UID effective", uid_effective_s)
                           , std::make_pair("PID", metadata->pid)
                           , path.string()
                           , type_and_comment
        );
}

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
    
std::pair<std::string, std::string> MetadataWorker::parseProc(int pid, io::yield_context& yield)
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
                if (boost::starts_with(line, "Uid"))
                {
                    std::vector<std::string> parsed_record;
                    boost::split(parsed_record, line, boost::is_any_of(" \t"));
                    return std::make_pair(parsed_record[1], parsed_record[2]);
                }
            }
        }
    }
    while(len > 0);
    
    return std::make_pair("-", "-");
}
