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
#include <map>

#include "log.hpp"
#include "utils.hpp"
#include "rtsp_client.hpp"

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::algorithm;


struct RtspResponse {
  int32_t cseq{-1};
  std::string content_type;
  uint64_t content_length{0};
  std::string body;
};

static std::string sdp_get_subject(const std::string& sdp) {
  std::stringstream ssstrem(sdp);
  std::string line;
  while (getline(ssstrem, line, '\n')) {
    if (line.substr(0, 2) == "s=") {
      auto subject = line.substr(2);
      trim(subject);
      return subject;
    }
  }
  return "";
}

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

  BOOST_LOG_TRIVIAL(debug) << "rtsp_client:: reading body length " 
                           << res.content_length;
  // read up to max_length
  if (res.content_length > 0 && res.content_length < max_length) {
    res.body.reserve(res.content_length);
    std::copy_n(std::istreambuf_iterator(s), res.content_length,
                std::back_inserter(res.body));
  }
  BOOST_LOG_TRIVIAL(debug) << "rtsp_client:: body " << res.body;

  return res;
}

std::pair<bool, RtspSource> RtspClient::process(
                RtspClient::Observer callback,
                const std::string& name,
                const std::string& domain,
                const std::string& path,
                const std::string& address,
                const std::string& port,
                bool wait_for_updates) {
  RtspSource rtsp_source;
  ip::tcp::iostream s;
  try {
    BOOST_LOG_TRIVIAL(debug) << "rtsp_client:: connecting to "
                             << "rtsp://" << address << ":" << port << path;
    s.connect(address, port.length() ? port : dft_port);
    if (!s) {
      BOOST_LOG_TRIVIAL(warning)
          << "rtsp_client:: unable to connect to " << address << ":" << port;
      return std::make_pair(false, rtsp_source);
    }

    uint16_t cseq = g_seq_number++;
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
    std::string request;
    std::getline(s, request);

    if (!s || rtsp_version.substr(0, 5) != "RTSP/") {
      BOOST_LOG_TRIVIAL(error) << "rtsp_client:: invalid response from "
                               << "rtsp://" << address << ":" << port << path;
      return std::make_pair(false, rtsp_source);
    }

    if (status_code != 200) {
      BOOST_LOG_TRIVIAL(error) << "rtsp_client:: response with status code "
                               << status_code << " from "
                               << "rtsp://" << address << ":" << port << path;
      return std::make_pair(false, rtsp_source);
    }

    bool is_announce = false;
    bool is_describe = true;
    std::string announced_name;
    do {
      auto res = read_response(s, max_body_length);
      if (is_describe && res.cseq != cseq) {
        BOOST_LOG_TRIVIAL(error)
            << "rtsp_client:: invalid response sequence " << res.cseq 
            << " from rtsp://" << address << ":" << port << path;
        return std::make_pair(false, rtsp_source);
      }

      if (!res.content_type.empty() && 
            res.content_type.rfind("application/sdp", 0) == std::string::npos) {
        BOOST_LOG_TRIVIAL(error) << "rtsp_client:: unsupported content-type "
                                 << res.content_type << " from "
                                 << "rtsp://" << address << ":" << port << path;
	if (is_describe) {
          return std::make_pair(false, rtsp_source);
	}
      } else {
        std::stringstream ss;
        ss << "rtsp:" << std::hex
           << crc16(reinterpret_cast<const uint8_t*>(res.body.c_str()),
                    res.body.length());
        /*<< std::hex << ip::address_v4::from_string(address.c_str()).to_ulong();*/
        rtsp_source.id = ss.str();
        rtsp_source.source = "mDNS";
        rtsp_source.address = address;
        rtsp_source.sdp = std::move(res.body);
	BOOST_LOG_TRIVIAL(info) << "rtsp_client:: completed "
                    << "rtsp://" << address << ":" << port << path;

        if (is_announce || is_describe) {
          if (is_announce && announced_name.empty()) {
            /* if no name from URL we try from SDP file */
            announced_name = sdp_get_subject(rtsp_source.sdp);
	  }
          callback(announced_name.empty() ? name : announced_name, domain, 
                   rtsp_source);
        }

	if (is_announce) {
          s << "RTSP/1.0 200 OK\r\n";
          s << "CSeq: " << res.cseq << "\r\n";
          s << "\r\n";
	} else if (!is_describe) {
          s << "RTSP/1.0 405 Method Not Allowed\r\n";
          s << "CSeq: " << res.cseq << "\r\n";
          s << "\r\n";
	}
      }

      if (wait_for_updates) {
        auto name_domain = std::make_pair(name, domain);
	g_mutex.lock();
	g_active_clients[name_domain] = &s;
	g_mutex.unlock();

        /* we start waiting for updates */
	do {
          std::getline(s, request);
	} while (request.empty() && !s.error());
	if (s.error()) {
          BOOST_LOG_TRIVIAL(info) << "rtsp_client:: end: " 
                                     << s.error().message();
          break;
	}
        BOOST_LOG_TRIVIAL(info) << "rtsp_client:: received " << request;
        boost::trim(request);
	is_describe = is_announce = false;
	announced_name = "";
        std::vector<std::string> fields;
        split(fields, request, boost::is_any_of(" "));
        if (fields.size() >= 2 && fields[0] == "ANNOUNCE") {
          auto const [ok, protocol, host, port, path] = parse_url(fields[1]);
          if (ok) {
            /* if we find a valid announced source name we use it 
	     * otherwise we try from SDP file or we use the mDNS name */
            if (path.rfind("/by-name/") != std::string::npos) {
              announced_name = path.substr(9);
	      BOOST_LOG_TRIVIAL(debug) << "rtsp_client:: found announced name "
                    << announced_name;
	    }
	  }
	  is_announce = true;
        }
      }
    } while (wait_for_updates);

  } catch (std::exception& e) {
    BOOST_LOG_TRIVIAL(warning)
        << "rtsp_client:: error with "
        << "rtsp://" << address << ":" << port << path << ": " << e.what();
  }

  if (wait_for_updates) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_active_clients.find(std::make_pair(name, domain));
    if (it != g_active_clients.end() && it->second == &s) {
      g_active_clients.erase(it);
    }
  }

  return std::make_pair(true, rtsp_source);
}


void RtspClient::stop(const std::string& name, const std::string& domain) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_active_clients.find(std::make_pair(name, domain));
  if (it != g_active_clients.end()) {
    BOOST_LOG_TRIVIAL(info)
        << "rtsp_client:: stopping client " << name << " " << domain;
#if BOOST_VERSION < 106600
    it->second->close();
#else
    it->second->socket().shutdown(tcp::socket::shutdown_both);
#endif
    g_active_clients.erase(it);
  }
}

void RtspClient::stop_all() {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_active_clients.begin();
  while (it != g_active_clients.end()) {
    BOOST_LOG_TRIVIAL(info)
        << "rtsp_client:: stopping client " 
        << it->first.first << " " << it->first.second;
#if BOOST_VERSION < 106600
    it->second->close();
#else
    it->second->socket().shutdown(tcp::socket::shutdown_both);
#endif
    it = g_active_clients.erase(it);
  }
}

std::pair<bool, RtspSource> RtspClient::describe(
                const std::string& path,
                const std::string& address,
                const std::string& port) {
  return RtspClient::process({}, {}, {}, path, address, port, false);
}

