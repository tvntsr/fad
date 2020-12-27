#ifndef _FAD_CONFIG_HPP
#define _FAD_CONFIG_HPP

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
        try
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            T t = m_vm[var].as<T>();

            return t;
        }
        catch(boost::bad_any_cast& err)
        {
            throw std::invalid_argument(std::string("wrong value: ") + var);
        }
    }

    bool isDaemon() const
    {
        return m_is_daemon;
    }
    
private:
    Config()
        : m_config_file("/etc/fad.conf")
        , m_is_daemon(false)
    {}
    ///:note: all private method are not thread safe

    bool applyConfig();
    /// reads cfg file
    void openConfigFile();

private:
    std::string  m_config_file;
    bool         m_is_daemon;

    boost::program_options::variables_map m_vm;

    mutable std::mutex m_mutex;
};

#endif //_FAD_CONFIG_HPP
