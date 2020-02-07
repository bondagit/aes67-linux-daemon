//
//  session_manager.hpp
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

#ifndef _SESSION_MANAGER_HPP_
#define _SESSION_MANAGER_HPP_

#include <future>
#include <map>
#include <shared_mutex>
#include <thread>

#include "config.hpp"
#include "driver_manager.hpp"
#include "igmp.hpp"
#include "sap.hpp"

struct StreamSource {
  uint8_t id{0};
  bool enabled{false};
  std::string name;
  std::string io;
  std::vector<uint8_t> map;
  uint32_t max_samples_per_packet{0};
  std::string codec;
  uint8_t ttl{0};
  uint8_t payload_type{0};
  uint8_t dscp{0};
  bool refclk_ptp_traceable{false};
};

struct StreamSink {
  uint8_t id;
  std::string name;
  std::string io;
  bool use_sdp{false};
  std::string source;
  std::string sdp;
  uint32_t delay{0};
  bool ignore_refclk_gmid{false};
  std::vector<uint8_t> map;
};

struct SinkStreamStatus {
  bool is_rtp_seq_id_error{false};
  bool is_rtp_ssrc_error{false};
  bool is_rtp_payload_type_error{false};
  bool is_rtp_sac_error{false};
  bool is_receiving_rtp_packet{false};
  bool is_muted{false};
  bool is_some_muted{false};
  int min_time{0};
};

struct PTPConfig {
  uint8_t domain{0};
  uint8_t dscp{0};
};

struct PTPStatus {
  std::string status;
  std::string gmid;
  int32_t jitter{0};
};

struct StreamInfo {
  TRTP_stream_info stream;
  uint64_t handle{0};
  bool enabled{0};
  bool refclk_ptp_traceable{false};
  bool ignore_refclk_gmid{false};
  std::string io;
  bool sink_use_sdp{true};
  std::string sink_source;
  std::string sink_sdp;
};

class SessionManager {
 public:
  constexpr static uint8_t stream_id_max = 63;

  static std::shared_ptr<SessionManager> create(
      std::shared_ptr<DriverManager> driver,
      std::shared_ptr<Config> config);
  SessionManager(const SessionManager&) = delete;
  SessionManager& operator=(const SessionManager&) = delete;
  virtual ~SessionManager(){ stop(); };

  // session manager interface
  bool start() {
    if (!running_) {
      running_ = true;
      res_ = std::async(std::launch::async, &SessionManager::worker, this);
    }
    return true;
  }

  bool stop() {
    if (running_) {
      running_ = false;
      auto ret = res_.get();
      for (auto source : get_sources()) {
        remove_source(source.id);
      }
      for (auto sink : get_sinks()) {
        remove_sink(sink.id);
      }
      return ret;
    }
    return true;
  }

  std::error_code add_source(const StreamSource& source);
  std::error_code get_source(uint8_t id, StreamSource& source) const;
  std::list<StreamSource> get_sources() const;
  std::error_code get_source_sdp(uint32_t id, std::string& sdp) const;
  std::error_code remove_source(uint32_t id);

  std::error_code add_sink(const StreamSink& sink);
  std::error_code get_sink(uint8_t id, StreamSink& sink) const;
  std::list<StreamSink> get_sinks() const;
  std::error_code get_sink_status(uint32_t id, SinkStreamStatus& status) const;
  std::error_code remove_sink(uint32_t id);

  std::error_code set_ptp_config(const PTPConfig& config);
  void get_ptp_config(PTPConfig& config) const;
  void get_ptp_status(PTPStatus& status) const;

  bool load_status();
  bool save_status();

  size_t process_sap();

 protected:
  constexpr static const char ptp_primary_mcast_addr[] = "224.0.1.129";
  constexpr static const char ptp_pdelay_mcast_addr[] = "224.0.1.107";

  std::string get_removed_source_sdp_(uint32_t id, uint32_t src_addr) const;
  std::string get_source_sdp_(uint32_t id, const StreamInfo& info) const;
  StreamSource get_source_(uint8_t id, const StreamInfo& info) const;
  StreamSink get_sink_(uint8_t id, const StreamInfo& info) const;

  bool parse_sdp(const std::string sdp, StreamInfo& info) const;
  bool worker();
  // singleton, use create() to build
  SessionManager(std::shared_ptr<DriverManager> driver,
                 std::shared_ptr<Config> config)
      : driver_(driver), config_(config){
    ptp_config_.domain = config->get_ptp_domain();
    ptp_config_.dscp = config->get_ptp_dscp();
  };

  std::shared_ptr<DriverManager> driver_;
  std::shared_ptr<Config> config_;
  std::future<bool> res_;
  std::atomic_bool running_{false};

  /* current sources */
  std::map<uint8_t /* id */, StreamInfo> sources_;
  mutable std::shared_mutex sources_mutex_;

  /* current sinks */
  std::map<uint8_t /* id */, StreamInfo> sinks_;
  mutable std::shared_mutex sinks_mutex_;

  /* current announced sources */
  std::map<uint32_t /* msg_id_hash */, uint32_t /* src_addr */>
      announced_sources_;

  /* number of deletions sent for a  a deleted source */
  std::unordered_map<uint32_t /* msg_id_hash */, int /* count */> 
      deleted_sources_count_;

  PTPConfig ptp_config_;
  PTPStatus ptp_status_;
  mutable std::shared_mutex ptp_mutex_;

  SAP sap_;
  IGMP igmp_;
};

#endif
