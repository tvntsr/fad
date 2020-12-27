#ifndef _FAD_REPORT_HPP
#define _FAD_REPORT_HPP

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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <tuple>
#include <sstream>
#include <iostream>

#include <boost/asio/write.hpp>

#include "log.hpp"


/// reports about fa event
class FadReport
{
public:
    /// boost::asio::io_context used as async engine to file operations,
    /// filename - file to write report
    FadReport(boost::asio::io_context& context, const std::string& filename)
        : m_file_name(filename)
        , m_file_writer(context)
    {
        int fd = open(filename.c_str(), O_APPEND | O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
        if (fd == -1)
        {
            throw FanotifyGroupError("report file open error", errno);
        }
        m_file_writer.assign(fd);
    }

    /// close report file and open it again
    void reportRotate()
    {
        //int old = m_file_writer.native_handle();
        int fd = open(m_file_name.c_str(), O_APPEND | O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
        if (fd == -1)
        {
            throw FanotifyGroupError("report file open error", errno);
        }
        
        m_file_writer.close();
        m_file_writer.assign(fd);
        
        LogInfo("Report rotated");
    }

    /// creates the report line and print it to file
    template<typename... Rest>
    void makeReport(boost::asio::yield_context yield, const std::tuple<Rest...>& item)
    {
        std::ostringstream report;
        report << time_stamp("Y-M-D h:m:s.u") << ": ";
        bool tab = false;

        // walk thru the tuple
        std::apply([&](auto&&... s){
                ((
                    reportItem(report, tab, std::forward<decltype(s)>(s))
                 ),
                 ...);
                   }, item);

        report << "\n";
        
        boost::asio::async_write(m_file_writer, boost::asio::buffer(report.str()), yield);
    }
private:

    template<typename Item1, typename Item2>
    std::ostream& reportItem(std::ostream& report, bool & tab, const std::pair<Item1, Item2>& item)
    {
        if (!tab) tab = true;
        else report << ",\t";
        
        report << item.first << ": " << item.second;

        return report;
    }
    
    template<typename Item>
    std::ostream& reportItem(std::ostream& report, bool & tab, const Item& item)
    {
        if (!tab) tab = true;
        else report << ",\t";

        report << item;

        return report;
    }

    // std::ostream& reportItem(std::ostream& report)
    // {
    //     return report;
    // }
private:
    std::string m_file_name;
    boost::asio::posix::stream_descriptor m_file_writer;
};

#endif //_FAD_REPORT_HPP
