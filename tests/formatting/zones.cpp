#include "date/tz.h"
#include <iostream>
#include <stdexcept>
int main() {
    try {
        // The configured path does not exist. Embedded loading must ignore it.
        date::set_install("/void-stage10-no-machine-zone-database");
        const auto& db=date::get_tzdb();
        if(db.version!="2026e" || db.zones.size()!=344 || db.links.size()!=253)throw std::runtime_error("rules version/count");
        std::size_t checks=0;
        for(const auto& zone:db.zones)for(int y:{1900,1970,2026,2050,9999}){zone.get_info(date::sys_days{date::year{y}/7/1});++checks;}
        for(const auto& link:db.links)date::locate_zone(link.name());
        auto* ny=date::locate_zone("America/New_York");
        if(ny->get_info(date::local_days{date::year{2026}/3/8}+std::chrono::hours{2}+std::chrono::minutes{30}).result!=date::local_info::nonexistent)throw std::runtime_error("gap");
        if(ny->get_info(date::local_days{date::year{2026}/11/1}+std::chrono::hours{1}+std::chrono::minutes{30}).result!=date::local_info::ambiguous)throw std::runtime_error("overlap");
        auto paris=date::locate_zone("Europe/Paris")->get_info(date::sys_days{date::year{1900}/1/1});
        if(paris.offset!=std::chrono::seconds{561})throw std::runtime_error("historical second offset");
        bool rejected=false;try{date::locate_zone("Void/Absent");}catch(const std::runtime_error&){rejected=true;}if(!rejected)throw std::runtime_error("unknown zone accepted");
        std::cout<<db.version<<": "<<db.zones.size()<<" zones, "<<db.links.size()<<" links, "<<checks<<" projections; embedded gap/overlap/history passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
