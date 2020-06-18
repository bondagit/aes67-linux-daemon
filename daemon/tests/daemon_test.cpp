//
//  daemon_test.cpp
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
#include <httplib.h>
#include <boost/foreach.hpp>
#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <set>

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE DaemonTest
#include <boost/test/unit_test.hpp>

#if !(defined(__arm__) || defined(__arm64__))
//#define _MEMORY_CHECK_
#endif

constexpr static const char g_daemon_address[] = "127.0.0.1";
constexpr static uint16_t g_daemon_port = 9999;
constexpr static const char g_sap_address[] = "224.2.127.254";
constexpr static uint16_t g_sap_port = 9875;
constexpr static uint16_t g_udp_size = 1024;
constexpr static uint16_t g_sap_header_len = 24;
constexpr static uint16_t g_stream_num_max = 64;

using namespace boost::process;
using namespace boost::asio::ip;
using namespace boost::asio;

struct DaemonInstance {
  DaemonInstance() {
    BOOST_TEST_MESSAGE("Starting up test daemon instance ...");
    int retry = 10;
    while (retry-- && daemon_.running()) {
      BOOST_TEST_MESSAGE("Checking daemon instance ...");
      httplib::Client cli(g_daemon_address, g_daemon_port);
      auto res = cli.Get("/");
      if (res) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    BOOST_REQUIRE(daemon_.running());
    ok = true;
  }

  ~DaemonInstance() {
    BOOST_TEST_MESSAGE("Tearing down test daemon instance...");
    auto pid = daemon_.native_handle();
    /* trigger normal daemon termination */
    kill(pid, SIGTERM);
    daemon_.wait();
    BOOST_REQUIRE_MESSAGE(!daemon_.exit_code(), "daemon exited normally");
    ok = false;
  }

  static bool is_ok() { return ok; }

 private:
  child daemon_ {
#if defined _MEMORY_CHECK_
    search_path("valgrind"),
#endif
        "../aes67-daemon", "-c", "daemon.conf", "-p", "9999"
  };
  inline static bool ok{false};
};

BOOST_TEST_GLOBAL_FIXTURE(DaemonInstance);

struct Client {
  Client() {
    socket_.open(listen_endpoint_.protocol());
    socket_.set_option(udp::socket::reuse_address(true));
    socket_.bind(listen_endpoint_);
    socket_.set_option(
        multicast::join_group(address::from_string(g_sap_address).to_v4(),
                              address::from_string(g_daemon_address).to_v4()));

    cli_.set_connection_timeout(30);
    cli_.set_read_timeout(30);
    cli_.set_write_timeout(30);
  }

  bool is_alive() {
    auto res = cli_.Get("/");
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return (res->status == 200);
  }

  std::pair<bool, std::string> get_config() {
    auto res = cli_.Get("/api/config");
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return {res->status == 200, res->body};
  }

  bool set_ptp_config(int domain, int dscp) {
    std::ostringstream os;
    os << "{ \"domain\": " << domain << ", \"dscp\": " << dscp << " }";
    auto res = cli_.Post("/api/ptp/config", os.str(), "application/json");
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return (res->status == 200);
  }

  std::pair<bool, std::string> get_ptp_status() {
    auto res = cli_.Get("/api/ptp/status");
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return {res->status == 200, res->body};
  }

  std::pair<bool, std::string> get_ptp_config() {
    auto res = cli_.Get("/api/ptp/config");
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return {res->status == 200, res->body};
  }

  bool add_source(int id) {
    std::string json = R"(
{
  "enabled": true,
  "name": "ALSA",
  "io": "Audio Device",
  "map": [ 0, 1 ],
  "max_samples_per_packet": 48,
  "codec": "L16",
  "ttl": 15,
  "payload_type": 98,
  "dscp": 34,
  "refclk_ptp_traceable": false
}
    )";

    boost::replace_first(json, "ALSA", "ALSA " + std::to_string(id));
    std::string url = std::string("/api/source/") + std::to_string(id);
    auto res = cli_.Put(url.c_str(), json, "application/json");
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return (res->status == 200);
  }

  bool update_source(int id) {
    std::string json = R"(
{
  "enabled": true,
  "name": "ALSA",
  "io": "Audio Device",
  "map": [ 0, 1 ],
  "max_samples_per_packet": 192,
  "codec": "L24",
  "ttl": 15,
  "payload_type": 98,
  "dscp": 34,
  "refclk_ptp_traceable": false
}
    )";

    boost::replace_first(json, "ALSA", "ALSA " + std::to_string(id));
    std::string url = std::string("/api/source/") + std::to_string(id);
    auto res = cli_.Put(url.c_str(), json, "application/json");
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return (res->status == 200);
  }

  std::pair<bool, std::string> get_source_sdp(int id) {
    std::string url = std::string("/api/source/sdp/") + std::to_string(id);
    auto res = cli_.Get(url.c_str());
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return {res->status == 200, res->body};
  }

  std::pair<bool, std::string> get_sink_status(int id) {
    std::string url = std::string("/api/sink/status/") + std::to_string(id);
    auto res = cli_.Get(url.c_str());
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return {res->status == 200, res->body};
  }

  std::pair<bool, std::string> get_streams() {
    std::string url = std::string("/api/streams");
    auto res = cli_.Get(url.c_str());
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return {res->status == 200, res->body};
  }

  std::pair<bool, std::string> get_sources() {
    std::string url = std::string("/api/sources");
    auto res = cli_.Get(url.c_str());
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return {res->status == 200, res->body};
  }

  std::pair<bool, std::string> get_sinks() {
    std::string url = std::string("/api/sinks");
    auto res = cli_.Get(url.c_str());
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return {res->status == 200, res->body};
  }

  bool remove_source(int id) {
    std::string url = std::string("/api/source/") + std::to_string(id);
    auto res = cli_.Delete(url.c_str());
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return (res->status == 200);
  }

  bool add_sink_sdp(int id) {
    std::string json = R"(
{
  "name": "ALSA",
  "io": "Audio Device",
  "source": "",
  "use_sdp": true,
  "sdp": "v=0\no=- 1 0 IN IP4 10.0.0.12\ns=ALSA (on ubuntu)_1\nc=IN IP4 239.2.0.12/15\nt=0 0\na=clock-domain:PTPv2 0\nm=audio 6004 RTP/AVP 98\nc=IN IP4 239.2.0.12/15\na=rtpmap:98 L16/44100/2\na=sync-time:0\na=framecount:64-192\na=ptime:1.088435374150\na=maxptime:1.088435374150\na=mediaclk:direct=0\na=ts-refclk:ptp=IEEE1588-2008:00-0C-29-FF-FE-0E-90-C8:0\na=recvonly",
  "delay": 1024,
  "ignore_refclk_gmid": true,
  "map": [ 0, 1 ]
}
  )";

    boost::replace_first(json, "ALSA", "ALSA " + std::to_string(id));
    std::string url = std::string("/api/sink/") + std::to_string(id);
    auto res = cli_.Put(url.c_str(), json, "application/json");
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return (res->status == 200);
  }

  bool add_sink_url(int id) {
    std::string json1 = R"(
{
  "io": "Audio Device",
  "use_sdp": false,
  "sdp": "",
  "delay": 1024,
  "ignore_refclk_gmid": true,
  "map": [ 0, 1 ],
  )";

    std::string json =
        json1 +
        std::string("\"name\": \"ALSA " + std::to_string(id) + "\",\n") +
        std::string("\"source\": \"http://") + g_daemon_address + ":" +
        std::to_string(g_daemon_port) + std::string("/api/source/sdp/") +
        std::to_string(id) + "\"\n}";
    std::string url = std::string("/api/sink/") + std::to_string(id);
    auto res = cli_.Put(url.c_str(), json, "application/json");
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return (res->status == 200);
  }

  bool remove_sink(int id) {
    std::string url = std::string("/api/sink/") + std::to_string(id);
    auto res = cli_.Delete(url.c_str());
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return (res->status == 200);
  }

  bool sap_wait_announcement(int id, const std::string& sdp, int count = 1) {
    char data[g_udp_size];
    while (count-- > 0) {
      BOOST_TEST_MESSAGE("waiting announcement for source " +
                         std::to_string(id));
      std::string sap_sdp;
      do {
        auto len = socket_.receive(boost::asio::buffer(data, g_udp_size));
        if (len <= g_sap_header_len) {
          continue;
        }
        sap_sdp.assign(data + g_sap_header_len, data + len);
      } while (data[0] != 0x20 || sap_sdp != sdp);
      BOOST_CHECK_MESSAGE(true, "SAP announcement SDP and source SDP match");
    }
    return true;
  }

  void sap_wait_all_deletions() {
    char data[g_udp_size];
    std::set<uint8_t> ids;
    while (ids.size() < g_stream_num_max) {
      auto len = socket_.receive(boost::asio::buffer(data, g_udp_size));
      if (len <= g_sap_header_len) {
        continue;
      }
      std::string sap_sdp_(data + g_sap_header_len, data + len);
      if (data[0] == 0x24 && sap_sdp_.length() > 3) {
        // o=- 56 0 IN IP4 127.0.0.1
        ids.insert(std::atoi(sap_sdp_.c_str() + 3));
        BOOST_TEST_MESSAGE("waiting deletion for " +
                           std::to_string(g_stream_num_max - ids.size()) +
                           " sources");
      }
    }
  }

  bool sap_wait_deletion(int id, const std::string& sdp, int count = 1) {
    char data[g_udp_size];
    while (count-- > 0) {
      BOOST_TEST_MESSAGE("waiting deletion for source " + std::to_string(id));
      std::string sap_sdp;
      do {
        auto len = socket_.receive(boost::asio::buffer(data, g_udp_size));
        if (len <= g_sap_header_len) {
          continue;
        }
        sap_sdp.assign(data + g_sap_header_len, data + len);
      } while (data[0] != 0x24 || sdp.find(sap_sdp) == std::string::npos);
      BOOST_CHECK_MESSAGE(true, "SAP deletion SDP matches");
    }
    return true;
  }

  std::pair<bool, std::string> get_remote_sap_sources() {
    std::string url = std::string("/api/browse/sources/sap");
    auto res = cli_.Get(url.c_str());
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return {res->status == 200, res->body};
  }

  std::pair<bool, std::string> get_remote_mdns_sources() {
    std::string url = std::string("/api/browse/sources/mdns");
    auto res = cli_.Get(url.c_str());
    BOOST_REQUIRE_MESSAGE(res != nullptr, "server returned response");
    return {res->status == 200, res->body};
  }

  bool wait_for_remote_mdns_sources(unsigned int num) {
    boost::property_tree::ptree pt;
    int retry = 10;
    do {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      auto json = get_remote_mdns_sources();
      BOOST_REQUIRE_MESSAGE(json.first, "got remote mdns sources");
      std::stringstream ss(json.second);
      boost::property_tree::read_json(ss, pt);
      // BOOST_TEST_MESSAGE(std::to_string(pt.get_child("remote_sources").size()));
    } while (pt.get_child("remote_sources").size() != num && retry--);
    return (retry > 0);
  }

 private:
  httplib::Client cli_{g_daemon_address, g_daemon_port};
  io_service io_service_;
  udp::socket socket_{io_service_};
  udp::endpoint listen_endpoint_{
      udp::endpoint(address::from_string("0.0.0.0"), g_sap_port)};
};

BOOST_AUTO_TEST_CASE(is_alive) {
  Client cli;
  BOOST_REQUIRE_MESSAGE(cli.is_alive(), "server is alive");
}

BOOST_AUTO_TEST_CASE(get_config) {
  Client cli;
  auto json = cli.get_config();
  BOOST_REQUIRE_MESSAGE(json.first, "got config");
  boost::property_tree::ptree pt;
  std::stringstream ss(json.second);
  boost::property_tree::read_json(ss, pt);
  auto http_port = pt.get<int>("http_port");
  // auto log_severity = pt.get<int>("log_severity");
  auto playout_delay = pt.get<int>("playout_delay");
  auto tic_frame_size_at_1fs = pt.get<int>("tic_frame_size_at_1fs");
  auto max_tic_frame_size = pt.get<int>("max_tic_frame_size");
  auto sample_rate = pt.get<int>("sample_rate");
  auto rtp_mcast_base = pt.get<std::string>("rtp_mcast_base");
  auto rtp_port = pt.get<int>("rtp_port");
  auto ptp_domain = pt.get<int>("ptp_domain");
  auto ptp_dscp = pt.get<int>("ptp_dscp");
  auto sap_interval = pt.get<int>("sap_interval");
  auto syslog_proto = pt.get<std::string>("syslog_proto");
  auto syslog_server = pt.get<std::string>("syslog_server");
  auto status_file = pt.get<std::string>("status_file");
  auto interface_name = pt.get<std::string>("interface_name");
  auto mac_addr = pt.get<std::string>("mac_addr");
  auto ip_addr = pt.get<std::string>("ip_addr");
  BOOST_CHECK_MESSAGE(http_port == 9999, "config as excepcted");
  // BOOST_CHECK_MESSAGE(log_severity == 5, "config as excepcted");
  BOOST_CHECK_MESSAGE(playout_delay == 0, "config as excepcted");
  BOOST_CHECK_MESSAGE(tic_frame_size_at_1fs == 192, "config as excepcted");
  BOOST_CHECK_MESSAGE(max_tic_frame_size == 1024, "config as excepcted");
  BOOST_CHECK_MESSAGE(sample_rate == 44100, "config as excepcted");
  BOOST_CHECK_MESSAGE(rtp_mcast_base == "239.1.0.1", "config as excepcted");
  BOOST_CHECK_MESSAGE(rtp_port == 6004, "config as excepcted");
  BOOST_CHECK_MESSAGE(ptp_domain == 0, "config as excepcted");
  BOOST_CHECK_MESSAGE(ptp_dscp == 46, "config as excepcted");
  BOOST_CHECK_MESSAGE(sap_interval == 1, "config as excepcted");
  BOOST_CHECK_MESSAGE(syslog_proto == "none", "config as excepcted");
  BOOST_CHECK_MESSAGE(syslog_server == "255.255.255.254:1234",
                      "config as excepcted");
  BOOST_CHECK_MESSAGE(status_file == "", "config as excepcted");
  BOOST_CHECK_MESSAGE(interface_name == "lo", "config as excepcted");
  BOOST_CHECK_MESSAGE(mac_addr == "00:00:00:00:00:00", "config as excepcted");
  BOOST_CHECK_MESSAGE(ip_addr == "127.0.0.1", "config as excepcted");
}

BOOST_AUTO_TEST_CASE(get_ptp_status) {
  Client cli;
  auto json = cli.get_ptp_status();
  BOOST_REQUIRE_MESSAGE(json.first, "got ptp status");
  boost::property_tree::ptree pt;
  std::stringstream ss(json.second);
  boost::property_tree::read_json(ss, pt);
  auto status = pt.get<std::string>("status");
  auto jitter = pt.get<int>("jitter");
  BOOST_REQUIRE_MESSAGE(status == "unlocked" && jitter == 0,
                        "ptp status as excepcted");
}

BOOST_AUTO_TEST_CASE(get_ptp_config) {
  Client cli;
  auto json = cli.get_ptp_config();
  BOOST_REQUIRE_MESSAGE(json.first, "got ptp config");
  boost::property_tree::ptree pt;
  std::stringstream ss(json.second);
  boost::property_tree::read_json(ss, pt);
  auto domain = pt.get<int>("domain");
  auto dscp = pt.get<int>("dscp");
  BOOST_REQUIRE_MESSAGE(domain == 0 && dscp == 46, "ptp config as excepcted");
}

BOOST_AUTO_TEST_CASE(set_ptp_config) {
  Client cli;
  auto res = cli.set_ptp_config(1, 48);
  BOOST_REQUIRE_MESSAGE(res, "set new ptp config");
  auto json = cli.get_ptp_config();
  BOOST_REQUIRE_MESSAGE(json.first, "got new ptp config");
  boost::property_tree::ptree pt;
  std::stringstream ss(json.second);
  boost::property_tree::read_json(ss, pt);
  auto domain = pt.get<int>("domain");
  auto dscp = pt.get<int>("dscp");
  BOOST_REQUIRE_MESSAGE(domain == 1 && dscp == 48, "ptp config as excepcted");
  std::ifstream fs("./daemon.conf");
  BOOST_REQUIRE_MESSAGE(fs.good(), "config file status as excepcted");
  boost::property_tree::read_json(fs, pt);
  domain = pt.get<int>("ptp_domain");
  dscp = pt.get<int>("ptp_dscp");
  BOOST_REQUIRE_MESSAGE(domain == 1 && dscp == 48,
                        "ptp config file as excepcted");
  res = cli.set_ptp_config(0, 46);
  BOOST_REQUIRE_MESSAGE(res, "set default ptp config");
}

BOOST_AUTO_TEST_CASE(add_invalid_source) {
  Client cli;
  BOOST_REQUIRE_MESSAGE(!cli.add_source(g_stream_num_max),
                        "not added source " + std::to_string(g_stream_num_max));
  BOOST_REQUIRE_MESSAGE(!cli.add_source(-1), "not added source -1");
}

BOOST_AUTO_TEST_CASE(remove_invalid_source) {
  Client cli;
  BOOST_REQUIRE_MESSAGE(
      !cli.remove_source(g_stream_num_max),
      "not removed source " + std::to_string(g_stream_num_max));
  BOOST_REQUIRE_MESSAGE(!cli.remove_source(-1), "not removed source -1");
}

BOOST_AUTO_TEST_CASE(add_remove_source) {
  Client cli;
  BOOST_REQUIRE_MESSAGE(cli.add_source(0), "added source 0");
  BOOST_REQUIRE_MESSAGE(cli.remove_source(0), "removed source 0");
}

BOOST_AUTO_TEST_CASE(add_update_remove_source) {
  Client cli;
  BOOST_REQUIRE_MESSAGE(cli.add_source(0), "added source 0");
  BOOST_REQUIRE_MESSAGE(cli.update_source(0), "updated source 0");
  BOOST_REQUIRE_MESSAGE(cli.remove_source(0), "removed source 0");
}

BOOST_AUTO_TEST_CASE(add_remove_sink) {
  Client cli;
  BOOST_REQUIRE_MESSAGE(cli.add_sink_sdp(0), "added sink 0");
  BOOST_REQUIRE_MESSAGE(cli.remove_sink(0), "removed sink 0");
}

BOOST_AUTO_TEST_CASE(add_update_remove_sink) {
  Client cli;
  BOOST_REQUIRE_MESSAGE(cli.add_sink_sdp(0), "added sink 0");
  BOOST_REQUIRE_MESSAGE(cli.add_sink_sdp(0), "updated sink 0");
  BOOST_REQUIRE_MESSAGE(cli.remove_sink(0), "removed sink 0");
}

BOOST_AUTO_TEST_CASE(source_check_sap) {
  Client cli;
  BOOST_REQUIRE_MESSAGE(cli.add_source(0), "added source 0");
  auto sdp = cli.get_source_sdp(0);
  BOOST_REQUIRE_MESSAGE(sdp.first, "got source sdp 0");
  cli.sap_wait_announcement(0, sdp.second);
  BOOST_REQUIRE_MESSAGE(cli.remove_source(0), "removed source 0");
  cli.sap_wait_deletion(0, sdp.second, 3);
}

BOOST_AUTO_TEST_CASE(source_check_sap_browser) {
  Client cli;
  BOOST_REQUIRE_MESSAGE(cli.add_source(0), "added source 0");
  auto sdp = cli.get_source_sdp(0);
  BOOST_REQUIRE_MESSAGE(sdp.first, "got source sdp 0");
  cli.sap_wait_announcement(0, sdp.second);
  auto json = cli.get_remote_sap_sources();
  BOOST_REQUIRE_MESSAGE(json.first, "got remote sap sources");
  boost::property_tree::ptree pt;
  std::stringstream ss(json.second);
  boost::property_tree::read_json(ss, pt);
  BOOST_FOREACH (auto const& v, pt.get_child("remote_sources")) {
    BOOST_REQUIRE_MESSAGE(
        v.second.get<std::string>("sdp") == sdp.second,
        "returned sap source " + v.second.get<std::string>("id"));
  }
  BOOST_REQUIRE_MESSAGE(cli.remove_source(0), "removed source 0");
  cli.sap_wait_deletion(0, sdp.second, 3);
  json = cli.get_remote_sap_sources();
  BOOST_REQUIRE_MESSAGE(json.first, "got remote sap sources");
  std::stringstream ss1(json.second);
  boost::property_tree::read_json(ss1, pt);
  BOOST_REQUIRE_MESSAGE(pt.get_child("remote_sources").size() == 0,
                        "no remote sap sources");
}

#ifdef _USE_AVAHI_
BOOST_AUTO_TEST_CASE(source_check_mdns_browser) {
  Client cli;
  BOOST_REQUIRE_MESSAGE(cli.add_source(0), "added source 0");
  auto sdp = cli.get_source_sdp(0);
  BOOST_REQUIRE_MESSAGE(sdp.first, "got source sdp 0");
  BOOST_REQUIRE_MESSAGE(cli.wait_for_remote_mdns_sources(1),
                        "remote mdns source found");
  auto json = cli.get_remote_mdns_sources();
  BOOST_REQUIRE_MESSAGE(json.first, "got remote mdns sources");
  boost::property_tree::ptree pt;
  std::stringstream ss(json.second);
  boost::property_tree::read_json(ss, pt);
  BOOST_FOREACH (auto const& v, pt.get_child("remote_sources")) {
    BOOST_REQUIRE_MESSAGE(
        v.second.get<std::string>("sdp") == sdp.second,
        "returned mdns source " + v.second.get<std::string>("id"));
  }
  BOOST_REQUIRE_MESSAGE(cli.remove_source(0), "removed source 0");
  BOOST_REQUIRE_MESSAGE(cli.wait_for_remote_mdns_sources(0),
                        "no remote mdns sources");
}

BOOST_AUTO_TEST_CASE(source_check_mdns_browser_update) {
  Client cli;
  BOOST_REQUIRE_MESSAGE(cli.add_source(0), "added source 0");
  BOOST_REQUIRE_MESSAGE(cli.wait_for_remote_mdns_sources(1),
                        "remote mdns source found");
  BOOST_REQUIRE_MESSAGE(cli.update_source(0), "updated source 0");
  auto sdp = cli.get_source_sdp(0);
  BOOST_REQUIRE_MESSAGE(sdp.first, "got source sdp 0");
  int retry = 10;
  bool found = false;
  do {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto json = cli.get_remote_mdns_sources();
    BOOST_REQUIRE_MESSAGE(json.first, "got remote mdns sources");
    std::stringstream ss(json.second);
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(ss, pt);
    // BOOST_TEST_MESSAGE(std::to_string(pt.get_child("remote_sources").size()));
    BOOST_FOREACH (auto const& v, pt.get_child("remote_sources")) {
      if (v.second.get<std::string>("sdp") == sdp.second) {
        found = true;
      }
    }
  } while (retry-- && !found);
  BOOST_REQUIRE_MESSAGE(retry > 0, "remote mdns source updated");
  BOOST_REQUIRE_MESSAGE(cli.remove_source(0), "removed source 0");
  BOOST_REQUIRE_MESSAGE(cli.wait_for_remote_mdns_sources(0),
                        "no remote mdns sources");
}
#endif

BOOST_AUTO_TEST_CASE(sink_check_status) {
  Client cli;
  BOOST_REQUIRE_MESSAGE(cli.add_sink_sdp(0), "added sink 0");
  auto json = cli.get_sink_status(0);
  BOOST_REQUIRE_MESSAGE(json.first, "got sink status 0");
  boost::property_tree::ptree pt;
  std::stringstream ss(json.second);
  boost::property_tree::read_json(ss, pt);
  // auto is_sink_muted = pt.get<bool>("sink_flags.muted");
  auto is_sink_some_muted = pt.get<bool>("sink_flags.some_muted");
  auto is_sink_all_muted = pt.get<bool>("sink_flags.all_muted");
  // BOOST_REQUIRE_MESSAGE(is_sink_muted, "sink is muted");
  BOOST_REQUIRE_MESSAGE(!is_sink_all_muted, "all sinks are mutes");
  BOOST_REQUIRE_MESSAGE(!is_sink_some_muted, "some sinks are muted");
  BOOST_REQUIRE_MESSAGE(cli.remove_sink(0), "removed sink 0");
}

BOOST_AUTO_TEST_CASE(add_remove_all_sources) {
  Client cli;
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.add_source(id),
                          std::string("added source ") + std::to_string(id));
  }
  auto json = cli.get_sources();
  BOOST_REQUIRE_MESSAGE(json.first, "got sources");
  boost::property_tree::ptree pt;
  std::stringstream ss(json.second);
  boost::property_tree::read_json(ss, pt);
  uint8_t id = 0;
  BOOST_FOREACH (auto const& v, pt.get_child("sources")) {
    BOOST_REQUIRE_MESSAGE(v.second.get<uint8_t>("id") == id,
                          "returned source " + std::to_string(id));
    ++id;
  }
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.remove_source(id),
                          std::string("removed source ") + std::to_string(id));
  }
}

BOOST_AUTO_TEST_CASE(add_remove_all_sinks) {
  Client cli;
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.add_sink_sdp(id),
                          std::string("added sink ") + std::to_string(id));
  }
  auto json = cli.get_sinks();
  BOOST_REQUIRE_MESSAGE(json.first, "got sinks");
  boost::property_tree::ptree pt;
  std::stringstream ss(json.second);
  boost::property_tree::read_json(ss, pt);
  uint8_t id = 0;
  BOOST_FOREACH (auto const& v, pt.get_child("sinks")) {
    BOOST_REQUIRE_MESSAGE(v.second.get<uint8_t>("id") == id,
                          "returned sink " + std::to_string(id));
    ++id;
  }
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.remove_sink(id),
                          std::string("removed sink ") + std::to_string(id));
  }
}

BOOST_AUTO_TEST_CASE(add_remove_check_all) {
  Client cli;
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.add_source(id),
                          std::string("added source ") + std::to_string(id));
  }
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.add_sink_sdp(id),
                          std::string("added sink ") + std::to_string(id));
  }
  auto json = cli.get_streams();
  BOOST_REQUIRE_MESSAGE(json.first, "got streams");
  boost::property_tree::ptree pt;
  std::stringstream ss(json.second);
  boost::property_tree::read_json(ss, pt);
  uint8_t id = 0;
  BOOST_FOREACH (auto const& v, pt.get_child("sources")) {
    BOOST_REQUIRE_MESSAGE(v.second.get<uint8_t>("id") == id,
                          "returned source " + std::to_string(id));
    ++id;
  }
  id = 0;
  BOOST_FOREACH (auto const& v, pt.get_child("sinks")) {
    BOOST_REQUIRE_MESSAGE(v.second.get<uint8_t>("id") == id,
                          "returned sink " + std::to_string(id));
    ++id;
  }
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.remove_source(id),
                          std::string("removed source ") + std::to_string(id));
  }
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.remove_sink(id),
                          std::string("removed sink ") + std::to_string(id));
  }
}

BOOST_AUTO_TEST_CASE(add_remove_update_check_all) {
  Client cli;
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.add_source(id),
                          std::string("added source ") + std::to_string(id));
  }
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.update_source(id),
                          std::string("updated source ") + std::to_string(id));
  }
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.add_sink_sdp(id),
                          std::string("added sink ") + std::to_string(id));
  }
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.add_sink_url(id),
                          std::string("updated sink ") + std::to_string(id));
  }
  auto json = cli.get_streams();
  BOOST_REQUIRE_MESSAGE(json.first, "got streams");
  boost::property_tree::ptree pt;
  std::stringstream ss(json.second);
  boost::property_tree::read_json(ss, pt);
  uint8_t id = 0;
  BOOST_FOREACH (auto const& v, pt.get_child("sources")) {
    BOOST_REQUIRE_MESSAGE(v.second.get<uint8_t>("id") == id,
                          "returned source " + std::to_string(id));
    ++id;
  }
  id = 0;
  BOOST_FOREACH (auto const& v, pt.get_child("sinks")) {
    BOOST_REQUIRE_MESSAGE(v.second.get<uint8_t>("id") == id,
                          "returned sink " + std::to_string(id));
    ++id;
  }
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.remove_source(id),
                          std::string("removed source ") + std::to_string(id));
  }
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.remove_sink(id),
                          std::string("removed sink ") + std::to_string(id));
  }
}

BOOST_AUTO_TEST_CASE(add_remove_check_sap_browser_all) {
  Client cli;
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.add_source(id),
                          std::string("added source ") + std::to_string(id));
  }
  for (int id = 0; id < g_stream_num_max; id++) {
    auto sdp = cli.get_source_sdp(id);
    BOOST_REQUIRE_MESSAGE(sdp.first,
                          std::string("got source sdp ") + std::to_string(id));
    cli.sap_wait_announcement(id, sdp.second);
  }
  boost::property_tree::ptree pt;
  int retry = 10;
  do {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto json = cli.get_remote_sap_sources();
    BOOST_REQUIRE_MESSAGE(json.first, "got remote sap sources");
    std::stringstream ss(json.second);
    boost::property_tree::read_json(ss, pt);
    // BOOST_TEST_MESSAGE(std::to_string(pt.get_child("remote_sources").size()));
  } while (pt.get_child("remote_sources").size() != g_stream_num_max &&
           retry--);
  BOOST_REQUIRE_MESSAGE(
      pt.get_child("remote_sources").size() == g_stream_num_max,
      "found " + std::to_string(pt.get_child("remote_sources").size()) +
          " remote sap sources");
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.add_sink_sdp(id),
                          std::string("added sink ") + std::to_string(id));
  }
  auto json = cli.get_streams();
  BOOST_REQUIRE_MESSAGE(json.first, "got streams");
  std::stringstream ss1(json.second);
  boost::property_tree::read_json(ss1, pt);
  uint8_t id = 0;
  BOOST_FOREACH (auto const& v, pt.get_child("sources")) {
    BOOST_REQUIRE_MESSAGE(v.second.get<uint8_t>("id") == id,
                          "returned source " + std::to_string(id));
    ++id;
  }
  id = 0;
  BOOST_FOREACH (auto const& v, pt.get_child("sinks")) {
    BOOST_REQUIRE_MESSAGE(v.second.get<uint8_t>("id") == id,
                          "returned sink " + std::to_string(id));
    ++id;
  }
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.remove_source(id),
                          std::string("removed source ") + std::to_string(id));
  }
  cli.sap_wait_all_deletions();
  retry = 10;
  do {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto json = cli.get_remote_sap_sources();
    BOOST_REQUIRE_MESSAGE(json.first, "got remote sap sources");
    std::stringstream ss2(json.second);
    boost::property_tree::read_json(ss2, pt);
    // BOOST_TEST_MESSAGE(std::to_string(pt.get_child("remote_sources").size()));
  } while (pt.get_child("remote_sources").size() > 0 && retry--);
  BOOST_REQUIRE_MESSAGE(pt.get_child("remote_sources").size() == 0,
                        "no remote sap sources");
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.remove_sink(id),
                          std::string("removed sink ") + std::to_string(id));
  }
}

#ifdef _USE_AVAHI_
BOOST_AUTO_TEST_CASE(add_remove_check_mdns_browser_all) {
  Client cli;
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.add_source(id),
                          std::string("added source ") + std::to_string(id));
  }
  BOOST_REQUIRE_MESSAGE(cli.wait_for_remote_mdns_sources(g_stream_num_max),
                        "remote mdns sources found");
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.add_sink_sdp(id),
                          std::string("added sink ") + std::to_string(id));
  }
  auto json = cli.get_streams();
  BOOST_REQUIRE_MESSAGE(json.first, "got streams");
  std::stringstream ss1(json.second);
  boost::property_tree::ptree pt;
  boost::property_tree::read_json(ss1, pt);
  uint8_t id = 0;
  BOOST_FOREACH (auto const& v, pt.get_child("sources")) {
    BOOST_REQUIRE_MESSAGE(v.second.get<uint8_t>("id") == id,
                          "returned source " + std::to_string(id));
    ++id;
  }
  id = 0;
  BOOST_FOREACH (auto const& v, pt.get_child("sinks")) {
    BOOST_REQUIRE_MESSAGE(v.second.get<uint8_t>("id") == id,
                          "returned sink " + std::to_string(id));
    ++id;
  }
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.remove_source(id),
                          std::string("removed source ") + std::to_string(id));
  }
  BOOST_REQUIRE_MESSAGE(cli.wait_for_remote_mdns_sources(0),
                        "no remote mdns sources found");
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.remove_sink(id),
                          std::string("removed sink ") + std::to_string(id));
  }
}

BOOST_AUTO_TEST_CASE(add_remove_check_mdns_browser_update_all) {
  Client cli;
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.add_source(id),
                          std::string("added source ") + std::to_string(id));
  }
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.update_source(id),
                          std::string("updated source ") + std::to_string(id));
  }
  std::vector<std::string> sdps{g_stream_num_max};
  for (int id = 0; id < g_stream_num_max; id++) {
    auto sdp = cli.get_source_sdp(id);
    BOOST_REQUIRE_MESSAGE(sdp.first, "got source sdp id " + std::to_string(id));
    sdps[id] = sdp.second;
  }
  int retry = 10, found;
  do {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto json = cli.get_remote_mdns_sources();
    BOOST_REQUIRE_MESSAGE(json.first, "got remote mdns sources");
    std::stringstream ss(json.second);
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(ss, pt);
    found = 0;
    for (int id = 0; id < g_stream_num_max; id++) {
      BOOST_FOREACH (auto const& v, pt.get_child("remote_sources")) {
        if (v.second.get<std::string>("sdp") == sdps[id]) {
          found++;
        }
      }
    }
  } while (retry-- && found < g_stream_num_max);
  BOOST_REQUIRE_MESSAGE(retry > 0, "all remote mdns source updated");
  for (int id = 0; id < g_stream_num_max; id++) {
    BOOST_REQUIRE_MESSAGE(cli.remove_source(id),
                          std::string("removed source ") + std::to_string(id));
  }
  BOOST_REQUIRE_MESSAGE(cli.wait_for_remote_mdns_sources(0),
                        "no remote mdns sources found");
}
#endif
