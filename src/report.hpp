#ifndef _FAD_REPORT_HPP
#define _FAD_REPORT_HPP

#include <sstream>
#include <iostream>

class FadReport
{
public:
    FadReport(const std::string& filename)
    {

    }

    template<typename Item, typename... Rest>
    void makeReport(const Item& item, const Rest... rest)
    {
        std::ostringstream report;
        report << time_stamp("Y-M-D h:m:s.u") << ": " << item;
        
        reportItem(report, rest...);

        std::cerr << report.str() << std::endl;
    }
    
private:

    template<typename Item, typename... Rest>
    std::ostream& reportItem(std::ostream& report, const Item& item, const Rest... rest)
    {
        report << ",\t" << item;

        return reportItem(report, rest...);
    }

    
    template<typename Item>
    std::ostream& reportItem(std::ostream& report, const Item& item)
    {
        return report;
    }

    std::ostream& reportItem(std::ostream& report)
    {
        return report;
    }

};

#endif //_FAD_REPORT_HPP
