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

#include "config.hpp"
#include "sap.hpp"
#include "igmp.hpp"

struct RemoteSource {
  std::string id;
  std::string source;
  std::string address;
  std::string name;
  std::string sdp;
  uint32_t last_seen; /* seconds from daemon startup */
  uint32_t announce_period; /* period between annoucementis */
};

class Browser {
 public:
  static std::shared_ptr<Browser> create(
      std::shared_ptr<Config> config);
  Browser() = delete;
  Browser(const Browser&) = delete;
  Browser& operator=(const Browser&) = delete;
  virtual ~Browser(){ stop(); };

  // session manager interface
  bool start() {
    if (!running_) {
      running_ = true;
      res_ = std::async(std::launch::async, &Browser::worker, this);
    }
    return true;
  }

  bool stop() {
    if (running_) {
      running_ = false;
      return res_.get();
    }
    return true;
  }

  std::list<RemoteSource> get_remote_sources();

 protected:
  // singleton, use create() to build
  Browser(std::shared_ptr<Config> config)
      : config_(config){};

  bool worker();

  std::shared_ptr<Config> config_;
  std::future<bool> res_;
  std::atomic_bool running_{false};

  /* current sources */
  std::map<std::string /* id */, RemoteSource> sources_;
  mutable std::shared_mutex sources_mutex_;

  SAP sap_{config_->get_sap_mcast_addr()};
  IGMP igmp_;
};

#endif
