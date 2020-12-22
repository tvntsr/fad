#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fanotify.h>
#include <unistd.h>


#include <string>
#include <filesystem>
#include <fstream>
#include <vector>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

#include <boost/asio/read_until.hpp>
#include <boost/asio/buffer.hpp>
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
    
    // void makeProcPath(char* procfd_path, size_t max_len, int fd, const char* mask)
    // {
    //     /* Retrieve and print pathname of the accessed file */
    //     snprintf(procfd_path, sizeof(procfd_path),
    //              mask,
    //              fd);
    // }

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
        boost::asio::dynamic_string_buffer input_buffer(line);
        //std::ifstream fproc(p);
        boost::asio::posix::stream_descriptor file_reader(m_notify_fd.get_executor(),
                                                          open(p.string().c_str(), O_RDONLY));

        for (;  0 < boost::asio::async_read_until(file_reader, input_buffer, '\n', yield); )
        // read until you reach the end of the file
//        for (std::string line; std::getline(fproc, line); )
        {
            if (boost::starts_with(line, "Uid"))
            {
                std::vector<std::string> parsed_record;
                boost::split(parsed_record, line, boost::is_any_of(" \t"));
                //fproc.close();
            
                return std::make_pair(std::stoi(parsed_record[1]), std::stoi(parsed_record[2]));
            }
        }
    
        return std::make_pair(-1, -1);
    }
   
    void operator()(const fanotify_event_metadata *metadata, ssize_t len, io::yield_context& yield)
    {
        // char procfd_path[PATH_MAX];
        // char path[PATH_MAX];

        // ssize_t path_len;
        fprintf(stderr, "Start operator()");
        /* Loop over all events in the buffer */
        while (FAN_EVENT_OK(metadata, len))
        {
            /* Check that run-time and compile-time structures match */
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
                    printf("FAN_OPEN_PERM: ");

                    struct fanotify_response response;
                    /* Allow file to be opened */
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

                // snprintf(procfd_path, sizeof(procfd_path),
                //          "/proc/self/fd/%d", metadata->fd);

                auto path = fs::read_symlink(p);
                // path_len = readlink(procfd_path, path,
                //                     sizeof(path) - 1);
                // if (path_len == -1) {
                //     perror("readlink");
                //     exit(EXIT_FAILURE);
                // }

                // path[path_len] = '\0';
                printf("File %s\n", path.string().c_str());

                /* Close the file descriptor of the event */

                close(metadata->fd);
            }

            /* Advance to next event */

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

    fprintf(stderr, "FAN_START\n");
    // do some fun
    for(;;)
    {
        // Read some events
        fprintf(stderr, "FAN_LOOP, fd %d\n", m_notify_fd.native_handle());

        //len = read(m_notify_fd.native_handle(), (void *) &buf, sizeof(buf));
        boost::system::error_code ec;
        len = m_notify_fd.async_read_some(io::buffer(buf), yield[ec]);
        
        if (len == -1 && errno != EAGAIN)
        {
            throw FanotifyGroupError("read error", errno);
        }
        /* Check if end of available data reached */
        if (len <= 0)
            break;

        fprintf(stderr, "FAN_LOOP, got event\n");
        /* Point to the first event in the buffer */
        metadata = buf;

        boost::asio::spawn(m_context, [&](boost::asio::yield_context yield)
                                       {
                                           MetadataWorker worker(m_notify_fd);
                                           worker(metadata, len, yield);
                                       }
            );


    }
}
