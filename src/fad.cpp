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
#include <utility>

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
        LogInfo("Act as daemon...");

        int ret = daemon(0, 0);
        if (ret == -1)
        {
            LogFatal("Cannot daemonize...");
        }
    }
}

static void
logInit()
{
    auto& config = Config::get();
    
    initLog(config.getValue<std::string>("logfile"));

    LogLevel level =
        translateLogName(config.getValue<std::string>("loglevel"));
    
    setLogLevel(level);
}

int
main(int argc, char *argv[])
{
    /* Check mount point is supplied */
    auto& config = Config::get();

    try
    {
        if (!config.init(argc, argv))
            return -1;

        logInit();
        
        daemonizeIfConfigured(config);

        LogInfo("Started");
        
        boost::asio::io_context io_context;
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto){ io_context.stop(); });

        // Create the file descriptor for accessing the fanotify API
        FanotifyGroup f_group(io_context, FAN_CLOEXEC | FAN_CLASS_NOTIF /*| FAN_NONBLOCK*/);

        auto watches = config.getValue<vector<string>>("watch");
        for (auto& i: watches)
        {
            /* Mark the mount for:
               - permission events before opening files
               - notification events after closing a write-enabled
               file descriptor */
            LogInfo("Will be watching " << i);
            f_group.addMark(i, FAN_ACCESS | FAN_MODIFY | FAN_CLOSE_WRITE | FAN_ONDIR  );

        }
        boost::asio::spawn(io_context, [&](boost::asio::yield_context yield)
                                       {f_group.asyncEvent(yield);});

        LogInfo("Listening for events");


        io_context.run();
    }
    catch(std::exception& err)
    {
        LogError("Terminating due to error, " << err.what());
    }
    
    LogInfo("Listening for events stopped.");
    exit(EXIT_SUCCESS);
}
