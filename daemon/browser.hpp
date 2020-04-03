//
//  browser.hpp
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

#ifndef _BROWSER_HPP_
#define _BROWSER_HPP_

#include <future>
#include <shared_mutex>
#include <thread>
#include <chrono>
#include <list>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

#include "config.hpp"
#include "sap.hpp"
#include "igmp.hpp"
#include "mdns_client.hpp"

using namespace boost::multi_index;

struct RemoteSource {
  std::string id;
  std::string source;
  std::string address;
  std::string name;
  std::string sdp;
  uint32_t last_seen; /* seconds from daemon startup */
  uint32_t announce_period; /* period between annoucements */
};

class Browser : public MDNSClient {
 public:
  static std::shared_ptr<Browser> create(
      std::shared_ptr<Config> config);
  Browser() = delete;
  Browser(const Browser&) = delete;
  Browser& operator=(const Browser&) = delete;
  virtual ~Browser(){ terminate(); };

  bool init() override;
  bool terminate() override;

  std::list<RemoteSource> get_remote_sources() const;

 protected:
  // singleton, use create() to build
  Browser(std::shared_ptr<Config> config):
      MDNSClient(config),
      startup_(std::chrono::steady_clock::now()){};

  bool worker();

  virtual void on_new_rtsp_source(
                   const std::string& name,
                   const std::string& domain,
                   const RTSPSSource& source) override;
  virtual void on_remove_rtsp_source(
                   const std::string& name,
                   const std::string& domain) override;

  std::future<bool> res_;
  std::atomic_bool running_{false};

  /* current sources */
  struct id_tag{};
  using by_id = hashed_unique<tag<id_tag>, member<RemoteSource,
                              std::string, &RemoteSource::id>>;
  struct name_tag{};
  using by_name = ordered_non_unique<tag<name_tag>, member<RemoteSource,
                                     std::string, &RemoteSource::name>>;
  using sources_t = multi_index_container<RemoteSource,
                                          indexed_by<by_id, by_name>>;
    
  sources_t sources_;
  mutable std::shared_mutex sources_mutex_;

  SAP sap_{config_->get_sap_mcast_addr()};
  IGMP igmp_;
  std::chrono::time_point<std::chrono::steady_clock> startup_;
};

#endif
