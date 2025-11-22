//
//  config.cpp
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

#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <boost/asio.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>

#include "interface.hpp"
#include "json.hpp"
#include "config.hpp"
#include "utils.hpp"

using namespace boost::asio;

std::shared_ptr<Config> Config::parse(const std::string& filename,
                                      bool driver_restart) {
  Config config;

  std::ifstream jsonstream(filename);
  if (!jsonstream) {
    std::cerr << "Fatal: error opening config file " << filename << std::endl;
    return nullptr;
  }

  try {
    config = json_to_config(jsonstream);
  } catch (std::exception const& e) {
    std::cerr << "Configuration file " << filename << " : " << e.what()
              << std::endl;
    return nullptr;
  }

  if (config.log_severity_ < 0 || config.log_severity_ > 5)
    config.log_severity_ = 2;
  if (config.playout_delay_ > 4000)
    config.playout_delay_ = 4000;
  if (config.tic_frame_size_at_1fs_ == 0 || config.tic_frame_size_at_1fs_ > 192)
    config.tic_frame_size_at_1fs_ = 192;
  if (config.max_tic_frame_size_ < config.tic_frame_size_at_1fs_ ||
      config.max_tic_frame_size_ > 1024)
    config.max_tic_frame_size_ = 1024;
  if (config.sample_rate_ == 0)
    config.sample_rate_ = 48000;
  if (config.streamer_channels_ < 2 || config.streamer_channels_ > 16)
    config.streamer_channels_ = 8;
  if (config.streamer_file_duration_ < 1 || config.streamer_file_duration_ > 4)
    config.streamer_file_duration_ = 1;
  if (config.streamer_files_num_ < 4 || config.streamer_files_num_ > 16)
    config.streamer_files_num_ = 8;
  if (config.streamer_player_buffer_files_num_ < 1 ||
      config.streamer_player_buffer_files_num_ > 2)
    config.streamer_player_buffer_files_num_ = 1;

  boost::system::error_code ec;
#if BOOST_VERSION < 108700
  ip::address_v4::from_string(config.rtp_mcast_base_.c_str(), ec);
#else
  ip::make_address(config.rtp_mcast_base_.c_str(), ec);
#endif
  if (ec) {
    config.rtp_mcast_base_ = "239.1.0.1";
  }
#if BOOST_VERSION < 108700
  ip::address_v4::from_string(config.sap_mcast_addr_.c_str(), ec);
#else
  ip::make_address(config.sap_mcast_addr_.c_str(), ec);
#endif
  if (ec) {
    config.sap_mcast_addr_ = "224.2.127.254";
  }
  if (config.ptp_domain_ > 127)
    config.ptp_domain_ = 0;

  auto [mac_addr, mac_str] = get_interface_mac(config.interface_name_);
  if (mac_str.empty()) {
    std::cerr << "Cannot retrieve MAC address for interface "
              << config.interface_name_ << std::endl;
    return nullptr;
  }
  config.mac_addr_ = mac_addr;
  config.mac_str_ = mac_str;

  auto interface_idx = get_interface_index(config.interface_name_);
  if (interface_idx < 0) {
    std::cerr << "Cannot retrieve index for interface "
              << config.interface_name_ << std::endl;
  } else {
    config.interface_idx_ = interface_idx;
  }

  auto [ip_addr, ip_str, is_new] =
      get_new_interface_ip(config.interface_name_, config.ip_str_);
  if (ip_str.empty()) {
    std::cerr << "Cannot retrieve IPv4 address for interface "
              << config.interface_name_ << std::endl;
  }
  config.ip_addr_ = ip_addr;
  config.ip_str_ = ip_str;

  config.config_filename_ = filename;
  config.daemon_restart_ = false;
  config.driver_restart_ = driver_restart;

  return std::make_shared<Config>(config);
}

bool Config::save(const Config& config) {
  if (*this != config) {
    std::ofstream js(config_filename_);
    if (!js) {
      BOOST_LOG_TRIVIAL(fatal)
          << "Config:: cannot save to file " << config_filename_;
      return false;
    }
    js << config_to_json(config);

    driver_restart_ =
        get_tic_frame_size_at_1fs() != config.get_tic_frame_size_at_1fs() ||
        get_max_tic_frame_size() != config.get_max_tic_frame_size() ||
        get_interface_name() != config.get_interface_name();

    daemon_restart_ =
        driver_restart_ || get_http_port() != config.get_http_port() ||
        get_rtsp_port() != config.get_rtsp_port() ||
        get_http_base_dir() != config.get_http_base_dir() ||
        get_rtp_mcast_base() != config.get_rtp_mcast_base() ||
        get_sap_mcast_addr() != config.get_sap_mcast_addr() ||
        get_rtp_port() != config.get_rtp_port() ||
        get_status_file() != config.get_status_file() ||
        get_mdns_enabled() != config.get_mdns_enabled() ||
        get_custom_node_id() != config.get_custom_node_id() ||
        get_streamer_channels() != config.get_streamer_channels() ||
        get_streamer_file_duration() != config.get_streamer_file_duration() ||
        get_streamer_files_num() != config.get_streamer_files_num() ||
        get_streamer_player_buffer_files_num() !=
            config.get_streamer_player_buffer_files_num() ||
        get_streamer_enabled() != config.get_streamer_enabled();

    if (!daemon_restart_)
      *this = config;

    BOOST_LOG_TRIVIAL(info) << "Config:: file saved";
  } else {
    BOOST_LOG_TRIVIAL(info) << "Config:: unchanged";
  }
  return true;
}

std::string Config::get_node_id() const {
  if (custom_node_id_.empty()) {
    return get_host_node_id(get_ip_addr());
  } else {
    return custom_node_id_;
  }
}

bool Config::get_streamer_enabled() const {
#ifndef _USE_STREAMER_
  return false;
#endif
  return streamer_enabled_;
}

bool Config::get_mdns_enabled() const {
#ifndef _USE_AVAHI_
  return false;
#endif
  return mdns_enabled_;
}
