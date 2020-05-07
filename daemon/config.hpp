//
//  config.hpp
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

#ifndef _CONFIG_HPP_
#define _CONFIG_HPP_

#include <cstdint>
#include <memory>
#include <string>

class Config {
 public:
  /* save new config to json file */
  bool save(const Config& config, bool need_restart = true);
  /* build config from json file */
  static std::shared_ptr<Config> parse(const std::string& filename);

  /* attributes retrieved from config json */
  uint16_t get_http_port() const { return http_port_; };
  uint16_t get_rtsp_port() const { return rtsp_port_; };
  const std::string get_http_base_dir() const { return http_base_dir_; };
  int get_log_severity() const { return log_severity_; };
  uint32_t get_playout_delay() const { return playout_delay_; };
  uint32_t get_tic_frame_size_at_1fs() const { return tic_frame_size_at_1fs_; };
  uint32_t get_max_tic_frame_size() const { return max_tic_frame_size_; };
  uint32_t get_sample_rate() const { return sample_rate_; };
  const std::string& get_rtp_mcast_base() const { return rtp_mcast_base_; };
  const std::string& get_sap_mcast_addr() const { return sap_mcast_addr_; };
  uint16_t get_rtp_port() const { return rtp_port_; };
  uint8_t get_ptp_domain() const { return ptp_domain_; };
  uint8_t get_ptp_dscp() const { return ptp_dscp_; };
  uint16_t get_sap_interval() const { return sap_interval_; };
  const std::string& get_syslog_proto() const { return syslog_proto_; };
  const std::string& get_syslog_server() const { return syslog_server_; };
  const std::string& get_status_file() const { return status_file_; };
  const std::string& get_interface_name() const { return interface_name_; };
  const std::string& get_config_filename() const { return config_filename_; };

  /* attributes set during init */
  const std::array<uint8_t, 6>& get_mac_addr() const { return mac_addr_; };
  const std::string& get_mac_addr_str() const { return mac_str_; };
  uint32_t get_ip_addr() const { return ip_addr_; };
  const std::string& get_ip_addr_str() const { return ip_str_; };
  bool get_need_restart() const { return need_restart_; };
  bool get_mdns_enabled() const { return mdns_enabled_; };
  int get_interface_idx() { return interface_idx_; };

  void set_http_port(uint16_t http_port) { http_port_ = http_port; };
  void set_rtsp_port(uint16_t rtsp_port) { rtsp_port_ = rtsp_port; };
  void set_http_base_dir(const std::string& http_base_dir) { http_base_dir_ = http_base_dir; };
  void set_log_severity(int log_severity) { log_severity_ = log_severity; };
  void set_playout_delay(uint32_t playout_delay) {
    playout_delay_ = playout_delay;
  };
  void set_tic_frame_size_at_1fs(uint32_t tic_frame_size_at_1fs) {
    tic_frame_size_at_1fs_ = tic_frame_size_at_1fs;
  };
  void set_max_tic_frame_size(uint32_t max_tic_frame_size) {
    max_tic_frame_size_ = max_tic_frame_size;
  };
  void set_sample_rate(uint32_t sample_rate) { sample_rate_ = sample_rate; };
  void set_rtp_mcast_base(const std::string& rtp_mcast_base) {
    rtp_mcast_base_ = rtp_mcast_base;
  };
  void set_sap_mcast_addr(const std::string& sap_mcast_addr) {
    sap_mcast_addr_ = sap_mcast_addr;
  };
  void set_rtp_port(uint16_t rtp_port) { rtp_port_ = rtp_port; };
  void set_ptp_domain(uint8_t ptp_domain) { ptp_domain_ = ptp_domain; };
  void set_ptp_dscp(uint8_t ptp_dscp) { ptp_dscp_ = ptp_dscp; };
  void set_sap_interval(uint16_t sap_interval) {
    sap_interval_ = sap_interval;
  };
  void set_syslog_proto(const std::string& syslog_proto) {
    syslog_proto_ = syslog_proto;
  };
  void set_syslog_server(const std::string& syslog_server) {
    syslog_server_ = syslog_server;
  };
  void set_status_file(const std::string& status_file) {
    status_file_ = status_file;
  };
  void set_interface_name(const std::string& interface_name) {
    interface_name_ = interface_name;
  };
  void set_ip_addr_str(const std::string& ip_str) { ip_str_ = ip_str; };
  void set_ip_addr(uint32_t ip_addr) { ip_addr_ = ip_addr; };
  void set_mac_addr_str(const std::string& mac_str) { mac_str_ = mac_str; };
  void set_mac_addr(const std::array<uint8_t, 6>& mac_addr) {
    mac_addr_ = mac_addr;
  };
  void set_mdns_enabled(bool enabled) {
    mdns_enabled_ = enabled;
  };
  void set_interface_idx(int index) { interface_idx_ = index; };

 private:
  /* from json */
  uint16_t http_port_{8080};
  uint16_t rtsp_port_{8854};
  std::string http_base_dir_{"../webui/build"};
  int log_severity_{2};
  uint32_t playout_delay_{0};
  uint32_t tic_frame_size_at_1fs_{48};
  uint32_t max_tic_frame_size_{1024};
  uint32_t sample_rate_{48000};
  std::string rtp_mcast_base_{"239.1.0.1"};
  std::string sap_mcast_addr_{"224.2.127.254"};
  uint16_t rtp_port_{5004};
  uint8_t ptp_domain_{0};
  uint8_t ptp_dscp_{46};
  uint16_t sap_interval_{300};
  std::string syslog_proto_{""};
  std::string syslog_server_{""};
  std::string status_file_{"./status.json"};
  std::string interface_name_{"eth0"};
  bool mdns_enabled_{true};

  /* set during init */
  std::array<uint8_t, 6> mac_addr_{0, 0, 0, 0, 0, 0};
  std::string mac_str_;
  uint32_t ip_addr_{0};
  std::string ip_str_;
  int interface_idx_;
  std::string config_filename_;

  /* reconfig needs daemon restart */
  bool need_restart_{false};
};

#endif
