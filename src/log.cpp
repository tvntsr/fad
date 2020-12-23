#include <stdlib.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "log.hpp"

static
void closeLog();

// Log level names, used in cfg and setloglevel commands
static
struct LogLevelName
{
    LogLevel    lev;
    const char* name;
}
level_names[] =
{
    {LogDebug, "Debug"},
    {LogInfo,  "Info" }, 
    {LogWarn,  "Warn" },
    {LogError, "Error"},
    {LogFatal, "Fatal"}
};

using namespace std;

/// Translates string log level representation  into numeric value
LogLevel translateLogName(const char* name)
{
    int i = 0;
    while(true)
    {
        if (strcasecmp(name, level_names[i].name) == 0)
        {
            return level_names[i].lev;
        }
        if (level_names[i].lev == LogFatal)
            break;

        ++i;
    }
    // Error, name not found, throw an exception
    throw invalid_argument(
        string("translateLogName: incorrect loglevel name:") + string(name));
}

string translateLogLevel(LogLevel lv)
{
    int i = 0;
    while(true)
    {
        if (level_names[i].lev == lv)
        {
            return string(level_names[i].name);
        }

        if (level_names[i].lev == LogFatal)
            break;

        ++i;
    }

    // Error, name not found, throw an exception
    throw invalid_argument("translateLogLevel: Incorrect loglevel value");
}

static
LogLevel log_level = LogDebug;

static 
std::string log_file_name;

static
std::ofstream Logger;

static bool initialed = false;

static
std::mutex* log_mutex = 0;

extern "C"
{
    static void close_at_exit()
    {
        closeLog();
    }
}

void initLog(const std::string& name)
{
    if (initialed)
        return;

    Logger.clear();
    Logger.open(name.c_str(), 
                std::ios_base::out | std::ios_base::app);
    
    if (!Logger)
    {
        // Log is not working... exit? 
        throw runtime_error("Cannot open log file");
    }
    log_file_name = name;
    initialed = true;

    log_mutex = new std::mutex;

    std::unique_lock<std::mutex> lock(*log_mutex);
    getLog() << time_stamp("Y-M-D h:m:s.u")
             << " Started..." 
             << std::endl;

    atexit(close_at_exit);
}

void rotateLog()
{
    if (!initialed)
        return;
    
    std::unique_lock<std::mutex> lock(*log_mutex);
    Logger.close();

    Logger.open(log_file_name.c_str(), 
                std::ios_base::out | std::ios_base::app);

    getLog() << time_stamp("Y-M-D h:m:s.u")
             << " Log rotated..." 
             << std::endl;
}


/// closes log file
void closeLog()
{
    if (!initialed)
        return;

    // lock the access
    {
        std::unique_lock<std::mutex> lock(*log_mutex);
        getLog() << time_stamp("Y-M-D h:m:s.u")
                 << " Terminated..." 
                 << std::endl;

        Logger.close();
    
        initialed = false;
    }
    if(log_mutex)
        delete log_mutex;
}

std::ostream& getLog()
{
    return initialed? Logger : std::cerr;
}

void setLogLevel(const LogLevel& l)
{
    std::unique_lock<std::mutex> lock(*log_mutex);
    Logger.flush();
    log_level = l;
}

LogLevel getLogLevel()
{
    return log_level;
}

std::unique_lock<std::mutex> lockLog()
{
    if (!log_mutex)
        log_mutex = new std::mutex;
        //throw runtime_error("Log is not initialed");

    return std::unique_lock<std::mutex>(*log_mutex);
}
