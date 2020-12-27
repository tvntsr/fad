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

#include <unistd.h>

#include <iostream>
#include <vector>

//#include <boost/program_options/parsers.hpp>
#include "config.hpp"

namespace po = boost::program_options;
Config& Config::get()
{
    static Config config;

    return config;
}

bool Config::init(int argc, char** argv)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Declare the supported options.
    po::options_description desc("Program options");
    desc.add_options()
        ("help,h", "produce help message")
        ("conf,c",
         po::value<std::string>()->default_value(m_config_file),
         "defines configuration file")
        ("daemon,d", "program acts as daemon");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) 
    {
        std::cerr << desc << std::endl;
        return false;
    }

    m_config_file = vm["conf"].as<std::string>();
    m_is_daemon   = (vm.count("daemon") != 0);

    return applyConfig();
}

void Config::reconfigure()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    applyConfig();
}

void Config::openConfigFile()
{
    // Declare the supported config file options.
    po::options_description cfg("Config file options");
    cfg.add_options()
        ("watch",
         po::value<std::vector<std::string>>()->multitoken(),
         "directories to watch")
        ("logfile", 
         po::value<std::string>()->default_value("fad.log"),
         "path and log file name")
        ("loglevel",
         po::value<std::string>()->default_value("Debug"),
         "Log level, values are: Fatal, Error, Warn, Info, Debug")
        ("report", 
         po::value<std::string>()->default_value("fad.report"),
         "path and report file name")
        ("pidfile", 
         po::value<std::string>()->default_value("fad.pid"),
         "path and pid file name")
        ("daemon", 
         po::value<bool>()->default_value(false),
         "program acts as daemon");

    po::store(po::parse_config_file<char>(m_config_file.c_str(), cfg), m_vm);
    po::notify(m_vm);

    if (!m_is_daemon)
    {
        m_is_daemon = m_vm["daemon"].as<bool>();
    }
}

bool Config::applyConfig()
{
    openConfigFile();

    return true;
}
