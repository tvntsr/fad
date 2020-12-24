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
#include "metadataworker.hpp"
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
        LogDebug("FAN_LOOP, fd " << m_notify_fd.native_handle());

        boost::system::error_code ec;
        len = m_notify_fd.async_read_some(io::buffer(buf), yield[ec]);
        
        if (len == -1 && errno != EAGAIN)
        {
            throw FanotifyGroupError("read error", errno);
        }
        // Check if end of available data reached
        if (len <= 0)
            return;//break;

        LogDebug("FAN_LOOP, got event");
        
        // Point to the first event in the buffer
        metadata = buf;
        
        worker(metadata, len, yield);
    }
}
