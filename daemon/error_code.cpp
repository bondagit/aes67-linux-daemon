//
//  error_code.cpp
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

#include "error_code.hpp"

struct DriverErrCategory : std::error_category {
  const char* name() const noexcept override;
  std::string message(int ev) const override;
};

const char* DriverErrCategory::name() const noexcept {
  return "driver";
}

std::string DriverErrCategory::message(int ev) const {
  switch (static_cast<DriverErrc>(ev)) {
    case DriverErrc::unknown:
      return "unhandled error code";
    case DriverErrc::invalid_data_size:
      return "invalid data size";
    case DriverErrc::invalid_value:
      return "invalid value specified";
    case DriverErrc::command_failed:
      return "command failed";
    case DriverErrc::command_not_found:
      return "command not found";
    case DriverErrc::unknown_command:
      return "unknown command";
    case DriverErrc::invalid_daemon_response:
      return "invalid daemon response";
    case DriverErrc::invalid_daemon_response_size:
      return "invalid daemon response";
    default:
      return "(unrecognized driver error)";
  }
}

std::error_code get_driver_error(int code) {
  switch (code) {
    case -302:
      return DriverErrc::invalid_daemon_response_size;
    case -303:
      return DriverErrc::invalid_daemon_response;
    case -314:
      return DriverErrc::unknown_command;
    case -315:
      return DriverErrc::invalid_data_size;
    case -401:
      return DriverErrc::command_failed;
    case -404:
      return DriverErrc::command_not_found;
    case -805:
      return DriverErrc::invalid_value;
    default:
      return DriverErrc::unknown;
  }
}

const DriverErrCategory theDriverErrCategory{};

std::error_code make_error_code(DriverErrc e) {
  return {static_cast<int>(e), theDriverErrCategory};
}

struct DaemonErrCategory : std::error_category {
  const char* name() const noexcept override;
  std::string message(int ev) const override;
};

const char* DaemonErrCategory::name() const noexcept {
  return "daemon";
}

std::string DaemonErrCategory::message(int ev) const {
  switch (static_cast<DaemonErrc>(ev)) {
    case DaemonErrc::invalid_stream_id:
      return "invalid stream id";
    case DaemonErrc::stream_id_in_use:
      return "stream id is in use";
    case DaemonErrc::stream_name_in_use:
      return "stream name is in use";
    case DaemonErrc::stream_id_not_in_use:
      return "stream not in use";
    case DaemonErrc::invalid_url:
      return "invalid URL";
    case DaemonErrc::cannot_retrieve_sdp:
      return "cannot retrieve SDP";
    case DaemonErrc::cannot_parse_sdp:
      return "cannot parse SDP";
    case DaemonErrc::send_invalid_size:
      return "send data size too big";
    case DaemonErrc::send_u2k_failed:
      return "failed to send command to driver";
    case DaemonErrc::send_k2u_failed:
      return "failed to send event response to driver";
    case DaemonErrc::receive_u2k_failed:
      return "failed to receive response from driver";
    case DaemonErrc::receive_k2u_failed:
      return "failed to receive event from driver";
    case DaemonErrc::invalid_driver_response:
      return "unexpected driver command response code";
    default:
      return "(unrecognized daemon error)";
  }
}

const DaemonErrCategory theDaemonErrCategory{};

std::error_code make_error_code(DaemonErrc e) {
  return {static_cast<int>(e), theDaemonErrCategory};
}
