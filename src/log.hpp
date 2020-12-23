#ifndef _FAD_LOG_HPP
#define _FAD_LOG_HPP

#include <sys/time.h>

#include <string>
#include <sstream>
#include <mutex>

/// Simplest log interface: log is the fstream object
/// All access to this object should be locked

enum LogLevel
{
    LogDebug,
    LogInfo,
    LogWarn,
    LogError,
    LogFatal
};

/// Translates string log level representation into numeric value
extern
LogLevel translateLogName(const char* name);

inline
LogLevel translateLogName(const std::string& name)
{
    return translateLogName(name.c_str());
}

extern
std::string translateLogLevel(LogLevel lv);

/// Initiale log file
extern
void initLog(const std::string& name);

/// Rotate log file
extern
void rotateLog();

/// returns the log's stream
extern
std::ostream& getLog();

/// lock the access to the log stream
extern
std::unique_lock<std::mutex> lockLog();
    
extern
void setLogLevel(const LogLevel&);
extern
LogLevel getLogLevel();

/// Log output manipulators

/// Nice looking digit printing
struct _NiceDigits {int width; char fill;};
inline _NiceDigits setdw(int width, char fill = '0')
{
    _NiceDigits out;
    out.width = width;
    out.fill  = fill;

    return out;
}
template <class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator <<(std::basic_ostream<CharT, Traits>& os,
            const _NiceDigits& nice)
{
    os.width(nice.width);
    os.fill(nice.fill);

    return os;
}

/// Time stamps
struct _TimeStamp{ std::string stamp_data;};

inline _TimeStamp time_stamp(const std::string& format)
{
    _TimeStamp stamp;
    stamp.stamp_data = format;

    return stamp;
}
inline _TimeStamp time_stamp(const char* format)
{
    _TimeStamp stamp;
    stamp.stamp_data.assign(format);

    return stamp;
}

template <class CharT, class Traits>
std::basic_ostream<CharT, Traits>&
operator <<(std::basic_ostream<CharT, Traits>& os,
            const _TimeStamp& which)
{
    std::ostringstream buf;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t tt = tv.tv_sec;

    tm timt;
    localtime_r(&tt,&timt); 

    for (size_t i = 0; i < which.stamp_data.length(); ++i)
    {
	switch (which.stamp_data[i])
	{
	    case 'Y': buf << setdw(4) << (timt.tm_year+1900); 
                break ;
	    case 'M': buf << setdw(2) << (timt.tm_mon+1);     
                break ;
	    case 'D': buf << setdw(2) << timt.tm_mday;       
                break ;
	    case 'h': buf << setdw(2) << timt.tm_hour;        
                break ;
	    case 'm': buf << setdw(2) << timt.tm_min;         
                break ;
	    case 's': buf << setdw(2) << timt.tm_sec;        
                break ;
	    case 'u': buf << setdw(3) << tv.tv_usec/1000;     
                break ;
	    default: if (which.stamp_data[i] >= 0x20)
                    buf  << which.stamp_data[i];
	}
    }
    os << buf.str();

    return os;
}

/// Macro(s) to work 
#define LogDebug(op)                                            \
    do                                                          \
    {                                                           \
        std::unique_lock<std::mutex> lock = lockLog();          \
        if(getLogLevel() == LogDebug)                           \
        {                                                       \
            getLog() << time_stamp("Y-M-D h:m:s.u")             \
                     << " (Debug: "                             \
                     << __FILE__ << ":" << __LINE__ << "): "    \
                     << op                                      \
                     << std::endl;                              \
        }                                                       \
    }while(false)

#define LogInfo(op)                                             \
    do                                                          \
    {                                                           \
        std::unique_lock<std::mutex> lock = lockLog();          \
        if(getLogLevel() == LogDebug)                           \
        {                                                       \
            getLog() << time_stamp("Y-M-D h:m:s.u")             \
                     << " (Info: "                              \
                     << __FILE__ << ":" << __LINE__ << "): "    \
                     << op                                      \
                     << std::endl;                              \
        }                                                       \
    }while(false)


#define LogWarning(op)                                          \
    do                                                          \
    {                                                           \
        std::unique_lock<std::mutex> lock = lockLog();          \
        if(getLogLevel() <= LogWarn)                            \
        {                                                       \
            getLog() << time_stamp("Y-M-D h:m:s.u")             \
                     << " (Warning";                            \
                                                                \
            if(getLogLevel() == LogDebug)                       \
                getLog() << ": "                                \
                         <<__FILE__ << ":" << __LINE__;         \
                                                                \
            getLog() << "): "                                   \
                     << op                                      \
                     << std::endl;                              \
        }                                                       \
    }while(false)

#define LogError(op)                                            \
    do                                                          \
    {                                                           \
        std::unique_lock<std::mutex> lock = lockLog();          \
        if(getLogLevel() <= LogError)                           \
        {                                                       \
            getLog() << time_stamp("Y-M-D h:m:s.u")             \
                     << " (Error";                              \
            if(getLogLevel() == LogDebug)                       \
                getLog() << ": "                                \
                         <<__FILE__ << ":" << __LINE__;         \
                                                                \
            getLog() << "): "                                   \
                     << op                                      \
                     << std::endl;                              \
        }                                                       \
    }while(false)

#define LogFatal(op)                                            \
    do                                                          \
    {                                                           \
        std::unique_lock<std::mutex> lock = lockLog();          \
        if(getLogLevel() <= LogFatal)                           \
        {                                                       \
            getLog() << time_stamp("Y-M-D h:m:s.u")             \
                     << " (FATAL";                              \
            if(getLogLevel() == LogDebug)                       \
                getLog() << ": "                                \
                         <<__FILE__ << ":" << __LINE__;         \
                                                                \
            getLog() << "): "                                   \
                     << op                                      \
                     << std::endl;                              \
        }                                                       \
        exit(1);                                                \
    }while(false)

#endif //_FAD_LOG_HPP
