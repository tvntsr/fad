#ifndef _FAD_FANOTIFY_GROUP_ERROR_HPP
#define _FAD_FANOTIFY_GROUP_ERROR_HPP

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

#include <string>
#include <exception>

/// Main error, it could be critical
class FanotifyGroupError : public std::exception
{
    std::string m_message;
public:
    FanotifyGroupError(const std::string& message, int err)
        : m_message(message)
    {
        makeErrorMessage(err);
    }

    FanotifyGroupError(std::string&& message, int err)
        : m_message(message)
    {
        makeErrorMessage(err);
    }
    
    ~FanotifyGroupError() noexcept
    {}

    const char* what() const noexcept
    {
        return m_message.c_str();
    }
private:
    void makeErrorMessage(int err)
    {
        m_message += ", error ";
        m_message += std::to_string(err);
        // m_message += ",";
        
        // std::array<char, 1024> buff;
        // int r = strerror_r(err, buff.data(), 1024);
        // printf("Err %d, errno %d \n", r, errno);
        
        // if (r>0)
        // {
        //     std::array<char, 4*1024> buff;
        //     int r = strerror_r(err, buff.data(), 1024);
        //     m_message += buff.data();
        // }
        // else
        //     m_message += buff.data();
    }
};


/// No data availabe for some reason, such error could be skipped on upper level
class FanotifyNoDataError : public std::exception
{
public:
    FanotifyNoDataError()
    {
    }

    ~FanotifyNoDataError() noexcept
    {}

    const char* what() const noexcept
    {
        return "No data available";
    }
};

#endif //_FAD_FANOTIFY_GROUP_ERROR_HPP
