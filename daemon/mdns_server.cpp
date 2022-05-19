//
//  mdns_server.cpp
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

#include <boost/asio.hpp>

#include "config.hpp"
#include "interface.hpp"
#include "log.hpp"
#include "utils.hpp"
#include "mdns_server.hpp"

#ifdef _USE_AVAHI_
struct AvahiLockGuard {
  AvahiLockGuard() = delete;
  AvahiLockGuard(AvahiThreadedPoll* poll) : poll_(poll) {
    if (poll_ != nullptr) {
      avahi_threaded_poll_lock(poll_);
    }
  }
  ~AvahiLockGuard() {
    if (poll_ != nullptr) {
      avahi_threaded_poll_unlock(poll_);
    }
  }

 private:
  AvahiThreadedPoll* poll_{nullptr};
};

void MDNSServer::entry_group_callback(AvahiEntryGroup* g,
                                      AvahiEntryGroupState state,
                                      void* userdata) {
  MDNSServer& mdns = *(reinterpret_cast<MDNSServer*>(userdata));
  /* Called whenever the entry group state changes */
  auto it = mdns.groups_.right.find(g);
  if (it == mdns.groups_.right.end()) {
    BOOST_LOG_TRIVIAL(debug)
        << "mdns_server:: cannot find name associated to group, skipping ...";
    return;
  }

  switch (state) {
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
      /* The entry group has been established successfully */
      BOOST_LOG_TRIVIAL(debug) << "mdns_server:: entry group name ("
                               << it->second << ") established";
      break;

    case AVAHI_ENTRY_GROUP_COLLISION: {
      BOOST_LOG_TRIVIAL(warning)
          << "mdns_server:: entry group name (" << it->second << ") collision";
      break;
    }

    case AVAHI_ENTRY_GROUP_FAILURE:
      BOOST_LOG_TRIVIAL(error) << "mdns_server:: entry group name "
                               << "(" << it->second << ") "
                               << "failure"
                               << avahi_strerror(avahi_client_errno(
                                      avahi_entry_group_get_client(g)));
      /* Some kind of failure happened while we were registering our services */
      avahi_threaded_poll_quit(mdns.poll_.get());
      break;

    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
      break;
  }
}

bool MDNSServer::create_services(AvahiClient* client) {
  if (!config_->get_mdns_enabled()) {
    return false;
  }

  std::unique_ptr<AvahiEntryGroup, decltype(&avahi_entry_group_free)> group{
      avahi_entry_group_new(client, entry_group_callback, this),
      &avahi_entry_group_free};
  if (group == nullptr) {
    BOOST_LOG_TRIVIAL(error)
        << "mdns_server:: create_services() failed avahi_entry_group_new(): "
        << avahi_strerror(avahi_client_errno(client));
    return false;
  }

  /* register ravenna services, without user defined name */
  int ret = avahi_entry_group_add_service(
      group.get(), this->config_->get_interface_idx(), AVAHI_PROTO_INET, {},
      node_id_.c_str(), "_http._tcp", nullptr, nullptr,
      this->config_->get_http_port(), nullptr);
  if (ret < 0) {
    BOOST_LOG_TRIVIAL(error)
        << "mdns_server:: add_service() " << node_id_
        << "failed to add entry _http._tcp" << avahi_strerror(ret);
    return false;
  }
  BOOST_LOG_TRIVIAL(info) << "mdns_server:: adding service _http._tcp for "
                          << node_id_;

  ret = avahi_entry_group_add_service_subtype(
      group.get(), this->config_->get_interface_idx(), AVAHI_PROTO_INET, {},
      node_id_.c_str(), "_http._tcp", nullptr, "_ravenna._sub._http._tcp");
  if (ret < 0) {
    BOOST_LOG_TRIVIAL(error) << "mdns_server:: add_service() " << node_id_
                             << "failed to add subtype _ravenna._sub._http._tcp"
                             << avahi_strerror(ret);
    return false;
  }

  ret = avahi_entry_group_add_service(
      group.get(), this->config_->get_interface_idx(), AVAHI_PROTO_INET, {},
      node_id_.c_str(), "_rtsp._tcp", nullptr, nullptr,
      this->config_->get_rtsp_port(), nullptr);
  if (ret < 0) {
    BOOST_LOG_TRIVIAL(error)
        << "mdns_server:: add_service() " << node_id_
        << "failed to add entry _rtsp._tcp" << avahi_strerror(ret);
    return false;
  }
  BOOST_LOG_TRIVIAL(info) << "mdns_server:: adding service _rtsp._tcp for "
                          << node_id_;

  ret = avahi_entry_group_add_service_subtype(
      group.get(), this->config_->get_interface_idx(), AVAHI_PROTO_INET, {},
      node_id_.c_str(), "_rtsp._tcp", nullptr, "_ravenna._sub._rtsp._tcp");
  if (ret < 0) {
    BOOST_LOG_TRIVIAL(error) << "mdns_server:: add_service() " << node_id_
                             << "failed to add subtype _ravenna._sub._rtsp._tcp"
                             << avahi_strerror(ret);
    return false;
  }

  /* Tell the server to register the service */
  ret = avahi_entry_group_commit(group.get());
  if (ret < 0) {
    BOOST_LOG_TRIVIAL(error)
        << "mdns_server:: create_services() failed to commit entry group "
        << avahi_strerror(ret);
    return false;
  }
  groups_.insert(entry_group_bimap_t::value_type(node_id_, group.release()));
  return true;
}

#endif

bool MDNSServer::add_service(const std::string& name, const std::string& sdp) {
  if (!running_ || !config_->get_mdns_enabled()) {
    return false;
  }

#ifdef _USE_AVAHI_
  AvahiLockGuard avahi_lock(this->poll_.get());
  if (avahi_client_get_state(client_.get()) != AVAHI_CLIENT_S_RUNNING) {
    BOOST_LOG_TRIVIAL(error)
        << "mdns_server:: add_service() failed client is not running";
    return false;
  }

  std::unique_ptr<AvahiEntryGroup, decltype(&avahi_entry_group_free)> group{
      avahi_entry_group_new(client_.get(), entry_group_callback, this),
      &avahi_entry_group_free};
  if (group == nullptr) {
    BOOST_LOG_TRIVIAL(error)
        << "mdns_server:: add_service() failed avahi_entry_group_new(): "
        << avahi_strerror(avahi_client_errno(client_.get()));
    return false;
  }

  std::stringstream ss;
  ss << node_id_ << " " << name;

  int ret = avahi_entry_group_add_service(
      group.get(), this->config_->get_interface_idx(), AVAHI_PROTO_INET, {},
      ss.str().c_str(), "_rtsp._tcp", nullptr, nullptr,
      this->config_->get_rtsp_port(), nullptr);
  if (ret < 0) {
    BOOST_LOG_TRIVIAL(error)
        << "mdns_server:: add_service() " << ss.str()
        << "failed to add service _rtsp._tcp" << avahi_strerror(ret);
    return false;
  }

  ret = avahi_entry_group_add_service_subtype(
      group.get(), this->config_->get_interface_idx(), AVAHI_PROTO_INET, {},
      ss.str().c_str(), "_rtsp._tcp", nullptr,
      "_ravenna_session._sub._rtsp._tcp");
  if (ret < 0) {
    BOOST_LOG_TRIVIAL(error)
        << "mdns_server:: add_service() " << ss.str()
        << "failed to add subtype _ravenna_session._sub._rtsp._tcp"
        << avahi_strerror(ret);
    return false;
  }

  /* Tell the server to register the service */
  ret = avahi_entry_group_commit(group.get());
  if (ret < 0) {
    BOOST_LOG_TRIVIAL(error)
        << "mdns_server:: add_service() " << ss.str()
        << "failed to commit entry group " << avahi_strerror(ret);
    return false;
  }

  BOOST_LOG_TRIVIAL(info) << "mdns_server:: adding service for " << ss.str();
  groups_.insert(entry_group_bimap_t::value_type(name, group.release()));
#endif
  return true;
}

bool MDNSServer::remove_service(const std::string& name) {
  if (!running_ || !config_->get_mdns_enabled()) {
    return false;
  }

#ifdef _USE_AVAHI_
  AvahiLockGuard avahi_lock(poll_.get());
  if (avahi_client_get_state(client_.get()) != AVAHI_CLIENT_S_RUNNING) {
    BOOST_LOG_TRIVIAL(error)
        << "mdns_server:: remove_sub_service() failed client is not running";
    return false;
  }

  auto it = groups_.left.find(name);
  if (it == groups_.left.end()) {
    return false;
  }
  BOOST_LOG_TRIVIAL(info) << "mdns_server:: removing service _rtsp._tcp for "
                          << name;
  avahi_entry_group_free(it->second);
  groups_.left.erase(name);
#endif

  return true;
}

#ifdef _USE_AVAHI_
void MDNSServer::client_callback(AvahiClient* client,
                                 AvahiClientState state,
                                 void* userdata) {
  MDNSServer& mdns = *(reinterpret_cast<MDNSServer*>(userdata));
  /* Called whenever the client or server state changes */

  switch (state) {
    case AVAHI_CLIENT_FAILURE:
      BOOST_LOG_TRIVIAL(fatal) << "mdns_server:: server connection failure: "
                               << avahi_strerror(avahi_client_errno(client));
      /* TODO reconnect if disconnected */
      avahi_threaded_poll_quit(mdns.poll_.get());
      break;

    case AVAHI_CLIENT_S_RUNNING:
      /* The server has startup successfully and registered its host
       * name on the network, so it's time to create our services */
      if (!mdns.create_services(client)) {
        avahi_threaded_poll_quit(mdns.poll_.get());
      }
      break;

    case AVAHI_CLIENT_S_REGISTERING:
      /* The server records are now being established. This
       * might be caused by a host name change. We need to wait
       * for our own records to register until the host name is
       * properly esatblished. */

    case AVAHI_CLIENT_S_COLLISION:
      /* Let's drop our registered services. When the server is back
       * in AVAHI_SERVER_RUNNING state we will register them
       * again with the new host name. */
      /* TODO */
      break;

    case AVAHI_CLIENT_CONNECTING:
      break;
  }
}
#endif

bool MDNSServer::init() {
  if (running_) {
    return true;
  }

#ifdef _USE_AVAHI_
  /* allocate poll loop object */
  poll_.reset(avahi_threaded_poll_new());
  if (poll_ == nullptr) {
    BOOST_LOG_TRIVIAL(fatal)
        << "mdns_server:: failed to create threaded poll object";
    return false;
  }

  /* allocate a new client */
  int error;
  client_.reset(avahi_client_new(avahi_threaded_poll_get(poll_.get()),
                                 AVAHI_CLIENT_NO_FAIL, client_callback, this,
                                 &error));
  if (client_ == nullptr) {
    BOOST_LOG_TRIVIAL(fatal)
        << "mdns_server:: failed to create client: " << avahi_strerror(error);
    return false;
  }

  (void)avahi_threaded_poll_start(poll_.get());
#endif

  session_manager_->add_source_observer(
      SessionManager::ObserverType::add_source,
      std::bind(&MDNSServer::add_service, this, std::placeholders::_2,
                std::placeholders::_3));

  session_manager_->add_source_observer(
      SessionManager::ObserverType::remove_source,
      std::bind(&MDNSServer::remove_service, this, std::placeholders::_2));

  running_ = true;
  return true;
}

bool MDNSServer::terminate() {
  if (running_) {
    running_ = false;
#ifdef _USE_AVAHI_
    /* remove base services */
    groups_.left.erase(node_id_);
    avahi_threaded_poll_stop(poll_.get());
#endif
  }
  return true;
}
