#ifndef _FAD_CONFIG_HPP
#define _FAD_CONFIG_HPP

#include <mutex>
#include <boost/program_options.hpp>

/// Implements config file options as well as command line
class Config
{
public:
    /// get the Config instance
    static Config& get();

    ~Config()
    {}

    /// parses command line and reads the config file
    bool init(int argc, char** argv);
    /// rereads the config file
    void reconfigure();

    /// value from config file
    template<typename T>
    T getValue(const char* var) const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        T t = m_vm[var].as<T>();

        return t;
    }

    bool isDaemon() const
    {
        return m_is_daemon;
    }
    
private:
    Config()
        : m_config_file("config.cfg")
        , m_is_daemon(false)
    {}
    ///:note: all private method are not thread safe

    bool applyConfig();
    /// reads cfg file
    void openConfigFile();
    /// init Log
    //void openLogFile();

private:
    std::string  m_config_file;
    bool         m_is_daemon;

    boost::program_options::variables_map m_vm;

    mutable std::mutex m_mutex;
};

#endif //_FAD_CONFIG_HPP
