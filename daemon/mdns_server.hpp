//
//  mdns_server.hpp
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

#ifndef _MDNS_SERVER_HPP_
#define _MDNS_SERVER_HPP_

#ifdef _USE_AVAHI_
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/thread-watch.h>
#endif

#include <atomic>
#include <boost/bimap.hpp>
#include <mutex>
#include <thread>

#include "config.hpp"
#include "session_manager.hpp"
#include "utils.hpp"

class MDNSServer {
 public:
  MDNSServer(std::shared_ptr<SessionManager> session_manager,
             std::shared_ptr<Config> config)
      : session_manager_(session_manager), config_(config) {}

  MDNSServer() = delete;
  MDNSServer(const MDNSServer&) = delete;
  MDNSServer& operator=(const MDNSServer&) = delete;
  virtual ~MDNSServer() { terminate(); };

  virtual bool init();
  virtual bool terminate();

  bool add_service(const std::string& name, const std::string& sdp);
  bool remove_service(const std::string& name);

 protected:
  std::atomic_bool running_{false};
  std::shared_ptr<SessionManager> session_manager_;
  std::shared_ptr<Config> config_;
  std::string node_id_{get_node_id(config_->get_ip_addr())};

#ifdef _USE_AVAHI_
  using entry_group_bimap_t =
      boost::bimap<std::string /* name */, AvahiEntryGroup*>;

  entry_group_bimap_t groups_;

  /* order is important here */
  std::unique_ptr<AvahiThreadedPoll, decltype(&avahi_threaded_poll_free)> poll_{
      nullptr, &avahi_threaded_poll_free};
  std::unique_ptr<AvahiClient, decltype(&avahi_client_free)> client_{
      nullptr, &avahi_client_free};

  static void entry_group_callback(AvahiEntryGroup* g,
                                   AvahiEntryGroupState state,
                                   void* userdata);
  static void client_callback(AvahiClient* c,
                              AvahiClientState state,
                              void* userdata);

  bool create_services(AvahiClient* client);
#endif
};

#endif
