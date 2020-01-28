//
//  error_code.hpp
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

#ifndef _ERROR_CODE_HPP_
#define _ERROR_CODE_HPP_

#include <system_error>

// Driver errors
enum class DriverErrc {
  unknown = 10,                      // unhandled code from driver
  invalid_data_size = 11,            // driver -315 invalid data size
  invalid_value = 12,                // driver -815 invalid value specified
  command_failed = 13,               // driver -401 command failed
  command_not_found = 14,            // driver -404 command not found
  unknown_command = 15,              // driver -314 unknown command
  invalid_daemon_response = 16,      // driver -303 invalid daemon response
  invalid_daemon_response_size = 17  // driver -302 invalid daemon response
};

namespace std {
template <>
struct is_error_code_enum<DriverErrc> : true_type {};
}  // namespace std

std::error_code make_error_code(DriverErrc);

std::error_code get_driver_error(int code);

// Daemon errors
enum class DaemonErrc {
  invalid_stream_id = 40,       // daemon invalid stream id
  stream_id_in_use = 41,        // daemon stream id is in use
  stream_id_not_in_use = 42,    // daemon stream not in use
  invalid_url = 43,             // daemon invalid URL
  cannot_retrieve_sdp = 44,     // daemon cannot retrieve SDP
  cannot_parse_sdp = 45,        // daemon cannot parse SDP
  send_invalid_size = 50,       // daemon data size too big for buffer
  send_u2k_failed = 51,         // daemon failed to send command to driver
  send_k2u_failed = 52,         // daemon failed to send event response to driver
  receive_u2k_failed = 53,      // daemon failed to receive response from driver
  receive_k2u_failed = 54,      // daemon failed to receive event from driver
  invalid_driver_response = 55  // unexpected driver command response code
};

namespace std {
template <>
struct is_error_code_enum<DaemonErrc> : true_type {};
}  // namespace std

std::error_code make_error_code(DaemonErrc);

#endif
