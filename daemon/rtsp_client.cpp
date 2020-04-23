//
//  rtsp_client.cpp
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

#include <httplib.h>

#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iomanip>
#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <chrono>

#include "log.hpp"
#include "utils.hpp"
#include "rtsp_client.hpp"

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::algorithm;

struct RtspResponse {
  uint32_t cseq;
  std::string content_type;
  uint64_t content_length;
  std::string body;
};

RtspResponse read_response(tcp::iostream& s, uint16_t max_length) {
  RtspResponse res;
  std::string header;
  /*
   RTSP/1.0 200 OK
   CSeq: 312
   Date: 23 Jan 1997 15:35:06 GMT
   Content-Type: application/sdp
   Content-Length: 376

  */

  try {
    while (std::getline(s, header) && header != "" && header != "\r") {
      to_lower(header);
      trim(header);
      if (header.rfind("cseq:", 0) != std::string::npos) {
        res.cseq = std::stoi(header.substr(5));
      } else if (header.rfind("content-type:", 0) != std::string::npos) {
        res.content_type = header.substr(13);
        trim(res.content_type);
      } else if (header.rfind("content-length:", 0) != std::string::npos) {
        res.content_length = std::stoi(header.substr(15));
      }
    }
  } catch (...) {
    BOOST_LOG_TRIVIAL(error) << "rtsp_client:: invalid response header, "
                             << "cannot perform number conversion";
  }

  // read up to max_length
  if (res.content_length > 0 && res.content_length < max_length) {
    res.body.reserve(res.content_length);
    std::copy_n(std::istreambuf_iterator(s), res.content_length,
                std::back_inserter(res.body));
  }

  return res;
}

std::pair<bool, RtspSource> RtspClient::describe(const std::string& path,
                                                  const std::string& address,
                                                  const std::string& port) {
  RtspSource rs;
  bool success{false};
  try {
    tcp::iostream s;

#if BOOST_VERSION < 106700
    s.expires_from_now(boost::posix_time::seconds(client_timeout));
#else
    s.expires_from_now(std::chrono::seconds(client_timeout));
#endif

    BOOST_LOG_TRIVIAL(debug) << "rtsp_client:: connecting to "
                             << "rtsp://" << address << ":" << port << path;
    s.connect(address, port.length() ? port : dft_port);
    if (!s) {
      BOOST_LOG_TRIVIAL(warning)
          << "rtsp_client:: unable to connect to " << address << ":" << port;
      return std::make_pair(success, rs);
    }

    uint16_t cseq = seq_number++;
    s << "DESCRIBE rtsp://" << address << ":" << port
      << httplib::detail::encode_url(path) << " RTSP/1.0\r\n";
    s << "CSeq: " << cseq << "\r\n";
    s << "User-Agent: aes67-daemon\r\n";
    s << "Accept: application/sdp\r\n\r\n";

    // By default, the stream is tied with itself. This means that the stream
    // automatically flush the buffered output before attempting a read. It is
    // not necessary not explicitly flush the stream at this point.

    // Check that response is OK.
    std::string rtsp_version;
    s >> rtsp_version;
    unsigned int status_code;
    s >> status_code;
    std::string status_message;
    std::getline(s, status_message);

    if (!s || rtsp_version.substr(0, 5) != "RTSP/") {
      BOOST_LOG_TRIVIAL(error) << "rtsp_client:: invalid response from "
                               << "rtsp://" << address << ":" << port << path;
      return std::make_pair(success, rs);
    }

    if (status_code != 200) {
      BOOST_LOG_TRIVIAL(error) << "rtsp_client:: response with status code "
                               << status_code << " from "
                               << "rtsp://" << address << ":" << port << path;
      return std::make_pair(success, rs);
    }

    auto res = read_response(s, max_body_length);
    if (res.content_type.rfind("application/sdp", 0) == std::string::npos) {
      BOOST_LOG_TRIVIAL(error) << "rtsp_client:: unsupported content-type "
                               << res.content_type << " from "
                               << "rtsp://" << address << ":" << port << path;
      return std::make_pair(success, rs);
    }
    if (res.cseq != cseq) {
      BOOST_LOG_TRIVIAL(error)
          << "rtsp_client:: invalid response sequence " << res.cseq << " from "
          << "rtsp://" << address << ":" << port << path;
      return std::make_pair(success, rs);
    }

    std::stringstream ss;
    ss << "rtsp:" << std::hex
       << crc16(reinterpret_cast<const uint8_t*>(res.body.c_str()),
                res.body.length());
       /*<< std::hex << ip::address_v4::from_string(address.c_str()).to_ulong();*/

    rs.id = ss.str();
    rs.source = "mDNS";
    rs.address = address;
    rs.sdp = std::move(res.body);

    BOOST_LOG_TRIVIAL(info) << "rtsp_client:: describe completed "
                            << "rtsp://" << address << ":" << port << path;

    success = true;
  } catch (std::exception& e) {
    BOOST_LOG_TRIVIAL(warning)
        << "rtsp_client:: error with "
        << "rtsp://" << address << ":" << port << path << ": " << e.what();
  }

  return std::make_pair(success, rs);
}
