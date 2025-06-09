//
//  json.cpp
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

#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <iostream>
#include <regex>
#include <string>
#include <chrono>

#include "log.hpp"
#include "utils.hpp"
#include "json.hpp"

using namespace std::chrono;
using second_t = duration<double, std::ratio<1> >;

static inline std::string remove_undesired_chars(const std::string& s) {
  std::regex html_regex("[^ A-Za-z0-9:~.,_/=%()\\r\\n\\t\?#-]?");
  return std::regex_replace(s, html_regex, "");
}

static std::string escape_json(const std::string& js) {
  std::string s(remove_undesired_chars(js));
  std::ostringstream ss;
  for (auto c = s.cbegin(); c != s.cend(); c++) {
    switch (*c) {
      case '"':
        ss << "\\\"";
        break;
      case '\\':
        ss << "\\\\";
        break;
      case '\b':
        ss << "\\b";
        break;
      case '\f':
        ss << "\\f";
        break;
      case '\n':
        ss << "\\n";
        break;
      case '\r':
        ss << "\\r";
        break;
      case '\t':
        ss << "\\t";
        break;
      default:
        if ('\x00' <= *c && *c <= '\x1f') {
          ss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
             << (int)*c;
        } else {
          ss << *c;
        }
    }
  }
  return ss.str();
}

std::string config_to_json(const Config& config) {
  std::stringstream ss;
  ss << "{" << "\n  \"http_port\": " << config.get_http_port()
     << ",\n  \"rtsp_port\": " << config.get_rtsp_port()
     << ",\n  \"http_base_dir\": \"" << config.get_http_base_dir() << "\""
     << ",\n  \"log_severity\": " << config.get_log_severity()
     << ",\n  \"playout_delay\": " << config.get_playout_delay()
     << ",\n  \"tic_frame_size_at_1fs\": " << config.get_tic_frame_size_at_1fs()
     << ",\n  \"max_tic_frame_size\": " << config.get_max_tic_frame_size()
     << ",\n  \"sample_rate\": " << config.get_sample_rate()
     << ",\n  \"rtp_mcast_base\": \""
     << escape_json(config.get_rtp_mcast_base()) << "\""
     << ",\n  \"rtp_port\": " << config.get_rtp_port()
     << ",\n  \"ptp_domain\": " << unsigned(config.get_ptp_domain())
     << ",\n  \"ptp_dscp\": " << unsigned(config.get_ptp_dscp())
     << ",\n  \"sap_mcast_addr\": \""
     << escape_json(config.get_sap_mcast_addr()) << "\""
     << ",\n  \"sap_interval\": " << config.get_sap_interval()
     << ",\n  \"syslog_proto\": \"" << escape_json(config.get_syslog_proto())
     << "\"" << ",\n  \"syslog_server\": \""
     << escape_json(config.get_syslog_server()) << "\""
     << ",\n  \"status_file\": \"" << escape_json(config.get_status_file())
     << "\"" << ",\n  \"interface_name\": \""
     << escape_json(config.get_interface_name()) << "\""
     << ",\n  \"mdns_enabled\": " << std::boolalpha << config.get_mdns_enabled()
     << ",\n  \"custom_node_id\": \""
     << escape_json(config.get_custom_node_id()) << "\""
     << ",\n  \"node_id\": \"" << escape_json(config.get_node_id()) << "\""
     << ",\n  \"ptp_status_script\": \""
     << escape_json(config.get_ptp_status_script()) << "\""
     << ",\n  \"mac_addr\": \"" << escape_json(config.get_mac_addr_str())
     << "\"" << ",\n  \"ip_addr\": \"" << escape_json(config.get_ip_addr_str())
     << "\"" << ",\n  \"streamer_channels\": "
     << unsigned(config.get_streamer_channels())
     << ",\n  \"streamer_files_num\": "
     << unsigned(config.get_streamer_files_num())
     << ",\n  \"streamer_file_duration\": "
     << unsigned(config.get_streamer_file_duration())
     << ",\n  \"streamer_player_buffer_files_num\": "
     << unsigned(config.get_streamer_player_buffer_files_num())
     << ",\n  \"streamer_enabled\": " << std::boolalpha
     << config.get_streamer_enabled()
     << ",\n  \"transcriber_enabled\": " << std::boolalpha
     << config.get_transcriber_enabled() << ",\n  \"transcriber_channels\": "
     << unsigned(config.get_transcriber_channels())
     << ",\n  \"transcriber_files_num\": "
     << unsigned(config.get_transcriber_files_num())
     << ",\n  \"transcriber_file_duration\": "
     << unsigned(config.get_transcriber_file_duration())
     << ",\n  \"transcriber_model\": \""
     << escape_json(config.get_transcriber_model()) << "\""
     << ",\n  \"transcriber_language\": \""
     << escape_json(config.get_transcriber_language()) << "\""
     << ",\n  \"transcriber_openvino_device\": \""
     << escape_json(config.get_transcriber_openvino_device()) << "\""
     << ",\n  \"auto_sinks_update\": " << std::boolalpha
     << config.get_auto_sinks_update() << "\n}\n";
  return ss.str();
}

std::string source_to_json(const StreamSource& source) {
  std::stringstream ss;
  ss << "\n  {" << "\n    \"id\": " << unsigned(source.id)
     << ",\n    \"enabled\": " << std::boolalpha << source.enabled
     << ",\n    \"name\": \"" << escape_json(source.name) << "\""
     << ",\n    \"io\": \"" << escape_json(source.io) << "\""
     << ",\n    \"max_samples_per_packet\": " << source.max_samples_per_packet
     << ",\n    \"codec\": \"" << escape_json(source.codec) << "\""
     << ",\n    \"address\": \"" << escape_json(source.address) << "\""
     << ",\n    \"ttl\": " << unsigned(source.ttl)
     << ",\n    \"payload_type\": " << unsigned(source.payload_type)
     << ",\n    \"dscp\": " << +unsigned(source.dscp)
     << ",\n    \"refclk_ptp_traceable\": " << std::boolalpha
     << source.refclk_ptp_traceable << ",\n    \"map\": [ ";
  int i = 0;
  for (int value : source.map) {
    if (i++ > 0)
      ss << ", ";
    ss << value;
  }
  ss << " ]\n  }";
  return ss.str();
}

std::string sink_to_json(const StreamSink& sink) {
  std::stringstream ss;
  ss << "\n  {" << "\n    \"id\": " << unsigned(sink.id)
     << ",\n    \"name\": \"" << escape_json(sink.name) << "\""
     << ",\n    \"io\": \"" << escape_json(sink.io) << "\""
     << ",\n    \"use_sdp\": " << std::boolalpha << sink.use_sdp
     << ",\n    \"source\": \"" << escape_json(sink.source) << "\""
     << ",\n    \"sdp\": \"" << escape_json(sink.sdp) << "\""
     << ",\n    \"delay\": " << sink.delay
     << ",\n    \"ignore_refclk_gmid\": " << std::boolalpha
     << sink.ignore_refclk_gmid << ",\n    \"map\": [ ";
  int i = 0;
  for (int value : sink.map) {
    if (i++ > 0)
      ss << ", ";
    ss << value;
  }
  ss << " ]\n  }";
  return ss.str();
}

std::string sink_status_to_json(const SinkStreamStatus& status) {
  std::stringstream ss;
  ss << "{";
  ss << "\n  \"sink_flags\":\n  {" << std::boolalpha
     << "  \n    \"rtp_seq_id_error\": " << status.is_rtp_seq_id_error
     << ", \n    \"rtp_ssrc_error\": " << status.is_rtp_ssrc_error
     << ", \n    \"rtp_payload_type_error\": "
     << status.is_rtp_payload_type_error
     << ", \n    \"rtp_sac_error\": " << status.is_rtp_sac_error
     << ", \n    \"receiving_rtp_packet\": " << status.is_receiving_rtp_packet
     << ", \n    \"some_muted\": " << status.is_some_muted
     << ", \n    \"all_muted\": " << status.is_all_muted
     << ", \n    \"muted\": " << status.is_muted << "\n  },"
     << "\n  \"sink_min_time\": " << status.min_time << "\n}\n";
  return ss.str();
}

std::string ptp_config_to_json(const PTPConfig& ptp_config) {
  std::stringstream ss;
  ss << "{" << " \"domain\": " << unsigned(ptp_config.domain)
     << ", \"dscp\": " << unsigned(ptp_config.dscp) << " }\n";
  return ss.str();
}

std::string ptp_status_to_json(const PTPStatus& status) {
  std::stringstream ss;
  ss << "{" << " \"status\": \"" << escape_json(status.status) << "\""
     << ", \"gmid\": \"" << escape_json(status.gmid) << "\""
     << ", \"jitter\": " << status.jitter << " }\n";
  return ss.str();
}

std::string sources_to_json(const std::list<StreamSource>& sources) {
  int count = 0;
  std::stringstream ss;
  ss << "{\n  \"sources\": [";
  for (auto const& source : sources) {
    if (count++) {
      ss << ", ";
    }
    ss << source_to_json(source);
  }
  ss << "  ]\n}\n";
  return ss.str();
}

std::string sinks_to_json(const std::list<StreamSink>& sinks) {
  int count = 0;
  std::stringstream ss;
  ss << "{\n  \"sinks\": [";
  for (auto const& sink : sinks) {
    if (count++) {
      ss << ", ";
    }
    ss << sink_to_json(sink);
  }
  ss << "  ]\n}\n";
  return ss.str();
}

std::string streams_to_json(const std::list<StreamSource>& sources,
                            const std::list<StreamSink>& sinks) {
  int count = 0;
  std::stringstream ss;
  ss << "{\n  \"sources\": [";
  for (auto const& source : sources) {
    if (count++) {
      ss << ", ";
    }
    ss << source_to_json(source);
  }
  count = 0;
  ss << "  ],\n  \"sinks\": [";
  for (auto const& sink : sinks) {
    if (count++) {
      ss << ", ";
    }
    ss << sink_to_json(sink);
  }
  ss << "  ]\n}\n";
  return ss.str();
}

std::string remote_source_to_json(const RemoteSource& source) {
  std::stringstream ss;
  ss << "\n  {" << "\n    \"source\": \"" << escape_json(source.source) << "\""
     << ",\n    \"id\": \"" << escape_json(source.id) << "\""
     << ",\n    \"name\": \"" << escape_json(source.name) << "\""
     << ",\n    \"domain\": \"" << escape_json(source.domain) << "\""
     << ",\n    \"address\": \"" << escape_json(source.address) << "\""
     << ",\n    \"sdp\": \"" << escape_json(source.sdp) << "\""
     << ",\n    \"last_seen\": "
     << unsigned(duration_cast<second_t>(steady_clock::now() -
                                         source.last_seen_timepoint)
                     .count())
     << ",\n    \"announce_period\": " << unsigned(source.announce_period)
     << " \n  }";
  return ss.str();
}

std::string remote_sources_to_json(const std::list<RemoteSource>& sources) {
  int count = 0;
  std::stringstream ss;
  ss << "{\n  \"remote_sources\": [";
  for (auto const& source : sources) {
    if (count++) {
      ss << ", ";
    }
    ss << remote_source_to_json(source);
  }
  ss << "  ]\n}\n";
  return ss.str();
}

#ifdef _USE_STREAMER_
std::string streamer_info_to_json(const StreamerInfo& info) {
  std::stringstream ss;
  ss << "{" << "\n   \"status\": " << unsigned(info.status)
     << ",\n   \"file_duration\": " << unsigned(info.file_duration)
     << ",\n   \"files_num\": " << unsigned(info.files_num)
     << ",\n   \"player_buffer_files_num\": "
     << unsigned(info.player_buffer_files_num)
     << ",\n   \"start_file_id\": " << unsigned(info.start_file_id)
     << ",\n   \"current_file_id\": " << unsigned(info.current_file_id)
     << ",\n   \"channels\": " << unsigned(info.channels)
     << ",\n   \"format\": \"" << info.format << "\""
     << ",\n   \"rate\": " << unsigned(info.rate) << "\n}\n";
  return ss.str();
}
#endif

Config json_to_config_(std::istream& js, Config& config) {
  try {
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(js, pt);

    for (auto const& [key, val] : pt) {
      if (key == "http_port") {
        config.set_http_port(val.get_value<int>());
      } else if (key == "rtsp_port") {
        config.set_rtsp_port(val.get_value<int>());
      } else if (key == "http_base_dir") {
        config.set_http_base_dir(
            remove_undesired_chars(val.get_value<std::string>()));
      } else if (key == "streamer_channels") {
        config.set_streamer_channels(val.get_value<uint8_t>());
      } else if (key == "streamer_files_num") {
        config.set_streamer_files_num(val.get_value<uint8_t>());
      } else if (key == "streamer_file_duration") {
        config.set_streamer_file_duration(val.get_value<uint16_t>());
      } else if (key == "streamer_player_buffer_files_num") {
        config.set_streamer_player_buffer_files_num(val.get_value<uint8_t>());
      } else if (key == "streamer_enabled") {
        config.set_streamer_enabled(val.get_value<bool>());
      } else if (key == "transcriber_enabled") {
        config.set_transcriber_enabled(val.get_value<bool>());
      } else if (key == "transcriber_channels") {
        config.set_transcriber_channels(val.get_value<uint8_t>());
      } else if (key == "transcriber_files_num") {
        config.set_transcriber_files_num(val.get_value<uint8_t>());
      } else if (key == "transcriber_file_duration") {
        config.set_transcriber_file_duration(val.get_value<uint16_t>());
      } else if (key == "transcriber_model") {
        config.set_transcriber_model(
            remove_undesired_chars(val.get_value<std::string>()));
      } else if (key == "transcriber_language") {
        config.set_transcriber_language(
            remove_undesired_chars(val.get_value<std::string>()));
      } else if (key == "transcriber_openvino_device") {
        config.set_transcriber_openvino_device(
            remove_undesired_chars(val.get_value<std::string>()));
      } else if (key == "streamer_player_buffer_files_num") {
        config.set_streamer_player_buffer_files_num(val.get_value<uint8_t>());
      } else if (key == "log_severity") {
        config.set_log_severity(val.get_value<int>());
      } else if (key == "interface_name") {
        config.set_interface_name(
            remove_undesired_chars(val.get_value<std::string>()));
      } else if (key == "playout_delay") {
        config.set_playout_delay(val.get_value<uint32_t>());
      } else if (key == "tic_frame_size_at_1fs") {
        config.set_tic_frame_size_at_1fs(val.get_value<uint32_t>());
      } else if (key == "max_tic_frame_size") {
        config.set_max_tic_frame_size(val.get_value<uint32_t>());
      } else if (key == "sample_rate") {
        config.set_sample_rate(val.get_value<uint32_t>());
      } else if (key == "rtp_mcast_base") {
        config.set_rtp_mcast_base(
            remove_undesired_chars(val.get_value<std::string>()));
      } else if (key == "rtp_port") {
        config.set_rtp_port(val.get_value<uint16_t>());
      } else if (key == "ptp_domain") {
        config.set_ptp_domain(val.get_value<uint8_t>());
      } else if (key == "ptp_dscp") {
        config.set_ptp_dscp(val.get_value<uint8_t>());
      } else if (key == "sap_mcast_addr") {
        config.set_sap_mcast_addr(
            remove_undesired_chars(val.get_value<std::string>()));
      } else if (key == "sap_interval") {
        config.set_sap_interval(val.get_value<uint16_t>());
      } else if (key == "mdns_enabled") {
        config.set_mdns_enabled(val.get_value<bool>());
      } else if (key == "status_file") {
        config.set_status_file(
            remove_undesired_chars(val.get_value<std::string>()));
      } else if (key == "syslog_proto") {
        config.set_syslog_proto(
            remove_undesired_chars(val.get_value<std::string>()));
      } else if (key == "syslog_server") {
        config.set_syslog_server(
            remove_undesired_chars(val.get_value<std::string>()));
      } else if (key == "ptp_status_script") {
        config.set_ptp_status_script(
            remove_undesired_chars(val.get_value<std::string>()));
      } else if (key == "custom_node_id") {
        config.set_custom_node_id(
            remove_undesired_chars(val.get_value<std::string>()));
      } else if (key == "auto_sinks_update") {
        config.set_auto_sinks_update(val.get_value<bool>());
      } else if (key == "ip_addr") {
        config.set_ip_addr_str(val.get_value<std::string>());
      } else if (key == "mac_addr" || key == "node_id") {
        /* ignored */
      } else {
        std::cerr << "Warning: unkown configuration option " << key
                  << std::endl;
      }
    }
  } catch (boost::property_tree::json_parser::json_parser_error& je) {
    throw std::runtime_error("error parsing JSON at line " +
                             std::to_string(je.line()) + " :" + je.message());
  } catch (std::invalid_argument& e) {
    throw std::runtime_error(
        "error parsing JSON: cannot perform number conversion");
  } catch (std::out_of_range& e) {
    throw std::runtime_error(
        "error parsing JSON: number conversion out of range");
  } catch (std::exception& e) {
    throw std::runtime_error("error parsing JSON: " + std::string(e.what()));
  }
  return config;
}

Config json_to_config(std::istream& js, const Config& curConfig) {
  Config config(curConfig);
  return json_to_config_(js, config);
}

Config json_to_config(std::istream& js) {
  Config config;
  return json_to_config_(js, config);
}

Config json_to_config(const std::string& json, const Config& curConfig) {
  std::stringstream ss(json);
  return json_to_config(ss, curConfig);
}

Config json_to_config(const std::string& json) {
  std::stringstream ss(json);
  return json_to_config(ss);
}

StreamSource json_to_source(const std::string& id, const std::string& json) {
  /* JSON request
    "enabled": true,
    "name": "ALSA (on ubuntu)_1",
    "io": "Audio Device",
    "map": [ 0, 1, 2, 3, 4, 5, 6, 7 ],
    "max_samples_per_packet": 48,
    "codec": "L24",
    "address": "",
    "ttl": 15,
    "payload_type": 98,
    "dscp": 34,
    "refclk_ptp_traceable": false
  */
  StreamSource source;
  try {
    boost::property_tree::ptree pt;
    std::stringstream ss(json);
    boost::property_tree::read_json(ss, pt);

    source.id = std::stoi(id);
    source.enabled = pt.get<bool>("enabled");
    source.name = remove_undesired_chars(pt.get<std::string>("name"));
    source.io = remove_undesired_chars(pt.get<std::string>("io"));
    /* source map determite the association with
       ALSA output channels used to playing */
    BOOST_FOREACH (boost::property_tree::ptree::value_type& v,
                   pt.get_child("map")) {
      source.map.emplace_back(std::stoi(v.second.data()));
    }
    source.max_samples_per_packet = pt.get<uint32_t>("max_samples_per_packet");
    source.codec = remove_undesired_chars(pt.get<std::string>("codec"));
    source.address = remove_undesired_chars(pt.get<std::string>("address"));
    source.ttl = pt.get<uint8_t>("ttl");
    source.payload_type = pt.get<uint8_t>("payload_type");
    source.dscp = pt.get<uint8_t>("dscp");
    source.refclk_ptp_traceable = pt.get<bool>("refclk_ptp_traceable");
  } catch (boost::property_tree::json_parser::json_parser_error& je) {
    throw std::runtime_error("error parsing JSON at line " +
                             std::to_string(je.line()) + " :" + je.message());
  } catch (std::invalid_argument& e) {
    throw std::runtime_error(
        "error parsing JSON: cannot perform number conversion");
  } catch (std::out_of_range& e) {
    throw std::runtime_error(
        "error parsing JSON: number conversion out of range");
  } catch (std::exception& e) {
    throw std::runtime_error("error parsing JSON: " + std::string(e.what()));
  }
  return source;
}

StreamSink json_to_sink(const std::string& id, const std::string& json) {
  /* JSON request
    "name": "ALSA (on ubuntu)_1",
    "io": "Audio Device",
    "use_sdp": true,
    "source": "...",
    "sdp": "...",
    "delay": 384,
    "ignore_refclk_gmid": false,
    "map": [ 0, 1, 2, 3, 4, 5, 6, 7 ]
  */
  StreamSink sink;
  try {
    boost::property_tree::ptree pt;
    std::stringstream ss(json);
    boost::property_tree::read_json(ss, pt);

    sink.id = std::stoi(id);
    sink.name = remove_undesired_chars(pt.get<std::string>("name"));
    sink.io = remove_undesired_chars(pt.get<std::string>("io"));
    sink.source = remove_undesired_chars(pt.get<std::string>("source"));
    sink.use_sdp = pt.get<bool>("use_sdp");
    sink.sdp = remove_undesired_chars(pt.get<std::string>("sdp"));
    sink.delay = pt.get<uint32_t>("delay");
    sink.ignore_refclk_gmid = pt.get<bool>("ignore_refclk_gmid");
    /* source map determite the association with
       ALSA input channels used to recording */
    BOOST_FOREACH (boost::property_tree::ptree::value_type& v,
                   pt.get_child("map")) {
      sink.map.emplace_back(std::stoi(v.second.data()));
    }
  } catch (boost::property_tree::json_parser::json_parser_error& je) {
    throw std::runtime_error("error parsing JSON at line " +
                             std::to_string(je.line()) + " :" + je.message());
  } catch (std::invalid_argument& e) {
    throw std::runtime_error(
        "error parsing JSON: cannot perform number conversion");
  } catch (std::out_of_range& e) {
    throw std::runtime_error(
        "error parsing JSON: number conversion out of range");
  } catch (std::exception& e) {
    throw std::runtime_error("error parsing JSON: " + std::string(e.what()));
  }
  return sink;
}

PTPConfig json_to_ptp_config(const std::string& json) {
  PTPConfig ptpConfig;
  try {
    boost::property_tree::ptree pt;
    std::stringstream ss(json);
    boost::property_tree::read_json(ss, pt);

    ptpConfig.domain = pt.get<int>("domain");
    ptpConfig.dscp = pt.get<int>("dscp");
  } catch (boost::property_tree::json_parser::json_parser_error& je) {
    throw std::runtime_error("error parsing JSON at line " +
                             std::to_string(je.line()) + " :" + je.message());
  }
  return ptpConfig;
}

void json_to_sources(const std::string& json,
                     std::list<StreamSource>& sources) {
  std::stringstream ss(json);
  return json_to_sources(ss, sources);
}

static void parse_json_sources(boost::property_tree::ptree& pt,
                               std::list<StreamSource>& sources) {
  BOOST_FOREACH (auto const& v, pt.get_child("sources")) {
    StreamSource source;
    source.id = v.second.get<uint8_t>("id");
    source.enabled = v.second.get<bool>("enabled");
    source.name = v.second.get<std::string>("name");
    source.io = v.second.get<std::string>("io");
    source.max_samples_per_packet =
        v.second.get<uint32_t>("max_samples_per_packet");
    source.codec = v.second.get<std::string>("codec");
    source.address = v.second.get<std::string>("address");
    source.ttl = v.second.get<uint8_t>("ttl");
    source.payload_type = v.second.get<uint8_t>("payload_type");
    source.dscp = v.second.get<uint8_t>("dscp");
    source.refclk_ptp_traceable = v.second.get<bool>("refclk_ptp_traceable");
    /* source map determite the association with
       ALSA output channels used to playing */
    BOOST_FOREACH (const boost::property_tree::ptree::value_type& vm,
                   v.second.get_child("map")) {
      source.map.emplace_back(std::stoi(vm.second.data()));
    }
    sources.emplace_back(std::move(source));
  }
}

void json_to_sources(std::istream& js, std::list<StreamSource>& sources) {
  try {
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(js, pt);
    parse_json_sources(pt, sources);
  } catch (boost::property_tree::json_parser::json_parser_error& je) {
    throw std::runtime_error("error parsing JSON at line " +
                             std::to_string(je.line()) + " :" + je.message());
  }
}

void json_to_sinks(const std::string& json, std::list<StreamSink>& sinks) {
  std::stringstream ss(json);
  return json_to_sinks(ss, sinks);
}

static void parse_json_sinks(boost::property_tree::ptree& pt,
                             std::list<StreamSink>& sinks) {
  BOOST_FOREACH (auto const& v, pt.get_child("sinks")) {
    StreamSink sink;
    sink.id = v.second.get<uint8_t>("id");
    sink.name = v.second.get<std::string>("name");
    sink.io = v.second.get<std::string>("io");
    sink.source = v.second.get<std::string>("source");
    sink.use_sdp = v.second.get<bool>("use_sdp");
    sink.sdp = v.second.get<std::string>("sdp");
    sink.delay = v.second.get<uint32_t>("delay");
    sink.ignore_refclk_gmid = v.second.get<bool>("ignore_refclk_gmid");
    /* source map determite the association with
       ALSA input channels used to recording */
    BOOST_FOREACH (const boost::property_tree::ptree::value_type& vm,
                   v.second.get_child("map")) {
      sink.map.emplace_back(std::stoi(vm.second.data()));
    }
    sinks.emplace_back(std::move(sink));
  }
}

void json_to_sinks(std::istream& js, std::list<StreamSink>& sinks) {
  try {
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(js, pt);
    parse_json_sinks(pt, sinks);
  } catch (boost::property_tree::json_parser::json_parser_error& je) {
    throw std::runtime_error("error parsing JSON at line " +
                             std::to_string(je.line()) + " :" + je.message());
  }
}

void json_to_streams(const std::string& json,
                     std::list<StreamSource>& sources,
                     std::list<StreamSink>& sinks) {
  std::stringstream ss(json);
  json_to_streams(ss, sources, sinks);
}

void json_to_streams(std::istream& js,
                     std::list<StreamSource>& sources,
                     std::list<StreamSink>& sinks) {
  try {
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(js, pt);
    parse_json_sources(pt, sources);
    parse_json_sinks(pt, sinks);
  } catch (boost::property_tree::json_parser::json_parser_error& je) {
    throw std::runtime_error("error parsing JSON at line " +
                             std::to_string(je.line()) + " :" + je.message());
  }
}
