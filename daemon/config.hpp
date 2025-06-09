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
  bool save(const Config& config);
  /* build config from json file */
  static std::shared_ptr<Config> parse(const std::string& filename,
                                       bool driver_restart);

  /* attributes retrieved from config json */
  const std::string& get_http_addr_str() const { return http_addr_str_; };
  uint16_t get_http_port() const { return http_port_; };
  uint16_t get_rtsp_port() const { return rtsp_port_; };
  const std::string& get_http_base_dir() const { return http_base_dir_; };
  uint8_t get_streamer_files_num() const { return streamer_files_num_; };
  uint16_t get_streamer_file_duration() const {
    return streamer_file_duration_;
  };
  uint8_t get_streamer_player_buffer_files_num() const {
    return streamer_player_buffer_files_num_;
  };
  uint8_t get_streamer_channels() const { return streamer_channels_; };
  bool get_streamer_enabled() const;
  bool get_transcriber_enabled() const;
  uint8_t get_transcriber_channels() const { return transcriber_channels_; }
  uint8_t get_transcriber_files_num() const { return transcriber_files_num_; }
  uint16_t get_transcriber_file_duration() const {
    return transcriber_file_duration_;
  }
  const std::string& get_transcriber_model() const {
    return transcriber_model_;
  }
  const std::string& get_transcriber_language() const {
    return transcriber_language_;
  }
  const std::string& get_transcriber_openvino_device() const {
    return transcriber_openvino_device_;
  }
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
  const std::string& get_custom_node_id() const { return custom_node_id_; };
  std::string get_node_id() const;
  bool get_auto_sinks_update() const { return auto_sinks_update_; };

  /* attributes set during init */
  const std::array<uint8_t, 6>& get_mac_addr() const { return mac_addr_; };
  const std::string& get_mac_addr_str() const { return mac_str_; };
  uint32_t get_ip_addr() const { return ip_addr_; };
  const std::string& get_ip_addr_str() const { return ip_str_; };
  bool get_daemon_restart() const { return daemon_restart_; };
  bool get_driver_restart() const { return driver_restart_; };
  bool get_mdns_enabled() const;
  int get_interface_idx() const { return interface_idx_; };
  const std::string& get_ptp_status_script() const {
    return ptp_status_script_;
  }

  void set_http_addr_str(std::string_view http_addr_str) {
    http_addr_str_ = http_addr_str;
  };
  void set_http_port(uint16_t http_port) { http_port_ = http_port; };
  void set_rtsp_port(uint16_t rtsp_port) { rtsp_port_ = rtsp_port; };
  void set_http_base_dir(std::string_view http_base_dir) {
    http_base_dir_ = http_base_dir;
  };
  void set_streamer_channels(uint8_t streamer_channels) {
    streamer_channels_ = streamer_channels;
  };
  void set_streamer_files_num(uint8_t streamer_files_num) {
    streamer_files_num_ = streamer_files_num;
  };
  void set_streamer_file_duration(uint16_t streamer_file_duration) {
    streamer_file_duration_ = streamer_file_duration;
  };
  void set_streamer_player_buffer_files_num(
      uint8_t streamer_player_buffer_files_num) {
    streamer_player_buffer_files_num_ = streamer_player_buffer_files_num;
  };
  void set_streamer_enabled(uint8_t streamer_enabled) {
    streamer_enabled_ = streamer_enabled;
  };
  void set_transcriber_enabled(uint8_t transcriber_enabled) {
    transcriber_enabled_ = transcriber_enabled;
  };
  void set_transcriber_channels(uint8_t transcriber_channels) {
    transcriber_channels_ = transcriber_channels;
  }
  void set_transcriber_files_num(uint8_t transcriber_files_num) {
    transcriber_files_num_ = transcriber_files_num;
  }
  void set_transcriber_file_duration(uint8_t transcriber_file_duration) {
    transcriber_file_duration_ = transcriber_file_duration;
  }
  void set_transcriber_model(const std::string& transcriber_model) {
    transcriber_model_ = transcriber_model;
  }
  void set_transcriber_language(const std::string& transcriber_language) {
    transcriber_language_ = transcriber_language;
  }
  void set_transcriber_openvino_device(
      const std::string& transcriber_openvino_device) {
    transcriber_openvino_device_ = transcriber_openvino_device;
  }
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
  void set_rtp_mcast_base(std::string_view rtp_mcast_base) {
    rtp_mcast_base_ = rtp_mcast_base;
  };
  void set_sap_mcast_addr(std::string_view sap_mcast_addr) {
    sap_mcast_addr_ = sap_mcast_addr;
  };
  void set_rtp_port(uint16_t rtp_port) { rtp_port_ = rtp_port; };
  void set_ptp_domain(uint8_t ptp_domain) { ptp_domain_ = ptp_domain; };
  void set_ptp_dscp(uint8_t ptp_dscp) { ptp_dscp_ = ptp_dscp; };
  void set_sap_interval(uint16_t sap_interval) {
    sap_interval_ = sap_interval;
  };
  void set_syslog_proto(std::string_view syslog_proto) {
    syslog_proto_ = syslog_proto;
  };
  void set_syslog_server(std::string_view syslog_server) {
    syslog_server_ = syslog_server;
  };
  void set_status_file(std::string_view status_file) {
    status_file_ = status_file;
  };
  void set_interface_name(std::string_view interface_name) {
    interface_name_ = interface_name;
  };
  void set_ip_addr_str(std::string_view ip_str) { ip_str_ = ip_str; };
  void set_ip_addr(uint32_t ip_addr) { ip_addr_ = ip_addr; };
  void set_mac_addr_str(std::string_view mac_str) { mac_str_ = mac_str; };
  void set_mac_addr(const std::array<uint8_t, 6>& mac_addr) {
    mac_addr_ = mac_addr;
  };
  void set_mdns_enabled(bool enabled) { mdns_enabled_ = enabled; };
  void set_interface_idx(int index) { interface_idx_ = index; };
  void set_ptp_status_script(std::string_view script) {
    ptp_status_script_ = script;
  };
  void set_custom_node_id(std::string_view node_id) {
    custom_node_id_ = node_id;
  };
  void set_auto_sinks_update(bool auto_sinks_update) {
    auto_sinks_update_ = auto_sinks_update;
  };
  void set_driver_restart(bool restart) { driver_restart_ = restart; }

  friend bool operator!=(const Config& lhs, const Config& rhs) {
    return lhs.get_http_addr_str() != rhs.get_http_addr_str() ||
           lhs.get_http_port() != rhs.get_http_port() ||
           lhs.get_rtsp_port() != rhs.get_rtsp_port() ||
           lhs.get_http_base_dir() != rhs.get_http_base_dir() ||
           lhs.get_streamer_channels() != rhs.get_streamer_channels() ||
           lhs.get_streamer_files_num() != rhs.get_streamer_files_num() ||
           lhs.get_streamer_file_duration() !=
               rhs.get_streamer_file_duration() ||
           lhs.get_streamer_player_buffer_files_num() !=
               rhs.get_streamer_player_buffer_files_num() ||
           lhs.get_streamer_enabled() != rhs.get_streamer_enabled() ||
           lhs.get_transcriber_channels() != rhs.get_transcriber_channels() ||
           lhs.get_transcriber_files_num() != rhs.get_transcriber_files_num() ||
           lhs.get_transcriber_file_duration() !=
               rhs.get_transcriber_file_duration() ||
           lhs.get_transcriber_model() != rhs.get_transcriber_model() ||
           lhs.get_transcriber_language() != rhs.get_transcriber_language() ||
           lhs.get_transcriber_openvino_device() !=
               rhs.get_transcriber_openvino_device() ||
           lhs.get_transcriber_enabled() != rhs.get_transcriber_enabled() ||
           lhs.get_log_severity() != rhs.get_log_severity() ||
           lhs.get_playout_delay() != rhs.get_playout_delay() ||
           lhs.get_tic_frame_size_at_1fs() != rhs.get_tic_frame_size_at_1fs() ||
           lhs.get_max_tic_frame_size() != rhs.get_max_tic_frame_size() ||
           lhs.get_sample_rate() != rhs.get_sample_rate() ||
           lhs.get_rtp_mcast_base() != rhs.get_rtp_mcast_base() ||
           lhs.get_sap_mcast_addr() != rhs.get_sap_mcast_addr() ||
           lhs.get_rtp_port() != rhs.get_rtp_port() ||
           lhs.get_ptp_domain() != rhs.get_ptp_domain() ||
           lhs.get_ptp_dscp() != rhs.get_ptp_dscp() ||
           lhs.get_sap_interval() != rhs.get_sap_interval() ||
           lhs.get_syslog_proto() != rhs.get_syslog_proto() ||
           lhs.get_syslog_server() != rhs.get_syslog_server() ||
           lhs.get_status_file() != rhs.get_status_file() ||
           lhs.get_interface_name() != rhs.get_interface_name() ||
           lhs.get_mdns_enabled() != rhs.get_mdns_enabled() ||
           lhs.get_auto_sinks_update() != rhs.get_auto_sinks_update() ||
           lhs.get_custom_node_id() != rhs.get_custom_node_id();
  };
  friend bool operator==(const Config& lhs, const Config& rhs) {
    return !(lhs != rhs);
  };

 private:
  /* from json */
  std::string http_addr_str_{""};
  uint16_t http_port_{8080};
  uint16_t rtsp_port_{8854};
  std::string http_base_dir_{"../webui/dist"};
  uint8_t streamer_channels_{8};
  uint8_t streamer_files_num_{8};
  uint16_t streamer_file_duration_{1};
  uint8_t streamer_player_buffer_files_num_{1};
  bool streamer_enabled_{false};
  bool transcriber_enabled_{false};
  uint8_t transcriber_channels_{4};
  uint8_t transcriber_files_num_{4};
  uint16_t transcriber_file_duration_{5};
  std::string transcriber_model_{
      "../3rdparty/whisper.cpp/models/ggml-base.en.bin"};
  std::string transcriber_language_{"en"};
  std::string transcriber_openvino_device_{"CPU"};
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
  std::string ptp_status_script_;
  std::string custom_node_id_;
  std::string node_id_;
  bool auto_sinks_update_{true};

  /* set during init */
  std::array<uint8_t, 6> mac_addr_{0, 0, 0, 0, 0, 0};
  std::string mac_str_;
  uint32_t ip_addr_{0};
  std::string ip_str_;
  int interface_idx_;
  std::string config_filename_;

  /* reconfig needs driver restart */
  bool driver_restart_{true};
  /* reconfig needs daemon restart */
  bool daemon_restart_{false};
};

#endif
