//
//  driver_manager.cpp
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

#include <thread>

#include "log.hpp"
#include "driver_manager.hpp"

static const std::vector<std::string> alsa_msg_str = {"Start",
                                                      "Stop",
                                                      "Reset",
                                                      "StartIO",
                                                      "StopIO",
                                                      "SetSampleRate",
                                                      "GetSampleRate",
                                                      "GetAudioMode",
                                                      "SetDSDAudioMode",
                                                      "SetTICFrameSizeAt1FS",
                                                      "SetMaxTICFrameSize",
                                                      "SetNumberOfInputs",
                                                      "SetNumberOfOutputs",
                                                      "GetNumberOfInputs",
                                                      "GetNumberOfOutputs",
                                                      "SetInterfaceName",
                                                      "Add_RTPStream",
                                                      "Remove_RTPStream",
                                                      "Update_RTPStream_Name",
                                                      "GetPTPInfo",
                                                      "Hello",
                                                      "Bye",
                                                      "Ping",
                                                      "SetMasterOutputVolume",
                                                      "SetMasterOutputSwitch",
                                                      "GetMasterOutputVolume",
                                                      "GetMasterOutputSwitch",
                                                      "SetPlayoutDelay",
                                                      "SetCaptureDelay",
                                                      "GetRTPStreamStatus",
                                                      "SetPTPConfig",
                                                      "GetPTPConfig",
                                                      "GetPTPStatus"};

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
  if (!DriverHandler::init(config)) {
    return false;
  }

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
  return DriverHandler::terminate(config);
}

std::error_code DriverManager::hello() {
  this->send_command(MT_ALSA_Msg_Hello, 0, nullptr);
  return retcode_;
}

std::error_code DriverManager::bye() {
  this->send_command(MT_ALSA_Msg_Bye, 0, nullptr);
  return retcode_;
}

std::error_code DriverManager::start() {
  this->send_command(MT_ALSA_Msg_Start, 0, nullptr);
  return retcode_;
}

std::error_code DriverManager::stop() {
  this->send_command(MT_ALSA_Msg_Stop, 0, nullptr);
  return retcode_;
}

std::error_code DriverManager::reset() {
  this->send_command(MT_ALSA_Msg_Reset, 0, nullptr);
  return retcode_;
}

std::error_code DriverManager::set_ptp_config(const TPTPConfig& config) {
  BOOST_LOG_TRIVIAL(info) << "driver_manager:: setting PTP Domain "
                          << (int)config.ui8Domain << " DSCP "
                          << (int)config.ui8DSCP;
  this->send_command(MT_ALSA_Msg_SetPTPConfig, sizeof(TPTPConfig),
                     reinterpret_cast<const uint8_t*>(&config));
  return retcode_;
}

std::error_code DriverManager::get_ptp_config(TPTPConfig& config) {
  this->send_command(MT_ALSA_Msg_GetPTPConfig);
  if (!retcode_) {
    memcpy(&config, recv_data_, sizeof(TPTPConfig));
    BOOST_LOG_TRIVIAL(debug)
        << "driver_manager:: PTP Domain " << (int)config.ui8Domain << " DSCP "
        << (int)config.ui8DSCP;
  }
  return retcode_;
}

std::error_code DriverManager::get_ptp_status(TPTPStatus& status) {
  this->send_command(MT_ALSA_Msg_GetPTPStatus);
  if (!retcode_) {
    memcpy(&status, recv_data_, sizeof(TPTPStatus));
    BOOST_LOG_TRIVIAL(debug)
        << "driver_manager:: PTP Status "
        << ptp_status_str[status.nPTPLockStatus] << " GMID " << status.ui64GMID
        << " Jitter " << status.i32Jitter;
  }
  return retcode_;
}

std::error_code DriverManager::set_interface_name(const std::string& ifname) {
  BOOST_LOG_TRIVIAL(info) << "driver_manager:: setting interface " << ifname;
  this->send_command(MT_ALSA_Msg_SetInterfaceName, ifname.length() + 1,
                     reinterpret_cast<const uint8_t*>(ifname.c_str()));
  return retcode_;
}

std::error_code DriverManager::add_rtp_stream(
    const TRTP_stream_info& stream_info,
    uint64_t& stream_handle) {
  this->send_command(MT_ALSA_Msg_Add_RTPStream, sizeof(TRTP_stream_info),
                     reinterpret_cast<const uint8_t*>(&stream_info));
  if (!retcode_) {
    memcpy(&stream_handle, recv_data_, sizeof(stream_handle));
    BOOST_LOG_TRIVIAL(info)
        << "driver_manager:: add RTP stream success handle " << stream_handle;
  }
  return retcode_;
}

std::error_code DriverManager::get_rtp_stream_status(
    uint64_t stream_handle,
    TRTP_stream_status& stream_status) {
  this->send_command(MT_ALSA_Msg_GetRTPStreamStatus, sizeof(uint64_t),
                     reinterpret_cast<const uint8_t*>(&stream_handle));
  if (!retcode_) {
    memcpy(&stream_status, recv_data_, sizeof(stream_status));
  }
  return retcode_;
}

std::error_code DriverManager::remove_rtp_stream(uint64_t stream_handle) {
  this->send_command(MT_ALSA_Msg_Remove_RTPStream, sizeof(uint64_t),
                     reinterpret_cast<const uint8_t*>(&stream_handle));
  return retcode_;
}

std::error_code DriverManager::ping() {
  this->send_command(MT_ALSA_Msg_Ping);
  return retcode_;
}

std::error_code DriverManager::set_sample_rate(uint32_t sample_rate) {
  this->send_command(MT_ALSA_Msg_SetSampleRate, sizeof(uint32_t),
                     reinterpret_cast<const uint8_t*>(&sample_rate));
  return retcode_;
}

std::error_code DriverManager::set_tic_frame_size_at_1fs(uint64_t frame_size) {
  this->send_command(MT_ALSA_Msg_SetTICFrameSizeAt1FS, sizeof(uint64_t),
                     reinterpret_cast<const uint8_t*>(&frame_size));
  return retcode_;
}

std::error_code DriverManager::set_max_tic_frame_size(uint64_t frame_size) {
  this->send_command(MT_ALSA_Msg_SetMaxTICFrameSize, sizeof(uint64_t),
                     reinterpret_cast<const uint8_t*>(&frame_size));
  return retcode_;
}

std::error_code DriverManager::set_playout_delay(int32_t delay) {
  this->send_command(MT_ALSA_Msg_SetPlayoutDelay, sizeof(uint32_t),
                     reinterpret_cast<const uint8_t*>(&delay));
  return retcode_;
}

std::error_code DriverManager::get_sample_rate(uint32_t& sample_rate) {
  this->send_command(MT_ALSA_Msg_GetSampleRate);
  if (!retcode_) {
    memcpy(&sample_rate, recv_data_, sizeof(uint32_t));
    BOOST_LOG_TRIVIAL(info) << "driver_manager:: sample rate " << sample_rate;
  }
  return retcode_;
}

std::error_code DriverManager::get_number_of_inputs(int32_t& inputs) {
  this->send_command(MT_ALSA_Msg_GetNumberOfInputs);
  if (!retcode_) {
    memcpy(&inputs, recv_data_, sizeof(uint32_t));
    BOOST_LOG_TRIVIAL(info) << "driver_manager:: number of inputs " << inputs;
  }
  return retcode_;
}

std::error_code DriverManager::get_number_of_outputs(int32_t& outputs) {
  this->send_command(MT_ALSA_Msg_GetNumberOfOutputs);
  if (!retcode_) {
    memcpy(&outputs, recv_data_, sizeof(uint32_t));
    BOOST_LOG_TRIVIAL(info) << "driver_manager:: number of outputs " << outputs;
  }
  return retcode_;
}

void DriverManager::on_command_done(enum MT_ALSA_msg_id id,
                                    size_t size,
                                    const uint8_t* data) {
  BOOST_LOG_TRIVIAL(info) << "driver_manager:: cmd " << alsa_msg_str[id]
                          << " done data len " << size;
  memcpy(recv_data_, data, size);
  retcode_ = std::error_code{};
}

void DriverManager::on_command_error(enum MT_ALSA_msg_id id,
                                     std::error_code error) {
  BOOST_LOG_TRIVIAL(error) << "driver_manager:: cmd " << alsa_msg_str[id]
                           << " failed with error " << error.message();
  retcode_ = error;
}

void DriverManager::on_event(enum MT_ALSA_msg_id id,
                             size_t& resp_size,
                             uint8_t* resp,
                             size_t req_size,
                             const uint8_t* req) {
  BOOST_LOG_TRIVIAL(debug) << "driver_manager:: event " << alsa_msg_str[id]
                           << " data len " << req_size;
  switch (id) {
    case MT_ALSA_Msg_Hello:
      resp_size = 0;
      break;
    case MT_ALSA_Msg_Bye:
      resp_size = 0;
      break;
    case MT_ALSA_Msg_SetMasterOutputVolume:
      if (req_size == sizeof(int32_t)) {
        memcpy(&output_volume_, req, req_size);
        BOOST_LOG_TRIVIAL(info)
            << "driver_manager:: event SetMasterOutputVolume "
            << output_volume_;
      }
      resp_size = 0;
      break;
    case MT_ALSA_Msg_SetMasterOutputSwitch:
      if (req_size == sizeof(int32_t)) {
        memcpy(&output_switch_, req, req_size);
        BOOST_LOG_TRIVIAL(info)
            << "driver_manager:: event SetMasterOutputSwitch "
            << output_switch_;
      }
      resp_size = 0;
      break;
    case MT_ALSA_Msg_SetSampleRate:
      if (req_size == sizeof(uint32_t)) {
        memcpy(&sample_rate_, req, req_size);
        BOOST_LOG_TRIVIAL(info)
            << "driver_manager:: event SetSampleRate " << sample_rate_;
      }
      resp_size = 0;
      break;
    case MT_ALSA_Msg_GetMasterOutputVolume:
      resp_size = sizeof(int32_t);
      memcpy(resp, &output_volume_, resp_size);
      BOOST_LOG_TRIVIAL(info)
          << "driver_manager:: event GetMasterOutputVolume " << output_volume_;
      break;
    case MT_ALSA_Msg_GetMasterOutputSwitch:
      resp_size = sizeof(int32_t);
      memcpy(resp, &output_switch_, resp_size);
      BOOST_LOG_TRIVIAL(info)
          << "driver_manager:: event GetMasterOutputSwitch " << output_switch_;
      break;
    default:
      BOOST_LOG_TRIVIAL(error) << "driver_manager:: unknown event "
                               << alsa_msg_str[id] << " data len " << req_size;
      break;
  }
}

void DriverManager::on_event_error(enum MT_ALSA_msg_id id,
                                   std::error_code error) {
  BOOST_LOG_TRIVIAL(error) << "driver_manager:: event " << alsa_msg_str[id]
                           << " error " << error;
}
