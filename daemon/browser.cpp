//
//  browser.cpp
//
//  Copyright (c) 2019 2020 Andrea Bondavalli. All rights reserved.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <experimental/map>
#include "browser.hpp"

using namespace std::chrono;
using second_t  = std::chrono::duration<double, std::ratio<1> >;

std::shared_ptr<Browser> Browser::create(
    std::shared_ptr<Config> config) {
  // no need to be thread-safe here
  static std::weak_ptr<Browser> instance;
  if (auto ptr = instance.lock()) {
    return ptr;
  }
  auto ptr =
      std::shared_ptr<Browser>(new Browser(config));
  instance = ptr;
  return ptr;
}

std::list<RemoteSource> Browser::get_remote_sources() {
  std::list<RemoteSource> sources_list;
  std::shared_lock sources_lock(sources_mutex_);
  for (auto const& [id, source] : sources_) {
    sources_list.emplace_back(source);
  }
  return sources_list;
}

static std::string sdp_get_subject(const std::string& sdp) {
  std::stringstream ssstrem(sdp);
  std::string line;
  while (getline(ssstrem, line, '\n')) {
    if (line.substr(0, 2) == "s=") {
      return line.substr(2);
    }
  }
  return "";
}

bool Browser::worker() {
  sap_.set_multicast_interface(config_->get_ip_addr_str());
  // Join SAP muticast address
  igmp_.join(config_->get_ip_addr_str(), config_->get_sap_mcast_addr());
  auto startup = steady_clock::now();
  auto sap_timepoint = steady_clock::now();
  int sap_interval = 10;

  while (running_) {
    bool is_announce;
    uint16_t msg_id_hash;
    uint32_t addr;
    std::string sdp;

    if (sap_.receive(is_announce, msg_id_hash, addr, sdp)) {
      char id[13];
      snprintf(id, sizeof id, "%x%x", addr, msg_id_hash);
      //BOOST_LOG_TRIVIAL(debug) << "browser:: received SAP message for " << id;

      std::unique_lock sources_lock(sources_mutex_);
      auto it = sources_.find(id);
      if (it == sources_.end()) {
        // Source is not in the map
        if (is_announce) {
          BOOST_LOG_TRIVIAL(info) << "browser:: adding SAP source " << id;
          // annoucement, add new source
          RemoteSource source;
	  source.id = id;
	  source.sdp = sdp;
	  source.source = "SAP";
	  source.address = ip::address_v4(ntohl(addr)).to_string();
	  source.name = sdp_get_subject(sdp);
          source.last_seen = 
            duration_cast<second_t>(steady_clock::now() - startup).count();
          source.announce_period = 360; //default period 
	  sources_[id] = source;
        }
      } else {
        // Source is already in the map
        if (is_announce) {
          BOOST_LOG_TRIVIAL(debug)
                      << "browser:: refreshing SAP source " << (*it).second.id;
          // annoucement, update last seen and announce period
          uint32_t last_seen = 
            duration_cast<second_t>(steady_clock::now() - startup).count();
          (*it).second.announce_period = last_seen - (*it).second.last_seen;
          (*it).second.last_seen = last_seen;
	} else {
          BOOST_LOG_TRIVIAL(info)
                      << "browser:: removing SAP source " << (*it).second.id;
          // deletion, remove entry
	  sources_.erase(it);
        }
      }
    }

    // check if it's time to update the SAP remote sources
    if ((duration_cast<second_t>(steady_clock::now() - sap_timepoint).count())
          > sap_interval) {
      sap_timepoint = steady_clock::now();
      // remove all sessions no longer announced
      auto offset = duration_cast<second_t>(steady_clock::now() - startup).count();

      std::unique_lock sources_lock(sources_mutex_);
      std::experimental::erase_if(sources_, [offset](auto entry) {
        if ((offset - entry.second.last_seen) > (entry.second.announce_period * 10)) {
          // remove from remote SAP sources
          BOOST_LOG_TRIVIAL(info)
                      << "browser:: SAP source " << entry.second.id << " timeout";
          return true;
        }
        return false;
      });
    }
  }
 
  return true;
}

