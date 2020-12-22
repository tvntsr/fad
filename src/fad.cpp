//#define _GNU_SOURCE     /* Needed to get O_LARGEFILE definition */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fanotify.h>
#include <unistd.h>

#include <sstream>
#include <exception>
#include <filesystem>
#include <fstream>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

#include <boost/asio/signal_set.hpp>
#include <boost/asio/spawn.hpp>

#include "config.hpp"
#include "log.hpp"
#include "fanotify.hpp"

namespace fs = std::filesystem;
using namespace std;

static void
daemonizeIfConfigured(const Config& cfg)
{
    if (cfg.isDaemon())
    {
        LogWarning(<<"Act as daemon...");

        int ret = daemon(0, 0);
        if (ret == -1)
        {
            LogFatal(<< "Cannot daemonize...");
        }
    }
}

static std::tuple<int, int>
parse_proc(int pid)
{
    ostringstream str;
    str << "/proc/" << pid << "/status";

    fs::path p = str.str();

    if (!exists(p))
    {
        throw std::runtime_error("File does not exists");
    }

    ifstream fproc(p);

    // read until you reach the end of the file
    for (std::string line; std::getline(fproc, line); )
    {
        if (boost::starts_with(line, "Uid"))
        {
            std::vector<string> parsed_record;
            boost::split(parsed_record, line, boost::is_any_of(" \t"));
            fproc.close();
            
            return std::make_tuple(std::stoi(parsed_record[1]), std::stoi(parsed_record[2]));
        }
        
    }
    
    return std::make_tuple(-1, -1);
}

/* Read all available fanotify events from the file descriptor 'fd' */
static void
handle_events(int fd)
{
    const struct fanotify_event_metadata *metadata;
    struct fanotify_event_metadata buf[200];
    ssize_t len;
    char path[PATH_MAX];
    ssize_t path_len;
    char procfd_path[PATH_MAX];
    struct fanotify_response response;

    /* Loop while events can be read from fanotify file descriptor */

    for(;;) {

        /* Read some events */

        len = read(fd, (void *) &buf, sizeof(buf));
        if (len == -1 && errno != EAGAIN) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        /* Check if end of available data reached */

        if (len <= 0)
            break;

        /* Point to the first event in the buffer */

        metadata = buf;

        /* Loop over all events in the buffer */

        while (FAN_EVENT_OK(metadata, len)) {

            /* Check that run-time and compile-time structures match */

            if (metadata->vers != FANOTIFY_METADATA_VERSION) {
                fprintf(stderr,
                        "Mismatch of fanotify metadata version.\n");
                exit(EXIT_FAILURE);
            }

            /* metadata->fd contains either FAN_NOFD, indicating a
               queue overflow, or a file descriptor (a nonnegative
               integer). Here, we simply ignore queue overflow. */

            if (metadata->fd >= 0) {

                /* Handle open permission event */

                if (metadata->mask & FAN_OPEN_PERM) {
                    printf("FAN_OPEN_PERM: ");

                    /* Allow file to be opened */

                    response.fd = metadata->fd;
                    response.response = FAN_ALLOW;
                    write(fd, &response,
                          sizeof(struct fanotify_response));
                }

                auto uids = parse_proc(metadata->pid);
                
                printf("PID %ld, %d/%d, ", metadata->pid, std::get<0>(uids), std::get<1>(uids));
                /* Handle closing of writable file event */

                if (metadata->mask & FAN_CLOSE_WRITE)
                    printf("FAN_CLOSE_WRITE: ");

                /* Retrieve and print pathname of the accessed file */

                snprintf(procfd_path, sizeof(procfd_path),
                         "/proc/self/fd/%d", metadata->fd);
                path_len = readlink(procfd_path, path,
                                    sizeof(path) - 1);
                if (path_len == -1) {
                    perror("readlink");
                    exit(EXIT_FAILURE);
                }

                path[path_len] = '\0';
                printf("File %s\n", path);

                /* Close the file descriptor of the event */

                close(metadata->fd);
            }

            /* Advance to next event */

            metadata = FAN_EVENT_NEXT(metadata, len);
        }
    }
}

int
main(int argc, char *argv[])
{
    char buf;
    int fd, poll_num;
    nfds_t nfds;
    struct pollfd fds[2];

    /* Check mount point is supplied */

    if (argc != 2) {
        fprintf(stderr, "Usage: %s MOUNT\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("Press enter key to terminate.\n");

    boost::asio::io_context io_context;
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto){ io_context.stop(); });

    try
    {
        /* Create the file descriptor for accessing the fanotify API */
        FanotifyGroup f_group(io_context, FAN_CLOEXEC | FAN_CLASS_NOTIF /*| FAN_NONBLOCK*/);

        /* Mark the mount for:
           - permission events before opening files
           - notification events after closing a write-enabled
           file descriptor */
        f_group.addMark(argv[1], FAN_ACCESS | FAN_MODIFY | FAN_CLOSE_WRITE | FAN_ONDIR  );


        printf("Listening for events.\n");

        boost::asio::spawn(io_context, [&](boost::asio::yield_context yield)
                                       {f_group.asyncEvent(yield);});

        io_context.run();
    }
    catch(std::exception& err)
    {
        printf("Error %s\n", err.what());
    }
    printf("Listening for events stopped.\n");
    exit(EXIT_SUCCESS);
}

/*
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <cstdio>

using boost::asio::ip::tcp;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
namespace this_coro = boost::asio::this_coro;

#if defined(BOOST_ASIO_ENABLE_HANDLER_TRACKING)
# define use_awaitable \
  boost::asio::use_awaitable_t(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#endif

int main()
{
  try
  {
    boost::asio::io_context io_context(1);

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto){ io_context.stop(); });

    co_spawn(io_context, listener(), detached);

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::printf("Exception: %s\n", e.what());
  }
}

////

start_new_group(ioservice, yield_context yield)
{
   FanotifyGroup new_group(ioservice);
   new_group.asyncEvent(yeild);
}


spawn(ioservice, [ioservice](yield_context yield)
                 {start_new_group(ioservice, yield);})

ioservice.run();
 */
