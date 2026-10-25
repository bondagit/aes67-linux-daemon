//
//  nmos_browser.cpp
//
//  Copyright (c) 2019 2026 Andrea Bondavalli. All rights reserved.
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

#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include <sstream>

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <httplib.h>

#include "log.hpp"
#include "nmos_browser.hpp"

bool NmosBrowser::parse_http_url(const std::string& url, HttpUrl& parsed) {
  constexpr char scheme[] = "http://";
  if (url.compare(0, sizeof(scheme) - 1, scheme) != 0) {
    return false;
  }

  const auto authority_start = sizeof(scheme) - 1;
  const auto path_start = url.find_first_of("/?", authority_start);
  std::string authority =
      url.substr(authority_start, path_start == std::string::npos
                                      ? std::string::npos
                                      : path_start - authority_start);
  parsed.path = path_start == std::string::npos
                    ? "/"
                    : (url[path_start] == '?' ? "/" + url.substr(path_start)
                                              : url.substr(path_start));
  if (authority.empty()) {
    return false;
  }

  std::string port_text;
  if (authority.front() == '[') {
    const auto close = authority.find(']');
    if (close == std::string::npos) {
      return false;
    }
    parsed.host = authority.substr(1, close - 1);
    if (close + 1 < authority.size()) {
      if (authority[close + 1] != ':') {
        return false;
      }
      port_text = authority.substr(close + 2);
    }
  } else {
    const auto colon = authority.rfind(':');
    if (colon != std::string::npos) {
      parsed.host = authority.substr(0, colon);
      port_text = authority.substr(colon + 1);
    } else {
      parsed.host = authority;
    }
  }

  parsed.port = 80;
  if (!port_text.empty()) {
    size_t parsed_length = 0;
    unsigned long port = 0;
    try {
      port = std::stoul(port_text, &parsed_length);
    } catch (const std::exception&) {
      return false;
    }
    if (parsed_length != port_text.size() || port == 0 || port > 65535) {
      return false;
    }
    parsed.port = static_cast<uint16_t>(port);
  } else if ((!authority.empty() && authority.back() == ':') ||
             (authority.front() == '[' && authority.back() == ':')) {
    return false;
  }
  return !parsed.host.empty();
}

bool NmosBrowser::http_get(const std::string& host,
                           uint16_t port,
                           const std::string& path,
                           std::string& body) {
  httplib::Client client(host.c_str(), port);
  client.set_connection_timeout(2);
  client.set_read_timeout(3);
  const auto response = client.Get(path.c_str());
  if (!response || response->status != 200) {
    BOOST_LOG_TRIVIAL(warning)
        << "nmos_browser:: GET " << host << ":" << port << path
        << (response ? " returned HTTP " + std::to_string(response->status)
                     : " failed without a response");
    return false;
  }
  body = response->body;
  return true;
}

bool NmosBrowser::get_json_resource(const std::string& host,
                                    uint16_t port,
                                    const std::string& path,
                                    boost::property_tree::ptree& resource) {
  std::string body;
  if (!http_get(host, port, path, body)) {
    return false;
  }
  try {
    std::istringstream input(body);
    boost::property_tree::read_json(input, resource);
  } catch (const boost::property_tree::json_parser::json_parser_error& error) {
    BOOST_LOG_TRIVIAL(warning)
        << "nmos_browser:: invalid JSON from " << path << ": " << error.what();
    return false;
  }
  return true;
}

bool NmosBrowser::is_audio_sdp(const std::string& sdp) {
  std::istringstream input(sdp);
  std::string line;
  while (std::getline(input, line)) {
    boost::trim(line);
    if (line.compare(0, 8, "m=audio ") == 0) {
      return true;
    }
  }
  return false;
}

bool NmosBrowser::get_remote_sources(std::list<NmosSource>& sources) {
  const auto& query_address = config_->get_nmos_query_address();
  const auto query_port = config_->get_nmos_query_port();
  if (query_address.empty() || query_port == 0) {
    BOOST_LOG_TRIVIAL(warning)
        << "NmosBrowser:: discovery requires a query address and port";
    return false;
  }

  BOOST_LOG_TRIVIAL(debug) << "browser:: fetching NMOS sources from "
                           << query_address << ":" << query_port;

  boost::property_tree::ptree senders;
  if (!get_json_resource(query_address, query_port,
                         "/x-nmos/query/v1.3/senders?paging.limit=100",
                         senders)) {
    return false;
  }

  sources.clear();
  const auto local_node_id = make_uuid5("6ba7b810-9dad-11d1-80b4-00c04fd430c8",
                                        config_->get_node_id());
  const auto local_device_id = make_uuid5(local_node_id, "device");
  for (const auto& sender_entry : senders) {
    const auto& sender = sender_entry.second;
    const auto sender_id = sender.get<std::string>("id", "");
    const auto transport = sender.get<std::string>("transport", "");
    const auto device_id = sender.get<std::string>("device_id", "");
    const bool rtp_transport = transport == "urn:x-nmos:transport:rtp" ||
                               transport == "urn:x-nmos:transport:rtp.ucast" ||
                               transport == "urn:x-nmos:transport:rtp.mcast";
    if (sender_id.empty() || !rtp_transport || 
         // do not ignore local sender if it is on the loopback interface
        (device_id == local_device_id && config_->get_interface_name(0) != "lo")) {
      continue;
    }

    const auto flow_id = sender.get<std::string>("flow_id", "");
    if (flow_id.empty()) {
      BOOST_LOG_TRIVIAL(warning)
          << "NmosBrowser:: RTP sender " << sender_id << " has no flow_id";
      continue;
    }

    boost::property_tree::ptree flow;
    if (!get_json_resource(query_address, query_port,
                           "/x-nmos/query/v1.3/flows/" + flow_id, flow)) {
      continue;
    }

    const auto format = flow.get<std::string>("format", "");
    const auto media_type = flow.get<std::string>("media_type", "");
    const bool supported_audio = boost::iequals(media_type, "audio/L16") ||
                                 boost::iequals(media_type, "audio/L24") ||
                                 boost::iequals(media_type, "audio/AM824");
    if (format != "urn:x-nmos:format:audio" || !supported_audio) {
      continue;
    }

    const auto manifest_href = sender.get<std::string>("manifest_href", "");
    HttpUrl manifest_url;
    if (manifest_href.empty() || !parse_http_url(manifest_href, manifest_url)) {
      BOOST_LOG_TRIVIAL(warning) << "NmosBrowser:: audio sender " << sender_id
                                 << " has an invalid manifest_href";
      continue;
    }

    std::string sdp;
    if (!http_get(manifest_url.host, manifest_url.port, manifest_url.path,
                  sdp)) {
      continue;
    }
    if (!is_audio_sdp(sdp)) {
      BOOST_LOG_TRIVIAL(warning)
          << "NmosBrowser:: sender " << sender_id
          << " manifest does not contain an audio SDP media section";
      continue;
    }

    const auto origin = sdp_get_origin(sdp);
    if (origin.session_id.empty() || origin.unicast_address.empty()) {
      BOOST_LOG_TRIVIAL(warning) << "NmosBrowser:: sender " << sender_id
                                 << " manifest has no valid SDP origin";
      continue;
    }
    auto name = sender.get<std::string>("label", "");
    if (name.empty()) {
      name = sdp_get_subject(sdp);
    }
    if (name.empty()) {
      name = sender_id;
    }

    const auto source_id = "nmos:" + sender_id;

    BOOST_LOG_TRIVIAL(debug)
        << "browser:: discovered NMOS source " << source_id << " name " << name;

    sources.push_back({source_id, origin.unicast_address, std::move(name),
                          origin, std::move(sdp)});
  }

  return true;
}
