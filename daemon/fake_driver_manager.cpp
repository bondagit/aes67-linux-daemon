//
//  fake_driver_manager.cpp
//
//  Copyright (c) 2019 2022 Andrea Bondavalli. All rights reserved.
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

#include "log.hpp"
#include "fake_driver_manager.hpp"

static const std::vector<std::string> ptp_status_str = {"unlocked", "locking",
                                                        "locked"};

std::shared_ptr<DriverManager> DriverManager::create() {
  // no need to be thread-safe here
  static std::weak_ptr<DriverManager> instance;
  if (auto ptr = instance.lock()) {
    return ptr;
  }
  auto ptr = std::shared_ptr<DriverManager>(new DriverManager());
  instance = ptr;
  return ptr;
}

bool DriverManager::init(const Config& config) {
  sample_rate_ = config.get_sample_rate();

  TPTPConfig ptp_config;
  ptp_config.ui8Domain = config.get_ptp_domain();
  ptp_config.ui8DSCP = config.get_ptp_dscp();

  if (hello())
    return false;

  bool res(false);
  if (config.get_driver_restart()) {
    res = start() || reset() ||
          set_interface_name(config.get_interface_name()) ||
          set_ptp_config(ptp_config) ||
          set_tic_frame_size_at_1fs(config.get_tic_frame_size_at_1fs()) ||
          set_playout_delay(config.get_playout_delay()) ||
          set_max_tic_frame_size(config.get_max_tic_frame_size());
  }

  return !res;
}

bool DriverManager::terminate(const Config& config) {
  if (config.get_driver_restart()) {
    stop();
  }
  bye();
  return true;
}

std::error_code DriverManager::hello() {
  return std::error_code{};
}

std::error_code DriverManager::bye() {
  return std::error_code{};
}

std::error_code DriverManager::start() {
  return std::error_code{};
}

std::error_code DriverManager::stop() {
  return std::error_code{};
}

std::error_code DriverManager::reset() {
  return std::error_code{};
}

std::error_code DriverManager::set_ptp_config(const TPTPConfig& config) {
  BOOST_LOG_TRIVIAL(info) << "fake_driver_manager:: setting PTP Domain "
                          << (int)config.ui8Domain << " DSCP "
                          << (int)config.ui8DSCP;
  ptp_config_ = config;
  return std::error_code{};
}

std::error_code DriverManager::get_ptp_config(TPTPConfig& config) {
  config = ptp_config_;
  BOOST_LOG_TRIVIAL(debug) << "fake_driver_manager:: PTP Domain "
                           << (int)config.ui8Domain << " DSCP "
                           << (int)config.ui8DSCP;
  return std::error_code{};
}

std::error_code DriverManager::get_ptp_status(TPTPStatus& status) {
  status.nPTPLockStatus = PTPLS_UNLOCKED;
  status.ui64GMID = 0xABABABABABABABAB;
  status.i32Jitter = 0;
  BOOST_LOG_TRIVIAL(debug) << "fake_driver_manager:: PTP Status "
                           << ptp_status_str[status.nPTPLockStatus] << " GMID "
                           << status.ui64GMID << " Jitter " << status.i32Jitter;
  return std::error_code{};
}

std::error_code DriverManager::set_interface_name(const std::string& ifname) {
  return std::error_code{};
}

std::error_code DriverManager::add_rtp_stream(
    const TRTP_stream_info& stream_info,
    uint64_t& stream_handle) {
  stream_handle = ++g_handle;
  handles_.insert(stream_handle);
  BOOST_LOG_TRIVIAL(info) << "fake_driver_manager:: add RTP stream success handle "
                          << stream_handle;
  return std::error_code{};
}

std::error_code DriverManager::get_rtp_stream_status(
    uint64_t stream_handle,
    TRTP_stream_status& stream_status) {
  stream_status.u.flags = 0x0;
  stream_status.sink_min_time = 0;
  if (handles_.find(stream_handle) == handles_.end()) {
    return DriverErrc::invalid_value;
  }
  return std::error_code{};
}

std::error_code DriverManager::remove_rtp_stream(uint64_t stream_handle) {
  if (handles_.find(stream_handle) == handles_.end()) {
    return DriverErrc::invalid_value;
  }
  handles_.erase(stream_handle);
  return std::error_code{};
}

std::error_code DriverManager::ping() {
  return std::error_code{};
}

std::error_code DriverManager::set_sample_rate(uint32_t sample_rate) {
  sample_rate_ = sample_rate;
  return std::error_code{};
}

std::error_code DriverManager::set_tic_frame_size_at_1fs(uint64_t frame_size) {
  frame_size_ = frame_size;
  return std::error_code{};
}

std::error_code DriverManager::set_max_tic_frame_size(uint64_t frame_size) {
  max_frame_size_ = frame_size;
  return std::error_code{};
}

std::error_code DriverManager::set_playout_delay(int32_t delay) {
  delay_ = delay;
  return std::error_code{};
}

std::error_code DriverManager::get_sample_rate(uint32_t& sample_rate) {
  sample_rate = sample_rate_;
  BOOST_LOG_TRIVIAL(info) << "fake_driver_manager:: sample rate " << sample_rate;
  return std::error_code{};
}

std::error_code DriverManager::get_number_of_inputs(int32_t& inputs) {
  inputs = 0;
  return std::error_code{};
}

std::error_code DriverManager::get_number_of_outputs(int32_t& outputs) {
  outputs = 0;
  return std::error_code{};
}
