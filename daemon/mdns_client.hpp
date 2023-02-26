//
//  mdns_client.hpp
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

#ifndef _MDNS_CLIENT_HPP_
#define _MDNS_CLIENT_HPP_

#ifdef _USE_AVAHI_
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/thread-watch.h>
#endif

#include <future>
#include <list>
#include <set>
#include <shared_mutex>
#include <thread>

#include "config.hpp"
#include "rtsp_client.hpp"

class MDNSClient {
 public:
  explicit MDNSClient(std::shared_ptr<Config> config) : config_(config){};
  MDNSClient() = delete;
  MDNSClient(const MDNSClient&) = delete;
  MDNSClient& operator=(const MDNSClient&) = delete;
  virtual ~MDNSClient() = default;

  virtual bool init();
  virtual bool terminate();

 protected:
  virtual void on_change_rtsp_source(const std::string& name,
                                     const std::string& domain,
                                     const RtspSource& source){};
  virtual void on_remove_rtsp_source(const std::string& name,
                                     const std::string& domain){};

#ifdef _USE_AVAHI_
  static void resolve_callback(AvahiServiceResolver* r,
                               AvahiIfIndex interface,
                               AvahiProtocol protocol,
                               AvahiResolverEvent event,
                               const char* name,
                               const char* type,
                               const char* domain,
                               const char* host_name,
                               const AvahiAddress* address,
                               uint16_t port,
                               AvahiStringList* txt,
                               AvahiLookupResultFlags flags,
                               void* userdata);
  static void browse_callback(AvahiServiceBrowser* b,
                              AvahiIfIndex interface,
                              AvahiProtocol protocol,
                              AvahiBrowserEvent event,
                              const char* name,
                              const char* type,
                              const char* domain,
                              AvahiLookupResultFlags flags,
                              void* userdata);
  static void client_callback(AvahiClient* c,
                              AvahiClientState state,
                              void* userdata);
#endif

  void process_results();

  std::shared_ptr<Config> config_;

 private:
  std::list<std::future<void> > sources_res_;
  std::mutex sources_res_mutex_;

  std::atomic_bool running_{false};

#ifdef _USE_AVAHI_
  /* order is important here */
  std::unique_ptr<AvahiThreadedPoll, decltype(&avahi_threaded_poll_free)> poll_{
      nullptr, &avahi_threaded_poll_free};
  std::unique_ptr<AvahiClient, decltype(&avahi_client_free)> client_{
      nullptr, &avahi_client_free};
  std::unique_ptr<AvahiServiceBrowser, decltype(&avahi_service_browser_free)>
      sb_{nullptr, &avahi_service_browser_free};
  std::set<std::pair<std::string /*name*/, std::string /*domain */> >
      active_resolvers;
#endif
};

#endif
