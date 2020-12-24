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

void MetadataWorker::operator()(const fanotify_event_metadata *metadata, ssize_t len, io::yield_context& yield)
{
    LogDebug("Start operator()");
    // Loop over all events in the buffer
    while (FAN_EVENT_OK(metadata, len))
    {
        // Check that run-time and compile-time structures match
        if (metadata->vers != FANOTIFY_METADATA_VERSION)
        {
            throw FanotifyGroupError("mismatch of fanotify metadata version", errno);
        }

        /* metadata->fd contains either FAN_NOFD, indicating a
           queue overflow, or a file descriptor (a nonnegative
           integer). Here, we simply ignore queue overflow. */

        if (metadata->fd >= 0)
        {

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

            auto path = getRealFilePath(metadata);
            auto uids = parseProc(metadata->pid, yield);
            LogDebug("PID " << metadata->pid << ", " << uids.first << "/" << uids.second);
            printAccessType(metadata);
            // Handle closing of writable file event

            if (metadata->mask & FAN_CLOSE_WRITE)
                LogDebug("FAN_CLOSE_WRITE ");

            LogDebug("File " <<  path.string());

            // Close the file descriptor of the event
            close(metadata->fd);
        }

        // Advance to next event
        metadata = FAN_EVENT_NEXT(metadata, len);
    }

}

void MetadataWorker::printAccessType(const fanotify_event_metadata *metadata)
{
    if (metadata->mask & FAN_ACCESS)
        LogDebug("A file or a directory (but see BUGS) was accessed (read).");
    if (metadata->mask & FAN_OPEN)
        LogDebug("A file or a directory was opened.");
    if (metadata->mask & FAN_OPEN_EXEC)
        LogDebug("A file was opened with the intent to be executed.");
    if (metadata->mask & FAN_ATTRIB)
        LogDebug("A file or directory metadata was changed.");
    if (metadata->mask & FAN_CREATE)
        LogDebug("A child file or directory was created in a watched parent.");
    if (metadata->mask & FAN_DELETE)
        LogDebug("A child file or directory was deleted in a watched parent.");
    if (metadata->mask & FAN_DELETE_SELF)
        LogDebug("A watched file or directory was deleted.");
    if (metadata->mask & FAN_MOVED_FROM)
        LogDebug("A file or directory has been moved from a watched parent directory.");
    if (metadata->mask & FAN_MOVED_TO)
        LogDebug("A file or directory has been moved to a watched parent directory.");
    if (metadata->mask & FAN_MOVE_SELF)
        LogDebug("A watched file or directory was moved.");
    if (metadata->mask & FAN_MODIFY)
        LogDebug("A file was modified.");
    if (metadata->mask & FAN_CLOSE_WRITE)
        LogDebug("A file that was opened for writing (O_WRONLY or O_RDWR) was closed.");
    if (metadata->mask & FAN_CLOSE_NOWRITE)
        LogDebug("A file or directory that was opened read-only (O_RDONLY) was closed.");
    if (metadata->mask & FAN_Q_OVERFLOW)
        LogDebug("The event queue exceeded the limit of entries.");
    if (metadata->mask & FAN_ACCESS_PERM)
        LogDebug("An application wants to read a file or directory, for example using read(2) or readdir(2).");
    if (metadata->mask & FAN_OPEN_PERM)
        LogDebug("An application wants to open a file or directory.");
    if (metadata->mask & FAN_OPEN_EXEC_PERM)
        LogDebug("An  application  wants  to  open a file for execution.");
}
    
fs::path MetadataWorker::getRealFilePath(const fanotify_event_metadata *metadata)
{
    fs::path p = "/proc/self/fd/";
    p /= std::to_string(metadata->fd);
    if (!fs::exists(p))
    {
        throw std::runtime_error("link does not exists");
    }

    return fs::read_symlink(p);
}
    
std::pair<std::string, std::string> MetadataWorker::parseProc(int pid, io::yield_context& yield)
{
    fs::path p = "/proc";
    p /= std::to_string(pid);
    p /= "status";
        
    if (!fs::exists(p))
    {
        throw std::runtime_error("File does not exists");
    }

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
