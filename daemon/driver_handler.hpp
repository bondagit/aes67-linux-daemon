//
//  driver_handler.hpp
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

#ifndef _DRIVER_HANDLER_HPP_
#define _DRIVER_HANDLER_HPP_

#include <future>

#include "MT_ALSA_message_defs.h"
#include "config.hpp"
#include "error_code.hpp"
#include "log.hpp"
#include "netlink_client.hpp"

class DriverHandler {
 public:
  static constexpr size_t max_payload = MAX_PAYLOAD;
  static constexpr off_t data_offset =
      sizeof(struct MT_ALSA_msg) /*+ sizeof(int)*/;
  static constexpr int reply_timeout_secs = 1;  // 1sec in driver
  static constexpr size_t buffer_size =
      NLMSG_SPACE(max_payload) + sizeof(struct MT_ALSA_msg);

  DriverHandler(){};
  DriverHandler(const DriverHandler&) = delete;
  DriverHandler& operator=(const DriverHandler&) = delete;
  virtual ~DriverHandler() { terminate(); };

  virtual bool init(const Config& config);
  virtual bool terminate();

 protected:
  virtual void send_command(enum MT_ALSA_msg_id id,
                            size_t size = 0,
                            const uint8_t* data = nullptr);
  virtual void on_command_done(enum MT_ALSA_msg_id id,
                               size_t size = 0,
                               const uint8_t* data = nullptr) = 0;
  virtual void on_command_error(enum MT_ALSA_msg_id id,
                                std::error_code error) = 0;
  virtual void on_event(enum MT_ALSA_msg_id id,
                        size_t& res_size,
                        uint8_t* res,
                        size_t req_size = 0,
                        const uint8_t* req = nullptr) = 0;
  virtual void on_event_error(enum MT_ALSA_msg_id id,
                              std::error_code error) = 0;

 private:
  void send(enum MT_ALSA_msg_id id,
            NetlinkClient& client,
            uint8_t* buffer,
            size_t data_size,
            const uint8_t* data);
  bool event_receiver();

  std::future<bool> res_;
  std::atomic_bool running_{false};
  uint8_t command_buffer_[buffer_size];
  uint8_t event_buffer_[buffer_size];
  uint8_t response_buffer_[buffer_size];
  NetlinkClient client_u2k_{"commands"}; /* u2k for commands */
  NetlinkClient client_k2u_{"events"};   /* k2u for events */
  std::mutex mutex_;                     /* one command at a time */
};

#endif
