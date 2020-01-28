//
//  driver_manager.hpp
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

#ifndef _DRIVER_MANAGER_HPP_
#define _DRIVER_MANAGER_HPP_

#include <boost/asio.hpp>
#include <mutex>

#include "driver_handler.hpp"

#include "RTP_stream_info.h"
#include "audio_streamer_clock_PTP_defs.h"

class DriverManager : public DriverHandler {
 public:
  static std::shared_ptr<DriverManager> create();

  // driver interface
  bool init(const Config& config) override;
  bool terminate() override;

  std::error_code ping();  // unused, return error
  std::error_code set_ptp_config(const TPTPConfig& config);
  std::error_code get_ptp_config(TPTPConfig& config);
  std::error_code get_ptp_status(TPTPStatus& status);
  std::error_code set_interface_name(const std::string& ifname);
  std::error_code add_rtp_stream(const TRTP_stream_info& stream_info,
                                 uint64_t& stream_handle);
  std::error_code get_rtp_stream_status(uint64_t stream_handle,
                                        TRTP_stream_status& stream_status);
  std::error_code remove_rtp_stream(uint64_t stream_handle);
  std::error_code get_sample_rate(uint32_t& sample_rate);
  std::error_code set_sample_rate(uint32_t sample_rate);
  std::error_code set_tic_frame_size_at_1fs(uint64_t frame_size);
  std::error_code set_max_tic_frame_size(uint64_t frame_size);
  std::error_code set_playout_delay(int32_t delay);
  std::error_code get_number_of_inputs(int32_t& inputs);
  std::error_code get_number_of_outputs(int32_t& outputs);

  int32_t get_current_output_volume() { return output_volume; };
  int32_t get_current_output_switch() { return output_switch; };
  uint32_t get_current_sample_rate() { return sample_rate; };

 protected:
  // singleton, use create to build
  DriverManager(){};

  // these are used in init/terminate
  std::error_code hello();
  std::error_code start();
  std::error_code stop();
  std::error_code reset();
  std::error_code bye();

  void on_command_done(enum MT_ALSA_msg_id id,
                       size_t size = 0,
                       const uint8_t* data = nullptr) override;
  void on_command_error(enum MT_ALSA_msg_id id, std::error_code error) override;
  void on_event(enum MT_ALSA_msg_id id,
                size_t& res_size,
                uint8_t* res,
                size_t req_size = 0,
                const uint8_t* req = nullptr) override;
  void on_event_error(enum MT_ALSA_msg_id id, std::error_code error) override;

  std::error_code retcode_;
  uint8_t recv_data_[NLMSG_SPACE(max_payload)]{0};

  int32_t output_volume{-20};
  int32_t output_switch{0};
  uint32_t sample_rate{0};
};

#endif
