//
//  seesion_manager.cpp
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

#define CPPHTTPLIB_PAYLOAD_MAX_LENGTH 4096  // max for SDP file
#define CPPHTTPLIB_READ_TIMEOUT_SECOND 10

#include <httplib.h>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <chrono>
#include <experimental/map>
#include <iostream>
#include <map>
#include <set>

#include "json.hpp"
#include "log.hpp"
#include "rtsp_client.hpp"
#include "utils.hpp"
#include "session_manager.hpp"


static uint8_t get_codec_word_lenght(const std::string& codec) {
  if (codec == "L16") {
    return 2;
  }
  if (codec == "L24") {
    return 3;
  }
  if (codec == "L2432" || codec == "AM824") {
    return 4;
  }
  if (codec == "DSD64") {
    return 1;
  }
  if (codec == "DSD128") {
    return 2;
  }
  if (codec == "DSD64_32" || codec == "DSD128_32" || codec == "DSD256") {
    return 4;
  }
  return 0;
}

bool SessionManager::parse_sdp(const std::string sdp, StreamInfo& info) const {
  /*
  v=0
  o=- 4 0 IN IP4 10.0.0.12
  s=ALSA (on ubuntu)_4
  c=IN IP4 239.1.0.12/15
  t=0 0
  a=clock-domain:PTPv2 0
  m=audio 5004 RTP/AVP 98
  c=IN IP4 239.1.0.12/15
  a=rtpmap:98 L16/44100/8
  a=sync-time:0
  a=framecount:64-192
  a=ptime:4.35374165
  a=mediaclk:direct=0
  a=ts-refclk:ptp=IEEE1588-2008:00-0C-29-FF-FE-0E-90-C8:0
  a=recvonly
  */

  int num = 0;
  try {
    enum class sdp_parser_status { init, time, media };
    sdp_parser_status status = sdp_parser_status::init;
    std::stringstream ssstrem(sdp);
    std::string line;
    while (getline(ssstrem, line, '\n')) {
      boost::trim(line);
      ++num;
      if (line[1] != '=') {
        BOOST_LOG_TRIVIAL(error)
            << "session_manager:: invalid SDP file at line " << num;
        return false;
      }
      std::string val = line.substr(2);
      switch (line[0]) {
        case 'v':
          /* v=0 */
          if (val != "0") {
            BOOST_LOG_TRIVIAL(error)
                << "session_manager:: unsupported SDP version at line " << num;
            return false;
          }
          break;
        case 'o':
          break;
        case 't':
          /* t=0 0 */
          status = sdp_parser_status::time;
          break;
        case 'a': {
          auto pos = val.find(':');
          if (pos == std::string::npos) {
            /* skip this attribute */
            break;
          }
          std::string name = val.substr(0, pos);
          std::string value = val.substr(pos + 1);
          switch (status) {
            case sdp_parser_status::init:
              break;
            case sdp_parser_status::time:
              /* time attributes */
              if (name == "clock-domain") {
                /* a=clock-domain:PTPv2 0 */
                if (value.substr(0, 5) != "PTPv2") {
                  BOOST_LOG_TRIVIAL(error)
                      << "session_manager:: unsupported PTP "
                         "clock version in SDP at line "
                      << num;
                  return false;
                }
              }
              break;
            case sdp_parser_status::media:
              /* audio media attributes */
              if (name == "rtpmap") {
                /* a=rtpmap:98 L16/44100/8 */
                std::vector<std::string> fields;
                boost::split(fields, value,
                             [line](char c) { return c == ' ' || c == '/'; });
                if (fields.size() < 4) {
                  BOOST_LOG_TRIVIAL(error) << "session_manager:: invalid audio "
                                              "rtpmap in SDP at line "
                                           << num;
                  return false;
                }
                // if matching payload
                if (info.stream.m_byPayloadType == std::stoi(fields[0])) {
                  strncpy(info.stream.m_cCodec, fields[1].c_str(),
                          sizeof(info.stream.m_cCodec) - 1);
                  info.stream.m_byWordLength = get_codec_word_lenght(fields[1]);
                  info.stream.m_ui32SamplingRate = std::stoul(fields[2]);
                  if (info.stream.m_byNbOfChannels != std::stoi(fields[3])) {
                    BOOST_LOG_TRIVIAL(warning)
                        << "session_manager:: invalid audio channel "
                           "number in SDP at line "
                        << num << ", using "
                        << (int)info.stream.m_byNbOfChannels;
                    /*return false; */
                  }
                }
              } else if (name == "sync-time") {
                /* a=sync-time:0 */
                info.stream.m_ui32RTPTimestampOffset = std::stoul(value);
              } else if (name == "framecount") {
                /* a=framecount:64-192 */
              } else if (name == "ptime") {
                /* a=mediaclk:ptime=4.35374165 */
                info.stream.m_ui32MaxSamplesPerPacket =
                    (static_cast<double>(info.stream.m_ui32SamplingRate) *
                     std::stod(value)) /
                    1000;
              } else if (name == "mediaclk") {
                /* a=mediaclk:direct=0 */
                std::vector<std::string> fields;
                boost::split(fields, value,
                             [line](char c) { return c == '='; });
                if (fields.size() == 2 && fields[0] == "direct") {
                  info.stream.m_ui32RTPTimestampOffset = std::stoul(fields[1]);
                }
              } else if (name == "ts-refclk" && !info.ignore_refclk_gmid) {
                /* a=ts-refclk:ptp=IEEE1588-2008:00-0C-29-FF-FE-0E-90-C8:0 */
                std::vector<std::string> fields;
                boost::split(fields, value,
                             [line](char c) { return c == ':'; });
                if (fields.size() == 3) {
                  if (fields[1] != ptp_status_.gmid ||
                      stoi(fields[2]) != ptp_config_.domain) {
                    BOOST_LOG_TRIVIAL(warning)
                        << "session_manager:: configured PTP grand master "
                           "clock "
                           "doesn't match the PTP clock in SDP at line "
                        << num;

                    return false;
                  }
                }
              }
              break;
          }
        } break;
        case 'm': {
          /* m=audio 5004 RTP/AVP 98 */
          std::vector<std::string> fields;
          boost::split(fields, val, [line](char c) { return c == ' '; });
          if (fields.size() < 4) {
            BOOST_LOG_TRIVIAL(error)
                << "session_manager:: invalid nedia in SDP at line " << num;
            return false;
          }
          if (fields[0] == "audio") {
            info.stream.m_usDestPort = std::stoi(fields[1]);
            info.stream.m_byPayloadType =
                std::stoi(fields[3]); /* take first payload */
            status = sdp_parser_status::media;
          }
          break;
        }
        case 'c':
          /* c=IN IP4 239.1.0.12/15 */
          /* connection info of audio media */
          if (status == sdp_parser_status::media ||
              /* generic connection info */
              status == sdp_parser_status::init) {
            std::vector<std::string> fields;
            boost::split(fields, val,
                         [line](char c) { return c == ' ' || c == '/'; });
            if (fields.size() != 4) {
              BOOST_LOG_TRIVIAL(error)
                  << "session_manager:: invalid connection in SDP at line "
                  << num;
              return false;
            }
            if (fields[0] != "IN" || fields[1] != "IP4") {
              BOOST_LOG_TRIVIAL(error)
                  << "session_manager:: unsupported connection in SDP at line "
                  << num;
              return false;
            }
            info.stream.m_ui32DestIP =
                ip::address_v4::from_string(fields[2].c_str()).to_ulong();
            if (info.stream.m_ui32DestIP == INADDR_NONE) {
              BOOST_LOG_TRIVIAL(error) << "session_manager:: invalid IPv4 "
                                          "connection address in SDP at line "
                                       << num;
              return false;
            }
            info.stream.m_byTTL = std::stoi(fields[3]);
          }
          break;
        default:
          break;
      }
    }
  } catch (...) {
    BOOST_LOG_TRIVIAL(fatal) << "session_manager:: invalid SDP at line " << num
                             << ", cannot perform number conversion";
    return false;
  }

  return true;
}

std::shared_ptr<SessionManager> SessionManager::create(
    std::shared_ptr<DriverManager> driver,
    std::shared_ptr<Config> config) {
  // no need to be thread-safe here
  static std::weak_ptr<SessionManager> instance;
  if (auto ptr = instance.lock()) {
    return ptr;
  }
  auto ptr =
      std::shared_ptr<SessionManager>(new SessionManager(driver, config));
  instance = ptr;
  return ptr;
}

std::error_code SessionManager::get_source(uint8_t id,
                                           StreamSource& source) const {
  std::shared_lock sources_lock(sources_mutex_);
  auto const it = sources_.find(id);
  if (it == sources_.end()) {
    BOOST_LOG_TRIVIAL(error)
        << "session_manager:: source " << id << " not in use";
    return DaemonErrc::stream_id_not_in_use;
  }
  const auto& info = (*it).second;
  source = get_source_(id, info);
  return std::error_code{};
}

std::error_code SessionManager::get_sink(uint8_t id, StreamSink& sink) const {
  std::shared_lock sinks_lock(sinks_mutex_);
  auto const it = sinks_.find(id);
  if (it == sinks_.end()) {
    BOOST_LOG_TRIVIAL(error)
        << "session_manager:: sink " << id << " not in use";
    return DaemonErrc::stream_id_not_in_use;
  }
  const auto& info = (*it).second;
  sink = get_sink_(id, info);
  return std::error_code{};
}

std::list<StreamSink> SessionManager::get_sinks() const {
  std::shared_lock sinks_lock(sinks_mutex_);
  std::list<StreamSink> sinks_list;
  for (auto const& [id, info] : sinks_) {
    sinks_list.emplace_back(get_sink_(id, info));
  }
  return sinks_list;
}

std::list<StreamSource> SessionManager::get_sources() const {
  std::shared_lock sources_lock(sources_mutex_);
  std::list<StreamSource> sources_list;
  for (auto const& [id, info] : sources_) {
    sources_list.emplace_back(get_source_(id, info));
  }
  return sources_list;
}

StreamSource SessionManager::get_source_(uint8_t id,
                                         const StreamInfo& info) const {
  return {id,
          info.enabled,
          info.stream.m_cName,
          info.io,
          info.stream.m_ui32MaxSamplesPerPacket,
          info.stream.m_cCodec,
          info.stream.m_byTTL,
          info.stream.m_byPayloadType,
          info.stream.m_ucDSCP,
          info.refclk_ptp_traceable,
          {info.stream.m_aui32Routing,
           info.stream.m_aui32Routing + info.stream.m_byNbOfChannels}};
}

StreamSink SessionManager::get_sink_(uint8_t id, const StreamInfo& info) const {
  return {id,
          info.stream.m_cName,
          info.io,
          info.sink_use_sdp,
          info.sink_source,
          info.sink_sdp,
          info.stream.m_ui32PlayOutDelay,
          info.ignore_refclk_gmid,
          {info.stream.m_aui32Routing,
           info.stream.m_aui32Routing + info.stream.m_byNbOfChannels}};
}

bool SessionManager::load_status() {
  if (config_->get_status_file().empty()) {
    return true;
  }

  std::ifstream jsonstream(config_->get_status_file());
  if (!jsonstream) {
    BOOST_LOG_TRIVIAL(fatal) << "session_manager:: cannot load status file "
                             << config_->get_status_file();
    return false;
  }

  std::list<StreamSource> sources_list;
  std::list<StreamSink> sinks_list;

  try {
    json_to_streams(jsonstream, sources_list, sinks_list);
  } catch (const std::runtime_error& e) {
    BOOST_LOG_TRIVIAL(fatal)
        << "session_manager:: cannot parse status file " << e.what();
    return false;
  }

  for (auto const& source : sources_list) {
    add_source(source);
  }
  for (auto const& sink : sinks_list) {
    add_sink(sink);
  }

  return true;
}

bool SessionManager::save_status() {
  if (config_->get_status_file().empty()) {
    return true;
  }

  std::ofstream jsonstream(config_->get_status_file());
  if (!jsonstream) {
    BOOST_LOG_TRIVIAL(fatal) << "session_manager:: cannot save to status file "
                             << config_->get_status_file();
    return false;
  }
  jsonstream << streams_to_json(get_sources(), get_sinks());
  BOOST_LOG_TRIVIAL(info) << "session_manager:: status file saved";

  return true;
}

static std::array<uint8_t, 6> get_mcast_mac_addr(uint32_t mcast_ip) {
  // As defined by IANA, the most significant 24 bits of an IPv4 multicast
  // MAC address are 0x01005E.  // Bit 25 is 0, and the other 23 bits are the
  // least significant 23 bits of an IPv4 multicast address.
  return {0x01,
          0x00,
          0x5e,
          static_cast<uint8_t>((mcast_ip >> 16) & 0x7F),
          static_cast<uint8_t>(mcast_ip >> 8),
          static_cast<uint8_t>(mcast_ip)};
}

uint8_t SessionManager::get_source_id(const std::string& name) const {
  const auto it = source_names_.find(name);
  return it != source_names_.end() ? it->second : (stream_id_max + 1);
}

void SessionManager::add_source_observer(ObserverType type, Observer cb) {
  switch (type) {
    case ObserverType::add_source:
      add_source_observers.push_back(cb);
      break;
    case ObserverType::remove_source:
      remove_source_observers.push_back(cb);
      break;
    case ObserverType::update_source:
      update_source_observers.push_back(cb);
      break;
  }
}

void SessionManager::on_add_source(const StreamSource& source,
                                   const StreamInfo& info) {
  for (auto cb : add_source_observers) {
    cb(source.id, source.name, get_source_sdp_(source.id, info));
  }
  igmp_.join(config_->get_ip_addr_str(),
             ip::address_v4(info.stream.m_ui32DestIP).to_string());
  source_names_[source.name] = source.id;
}

void SessionManager::on_remove_source(const StreamInfo& info) {
  for (auto cb : remove_source_observers) {
    cb(info.stream.m_uiId, info.stream.m_cName, {});
  }
  igmp_.leave(config_->get_ip_addr_str(),
              ip::address_v4(info.stream.m_ui32DestIP).to_string());
  source_names_.erase(info.stream.m_cName);
}

std::error_code SessionManager::add_source(const StreamSource& source) {
  if (source.id > stream_id_max) {
    BOOST_LOG_TRIVIAL(error) << "session_manager:: source id "
                             << std::to_string(source.id) << " is not valid";
    return DaemonErrc::invalid_stream_id;
  }

  StreamInfo info;
  memset(&info.stream, 0, sizeof info.stream);
  info.stream.m_bSource = 1;  // source
  info.stream.m_ui32CRTP_stream_info_sizeof = sizeof(info.stream);
  strncpy(info.stream.m_cName, source.name.c_str(),
          sizeof(info.stream.m_cName) - 1);
  info.stream.m_ucDSCP = source.dscp;  // IPv4 DSCP
  info.stream.m_byTTL = source.ttl;
  info.stream.m_byPayloadType = source.payload_type;
  info.stream.m_byWordLength = get_codec_word_lenght(source.codec);
  info.stream.m_byNbOfChannels = source.map.size();
  strncpy(info.stream.m_cCodec, source.codec.c_str(),
          sizeof(info.stream.m_cCodec) - 1);
  info.stream.m_ui32MaxSamplesPerPacket = source.max_samples_per_packet;
  info.stream.m_ui32SamplingRate =
      driver_->get_current_sample_rate();  // last set from driver or config
  info.stream.m_uiId = source.id;
  info.stream.m_ui32RTCPSrcIP = config_->get_ip_addr();
  info.stream.m_ui32SrcIP = config_->get_ip_addr();  // only for Source
  info.stream.m_ui32DestIP =
      ip::address_v4::from_string(config_->get_rtp_mcast_base().c_str())
          .to_ulong() +
      source.id;
  info.stream.m_usSrcPort = config_->get_rtp_port();
  info.stream.m_usDestPort = config_->get_rtp_port();
  info.stream.m_ui32SSRC = rand() % 65536;  // use random number
  std::copy(source.map.begin(), source.map.end(), info.stream.m_aui32Routing);
  auto mcast_mac_addr = get_mcast_mac_addr(info.stream.m_ui32DestIP);
  std::copy(std::begin(mcast_mac_addr), std::end(mcast_mac_addr),
            info.stream.m_ui8DestMAC);

  info.refclk_ptp_traceable = source.refclk_ptp_traceable;
  info.enabled = source.enabled;
  info.io = source.io;
  // info.m_ui32PlayOutDelay = 0; // only for Sink

  std::unique_lock sources_lock(sources_mutex_);
  auto const it = sources_.find(source.id);
  if (it != sources_.end()) {
    BOOST_LOG_TRIVIAL(info)
        << "session_manager:: source id " << std::to_string(source.id)
        << " is in use, updating";
    // remove previous stream if enabled
    if ((*it).second.enabled) {
      (void)driver_->remove_rtp_stream((*it).second.handle);
      on_remove_source((*it).second);
    }
  } else if (source_names_.find(source.name) != source_names_.end()) {
    BOOST_LOG_TRIVIAL(error)
        << "session_manager:: source name " << source.name << " is in use";
    return DaemonErrc::stream_name_in_use;
  }

  std::error_code ret;
  if (info.enabled) {
    ret = driver_->add_rtp_stream(info.stream, info.handle);
    if (ret) {
      if (it != sources_.end()) {
        /* update operation failed */
        sources_.erase(source.id);
      }
      return ret;
    }
    on_add_source(source, info);
  }

  // update source map
  sources_[source.id] = info;
  BOOST_LOG_TRIVIAL(info) << "session_manager:: added source "
                          << std::to_string(source.id) << " " << info.handle;
  return ret;
}

std::string SessionManager::get_removed_source_sdp_(uint32_t id,
                                                    uint32_t addr) const {
  std::string sdp("o=- " + std::to_string(id) + " 0 IN IP4 " +
                  ip::address_v4(addr).to_string() + "\n");
  return sdp;
}

std::string SessionManager::get_source_sdp_(uint32_t id,
                                            const StreamInfo& info) const {
  std::shared_lock ptp_lock(ptp_mutex_);
  uint32_t sample_rate = driver_->get_current_sample_rate();

  // need a 12 digit precision for ptime
  std::ostringstream ss_ptime;
  ss_ptime.precision(12);
  ss_ptime << std::fixed
           << static_cast<double>(info.stream.m_ui32MaxSamplesPerPacket) *
                  1000 / static_cast<double>(sample_rate);
  std::string ptime = ss_ptime.str();
  // remove trailing zeros or dot
  ptime.erase(ptime.find_last_not_of("0.") + 1, std::string::npos);

  // build SDP
  std::stringstream ss;
  ss << "v=0\n"
     << "o=- " << static_cast<unsigned>(id) << " 0 IN IP4 "
     << ip::address_v4(info.stream.m_ui32SrcIP).to_string() << "\n"
     << "s=" << get_node_id(config_->get_ip_addr()) << " "
     << info.stream.m_cName << "\n"
     << "c=IN IP4 " << ip::address_v4(info.stream.m_ui32DestIP).to_string()
     << "/" << static_cast<unsigned>(info.stream.m_byTTL) << "\n"
     << "t=0 0\n"
     << "a=clock-domain:PTPv2 " << static_cast<unsigned>(ptp_config_.domain)
     << "\n"
     << "m=audio " << info.stream.m_usSrcPort << " RTP/AVP "
     << static_cast<unsigned>(info.stream.m_byPayloadType) << "\n"
     << "c=IN IP4 " << ip::address_v4(info.stream.m_ui32DestIP).to_string()
     << "/" << static_cast<unsigned>(info.stream.m_byTTL) << "\n"
     << "a=rtpmap:" << static_cast<unsigned>(info.stream.m_byPayloadType) << " "
     << info.stream.m_cCodec << "/" << sample_rate << "/"
     << static_cast<unsigned>(info.stream.m_byNbOfChannels) << "\n"
     << "a=sync-time:0\n"
     << "a=framecount:" << info.stream.m_ui32MaxSamplesPerPacket << "\n"
     << "a=ptime:" << ptime << "\n"
     << "a=mediaclk:direct=0\n";
  if (info.refclk_ptp_traceable) {
    ss << "a=ts-refclk:ptp=traceable\n";
  } else {
    ss << "a=ts-refclk:ptp=IEEE1588-2008:" << ptp_status_.gmid << ":"
       << static_cast<unsigned>(ptp_config_.domain) << "\n";
  }
  ss << "a=recvonly\n";

  return ss.str();
}

std::error_code SessionManager::get_source_sdp(uint32_t id,
                                               std::string& sdp) const {
  std::shared_lock sources_lock(sources_mutex_);
  auto const it = sources_.find(id);
  if (it == sources_.end()) {
    BOOST_LOG_TRIVIAL(error)
        << "session_manager:: source " << id << " not in use";
    return DaemonErrc::stream_id_not_in_use;
  }
  const auto& info = (*it).second;
  sdp = get_source_sdp_(id, info);
  return std::error_code{};
}

std::error_code SessionManager::remove_source(uint32_t id) {
  if (id > stream_id_max) {
    BOOST_LOG_TRIVIAL(error) << "session_manager:: source id "
                             << std::to_string(id) << " is not valid";
    return DaemonErrc::invalid_stream_id;
  }

  std::unique_lock sources_lock(sources_mutex_);
  auto const it = sources_.find(id);
  if (it == sources_.end()) {
    BOOST_LOG_TRIVIAL(error)
        << "session_manager:: source " << id << " not in use";
    return DaemonErrc::stream_id_not_in_use;
  }

  std::error_code ret;
  const auto& info = (*it).second;
  if (info.enabled) {
    ret = driver_->remove_rtp_stream(info.handle);
    if (!ret) {
      on_remove_source(info);
    }
  }
  if (!ret) {
    sources_.erase(id);
  }

  return ret;
}

uint8_t SessionManager::get_sink_id(const std::string& name) const {
  const auto it = sink_names_.find(name);
  return it != sink_names_.end() ? it->second : (stream_id_max + 1);
}

void SessionManager::on_add_sink(const StreamSink& sink,
                                 const StreamInfo& info) {
  igmp_.join(config_->get_ip_addr_str(),
             ip::address_v4(info.stream.m_ui32DestIP).to_string());
  sink_names_[sink.name] = sink.id;
}

void SessionManager::on_remove_sink(const StreamInfo& info) {
  igmp_.leave(config_->get_ip_addr_str(),
              ip::address_v4(info.stream.m_ui32DestIP).to_string());
  sink_names_.erase(info.stream.m_cName);
}

std::error_code SessionManager::add_sink(const StreamSink& sink) {
  if (sink.id > stream_id_max) {
    BOOST_LOG_TRIVIAL(error) << "session_manager:: sink id "
                             << std::to_string(sink.id) << " is not valid";
    return DaemonErrc::invalid_stream_id;
  }

  StreamInfo info;
  memset(&info.stream, 0, sizeof info.stream);
  info.stream.m_bSource = 0;  // sink
  info.stream.m_ui32CRTP_stream_info_sizeof = sizeof(info.stream);
  strncpy(info.stream.m_cName, sink.name.c_str(),
          sizeof(info.stream.m_cName) - 1);
  info.stream.m_uiId = sink.id;
  info.stream.m_byNbOfChannels = sink.map.size();
  std::copy(sink.map.begin(), sink.map.end(), info.stream.m_aui32Routing);
  info.stream.m_ui32PlayOutDelay = sink.delay;
  info.stream.m_ui32RTCPSrcIP = config_->get_ip_addr();
  info.ignore_refclk_gmid = sink.ignore_refclk_gmid;
  info.io = sink.io;

  if (!sink.use_sdp) {
    auto const [ok, protocol, host, port, path] = parse_url(sink.source);
    if (!ok) {
      BOOST_LOG_TRIVIAL(error)
          << "session_manager:: cannot parse URL " << sink.source;
      return DaemonErrc::invalid_url;
    }

    std::string sdp;
    if (boost::iequals(protocol, "http")) {
      httplib::Client cli(host.c_str(),
                          !atoi(port.c_str()) ? 80 : atoi(port.c_str()));
      cli.set_timeout_sec(10);
      auto res = cli.Get(path.c_str());
      if (!res) {
        BOOST_LOG_TRIVIAL(error)
            << "session_manager:: annot retrieve SDP from URL " << sink.source;
        return DaemonErrc::cannot_retrieve_sdp;
      }
      if (res->status != 200) {
        BOOST_LOG_TRIVIAL(error)
            << "session_manager:: cannot retrieve SDP from URL " << sink.source
            << " server reply " << res->status;
        return DaemonErrc::cannot_retrieve_sdp;
      }
      sdp = std::move(res->body);
    } else if (boost::iequals(protocol, "rtsp")) {
      auto res = RtspClient::describe(path, host, port);
      if (!res.first) {
        BOOST_LOG_TRIVIAL(error)
            << "session_manager:: cannot retrieve SDP from URL " << sink.source;
        return DaemonErrc::cannot_retrieve_sdp;
      }
      sdp = std::move(res.second.sdp);
    } else {
      BOOST_LOG_TRIVIAL(error)
          << "session_manager:: unsupported protocol in URL " << sink.source;
      return DaemonErrc::invalid_url;
    }

    BOOST_LOG_TRIVIAL(info)
        << "session_manager:: SDP from URL " << sink.source << " :\n"
        << sdp;

    if (!parse_sdp(sdp, info)) {
      return DaemonErrc::cannot_parse_sdp;
    }

    info.sink_sdp = std::move(sdp);
  } else {
    BOOST_LOG_TRIVIAL(info) << "session_manager:: using SDP " << std::endl
                            << sink.sdp;
    if (!parse_sdp(sink.sdp, info)) {
      return DaemonErrc::cannot_parse_sdp;
    }

    info.sink_sdp = std::move(sink.sdp);
  }
  info.sink_source = sink.source;
  info.sink_use_sdp = true;  // save back and use with SDP file

  info.stream.m_ui32FrameSize = info.stream.m_ui32MaxSamplesPerPacket;
  if (!info.stream.m_ui32FrameSize) {
    // if not from SDP use config
    info.stream.m_ui32FrameSize = config_->get_max_tic_frame_size();
  }

  BOOST_LOG_TRIVIAL(info) << "session_manager:: sink frame size "
                          << info.stream.m_ui32FrameSize;
  BOOST_LOG_TRIVIAL(info) << "session_manager:: playout delay "
                          << info.stream.m_ui32PlayOutDelay;

  auto mcast_mac_addr = get_mcast_mac_addr(info.stream.m_ui32DestIP);
  std::copy(std::begin(mcast_mac_addr), std::end(mcast_mac_addr),
            info.stream.m_ui8DestMAC);

  std::unique_lock sinks_lock(sinks_mutex_);
  auto const it = sinks_.find(sink.id);
  if (it != sinks_.end()) {
    BOOST_LOG_TRIVIAL(info)
        << "session_manager:: sink id " << std::to_string(sink.id)
        << " is in use, updating";
    // remove previous stream
    (void)driver_->remove_rtp_stream((*it).second.handle);
    on_remove_sink((*it).second);
  } else if (sink_names_.find(sink.name) != sink_names_.end()) {
    BOOST_LOG_TRIVIAL(error)
        << "session_manager:: sink name " << sink.name << " is in use";
    return DaemonErrc::stream_name_in_use;
  }

  auto ret = driver_->add_rtp_stream(info.stream, info.handle);
  if (ret) {
    if (it != sinks_.end()) {
      /* update operation failed */
      sinks_.erase(sink.id);
    }
    return ret;
  }

  on_add_sink(sink, info);
  // update sinks map
  sinks_[sink.id] = info;
  BOOST_LOG_TRIVIAL(info) << "session_manager:: added sink "
                          << std::to_string(sink.id) << " " << info.handle;
  return ret;
}

std::error_code SessionManager::remove_sink(uint32_t id) {
  if (id > stream_id_max) {
    BOOST_LOG_TRIVIAL(error) << "session_manager:: sink id "
                             << std::to_string(id) << " is not valid";
    return DaemonErrc::stream_id_in_use;
  }

  std::unique_lock sinks_lock(sinks_mutex_);
  auto const it = sinks_.find(id);
  if (it == sinks_.end()) {
    BOOST_LOG_TRIVIAL(error)
        << "session_manager:: sink " << id << " not in use";
    return DaemonErrc::stream_id_not_in_use;
  }

  const auto& info = (*it).second;
  auto ret = driver_->remove_rtp_stream(info.handle);
  if (!ret) {
    igmp_.leave(config_->get_ip_addr_str(),
                ip::address_v4(info.stream.m_ui32DestIP).to_string());
    on_remove_sink(info);
    sinks_.erase(id);
  }

  return ret;
}

std::error_code SessionManager::get_sink_status(
    uint32_t id,
    SinkStreamStatus& sink_status) const {
  if (id > stream_id_max) {
    BOOST_LOG_TRIVIAL(error) << "session_manager:: sink id "
                             << std::to_string(id) << " is not valid";
    return DaemonErrc::invalid_stream_id;
  }

  std::shared_lock sinks_lock(sinks_mutex_);
  auto const it = sinks_.find(id);
  if (it == sinks_.end()) {
    BOOST_LOG_TRIVIAL(error)
        << "session_manager:: sink " << id << " not in use";
    return DaemonErrc::stream_id_not_in_use;
  }

  TRTP_stream_status status;
  const auto& info = (*it).second;
  auto ret = driver_->get_rtp_stream_status(info.handle, status);
  if (!ret) {
    sink_status.is_rtp_seq_id_error = status.u.flags & 0x01;
    sink_status.is_rtp_ssrc_error = status.u.flags & 0x02;
    sink_status.is_rtp_payload_type_error = status.u.flags & 0x04;
    sink_status.is_rtp_sac_error = status.u.flags & 0x08;
    sink_status.is_receiving_rtp_packet = status.u.flags & 0x10;
    sink_status.is_muted = status.u.flags & 0x20;
    sink_status.is_some_muted = status.u.flags & 0x40;
    sink_status.is_all_muted = status.u.flags & 0x80;
    sink_status.min_time = status.sink_min_time;
  }

  return ret;
}

std::error_code SessionManager::set_ptp_config(const PTPConfig& config) {
  TPTPConfig ptp_config;
  ptp_config.ui8Domain = config.domain;
  ptp_config.ui8DSCP = config.dscp;
  auto ret = driver_->set_ptp_config(ptp_config);
  if (!ret) {
    std::unique_lock ptp_lock(ptp_mutex_);
    ptp_config_ = config;
  }
  return ret;
}

void SessionManager::get_ptp_config(PTPConfig& config) const {
  std::shared_lock ptp_lock(ptp_mutex_);
  config = ptp_config_;
}

void SessionManager::get_ptp_status(PTPStatus& status) const {
  std::shared_lock ptp_lock(ptp_mutex_);
  status = ptp_status_;
}

size_t SessionManager::process_sap() {
  size_t sdp_len_sum = 0;
  // set to contain sources currently announced
  std::set<uint32_t> active_sources;

  // announce all active sources
  std::shared_lock sources_lock(sources_mutex_);
  for (auto const& [id, info] : sources_) {
    if (info.enabled) {
      // retrieve current active source SDP
      std::string sdp = get_source_sdp_(id, info);
      // compute source 16bit crc
      uint16_t msg_crc =
          crc16(reinterpret_cast<const uint8_t*>(sdp.c_str()), sdp.length());
      // compute source hash
      uint32_t msg_id_hash = (static_cast<uint32_t>(id) << 16) + msg_crc;
      // add/update this source in the announced sources
      announced_sources_[msg_id_hash] = info.stream.m_ui32RTCPSrcIP;
      // add this source to the currently active sources
      active_sources.insert(msg_id_hash);
      // remove this source from deleted sources (if present)
      deleted_sources_count_.erase(msg_id_hash);
      // send announcement for this source
      sap_.announcement(msg_crc, info.stream.m_ui32RTCPSrcIP, sdp);
      // update amount of byte sent
      sdp_len_sum += sdp.length();
    }
  }

  // check for sources that are no longer announced and send deletion/s
  for (auto const& [msg_id_hash, src_addr] : announced_sources_) {
    // check if this source is no longer announced
    if (active_sources.find(msg_id_hash) == active_sources.end()) {
      // retrieve deleted source SDP
      std::string sdp = get_removed_source_sdp_(msg_id_hash >> 16, src_addr);
      // send deletion for this source
      sap_.deletion(static_cast<uint16_t>(msg_id_hash), src_addr, sdp);
      // update amount of byte sent
      sdp_len_sum += sdp.length();
      // increase count
      deleted_sources_count_[msg_id_hash]++;
    }
  }

  // remove all deleted sources announced SAP::max_deletions times
  std::experimental::erase_if(announced_sources_, [this](auto source) {
    const auto& msg_id_hash = source.first;

    if (this->deleted_sources_count_[msg_id_hash] >= SAP::max_deletions) {
      // remove from deleted sources
      this->deleted_sources_count_.erase(msg_id_hash);
      // remove from announced sources
      return true;
    }
    return false;
  });

  return sdp_len_sum;
}

void SessionManager::on_update_sources() {
  // trigger sources SDP file update
  std::shared_lock sources_lock(sources_mutex_);
  for (auto const& [id, info] : sources_) {
    for (auto cb : update_source_observers) {
      cb(id, info.stream.m_cName, get_source_sdp_(id, info));
    }
  }
}

void SessionManager::on_ptp_status_locked() {
  // set sample rate, this may require seconds
  (void)driver_->set_sample_rate(driver_->get_current_sample_rate());
}

using namespace std::chrono;
using second_t = duration<double, std::ratio<1> >;

bool SessionManager::worker() {
  TPTPConfig ptp_config;
  TPTPStatus ptp_status;
  auto sap_timepoint = steady_clock::now();
  auto ptp_timepoint = steady_clock::now();
  int sap_interval = 1;
  int ptp_interval = 0;
  uint32_t sample_rate = driver_->get_current_sample_rate();

  sap_.set_multicast_interface(config_->get_ip_addr_str());

  // join PTP multicast addresses
  igmp_.join(config_->get_ip_addr_str(), ptp_primary_mcast_addr);

  while (running_) {
    // check if it's time to update the PTP status
    if ((duration_cast<second_t>(steady_clock::now() - ptp_timepoint).count()) >
        ptp_interval) {
      ptp_timepoint = steady_clock::now();
      if (driver_->get_ptp_config(ptp_config) ||
          driver_->get_ptp_status(ptp_status)) {
        BOOST_LOG_TRIVIAL(error)
            << "session_manager:: failed to retrieve PTP clock info";
        // return false;
      } else {
        char ptp_clock_id[24];
        uint8_t* pui64GMID = reinterpret_cast<uint8_t*>(&ptp_status.ui64GMID);
        snprintf(ptp_clock_id, sizeof(ptp_clock_id),
                 "%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X", pui64GMID[0],
                 pui64GMID[1], pui64GMID[2], pui64GMID[3], pui64GMID[4],
                 pui64GMID[5], pui64GMID[6], pui64GMID[7]);

        bool ptp_changed_gmid = false;
        bool ptp_changed_to_locked = false;
        // update PTP clock status
        ptp_mutex_.lock();
        // update status
        if (ptp_status_.gmid != ptp_clock_id) {
          ptp_status_.gmid = ptp_clock_id;
          ptp_changed_gmid = true;
        }
        ptp_status_.jitter = ptp_status.i32Jitter;
        std::string new_ptp_status;
        switch (ptp_status.nPTPLockStatus) {
          case PTPLS_UNLOCKED:
            new_ptp_status = "unlocked";
            break;
          case PTPLS_LOCKING:
            new_ptp_status = "locking";
            break;
          case PTPLS_LOCKED:
            new_ptp_status = "locked";
            break;
        }

        if (ptp_status_.status != new_ptp_status) {
          BOOST_LOG_TRIVIAL(info)
              << "session_manager:: new PTP clock status " << new_ptp_status;
          ptp_status_.status = new_ptp_status;
          if (new_ptp_status == "locked") {
            ptp_changed_to_locked = true;
          }
        }
        // end update PTP clock status
        ptp_mutex_.unlock();

        if (ptp_changed_to_locked) {
          on_ptp_status_locked();
        }

        if (ptp_changed_gmid ||
            sample_rate != driver_->get_current_sample_rate()) {
          /* master clock id changed or sample rate changed
           * we need to update all the sources */
          sample_rate = driver_->get_current_sample_rate();
          on_update_sources();
        }
      }
      ptp_interval = 10;
    }

    // check if it's time to send sap announcements
    if ((duration_cast<second_t>(steady_clock::now() - sap_timepoint).count()) >
        sap_interval) {
      sap_timepoint = steady_clock::now();

      auto sdp_len_sum = process_sap();

      if (config_->get_sap_interval()) {
        // if announcement interval specified in configuration
        sap_interval = config_->get_sap_interval();
      } else {
        // compute next announcement interval
        sap_interval = std::max(static_cast<size_t>(SAP::min_interval),
                                sdp_len_sum * 8 / SAP::bandwidth_limit);
        sap_interval +=
            (std::rand() % (sap_interval * 2 / 3)) - (sap_interval / 3);
      }

      BOOST_LOG_TRIVIAL(info) << "session_manager:: next SAP announcements in "
                              << sap_interval << " secs";
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // at end, send deletion for all announced sources
  for (auto const& [msg_id_hash, src_addr] : announced_sources_) {
    // retrieve deleted source SDP
    std::string sdp = get_removed_source_sdp_(msg_id_hash >> 16, src_addr);
    // send deletion for this source
    sap_.deletion(static_cast<uint16_t>(msg_id_hash), src_addr, sdp);
  }

  // leave PTP multicast addresses
  igmp_.leave(config_->get_ip_addr_str(), ptp_primary_mcast_addr);

  return true;
}