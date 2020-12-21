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
         po::value<std::string>()->default_value("data.log"),
         "path and log file name")
        ("loglevel",
         po::value<std::string>()->default_value("Debug"),
         "Log level, values are: Fatal, Error, Warn, Debug")
        ("daemon", 
         po::value<bool>()->default_value(false),
         "program acts as daemon");
    
    po::store(po::parse_config_file<char>(m_config_file.c_str(), cfg), m_vm);
    po::notify(m_vm);
}

bool Config::applyConfig()
{
    openConfigFile();

    return true;
}
