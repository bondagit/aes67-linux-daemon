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

#include <boost/asio.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <exception>
#include <iostream>
#include <sstream>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "config.hpp"
#include "interface.hpp"
#include "json.hpp"

using namespace boost::asio;

std::shared_ptr<Config> Config::parse(const std::string& filename) {
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
  if (config.tic_frame_size_at_1fs_ == 0 ||
      config.tic_frame_size_at_1fs_ > 8192)
    config.tic_frame_size_at_1fs_ = 192;
  if (config.max_tic_frame_size_ == 0 || config.max_tic_frame_size_ > 8192)
    config.max_tic_frame_size_ = 1024;
  if (config.sample_rate_ == 0)
    config.sample_rate_ = 44100;
  if (ip::address_v4::from_string(config.rtp_mcast_base_.c_str()).to_ulong() ==
      INADDR_NONE)
    config.rtp_mcast_base_ = "239.1.0.1";

  auto [mac_addr, mac_str] = get_interface_mac(config.interface_name_);
  if (mac_str.empty()) {
    std::cerr << "Cannot retrieve MAC address for interface "
              << config.interface_name_ << std::endl;
    return nullptr;
  }
  config.mac_addr_ = mac_addr;
  config.mac_str_ = mac_str;

  auto [ip_addr, ip_str] = get_interface_ip(config.interface_name_);
  if (ip_str.empty()) {
    std::cerr << "Cannot retrieve IPv4 address for interface "
              << config.interface_name_ << std::endl;
    return nullptr;
  }
  config.ip_addr_ = ip_addr;
  config.ip_str_ = ip_str;
  config.config_filename_ = filename;
  config.need_restart_ = false;

  return std::make_shared<Config>(config);
}

bool Config::save(const Config& config, bool need_restart) {
  std::ofstream js(config_filename_);
  if (!js) {
    BOOST_LOG_TRIVIAL(fatal)
        << "Config:: cannot save to file " << config_filename_;
    return false;
  }
  js << config_to_json(config);
  BOOST_LOG_TRIVIAL(info) << "Config:: file saved";
  need_restart_ = need_restart;
  return true;
}
