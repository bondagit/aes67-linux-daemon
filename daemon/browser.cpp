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

#include <boost/algorithm/string.hpp>

#include "utils.hpp"
#include "browser.hpp"

using namespace boost::algorithm;
using namespace std::chrono;
using second_t = duration<double, std::ratio<1> >;

std::shared_ptr<Browser> Browser::create(std::shared_ptr<Config> config) {
  // no need to be thread-safe here
  static std::weak_ptr<Browser> instance;
  if (auto ptr = instance.lock()) {
    return ptr;
  }
  auto ptr = std::shared_ptr<Browser>(new Browser(config));
  instance = ptr;
  return ptr;
}

std::list<RemoteSource> Browser::get_remote_sources(
    const std::string& _source) const {
  std::list<RemoteSource> sources_list;
  std::shared_lock sources_lock(sources_mutex_);
  // return list of remote sources ordered by name
  for (const auto& source : sources_.get<name_tag>()) {
    if (boost::iequals(source.source, _source) ||
        boost::iequals("all", _source)) {
      sources_list.push_back(source);
    }
  }
  return sources_list;
}

bool Browser::worker() {
  sap_.set_multicast_interface(config_->get_ip_addr_str());
  // Join SAP muticast address
  igmp_.join(config_->get_ip_addr_str(), config_->get_sap_mcast_addr());
  auto sap_timepoint = steady_clock::now();
  int sap_interval = 10;
  auto mdns_timepoint = steady_clock::now();
  int mdns_interval = 10;

  while (running_) {
    bool is_announce;
    uint16_t msg_id_hash;
    uint32_t addr;
    std::string sdp;

    if (sap_.receive(is_announce, msg_id_hash, addr, sdp)) {
      std::stringstream ss;
      ss << "sap:" << msg_id_hash;
      std::string id(ss.str());
      BOOST_LOG_TRIVIAL(debug) << "browser:: received SAP message for " << id;

      std::unique_lock sources_lock(sources_mutex_);

      auto it = sources_.get<id_tag>().find(id);
      if (it == sources_.end()) {
        // Source is not in the map
        if (is_announce) {
          // annoucement, add new source
          sources_.insert(
              {id,
               "SAP",
               ip::address_v4(ntohl(addr)).to_string(),
               sdp_get_subject(sdp),
               {},
               sdp,
               static_cast<uint32_t>(
                   duration_cast<second_t>(steady_clock::now() - startup_)
                       .count()),
               config_->get_sap_interval()});
        }
      } else {
        // Source is already in the map
        if (is_announce) {
          BOOST_LOG_TRIVIAL(debug)
              << "browser:: refreshing SAP source " << it->id;
          // annoucement, update last seen and announce period
          uint32_t last_seen =
              duration_cast<second_t>(steady_clock::now() - startup_).count();
          auto upd_source{*it};
          upd_source.announce_period = last_seen - upd_source.last_seen;
          upd_source.last_seen = last_seen;
          sources_.replace(it, upd_source);
        } else {
          BOOST_LOG_TRIVIAL(info) << "browser:: removing SAP source " << it->id
                                  << " name " << it->name;
          // deletion, remove entry
          sources_.erase(it);
        }
      }
    }

    // check if it's time to update the SAP remote sources
    if ((duration_cast<second_t>(steady_clock::now() - sap_timepoint).count()) >
        sap_interval) {
      sap_timepoint = steady_clock::now();
      // remove all sessions no longer announced
      auto offset =
          duration_cast<second_t>(steady_clock::now() - startup_).count();

      std::unique_lock sources_lock(sources_mutex_);
      for (auto it = sources_.begin(); it != sources_.end();) {
        if (it->source == "SAP" &&
            (offset - it->last_seen) > (it->announce_period * 10)) {
          // remove from remote SAP sources
          BOOST_LOG_TRIVIAL(info)
              << "browser:: SAP source " << it->id << " timeout";
          it = sources_.erase(it);
        } else {
          it++;
        }
      }
    }

    // check if it's time to process the mDNS RTSP sources
    if ((duration_cast<second_t>(steady_clock::now() - mdns_timepoint)
             .count()) > mdns_interval) {
      mdns_timepoint = steady_clock::now();
      process_results();
    }
  }

  return true;
}

void Browser::on_change_rtsp_source(const std::string& name,
                                    const std::string& domain,
                                    const RtspSource& s) {
  uint32_t last_seen =
      duration_cast<second_t>(steady_clock::now() - startup_).count();
  std::unique_lock sources_lock(sources_mutex_);
  /* search by name */
  auto rng = sources_.get<name_tag>().equal_range(name);
  while (rng.first != rng.second) {
    const auto& it = rng.first;
    if (it->source == "mDNS" && it->domain == domain) {
      /* mDNS source with same name and domain -> update */
      BOOST_LOG_TRIVIAL(info) << "browser:: updating RTSP source " << s.id
                              << " name " << name << " domain " << domain;
      auto upd_source{*it};
      upd_source.id = s.id;
      upd_source.sdp = s.sdp;
      upd_source.address = s.address;
      upd_source.last_seen = last_seen;
      sources_.get<name_tag>().replace(it, upd_source);
      return;
    }
    ++rng.first;
  }
  /* entry not found -> add */
  BOOST_LOG_TRIVIAL(info) << "browser:: adding RTSP source " << s.id << " name "
                          << name << " domain " << domain;
  sources_.insert(
      {s.id, s.source, s.address, name, domain, s.sdp, last_seen, 0});
}

void Browser::on_remove_rtsp_source(const std::string& name,
                                    const std::string& domain) {
  std::unique_lock sources_lock(sources_mutex_);
  auto& name_idx = sources_.get<name_tag>();
  auto rng = name_idx.equal_range(name);
  while (rng.first != rng.second) {
    const auto& it = rng.first;
    if (it->source == "mDNS" && it->domain == domain) {
      BOOST_LOG_TRIVIAL(info)
          << "browser:: removing RTSP source " << it->id << " name " << it->name
          << " domain " << it->domain;
      name_idx.erase(it);
      break;
    }
    ++rng.first;
  }
}

bool Browser::init() {
  if (!running_) {
    /* init mDNS client */
    if (config_->get_mdns_enabled() && !MDNSClient::init()) {
      return false;
    }
    running_ = true;
    res_ = std::async(std::launch::async, &Browser::worker, this);
  }
  return true;
}

bool Browser::terminate() {
  if (running_) {
    running_ = false;
    /* wait for worker to exit */
    res_.get();
    /* terminate mDNS client */
    if (config_->get_mdns_enabled()) {
      MDNSClient::terminate();
    }
  }
  return true;
}
