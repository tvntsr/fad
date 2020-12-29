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
#include "metadataworker.hpp"
#include "report.hpp"
#include "pid.hpp"

namespace fs = std::filesystem;
using namespace std;

auto EVENT_TO_SUBSRIBE = FAN_ACCESS    | FAN_MODIFY      | FAN_CLOSE_WRITE | FAN_CLOSE_NOWRITE |
                         FAN_OPEN      | FAN_OPEN_EXEC   | FAN_ATTRIB      | FAN_CREATE        |
                         FAN_DELETE    | FAN_DELETE_SELF | FAN_MOVED_FROM  | FAN_MOVED_TO      |
                         FAN_MOVE_SELF;

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
    else
        (void)!chdir("/"); // Set working dir to '/' always
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

        Pid pid(config.getValue<std::string>("pidfile"));
        
        LogInfo("Started");
        
        boost::asio::io_context io_context;

        FadReport report(io_context, config.getValue<std::string>("report"));

        boost::asio::signal_set signals(io_context, SIGUSR1, SIGINT, SIGTERM);
        std::function<void (const boost::system::error_code&, const int&)> sig_handler =
            [&](const boost::system::error_code& err, const int& sig)
                           {
                               switch (sig)
                               {
                                   case SIGUSR1:
                                       rotateLog();
                                       report.reportRotate();
                                       break;
                                   case SIGINT:
                                   case SIGTERM:
                                       io_context.stop();
                                       break;
                               }
                               signals.async_wait(sig_handler); // set up it again
                           };
        signals.async_wait(sig_handler);



        // Create the file descriptor for accessing the fanotify API
        FanotifyGroup f_group(io_context, FAN_CLOEXEC | FAN_REPORT_FID | FAN_CLASS_NOTIF | FAN_NONBLOCK);

        auto watches = config.getValue<vector<string>>("watch");
        for (auto& i: watches)
        {
            /* Mark the mount for:
               - permission events before opening files
               - notification events after closing a write-enabled
               file descriptor */
            LogInfo("Will be watching " << i);
//            f_group.addMark(i, EVENT_TO_SUBSRIBE | FAN_ONDIR);
            f_group.addMark(i, EVENT_TO_SUBSRIBE | FAN_EVENT_ON_CHILD | FAN_ONDIR);
        }
        
        boost::asio::spawn(io_context, [&](boost::asio::yield_context yield)
                                       {f_group.asyncEvent<MetadataWorker>(report, yield);});

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
