//
//  rtsp_include.hpp
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

#ifndef _RTSP_CLIENT_HPP_
#define _RTSP_CLIENT_HPP_

struct RtspSource {
  std::string id;
  std::string source;
  std::string address;
  std::string sdp;
};

class RtspClient {
 public:
  constexpr static uint16_t max_body_length = 4096;  // byte
  constexpr static uint16_t client_timeout = 10;     // sec
  constexpr static const char dft_port[] = "554";

  static std::pair<bool, RtspSource> describe(
      const std::string& path,
      const std::string& address,
      const std::string& port = dft_port);

  inline static std::atomic<uint16_t> seq_number;
};

#endif
