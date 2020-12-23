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

namespace io = boost::asio;
namespace fs = std::filesystem;

struct MetadataWorker
{
    MetadataWorker(boost::asio::posix::stream_descriptor& notify_fd)
        : m_notify_fd(notify_fd)
    {}
    
    std::pair<int, int> parseProc(int pid, io::yield_context& yield)
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

        fprintf(stderr, "Reading %s\n", p.string().c_str());
        size_t len;
        do
        {
            len = boost::asio::async_read_until(file_reader, input_buffer, '\n', yield);
            if (len > 0)
            {
                fprintf(stderr, "Read %d bytes\n", len);
                std::istream is(&input_buffer);
                for (std::string line; std::getline(is, line); )
                {
                    fprintf(stderr, "Line [%s], len %d\n", line.c_str(), len);
                    if (boost::starts_with(line, "Uid"))
                    {
                        std::vector<std::string> parsed_record;
                        boost::split(parsed_record, line, boost::is_any_of(" \t"));
                        return std::make_pair(std::stoi(parsed_record[1]), std::stoi(parsed_record[2]));
                    }
                }
            }
        }
        while(len > 0);
    
        return std::make_pair(-1, -1);
    }
   
    void operator()(const fanotify_event_metadata *metadata, ssize_t len, io::yield_context& yield)
    {
        fprintf(stderr, "Start operator()");
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
                if (metadata->mask & FAN_ACCESS)
                    printf("A file or a directory (but see BUGS) was accessed (read).");
                if (metadata->mask & FAN_OPEN)
                    printf("A file or a directory was opened.");
                if (metadata->mask & FAN_OPEN_EXEC)
                    printf("A file was opened with the intent to be executed.");
                if (metadata->mask & FAN_ATTRIB)
                    printf("A file or directory metadata was changed.");
                if (metadata->mask & FAN_CREATE)
                    printf("A child file or directory was created in a watched parent.");
                if (metadata->mask & FAN_DELETE)
                    printf("A child file or directory was deleted in a watched parent.");
                if (metadata->mask & FAN_DELETE_SELF)
                    printf("A watched file or directory was deleted.");
                if (metadata->mask & FAN_MOVED_FROM)
                    printf("A file or directory has been moved from a watched parent directory.");
                if (metadata->mask & FAN_MOVED_TO)
                    printf("A file or directory has been moved to a watched parent directory.");
                if (metadata->mask & FAN_MOVE_SELF)
                    printf("A watched file or directory was moved.");
                if (metadata->mask & FAN_MODIFY)
                    printf("A file was modified.");
                if (metadata->mask & FAN_CLOSE_WRITE)
                    printf("A file that was opened for writing (O_WRONLY or O_RDWR) was closed.");
                if (metadata->mask & FAN_CLOSE_NOWRITE)
                    printf("A file or directory that was opened read-only (O_RDONLY) was closed.");
                if (metadata->mask & FAN_Q_OVERFLOW)
                    printf("The event queue exceeded the limit of entries.");
                if (metadata->mask & FAN_ACCESS_PERM)
                    printf("An application wants to read a file or directory, for example using read(2) or readdir(2).");
                if (metadata->mask & FAN_OPEN_PERM)
                    printf("An application wants to open a file or directory.");
                if (metadata->mask & FAN_OPEN_EXEC_PERM)
                    printf("An  application  wants  to  open a file for execution.");

                /* Handle open permission event */
                if (metadata->mask & FAN_OPEN_PERM)
                {
                    printf("FAN_OPEN_PERM: ");

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

                auto uids = parseProc(metadata->pid, yield);
                printf("PID %ld, %d/%d, ", metadata->pid, uids.first, uids.second);
                /* Handle closing of writable file event */

                if (metadata->mask & FAN_CLOSE_WRITE)
                    printf("FAN_CLOSE_WRITE: ");

                /* Retrieve and print pathname of the accessed file */
                fs::path p = "/proc/self/fd/";
                p /= std::to_string(metadata->fd);
                if (!fs::exists(p))
                {
                    throw std::runtime_error("link does not exists");
                }

                auto path = fs::read_symlink(p);
                printf("File %s\n", path.string().c_str());

                // Close the file descriptor of the event

                close(metadata->fd);
            }

            // Advance to next event
            metadata = FAN_EVENT_NEXT(metadata, len);
        }

    }

private:
    boost::asio::posix::stream_descriptor& m_notify_fd;
};

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

void FanotifyGroup::asyncEvent(io::yield_context& yield)
{
    const struct fanotify_event_metadata *metadata;
    struct fanotify_event_metadata buf[200];
    ssize_t len;

    MetadataWorker worker(m_notify_fd);

    fprintf(stderr, "FAN_START\n");
    // do some fun
    for(;;)
    {
        // Read some events
        fprintf(stderr, "FAN_LOOP, fd %d\n", m_notify_fd.native_handle());

        boost::system::error_code ec;
        len = m_notify_fd.async_read_some(io::buffer(buf), yield[ec]);
        
        if (len == -1 && errno != EAGAIN)
        {
            throw FanotifyGroupError("read error", errno);
        }
        // Check if end of available data reached
        if (len <= 0)
            return;//break;

        fprintf(stderr, "FAN_LOOP, got event\n");
        // Point to the first event in the buffer
        metadata = buf;
        
        worker(metadata, len, yield);
    }
}
