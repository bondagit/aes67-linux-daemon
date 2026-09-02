//
//  nmos_manager.cpp
//
//  IS-04 Node API + Registration client implementation.
//  One Node, one Device, one Sender+Source+Flow per StreamSource,
//  one Receiver per StreamSink.
//

#include <arpa/inet.h>
#include <unistd.h>

#include <chrono>
#include <sstream>
#include <string>
#include <thread>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/version.hpp>
#if BOOST_VERSION >= 106700
#  include <boost/uuid/name_generator_sha1.hpp>
#else
#  include <boost/uuid/name_generator.hpp>
#endif
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "log.hpp"
#include "nmos_manager.hpp"

using namespace httplib;

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

// IS-04 interface IDs use dash-separated MAC (e.g. "aa-bb-cc-dd-ee-ff").
// Config returns colon-separated; convert here.
static std::string colon_to_dash_mac(const std::string& mac) {
  std::string out = mac;
  for (char& c : out)
    if (c == ':') c = '-';
  return out;
}

static std::string get_system_hostname() {
  char buf[256];
  if (gethostname(buf, sizeof(buf)) == 0) {
    buf[sizeof(buf) - 1] = '\0';
    return buf;
  }
  return "localhost";
}

// Build NMOS label: "<hostname> ALSA <first>-<last>" using 1-indexed channel numbers.
static std::string make_nmos_label(const std::vector<uint8_t>& map) {
  std::string h = get_system_hostname();
  if (map.empty()) return h + " ALSA";
  return h + " ALSA " + std::to_string(map.front() + 1)
         + "-" + std::to_string(map.back() + 1);
}

static std::string make_uuid5(const std::string& ns_str, const std::string& name) {
  boost::uuids::string_generator sgen;
  boost::uuids::uuid ns = sgen(ns_str);
#if BOOST_VERSION >= 106700
  boost::uuids::name_generator_sha1 ngen(ns);
#else
  boost::uuids::name_generator ngen(ns);
#endif
  return boost::uuids::to_string(ngen(name));
}

static std::string make_version() {
  auto now = std::chrono::system_clock::now().time_since_epoch();
  auto secs = std::chrono::duration_cast<std::chrono::seconds>(now).count();
  auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count() %
               1'000'000'000LL;
  return std::to_string(secs) + ":" + std::to_string(nanos);
}

static bool is_multicast(const std::string& addr) {
  struct in_addr a {};
  if (inet_pton(AF_INET, addr.c_str(), &a) == 1) {
    uint32_t ip = ntohl(a.s_addr);
    return ip >= 0xE0000000u && ip <= 0xEFFFFFFFu;
  }
  return false;
}

static void codec_to_nmos(const std::string& codec,
                           std::string& media_type,
                           int& bit_depth) {
  if (codec == "L16") {
    media_type = "audio/L16";
    bit_depth = 16;
  } else if (codec == "AM824") {
    media_type = "audio/AM824";
    bit_depth = 32;
  } else {
    media_type = "audio/L24";
    bit_depth = 24;
  }
}

static std::string make_channels_json(const std::vector<uint8_t>& map) {
  std::ostringstream ss;
  ss << "[";
  size_t n = map.size();
  for (size_t i = 0; i < n; ++i) {
    if (i > 0) ss << ", ";
    ss << "{\"label\": \"";
    if (n == 2) {
      ss << (i == 0 ? "Left" : "Right");
    } else {
      ss << "Ch" << (i + 1);
    }
    ss << "\"}";
  }
  ss << "]";
  return ss.str();
}

static void set_nmos_headers(Response& res) {
  res.set_header("Access-Control-Allow-Origin", "*");
  res.set_header("Access-Control-Allow-Methods", "GET, HEAD, OPTIONS");
  res.set_header("Access-Control-Allow-Headers", "Content-Type, Accept");
  res.set_header("Cache-Control", "no-cache, no-store");
}

static void nmos_ok(Response& res, const std::string& body) {
  set_nmos_headers(res);
  res.set_content(body, "application/json");
}

static void nmos_not_found(Response& res) {
  set_nmos_headers(res);
  res.status = 404;
  res.set_content(
      R"({"code": 404, "error": "Not Found", "debug": ""})",
      "application/json");
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::shared_ptr<NmosManager> NmosManager::create(
    std::shared_ptr<SessionManager> session_manager,
    std::shared_ptr<Config> config) {
  // no need to be thread-safe here
  static std::weak_ptr<NmosManager> instance;
  if (auto ptr = instance.lock()) {
    return ptr;
  }
  auto ptr = std::shared_ptr<NmosManager>(new NmosManager(session_manager, config));
  instance = ptr;
  return ptr;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool NmosManager::init() {
  if (running_) return true;
  BOOST_LOG_TRIVIAL(info) << "NmosManager:: initializing";
  std::string node_id_str = config_->get_node_id();

  // Convert node_id string to a UUID using DNS namespace (RFC 4122)
  node_id_ = make_uuid5("6ba7b810-9dad-11d1-80b4-00c04fd430c8", node_id_str);
  BOOST_LOG_TRIVIAL(info) << "NmosManager:: node_id = " << node_id_;

  device_id_ = make_uuid5(node_id_, "device");

  rebuild_device_json_locked();
  node_json_ = build_node_json();

  // Register session-manager observers
  session_manager_->add_ptp_status_observer(
      [this](const std::string& status) {
        return on_ptp_status_change(status);
      });
  session_manager_->add_source_observer(
      SessionManager::SourceObserverType::add_source,
      [this](uint8_t id, const std::string& name, const std::string& sdp) {
        return on_source_added(id, name, sdp);
      });
  session_manager_->add_source_observer(
      SessionManager::SourceObserverType::remove_source,
      [this](uint8_t id, const std::string& name, const std::string& sdp) {
        return on_source_removed(id, name, sdp);
      });
  session_manager_->add_sink_observer(
      SessionManager::SinkObserverType::add_sink,
      [this](uint8_t id, const std::string& name) {
        return on_sink_added(id, name);
      });
  session_manager_->add_sink_observer(
      SessionManager::SinkObserverType::remove_sink,
      [this](uint8_t id, const std::string& name) {
        return on_sink_removed(id, name);
      });

  running_ = true;
  setup_node_api();
  setup_connection_api();

  // Populate IS-04/IS-05 local state from existing session_manager snapshot
  // synchronously so the Node API serves correct responses from the first request,
  // independent of registry connectivity.
  for (const auto& src : session_manager_->get_sources())
    register_source_local(src.id);
  for (const auto& sink : session_manager_->get_sinks())
    register_sink_local(sink.id);

  BOOST_LOG_TRIVIAL(info) << "NmosManager:: starting async server thread";
  svr_res_ = std::async(std::launch::async, &NmosManager::server_worker, this);
  BOOST_LOG_TRIVIAL(info) << "NmosManager:: starting async registration thread";
  reg_res_ = std::async(std::launch::async, &NmosManager::registration_worker, this);
  BOOST_LOG_TRIVIAL(info) << "NmosManager::init() complete";
  return true;
}

bool NmosManager::terminate() {
  if (running_) {
    running_ = false;
    events_cv_.notify_all();
    node_api_svr_.stop();
    if (svr_res_.valid()) svr_res_.get();
    if (reg_res_.valid()) reg_res_.get();
  }
  return true;
}

// ---------------------------------------------------------------------------
// UUID helpers
// ---------------------------------------------------------------------------

std::string NmosManager::make_resource_uuid(const std::string& type,
                                             uint8_t id) const {
  return make_uuid5(node_id_, type + "-" + std::to_string(id));
}

// ---------------------------------------------------------------------------
// JSON builders
// ---------------------------------------------------------------------------

std::string NmosManager::build_node_json() const {
  PTPStatus ptp_status;
  session_manager_->get_ptp_status(ptp_status);
  std::string gmid = ptp_status.gmid;
  for (char& c : gmid) c = tolower(c);
  std::ostringstream ss;
  ss << "{"
     << "\n  \"id\": \"" << node_id_ << "\""
     << ",\n  \"version\": \"" << make_version() << "\""
     << ",\n  \"label\": \"" << config_->get_node_id() << "\""
     << ",\n  \"description\": \"AES67 Linux Daemon\""
     << ",\n  \"tags\": {}"
     << ",\n  \"href\": \"http://" << config_->get_ip_addr_str()
     << ":" << config_->get_nmos_node_port() << "/\""
     << ",\n  \"hostname\": \"" << get_system_hostname() << "\""
     << ",\n  \"api\": {"
     << "\n    \"versions\": [\"v1.3\"],"
     << "\n    \"endpoints\": [{"
     << "\n      \"host\": \"" << config_->get_ip_addr_str() << "\","
     << "\n      \"port\": " << config_->get_nmos_node_port() << ","
     << "\n      \"protocol\": \"http\","
     << "\n      \"authorization\": false"
     << "\n    }]"
     << "\n  }"
     << ",\n  \"services\": []"
     << ",\n  \"caps\": {}"
     << ",\n  \"clocks\": [{"
     << "\n    \"name\": \"clk0\","
     << "\n    \"ref_type\": \"ptp\","
     << "\n    \"traceable\": false,"
     << "\n    \"version\": \"IEEE1588-2008\","
     << "\n    \"gmid\": \"" << gmid << "\","
     << "\n    \"locked\": " << std::boolalpha << (ptp_status.status == "locked")
     << "\n  }]"
     << ",\n  \"interfaces\": [{"
     << "\n    \"name\": \"" << config_->get_interface_name() << "\","
     << "\n    \"port_id\": \"" << colon_to_dash_mac(config_->get_mac_addr_str()) << "\","
     << "\n    \"chassis_id\": \"" << colon_to_dash_mac(config_->get_mac_addr_str()) << "\""
     << "\n  }]"
     << "\n}";
  return ss.str();
}

void NmosManager::rebuild_device_json_locked() {
  std::ostringstream ss;
  ss << "{"
     << "\n  \"id\": \"" << device_id_ << "\""
     << ",\n  \"version\": \"" << make_version() << "\""
     << ",\n  \"label\": \"" << config_->get_nmos_label() << " Device\""
     << ",\n  \"description\": \"\""
     << ",\n  \"tags\": {}"
     << ",\n  \"type\": \"urn:x-nmos:device:generic\""
     << ",\n  \"node_id\": \"" << node_id_ << "\""
     << ",\n  \"senders\": [";
  bool first = true;
  for (const auto& [id, sr] : senders_) {
    if (!first) ss << ", ";
    ss << "\"" << sr.sender_id << "\"";
    first = false;
  }
  ss << "]"
     << ",\n  \"receivers\": [";
  first = true;
  for (const auto& [id, rr] : receivers_) {
    if (!first) ss << ", ";
    ss << "\"" << rr.receiver_id << "\"";
    first = false;
  }
  ss << "]"
     << ",\n  \"controls\": [{"
     << "\n    \"href\": \"http://" << config_->get_ip_addr_str()
     << ":" << config_->get_nmos_node_port() << "/x-nmos/connection/v1.1/\","
     << "\n    \"type\": \"urn:x-nmos:control:sr-ctrl/v1.1\","
     << "\n    \"authorization\": false"
     << "\n  }]"
     << "\n}";
  device_json_ = ss.str();
}

std::string NmosManager::build_source_json(const StreamSource& src,
                                            const std::string& source_id) const {
  uint32_t sample_rate = config_->get_sample_rate();
  uint32_t pkt = src.max_samples_per_packet > 0 ? src.max_samples_per_packet : 48;
  std::ostringstream ss;
  ss << "{"
     << "\n  \"id\": \"" << source_id << "\""
     << ",\n  \"version\": \"" << make_version() << "\""
     << ",\n  \"label\": \"" << make_nmos_label(src.map) << "\""
     << ",\n  \"description\": \"\""
     << ",\n  \"tags\": {}"
     << ",\n  \"device_id\": \"" << device_id_ << "\""
     << ",\n  \"parents\": []"
     << ",\n  \"clock_name\": \"clk0\""
     << ",\n  \"grain_rate\": {\"numerator\": " << sample_rate
     << ", \"denominator\": " << pkt << "}"
     << ",\n  \"caps\": {}"
     << ",\n  \"format\": \"urn:x-nmos:format:audio\""
     << ",\n  \"channels\": " << make_channels_json(src.map)
     << "\n}";
  return ss.str();
}

std::string NmosManager::build_flow_json(const StreamSource& src,
                                          const std::string& source_id,
                                          const std::string& flow_id) const {
  uint32_t sample_rate = config_->get_sample_rate();
  uint32_t pkt = src.max_samples_per_packet > 0 ? src.max_samples_per_packet : 48;
  std::string media_type;
  int bit_depth;
  codec_to_nmos(src.codec, media_type, bit_depth);
  std::ostringstream ss;
  ss << "{"
     << "\n  \"id\": \"" << flow_id << "\""
     << ",\n  \"version\": \"" << make_version() << "\""
     << ",\n  \"label\": \"" << make_nmos_label(src.map) << "\""
     << ",\n  \"description\": \"\""
     << ",\n  \"tags\": {}"
     << ",\n  \"grain_rate\": {\"numerator\": " << sample_rate
     << ", \"denominator\": " << pkt << "}"
     << ",\n  \"source_id\": \"" << source_id << "\""
     << ",\n  \"parents\": []"
     << ",\n  \"device_id\": \"" << device_id_ << "\""
     << ",\n  \"format\": \"urn:x-nmos:format:audio\""
     << ",\n  \"media_type\": \"" << media_type << "\""
     << ",\n  \"sample_rate\": {\"numerator\": " << sample_rate
     << ", \"denominator\": 1}"
     << ",\n  \"bit_depth\": " << bit_depth
     << ",\n  \"channels\": " << make_channels_json(src.map)
     << "\n}";
  return ss.str();
}

std::string NmosManager::build_sender_json(const StreamSource& src,
                                            uint8_t daemon_id,
                                            const std::string& flow_id,
                                            const std::string& sender_id,
                                            const std::string& active_receiver_id) const {
  bool mcast = is_multicast(src.address);
  std::string manifest = "http://" + config_->get_ip_addr_str() + ":" +
                         std::to_string(config_->get_nmos_node_port()) +
                         "/x-nmos/connection/v1.1/single/senders/" + sender_id + "/transportfile/";
  std::ostringstream ss;
  ss << "{"
     << "\n  \"id\": \"" << sender_id << "\""
     << ",\n  \"version\": \"" << make_version() << "\""
     << ",\n  \"label\": \"" << make_nmos_label(src.map) << "\""
     << ",\n  \"description\": \"\""
     << ",\n  \"tags\": {}"
     << ",\n  \"flow_id\": \"" << flow_id << "\""
     << ",\n  \"transport\": \""
     << (mcast ? "urn:x-nmos:transport:rtp.mcast"
               : "urn:x-nmos:transport:rtp.ucast")
     << "\""
     << ",\n  \"device_id\": \"" << device_id_ << "\""
     << ",\n  \"manifest_href\": \"" << manifest << "\""
     << ",\n  \"interface_bindings\": [\""
     << config_->get_interface_name(0) << "\"]"
     << ",\n  \"subscription\": {\"receiver_id\": ";
  if (active_receiver_id.empty()) ss << "null";
  else ss << "\"" << active_receiver_id << "\"";
  ss << ", \"active\": " << std::boolalpha << src.enabled << "}"
     << "\n}";
  return ss.str();
}

std::string NmosManager::build_receiver_json(const StreamSink& sink,
                                              const std::string& receiver_id,
                                              const std::string& active_sender_id) const {
  std::ostringstream ss;
  ss << "{"
     << "\n  \"id\": \"" << receiver_id << "\""
     << ",\n  \"version\": \"" << make_version() << "\""
     << ",\n  \"label\": \"" << make_nmos_label(sink.map) << "\""
     << ",\n  \"description\": \"\""
     << ",\n  \"tags\": {}"
     << ",\n  \"device_id\": \"" << device_id_ << "\""
     << ",\n  \"transport\": \"urn:x-nmos:transport:rtp.mcast\""
     << ",\n  \"interface_bindings\": [\""
     << config_->get_interface_name(0) << "\"]"
     << ",\n  \"format\": \"urn:x-nmos:format:audio\""
     << ",\n  \"caps\": {\"media_types\": [\"audio/L24\", \"audio/L16\"]}"
     << ",\n  \"subscription\": {\"sender_id\": ";
  if (active_sender_id.empty()) ss << "null";
  else ss << "\"" << active_sender_id << "\"";
  ss << ", \"active\": " << std::boolalpha << !active_sender_id.empty() << "}"
     << "\n}";
  return ss.str();
}

// ---------------------------------------------------------------------------
// Node API server
// ---------------------------------------------------------------------------

void NmosManager::setup_node_api() {

  node_api_svr_.set_logger([](const Request& req, const Response& res) {
    if (res.status == 200) {
      BOOST_LOG_TRIVIAL(info) << "nmos_node_svr:: " << req.method << " "
                              << req.path << " response " << res.status;
    } else {
      BOOST_LOG_TRIVIAL(error)
          << "nmos_node_svr:: " << req.method << " " << req.path << " response "
          << res.status << " " << res.body;
    }
  });
  // CORS preflight
  node_api_svr_.Options(R"(.*)", [](const Request&, Response& res) {
    set_nmos_headers(res);
    res.status = 200;
  });

  // Base discovery paths
  node_api_svr_.Get("/x-nmos/?", [](const Request&, Response& res) {
    nmos_ok(res, "[\"node/\"]");
  });
  node_api_svr_.Get("/x-nmos/node/?", [](const Request&, Response& res) {
    nmos_ok(res, "[\"v1.3/\"]");
  });
  node_api_svr_.Get("/x-nmos/node/v1.3/?", [](const Request&, Response& res) {
    nmos_ok(res, "[\"self/\", \"devices/\", \"sources/\", \"flows/\", \"senders/\", \"receivers/\"]");
  });

  // Self
  node_api_svr_.Get("/x-nmos/node/v1.3/self", [this](const Request&, Response& res) {
    std::shared_lock lock(resources_mutex_);
    nmos_ok(res, node_json_);
  });

  // Devices - list
  node_api_svr_.Get("/x-nmos/node/v1.3/devices/?", [this](const Request&, Response& res) {
    std::shared_lock lock(resources_mutex_);
    nmos_ok(res, "[" + device_json_ + "]");
  });
  // Devices - single
  node_api_svr_.Get(R"(/x-nmos/node/v1.3/devices/([^/]+))",
    [this](const Request& req, Response& res) {
      std::string id = req.matches[1];
      std::shared_lock lock(resources_mutex_);
      if (id == device_id_) {
        nmos_ok(res, device_json_);
      } else {
        nmos_not_found(res);
      }
    });

  // Sources - list
  node_api_svr_.Get("/x-nmos/node/v1.3/sources/?", [this](const Request&, Response& res) {
    std::shared_lock lock(resources_mutex_);
    std::ostringstream ss;
    ss << "[";
    bool first = true;
    for (const auto& [id, sr] : senders_) {
      if (!first) ss << ", ";
      ss << sr.source_json;
      first = false;
    }
    ss << "]";
    nmos_ok(res, ss.str());
  });
  // Sources - single
  node_api_svr_.Get(R"(/x-nmos/node/v1.3/sources/([^/]+))",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      std::shared_lock lock(resources_mutex_);
      for (const auto& [id, sr] : senders_) {
        if (sr.source_id == uuid) {
          nmos_ok(res, sr.source_json);
          return;
        }
      }
      nmos_not_found(res);
    });

  // Flows - list
  node_api_svr_.Get("/x-nmos/node/v1.3/flows/?", [this](const Request&, Response& res) {
    std::shared_lock lock(resources_mutex_);
    std::ostringstream ss;
    ss << "[";
    bool first = true;
    for (const auto& [id, sr] : senders_) {
      if (!first) ss << ", ";
      ss << sr.flow_json;
      first = false;
    }
    ss << "]";
    nmos_ok(res, ss.str());
  });
  // Flows - single
  node_api_svr_.Get(R"(/x-nmos/node/v1.3/flows/([^/]+))",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      std::shared_lock lock(resources_mutex_);
      for (const auto& [id, sr] : senders_) {
        if (sr.flow_id == uuid) {
          nmos_ok(res, sr.flow_json);
          return;
        }
      }
      nmos_not_found(res);
    });

  // Senders - SDP (must be registered before the single-sender handler)
  node_api_svr_.Get(R"(/x-nmos/node/v1.3/senders/([^/]+)/sdp)",
    [this](const Request& req, Response& res) {
      std::string sender_uuid = req.matches[1];
      uint8_t daemon_id = 0;
      bool found = false;
      {
        std::shared_lock lock(resources_mutex_);
        for (const auto& [id, sr] : senders_) {
          if (sr.sender_id == sender_uuid) {
            daemon_id = id;
            found = true;
            break;
          }
        }
      }
      if (!found) {
        nmos_not_found(res);
        return;
      }
      std::string sdp;
      if (auto ec = session_manager_->get_source_sdp(daemon_id, sdp); !ec) {
        set_nmos_headers(res);
        res.set_content(sdp, "application/sdp");
      } else {
        nmos_not_found(res);
      }
    });
  // Senders - list
  node_api_svr_.Get("/x-nmos/node/v1.3/senders/?", [this](const Request&, Response& res) {
    std::shared_lock lock(resources_mutex_);
    std::ostringstream ss;
    ss << "[";
    bool first = true;
    for (const auto& [id, sr] : senders_) {
      if (!first) ss << ", ";
      ss << sr.sender_json;
      first = false;
    }
    ss << "]";
    nmos_ok(res, ss.str());
  });
  // Senders - single
  node_api_svr_.Get(R"(/x-nmos/node/v1.3/senders/([^/]+))",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      std::shared_lock lock(resources_mutex_);
      for (const auto& [id, sr] : senders_) {
        if (sr.sender_id == uuid) {
          nmos_ok(res, sr.sender_json);
          return;
        }
      }
      nmos_not_found(res);
    });

  // Receivers - list
  node_api_svr_.Get("/x-nmos/node/v1.3/receivers/?", [this](const Request&, Response& res) {
    std::shared_lock lock(resources_mutex_);
    std::ostringstream ss;
    ss << "[";
    bool first = true;
    for (const auto& [id, rr] : receivers_) {
      if (!first) ss << ", ";
      ss << rr.receiver_json;
      first = false;
    }
    ss << "]";
    nmos_ok(res, ss.str());
  });
  // Receivers - single
  node_api_svr_.Get(R"(/x-nmos/node/v1.3/receivers/([^/]+))",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      std::shared_lock lock(resources_mutex_);
      for (const auto& [id, rr] : receivers_) {
        if (rr.receiver_id == uuid) {
          nmos_ok(res, rr.receiver_json);
          return;
        }
      }
      nmos_not_found(res);
    });
}

// ---------------------------------------------------------------------------
// IS-05 Connection Management API
// ---------------------------------------------------------------------------

// --- Static SDP helpers ---

static std::string sdp_extract_field(const std::string& sdp,
                                     const std::string& prefix) {
  auto pos = sdp.find(prefix);
  if (pos == std::string::npos) return "";
  pos += prefix.size();
  auto end = sdp.find_first_of("\r\n", pos);
  return sdp.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
}

// Extract first multicast/unicast destination IP from c= line.
static std::string sdp_connection_ip(const std::string& sdp) {
  std::string c = sdp_extract_field(sdp, "c=IN IP4 ");
  // strip /ttl/layers suffix
  auto slash = c.find('/');
  if (slash != std::string::npos) c = c.substr(0, slash);
  return c;
}

// Extract port from first m= line.
static uint16_t sdp_media_port(const std::string& sdp) {
  auto pos = sdp.find("m=audio ");
  if (pos == std::string::npos) return 5004;
  pos += 8;
  auto end = sdp.find(' ', pos);
  try { return static_cast<uint16_t>(std::stoi(sdp.substr(pos, end - pos))); }
  catch (...) { return 5004; }
}

// Extract source from a=source-filter line (SSM).
static std::string sdp_source_filter_ip(const std::string& sdp) {
  auto pos = sdp.find("a=source-filter: incl IN IP4 ");
  if (pos == std::string::npos) return "";
  pos += 29;
  // format: <mcast> <source>
  auto sp = sdp.find(' ', pos);
  if (sp == std::string::npos) return "";
  auto end = sdp.find_first_of("\r\n", sp + 1);
  return sdp.substr(sp + 1, end == std::string::npos ? std::string::npos : end - sp - 1);
}

// --- Transport params JSON helpers ---

std::string NmosManager::tp_sender_json(const SenderTp& tp) const {
  std::ostringstream ss;
  ss << std::boolalpha
     << "{\"source_ip\": \"" << tp.source_ip << "\""
     << ", \"destination_ip\": \"" << tp.destination_ip << "\""
     << ", \"source_port\": " << tp.source_port
     << ", \"destination_port\": " << tp.destination_port
     << ", \"rtp_enabled\": " << tp.rtp_enabled
     << "}";
  return ss.str();
}

std::string NmosManager::tp_receiver_json(const ReceiverTp& tp) const {
  std::ostringstream ss;
  ss << std::boolalpha
     << "{\"interface_ip\": \"" << tp.interface_ip << "\""
     << ", \"multicast_ip\": ";
  if (tp.multicast_ip.empty()) ss << "null";
  else ss << "\"" << tp.multicast_ip << "\"";
  ss << ", \"destination_port\": " << tp.destination_port
     << ", \"source_ip\": \"" << tp.source_ip << "\""
     << ", \"rtp_enabled\": " << tp.rtp_enabled
     << "}";
  return ss.str();
}

std::string NmosManager::activation_json(const Is05Activation& act) const {
  auto q = [](const std::string& s) -> std::string {
    return s.empty() ? "null" : "\"" + s + "\"";
  };
  std::ostringstream ss;
  ss << "{\"mode\": " << q(act.mode)
     << ", \"requested_time\": " << q(act.requested_time)
     << ", \"activation_time\": " << q(act.activation_time)
     << "}";
  return ss.str();
}

std::string NmosManager::staged_sender_json(const SenderResources& sr) const {
  std::ostringstream ss;
  ss << std::boolalpha
     << "{\"master_enable\": " << sr.staged_master_enable
     << ", \"receiver_id\": ";
  if (sr.staged_receiver_id.empty()) ss << "null";
  else ss << "\"" << sr.staged_receiver_id << "\"";
  ss << ", \"activation\": " << activation_json(sr.staged_act)
     << ", \"transport_params\": [" << tp_sender_json(sr.staged_tp) << "]}";
  return ss.str();
}

std::string NmosManager::active_sender_json(const SenderResources& sr) const {
  std::ostringstream ss;
  ss << std::boolalpha
     << "{\"master_enable\": " << sr.active_master_enable
     << ", \"receiver_id\": ";
  if (sr.active_receiver_id.empty()) ss << "null";
  else ss << "\"" << sr.active_receiver_id << "\"";
  ss << ", \"activation\": " << activation_json(sr.active_act)
     << ", \"transport_params\": [" << tp_sender_json(sr.active_tp) << "]}";
  return ss.str();
}

std::string NmosManager::staged_receiver_json(const ReceiverResources& rr) const {
  std::ostringstream ss;
  ss << std::boolalpha
     << "{\"master_enable\": " << rr.staged_master_enable
     << ", \"sender_id\": ";
  if (rr.staged_sender_id.empty()) ss << "null";
  else ss << "\"" << rr.staged_sender_id << "\"";
  ss << ", \"activation\": " << activation_json(rr.staged_act)
     << ", \"transport_params\": [" << tp_receiver_json(rr.staged_tp) << "]}";
  return ss.str();
}

std::string NmosManager::active_receiver_json(const ReceiverResources& rr) const {
  std::ostringstream ss;
  ss << std::boolalpha
     << "{\"master_enable\": " << rr.active_master_enable
     << ", \"sender_id\": ";
  if (rr.active_sender_id.empty()) ss << "null";
  else ss << "\"" << rr.active_sender_id << "\"";
  ss << ", \"activation\": " << activation_json(rr.active_act)
     << ", \"transport_params\": [" << tp_receiver_json(rr.active_tp) << "]}";
  return ss.str();
}

// --- Transport param builders from daemon state ---

NmosManager::SenderTp NmosManager::build_sender_tp(const StreamSource& src) const {
  SenderTp tp;
  tp.source_ip       = config_->get_ip_addr_str();
  tp.destination_ip  = src.address;
  tp.source_port     = config_->get_rtp_port();
  tp.destination_port = config_->get_rtp_port();
  tp.rtp_enabled     = src.enabled;
  return tp;
}

NmosManager::ReceiverTp NmosManager::build_receiver_tp_from_sdp(
    const std::string& sdp) const {
  ReceiverTp tp;
  tp.interface_ip      = config_->get_ip_addr_str();
  std::string dest_ip  = sdp_connection_ip(sdp);
  if (is_multicast(dest_ip)) {
    tp.multicast_ip = dest_ip;
  }
  tp.destination_port  = sdp_media_port(sdp);
  std::string src_ip   = sdp_source_filter_ip(sdp);
  tp.source_ip         = src_ip.empty() ? "auto" : src_ip;
  tp.rtp_enabled       = true;
  return tp;
}

// --- PATCH body parsing ---

// Merges a partial IS-05 PATCH body into the sender's staged state.
bool NmosManager::patch_sender_staged(uint8_t daemon_id,
                                      const std::string& body,
                                      std::string& err,
                                      std::string& staged_json_out) {
  // Parse with boost property_tree
  namespace pt_ns = boost::property_tree;
  pt_ns::ptree pt;
  try {
    std::istringstream ss(body);
    pt_ns::read_json(ss, pt);
  } catch (const std::exception& e) {
    err = e.what();
    return false;
  }

  std::unique_lock lock(resources_mutex_);
  auto it = senders_.find(daemon_id);
  if (it == senders_.end()) { err = "not found"; return false; }
  SenderResources& sr = it->second;

  if (auto v = pt.get_optional<bool>("master_enable"))
    sr.staged_master_enable = *v;
  if (auto v = pt.get_optional<std::string>("receiver_id"))
    sr.staged_receiver_id = (*v == "null") ? "" : *v;

  auto& act = sr.staged_act;
  if (auto v = pt.get_optional<std::string>("activation.mode"))
    act.mode = (*v == "null") ? "" : *v;
  if (auto v = pt.get_optional<std::string>("activation.requested_time"))
    act.requested_time = (*v == "null") ? "" : *v;

  // transport_params (array, first leg only)
  auto tp_child = pt.get_child_optional("transport_params");
  if (tp_child && !tp_child->empty()) {
    const auto& leg = tp_child->begin()->second;
    if (auto v = leg.get_optional<std::string>("source_ip"))
      sr.staged_tp.source_ip = *v;
    if (auto v = leg.get_optional<std::string>("destination_ip"))
      sr.staged_tp.destination_ip = *v;
    if (auto v = leg.get_optional<uint16_t>("source_port"))
      sr.staged_tp.source_port = *v;
    if (auto v = leg.get_optional<uint16_t>("destination_port"))
      sr.staged_tp.destination_port = *v;
    if (auto v = leg.get_optional<bool>("rtp_enabled"))
      sr.staged_tp.rtp_enabled = *v;
  }

  // Capture staged JSON BEFORE activation fires observer events
  staged_json_out = staged_sender_json(senders_.at(daemon_id));

  // Handle activation
  if (act.mode == "activate_immediate") {
    act.activation_time = make_version();
    // reset after copy to active
    lock.unlock();
    apply_sender_activation(daemon_id);
    // re-lock to reset staged activation
    lock.lock();
    if (senders_.count(daemon_id))
      senders_[daemon_id].staged_act = {};
  } else if (act.mode == "activate_scheduled_relative" ||
             act.mode == "activate_scheduled_absolute") {
    int64_t deadline = 0;
    auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (act.mode == "activate_scheduled_relative" && !act.requested_time.empty()) {
      // requested_time = "<secs>:<nanos>"
      auto colon = act.requested_time.find(':');
      try {
        int64_t s = std::stoll(act.requested_time.substr(0, colon));
        int64_t n = colon != std::string::npos
                    ? std::stoll(act.requested_time.substr(colon + 1)) : 0;
        deadline = now_ns + s * 1'000'000'000LL + n;
      } catch (...) { deadline = now_ns + 1'000'000'000LL; }
    } else {
      // absolute: use TAI — approximate as system clock + 1s fallback
      deadline = now_ns + 1'000'000'000LL;
    }
    act.deadline_ns = deadline;
    {
      std::lock_guard<std::mutex> pa_lock(pending_act_mutex_);
      pending_activations_.push_back({true, daemon_id, deadline});
    }
  }
  return true;
}

bool NmosManager::patch_receiver_staged(uint8_t daemon_id,
                                        const std::string& body,
                                        std::string& err,
                                        std::string& staged_json_out) {
  namespace pt_ns = boost::property_tree;
  pt_ns::ptree pt;
  try {
    std::istringstream ss(body);
    pt_ns::read_json(ss, pt);
  } catch (const std::exception& e) {
    err = e.what();
    return false;
  }

  std::unique_lock lock(resources_mutex_);
  auto it = receivers_.find(daemon_id);
  if (it == receivers_.end()) { err = "not found"; return false; }
  ReceiverResources& rr = it->second;

  if (auto v = pt.get_optional<bool>("master_enable"))
    rr.staged_master_enable = *v;
  if (auto v = pt.get_optional<std::string>("sender_id"))
    rr.staged_sender_id = (*v == "null") ? "" : *v;

  auto& act = rr.staged_act;
  if (auto v = pt.get_optional<std::string>("activation.mode"))
    act.mode = (*v == "null") ? "" : *v;
  if (auto v = pt.get_optional<std::string>("activation.requested_time"))
    act.requested_time = (*v == "null") ? "" : *v;

  auto tp_child = pt.get_child_optional("transport_params");
  if (tp_child && !tp_child->empty()) {
    const auto& leg = tp_child->begin()->second;
    if (auto v = leg.get_optional<std::string>("interface_ip"))
      rr.staged_tp.interface_ip = *v;
    if (auto v = leg.get_optional<std::string>("multicast_ip"))
      rr.staged_tp.multicast_ip = (*v == "null") ? "" : *v;
    if (auto v = leg.get_optional<uint16_t>("destination_port"))
      rr.staged_tp.destination_port = *v;
    if (auto v = leg.get_optional<std::string>("source_ip"))
      rr.staged_tp.source_ip = *v;
    if (auto v = leg.get_optional<bool>("rtp_enabled"))
      rr.staged_tp.rtp_enabled = *v;
  }

  // Capture staged JSON BEFORE activation fires observer events
  staged_json_out = staged_receiver_json(receivers_.at(daemon_id));

  if (act.mode == "activate_immediate") {
    act.activation_time = make_version();
    lock.unlock();
    apply_receiver_activation(daemon_id);
    lock.lock();
    if (receivers_.count(daemon_id))
      receivers_[daemon_id].staged_act = {};
  } else if (act.mode == "activate_scheduled_relative" ||
             act.mode == "activate_scheduled_absolute") {
    int64_t deadline = 0;
    auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (act.mode == "activate_scheduled_relative" && !act.requested_time.empty()) {
      auto colon = act.requested_time.find(':');
      try {
        int64_t s = std::stoll(act.requested_time.substr(0, colon));
        int64_t n = colon != std::string::npos
                    ? std::stoll(act.requested_time.substr(colon + 1)) : 0;
        deadline = now_ns + s * 1'000'000'000LL + n;
      } catch (...) { deadline = now_ns + 1'000'000'000LL; }
    } else {
      deadline = now_ns + 1'000'000'000LL;
    }
    act.deadline_ns = deadline;
    {
      std::lock_guard<std::mutex> pa_lock(pending_act_mutex_);
      pending_activations_.push_back({false, daemon_id, deadline});
    }
  }
  return true;
}

// --- Remote sender SDP fetch ---

// Fetch SDP for a remote sender UUID:
//   1. Query the IS-04 query/registry API for the sender object to get manifest_href
//   2. Fetch the SDP (application/sdp) from manifest_href
// Falls back to empty sdp on any error so the caller can decide what to do.
void NmosManager::fetch_remote_sender_sdp(const std::string& sender_uuid,
                                           std::string& sdp) {
  sdp.clear();

  // Step 1: query registry query API for sender
  const std::string reg_host = config_->get_nmos_registry_address();
  const uint16_t    reg_port = config_->get_nmos_registry_port();
  std::string manifest_href;

  {
    Client cli(reg_host.c_str(), reg_port);
    cli.set_connection_timeout(3);
    cli.set_read_timeout(3);
    const std::string path = "/x-nmos/query/v1.3/senders/" + sender_uuid;
    auto res = cli.Get(path.c_str());
    if (res && res->status == 200) {
      // Extract manifest_href from the sender JSON
      // Simple string search to avoid pulling in a full JSON parser here
      const std::string key = "\"manifest_href\":\"";
      auto pos = res->body.find(key);
      if (pos != std::string::npos) {
        pos += key.size();
        auto end = res->body.find('"', pos);
        if (end != std::string::npos)
          manifest_href = res->body.substr(pos, end - pos);
      }
    } else {
      BOOST_LOG_TRIVIAL(warning)
          << "NmosManager:: IS-05 remote sender lookup: registry query failed for "
          << sender_uuid << " (registry " << reg_host << ":" << reg_port << ")";
    }
  }

  if (manifest_href.empty()) {
    BOOST_LOG_TRIVIAL(warning)
        << "NmosManager:: IS-05 remote sender lookup: no manifest_href for " << sender_uuid;
    return;
  }

  // Step 2: fetch SDP from manifest_href
  // Parse manifest_href for host/port/path
  auto trim_http = [](const std::string& url) -> std::tuple<std::string,uint16_t,std::string> {
    // Expect "http://host[:port]/path"
    auto after = url;
    if (after.substr(0, 7) == "http://") after = after.substr(7);
    auto slash = after.find('/');
    std::string host_port = slash != std::string::npos ? after.substr(0, slash) : after;
    std::string path      = slash != std::string::npos ? after.substr(slash) : "/";
    auto colon = host_port.find(':');
    std::string host  = colon != std::string::npos ? host_port.substr(0, colon) : host_port;
    uint16_t    port  = 80;
    if (colon != std::string::npos) {
      try { port = static_cast<uint16_t>(std::stoi(host_port.substr(colon + 1))); }
      catch (...) {}
    }
    return {host, port, path};
  };

  auto [mh_host, mh_port, mh_path] = trim_http(manifest_href);
  Client mcli(mh_host.c_str(), mh_port);
  mcli.set_connection_timeout(3);
  mcli.set_read_timeout(3);
  auto mres = mcli.Get(mh_path.c_str());
  if (mres && (mres->status == 200 || mres->status == 206)) {
    sdp = mres->body;
    BOOST_LOG_TRIVIAL(info)
        << "NmosManager:: IS-05 fetched SDP for remote sender " << sender_uuid
        << " from " << manifest_href;
  } else {
    BOOST_LOG_TRIVIAL(warning)
        << "NmosManager:: IS-05 failed to fetch SDP from " << manifest_href;
  }
}

// --- Activation execution ---

void NmosManager::apply_sender_activation(uint8_t daemon_id) {
  std::unique_lock lock(resources_mutex_);
  auto it = senders_.find(daemon_id);
  if (it == senders_.end()) return;
  SenderResources& sr = it->second;

  // Promote staged → active
  sr.active_master_enable  = sr.staged_master_enable;
  sr.active_receiver_id    = sr.staged_receiver_id;
  sr.active_act            = sr.staged_act;
  sr.active_tp             = sr.staged_tp;
  sr.active_act.mode       = sr.staged_act.mode;
  sr.active_act.activation_time = sr.staged_act.activation_time;

  // Update IS-04 sender JSON to reflect new subscription
  StreamSource src;
  lock.unlock();
  if (!session_manager_->get_source(daemon_id, src)) {
    std::unique_lock l2(resources_mutex_);
    if (senders_.count(daemon_id)) {
      SenderResources& sr2 = senders_[daemon_id];
      sr2.sender_json = build_sender_json(src, daemon_id, sr2.flow_id,
                                          sr2.sender_id, sr2.active_receiver_id);
    }
  }
}

void NmosManager::apply_receiver_activation(uint8_t daemon_id) {
  // Snapshot staged state
  bool        master_enable;
  std::string sender_id;
  ReceiverTp  tp;
  std::string activation_time;
  std::string staged_act_mode;
  {
    std::shared_lock lock(resources_mutex_);
    auto it = receivers_.find(daemon_id);
    if (it == receivers_.end()) return;
    master_enable   = it->second.staged_master_enable;
    sender_id       = it->second.staged_sender_id;
    tp              = it->second.staged_tp;
    activation_time = it->second.staged_act.activation_time;
    staged_act_mode = it->second.staged_act.mode;
  }

  // Resolve SDP for remote sender connection
  std::string sdp;
  if (master_enable && !sender_id.empty()) {
    uint8_t src_daemon_id = 0;
    bool found_local = false;
    {
      std::shared_lock lock(resources_mutex_);
      for (const auto& [sid, sr] : senders_) {
        if (sr.sender_id == sender_id) { src_daemon_id = sid; found_local = true; break; }
      }
    }
    if (found_local) {
      session_manager_->get_source_sdp(src_daemon_id, sdp);
    } else {
      // Remote sender: query registry for manifest_href, then fetch SDP
      fetch_remote_sender_sdp(sender_id, sdp);
    }
    if (!sdp.empty())
      tp = build_receiver_tp_from_sdp(sdp);
  }

  // Reject non-audio SDPs before touching active state or calling add_sink.
  if (master_enable && !sdp.empty() &&
      sdp.find("m=audio") == std::string::npos) {
    BOOST_LOG_TRIVIAL(warning)
        << "NmosManager:: receiver " << +daemon_id
        << " activation rejected: SDP has no audio media section";
    return;
  }

  // Promote staged → active BEFORE calling add_sink.
  // add_sink fires remove+add observers which cause register_sink to run in the
  // registration_worker thread. We set preserved_active_sender_ids_ so that
  // register_sink can restore the IS-05 active sender across that cycle.
  {
    std::unique_lock lock(resources_mutex_);
    auto it = receivers_.find(daemon_id);
    if (it == receivers_.end()) return;
    ReceiverResources& rr = it->second;
    rr.active_master_enable = master_enable;
    rr.active_sender_id     = master_enable ? sender_id : "";
    rr.active_act.mode      = staged_act_mode;
    rr.active_act.activation_time = activation_time;
    rr.active_tp            = tp;
    // Preserve across the remove+add observer cycle triggered by add_sink below
    preserved_active_sender_ids_[daemon_id] = rr.active_sender_id;
  }

  // Apply session_manager connection (may trigger remove+add observer events)
  if (master_enable && !sdp.empty()) {
    StreamSink sink;
    if (!session_manager_->get_sink(daemon_id, sink)) {
      sink.use_sdp = true;
      sink.sdp     = sdp;
      sink.source  = "";
      session_manager_->add_sink(sink);
    }
  } else if (!master_enable) {
    StreamSink sink;
    if (!session_manager_->get_sink(daemon_id, sink)) {
      sink.use_sdp = false;
      sink.sdp     = "";
      sink.source  = "";
      session_manager_->add_sink(sink);
    }
  }

  // Update IS-04 receiver JSON to reflect new subscription
  StreamSink sink;
  if (!session_manager_->get_sink(daemon_id, sink)) {
    std::unique_lock lock(resources_mutex_);
    auto it = receivers_.find(daemon_id);
    if (it != receivers_.end()) {
      it->second.receiver_json = build_receiver_json(
          sink, it->second.receiver_id, it->second.active_sender_id);
    }
  }
}

// Process scheduled activations — called from registration_worker loop.
void NmosManager::process_scheduled_activations() {
  auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();

  std::vector<PendingActivation> due;
  {
    std::lock_guard<std::mutex> lock(pending_act_mutex_);
    auto part = std::stable_partition(
        pending_activations_.begin(), pending_activations_.end(),
        [now_ns](const PendingActivation& pa) { return pa.deadline_ns > now_ns; });
    due.assign(part, pending_activations_.end());
    pending_activations_.erase(part, pending_activations_.end());
  }

  for (const auto& pa : due) {
    // Fill activation_time in staged before promoting
    {
      std::unique_lock lock(resources_mutex_);
      if (pa.is_sender) {
        auto it = senders_.find(pa.daemon_id);
        if (it != senders_.end())
          it->second.staged_act.activation_time = make_version();
      } else {
        auto it = receivers_.find(pa.daemon_id);
        if (it != receivers_.end())
          it->second.staged_act.activation_time = make_version();
      }
    }
    if (pa.is_sender) apply_sender_activation(pa.daemon_id);
    else              apply_receiver_activation(pa.daemon_id);
    // Reset staged activation
    {
      std::unique_lock lock(resources_mutex_);
      if (pa.is_sender) {
        auto it = senders_.find(pa.daemon_id);
        if (it != senders_.end()) it->second.staged_act = {};
      } else {
        auto it = receivers_.find(pa.daemon_id);
        if (it != receivers_.end()) it->second.staged_act = {};
      }
    }
  }
}

// --- IS-05 HTTP routes ---

static void conn_ok(Response& res, const std::string& body) {
  res.set_header("Access-Control-Allow-Origin", "*");
  res.set_header("Cache-Control", "no-cache, no-store");
  res.set_content(body, "application/json");
}

static void conn_not_found(Response& res) {
  res.set_header("Access-Control-Allow-Origin", "*");
  res.status = 404;
  res.set_content(R"({"code":404,"error":"Not Found","debug":""})",
                  "application/json");
}

static void conn_bad_request(Response& res, const std::string& msg) {
  res.set_header("Access-Control-Allow-Origin", "*");
  res.status = 400;
  res.set_content("{\"code\":400,\"error\":\"Bad Request\",\"debug\":\"" + msg + "\"}",
                  "application/json");
}

void NmosManager::setup_connection_api() {
  // Discovery roots
  node_api_svr_.Get("/x-nmos/connection/",
    [](const Request&, Response& res) {
      conn_ok(res, "[\"v1.1/\"]");
    });
  node_api_svr_.Get("/x-nmos/connection/v1.1/",
    [](const Request&, Response& res) {
      conn_ok(res, "[\"single/\", \"bulk/\"]");
    });
  node_api_svr_.Get("/x-nmos/connection/v1.1/single/",
    [](const Request&, Response& res) {
      conn_ok(res, "[\"senders/\", \"receivers/\"]");
    });
  node_api_svr_.Get("/x-nmos/connection/v1.1/bulk/",
    [](const Request&, Response& res) {
      conn_ok(res, "[\"senders/\", \"receivers/\"]");
    });

  // ---- Single senders ----

  // List
  node_api_svr_.Get("/x-nmos/connection/v1.1/single/senders/",
    [this](const Request&, Response& res) {
      std::shared_lock lock(resources_mutex_);
      std::ostringstream ss;
      ss << "[";
      bool first = true;
      for (const auto& [id, sr] : senders_) {
        if (!first) ss << ", ";
        ss << "\"" << sr.sender_id << "/\"";
        first = false;
      }
      ss << "]";
      conn_ok(res, ss.str());
    });

  // Index
  node_api_svr_.Get(R"(/x-nmos/connection/v1\.1/single/senders/([^/]+)/?)",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      std::shared_lock lock(resources_mutex_);
      for (const auto& [id, sr] : senders_) {
        if (sr.sender_id == uuid) {
          conn_ok(res, "[\"constraints/\", \"staged/\", \"active/\", "
                       "\"transportfile/\", \"transporttype\"]");
          return;
        }
      }
      conn_not_found(res);
    });

  // Constraints
  node_api_svr_.Get(R"(/x-nmos/connection/v1\.1/single/senders/([^/]+)/constraints/?)",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      std::shared_lock lock(resources_mutex_);
      for (const auto& [id, sr] : senders_) {
        if (sr.sender_id == uuid) { conn_ok(res, "[{}]"); return; }
      }
      conn_not_found(res);
    });

  // Staged GET
  node_api_svr_.Get(R"(/x-nmos/connection/v1\.1/single/senders/([^/]+)/staged/?)",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      std::shared_lock lock(resources_mutex_);
      for (const auto& [id, sr] : senders_) {
        if (sr.sender_id == uuid) { conn_ok(res, staged_sender_json(sr)); return; }
      }
      conn_not_found(res);
    });

  // Staged PATCH
  node_api_svr_.Patch(R"(/x-nmos/connection/v1\.1/single/senders/([^/]+)/staged)",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      uint8_t daemon_id = 0;
      bool found = false;
      {
        std::shared_lock lock(resources_mutex_);
        for (const auto& [id, sr] : senders_) {
          if (sr.sender_id == uuid) { daemon_id = id; found = true; break; }
        }
      }
      if (!found) { conn_not_found(res); return; }
      std::string err, staged_json;
      if (!patch_sender_staged(daemon_id, req.body, err, staged_json)) {
        conn_bad_request(res, err); return;
      }
      conn_ok(res, staged_json);
    });

  // Active GET
  node_api_svr_.Get(R"(/x-nmos/connection/v1\.1/single/senders/([^/]+)/active/?)",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      std::shared_lock lock(resources_mutex_);
      for (const auto& [id, sr] : senders_) {
        if (sr.sender_id == uuid) { conn_ok(res, active_sender_json(sr)); return; }
      }
      conn_not_found(res);
    });

  // Transport file (SDP)
  node_api_svr_.Get(R"(/x-nmos/connection/v1\.1/single/senders/([^/]+)/transportfile/?)",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      uint8_t daemon_id = 0;
      bool found = false, enabled = false;
      {
        std::shared_lock lock(resources_mutex_);
        for (const auto& [id, sr] : senders_) {
          if (sr.sender_id == uuid) {
            daemon_id = id; found = true;
            enabled = sr.active_master_enable; break;
          }
        }
      }
      if (!found) { conn_not_found(res); return; }
      if (!enabled) { conn_not_found(res); return; }
      std::string sdp;
      if (!session_manager_->get_source_sdp(daemon_id, sdp)) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(sdp, "application/sdp");
      } else conn_not_found(res);
    });

  // Transport type
  node_api_svr_.Get(R"(/x-nmos/connection/v1\.1/single/senders/([^/]+)/transporttype)",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      std::shared_lock lock(resources_mutex_);
      for (const auto& [id, sr] : senders_) {
        if (sr.sender_id == uuid) {
          conn_ok(res, "\"urn:x-nmos:transport:rtp.mcast\""); return;
        }
      }
      conn_not_found(res);
    });

  // ---- Single receivers ----

  node_api_svr_.Get("/x-nmos/connection/v1.1/single/receivers/",
    [this](const Request&, Response& res) {
      std::shared_lock lock(resources_mutex_);
      std::ostringstream ss;
      ss << "[";
      bool first = true;
      for (const auto& [id, rr] : receivers_) {
        if (!first) ss << ", ";
        ss << "\"" << rr.receiver_id << "/\"";
        first = false;
      }
      ss << "]";
      conn_ok(res, ss.str());
    });

  node_api_svr_.Get(R"(/x-nmos/connection/v1\.1/single/receivers/([^/]+)/?)",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      std::shared_lock lock(resources_mutex_);
      for (const auto& [id, rr] : receivers_) {
        if (rr.receiver_id == uuid) {
          conn_ok(res, "[\"constraints/\", \"staged/\", \"active/\", \"transporttype\"]");
          return;
        }
      }
      conn_not_found(res);
    });

  node_api_svr_.Get(R"(/x-nmos/connection/v1\.1/single/receivers/([^/]+)/constraints/?)",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      std::shared_lock lock(resources_mutex_);
      for (const auto& [id, rr] : receivers_) {
        if (rr.receiver_id == uuid) { conn_ok(res, "[{}]"); return; }
      }
      conn_not_found(res);
    });

  node_api_svr_.Get(R"(/x-nmos/connection/v1\.1/single/receivers/([^/]+)/staged/?)",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      std::shared_lock lock(resources_mutex_);
      for (const auto& [id, rr] : receivers_) {
        if (rr.receiver_id == uuid) { conn_ok(res, staged_receiver_json(rr)); return; }
      }
      conn_not_found(res);
    });

  node_api_svr_.Patch(R"(/x-nmos/connection/v1\.1/single/receivers/([^/]+)/staged)",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      uint8_t daemon_id = 0;
      bool found = false;
      {
        std::shared_lock lock(resources_mutex_);
        for (const auto& [id, rr] : receivers_) {
          if (rr.receiver_id == uuid) { daemon_id = id; found = true; break; }
        }
      }
      if (!found) { conn_not_found(res); return; }
      std::string err, staged_json;
      if (!patch_receiver_staged(daemon_id, req.body, err, staged_json)) {
        conn_bad_request(res, err); return;
      }
      conn_ok(res, staged_json);
    });

  node_api_svr_.Get(R"(/x-nmos/connection/v1\.1/single/receivers/([^/]+)/active/?)",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      std::shared_lock lock(resources_mutex_);
      for (const auto& [id, rr] : receivers_) {
        if (rr.receiver_id == uuid) { conn_ok(res, active_receiver_json(rr)); return; }
      }
      conn_not_found(res);
    });

  node_api_svr_.Get(R"(/x-nmos/connection/v1\.1/single/receivers/([^/]+)/transporttype)",
    [this](const Request& req, Response& res) {
      std::string uuid = req.matches[1];
      std::shared_lock lock(resources_mutex_);
      for (const auto& [id, rr] : receivers_) {
        if (rr.receiver_id == uuid) {
          conn_ok(res, "\"urn:x-nmos:transport:rtp.mcast\""); return;
        }
      }
      conn_not_found(res);
    });

  // ---- Bulk endpoints ----

  node_api_svr_.Get("/x-nmos/connection/v1.1/bulk/senders",
    [](const Request&, Response& res) {
      res.status = 405;
      res.set_header("Allow", "POST");
      res.set_content("{\"code\":405,\"error\":\"Method Not Allowed\",\"debug\":\"\"}",
                      "application/json");
    });
  node_api_svr_.Get("/x-nmos/connection/v1.1/bulk/receivers",
    [](const Request&, Response& res) {
      res.status = 405;
      res.set_header("Allow", "POST");
      res.set_content("{\"code\":405,\"error\":\"Method Not Allowed\",\"debug\":\"\"}",
                      "application/json");
    });

  auto bulk_handler = [this](const Request& req, Response& res,
                              bool is_sender) {
    // Body: [{id: "uuid", params: {...}}, ...]
    namespace pt_ns = boost::property_tree;
    pt_ns::ptree pt;
    try {
      std::istringstream ss(req.body);
      pt_ns::read_json(ss, pt);
    } catch (...) {
      conn_bad_request(res, "invalid JSON"); return;
    }

    std::ostringstream out;
    out << "[";
    bool first_out = true;
    for (const auto& [key, item] : pt) {
      std::string uuid = item.get<std::string>("id", "");
      // Serialize the sub-object back to JSON for patch_*_staged
      std::ostringstream params_ss;
      try {
        pt_ns::write_json(params_ss, item.get_child("params"));
      } catch (...) { params_ss.str("{}"); }

      uint8_t daemon_id = 0;
      bool found = false;
      {
        std::shared_lock lock(resources_mutex_);
        if (is_sender) {
          for (const auto& [id, sr] : senders_) {
            if (sr.sender_id == uuid) { daemon_id = id; found = true; break; }
          }
        } else {
          for (const auto& [id, rr] : receivers_) {
            if (rr.receiver_id == uuid) { daemon_id = id; found = true; break; }
          }
        }
      }

      int code = 200;
      std::string err, staged_json_ignored;
      if (!found) { code = 404; err = "not found"; }
      else {
        bool ok = is_sender ? patch_sender_staged(daemon_id, params_ss.str(), err, staged_json_ignored)
                            : patch_receiver_staged(daemon_id, params_ss.str(), err, staged_json_ignored);
        if (!ok) code = 400;
      }

      if (!first_out) out << ", ";
      out << "{\"id\": \"" << uuid << "\", \"code\": " << code;
      if (!err.empty()) out << ", \"error\": \"" << err << "\"";
      out << "}";
      first_out = false;
    }
    out << "]";
    res.status = 200;
    conn_ok(res, out.str());
  };

  node_api_svr_.Post("/x-nmos/connection/v1.1/bulk/senders",
    [bulk_handler](const Request& req, Response& res) {
      bulk_handler(req, res, true);
    });
  node_api_svr_.Post("/x-nmos/connection/v1.1/bulk/receivers",
    [bulk_handler](const Request& req, Response& res) {
      bulk_handler(req, res, false);
    });
}

bool NmosManager::server_worker() {
  BOOST_LOG_TRIVIAL(info) << "NmosManager:: Node API listening on port "
                          << config_->get_nmos_node_port();
  node_api_svr_.listen("0.0.0.0", config_->get_nmos_node_port());
  BOOST_LOG_TRIVIAL(info) << "NmosManager:: Node API server started";
  return true;
}

// ---------------------------------------------------------------------------
// Registration client helpers
// ---------------------------------------------------------------------------

bool NmosManager::register_resource(const std::string& type,
                                     const std::string& data_json) {
  Client cli(config_->get_nmos_registry_address(),
                      config_->get_nmos_registry_port());
  cli.set_connection_timeout(5, 0);
  cli.set_read_timeout(10, 0);

  std::string body = "{\"type\": \"" + type + "\", \"data\": " + data_json + "}";
  auto res = cli.Post("/x-nmos/registration/v1.3/resource", body, "application/json");
  if (!res) {
    BOOST_LOG_TRIVIAL(error) << "NmosManager:: register " << type
                             << " failed (no response)";
    return false;
  }
  if (res->status != 200 && res->status != 201) {
    BOOST_LOG_TRIVIAL(error) << "NmosManager:: register " << type
                             << " returned HTTP " << res->status;
    return false;
  }
  BOOST_LOG_TRIVIAL(debug) << "NmosManager:: registered " << type;
  return true;
}

bool NmosManager::unregister_resource(const std::string& type,
                                       const std::string& id) {
  Client cli(config_->get_nmos_registry_address(),
                      config_->get_nmos_registry_port());
  cli.set_connection_timeout(5, 0);
  cli.set_read_timeout(10, 0);

  std::string path = "/x-nmos/registration/v1.3/resource/" + type + "s/" + id;
  auto res = cli.Delete(path.c_str());
  if (!res) {
    BOOST_LOG_TRIVIAL(warning) << "NmosManager:: unregister " << type << " " << id
                               << " failed (no response)";
    return false;
  }
  if (res->status != 204) {
    BOOST_LOG_TRIVIAL(warning) << "NmosManager:: unregister " << type
                               << " returned HTTP " << res->status;
    return false;
  }
  return true;
}

bool NmosManager::heartbeat() {
  Client cli(config_->get_nmos_registry_address(),
                      config_->get_nmos_registry_port());
  cli.set_connection_timeout(5, 0);
  cli.set_read_timeout(10, 0);

  std::string path = "/x-nmos/registration/v1.3/health/nodes/" + node_id_;
  auto res = cli.Post(path.c_str(), "", "application/json");
  if (!res) {
    BOOST_LOG_TRIVIAL(warning) << "NmosManager:: heartbeat failed (no response)";
    return false;
  }
  if (res->status == 404) {
    BOOST_LOG_TRIVIAL(warning) << "NmosManager:: node expired from registry, re-registering";
    return full_registration();
  }
  if (res->status != 200) {
    BOOST_LOG_TRIVIAL(warning) << "NmosManager:: heartbeat returned HTTP "
                               << res->status;
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Resource registration
// ---------------------------------------------------------------------------


bool NmosManager::register_node() {
  auto node_json = build_node_json();
  register_resource("node", node_json);
  std::unique_lock lock(resources_mutex_);
  node_json_ = node_json;
  return true;
}

bool NmosManager::register_source_local(uint8_t id) {
  StreamSource src;
  if (auto ec = session_manager_->get_source(id, src); ec) {
    BOOST_LOG_TRIVIAL(error) << "NmosManager:: get_source(" << +id
                             << ") failed: " << ec.message();
    return false;
  }
  std::string source_id = make_resource_uuid("source", id);
  std::string flow_id   = make_resource_uuid("flow",   id);
  std::string sender_id = make_resource_uuid("sender", id);
  SenderTp tp = build_sender_tp(src);
  std::unique_lock lock(resources_mutex_);
  SenderResources& sr   = senders_[id];
  sr.source_id          = source_id;
  sr.flow_id            = flow_id;
  sr.sender_id          = sender_id;
  sr.source_json        = build_source_json(src, source_id);
  sr.flow_json          = build_flow_json(src, source_id, flow_id);
  sr.sender_json        = build_sender_json(src, id, flow_id, sender_id, "");
  sr.staged_master_enable = src.enabled;
  sr.staged_tp            = tp;
  sr.active_master_enable = src.enabled;
  sr.active_tp            = tp;
  rebuild_device_json_locked();
  return true;
}

bool NmosManager::register_source(uint8_t id) {
  if (!register_source_local(id)) return false;
  std::string src_json, fl_json, snd_json, dev_json;
  {
    std::shared_lock lock(resources_mutex_);
    auto it = senders_.find(id);
    if (it == senders_.end()) return false;
    src_json = it->second.source_json;
    fl_json  = it->second.flow_json;
    snd_json = it->second.sender_json;
    dev_json = device_json_;
  }
  // Best-effort registry push — failures are logged but not fatal.
  register_resource("source", src_json);
  register_resource("flow",   fl_json);
  register_resource("sender", snd_json);
  register_resource("device", dev_json);
  return true;
}

bool NmosManager::unregister_source(uint8_t id) {
  SenderResources sr;
  std::string dev_json;
  {
    std::unique_lock lock(resources_mutex_);
    auto it = senders_.find(id);
    if (it == senders_.end()) return true;
    sr = it->second;
    senders_.erase(it);
    rebuild_device_json_locked();
    dev_json = device_json_;
  }
  register_resource("device", dev_json);
  unregister_resource("sender", sr.sender_id);
  unregister_resource("flow",   sr.flow_id);
  unregister_resource("source", sr.source_id);
  return true;
}

bool NmosManager::register_sink_local(uint8_t id) {
  StreamSink sink;
  if (auto ec = session_manager_->get_sink(id, sink); ec) {
    BOOST_LOG_TRIVIAL(error) << "NmosManager:: get_sink(" << +id
                             << ") failed: " << ec.message();
    return false;
  }
  std::string receiver_id = make_resource_uuid("receiver", id);
  ReceiverTp tp;
  tp.interface_ip = config_->get_ip_addr_str();
  if (sink.use_sdp && !sink.sdp.empty())
    tp = build_receiver_tp_from_sdp(sink.sdp);
  bool connected = sink.use_sdp && !sink.sdp.empty();
  std::unique_lock lock(resources_mutex_);
  ReceiverResources& rr   = receivers_[id];
  rr.receiver_id          = receiver_id;
  rr.receiver_json        = build_receiver_json(sink, receiver_id, "");
  rr.staged_master_enable = connected;
  rr.staged_tp            = tp;
  rr.active_master_enable = connected;
  rr.active_tp            = tp;
  // Restore IS-05 active sender preserved through a remove+add cycle
  auto pres = preserved_active_sender_ids_.find(id);
  if (pres != preserved_active_sender_ids_.end()) {
    rr.active_sender_id  = pres->second;
    preserved_active_sender_ids_.erase(pres);
    rr.receiver_json = build_receiver_json(sink, receiver_id, rr.active_sender_id);
  }
  rebuild_device_json_locked();
  return true;
}

bool NmosManager::register_sink(uint8_t id) {
  if (!register_sink_local(id)) return false;
  std::string rcv_json, dev_json;
  {
    std::shared_lock lock(resources_mutex_);
    auto it = receivers_.find(id);
    if (it == receivers_.end()) return false;
    rcv_json = it->second.receiver_json;
    dev_json = device_json_;
  }
  // Best-effort registry push.
  register_resource("receiver", rcv_json);
  register_resource("device", dev_json);
  return true;
}

bool NmosManager::unregister_sink(uint8_t id) {
  ReceiverResources rr;
  std::string dev_json;
  {
    std::unique_lock lock(resources_mutex_);
    auto it = receivers_.find(id);
    if (it == receivers_.end()) return true;
    rr = it->second;
    receivers_.erase(it);
    rebuild_device_json_locked();
    dev_json = device_json_;
  }
  register_resource("device", dev_json);
  unregister_resource("receiver", rr.receiver_id);
  return true;
}

bool NmosManager::full_registration() {
  // Re-sync local state in case any resources were added between init()'s
  // pre-population and now (e.g., loaded from status file asynchronously).
  for (const auto& src : session_manager_->get_sources())
    register_source_local(src.id);
  for (const auto& sink : session_manager_->get_sinks())
    register_sink_local(sink.id);

  // Collect all JSON strings under shared lock, then push to registry outside
  // the lock so PATCH requests are not blocked during slow network I/O.
  std::vector<std::pair<std::string, std::string>> to_push;
  to_push.emplace_back("node", node_json_);
  {
    std::shared_lock lock(resources_mutex_);
    to_push.emplace_back("device", device_json_);
    for (const auto& [id, sr] : senders_) {
      to_push.emplace_back("source", sr.source_json);
      to_push.emplace_back("flow",   sr.flow_json);
      to_push.emplace_back("sender", sr.sender_json);
    }
    for (const auto& [id, rr] : receivers_)
      to_push.emplace_back("receiver", rr.receiver_json);
  }

  BOOST_LOG_TRIVIAL(info) << "NmosManager:: registering with registry at "
                          << config_->get_nmos_registry_address() << ":"
                          << config_->get_nmos_registry_port();
  for (const auto& [type, json] : to_push)
    register_resource(type, json);

  return true;
}

// ---------------------------------------------------------------------------
// Observer callbacks — push events to the queue
// ---------------------------------------------------------------------------

bool NmosManager::on_ptp_status_change(const std::string& status) {
  if (!running_) return true;
  {
    std::unique_lock lock(events_mutex_);
    pending_events_.push({EventType::PtpStatusChange, 0});
  }
  events_cv_.notify_one();
  return true;
}

bool NmosManager::on_source_added(uint8_t id, const std::string& /*name*/,
                                   const std::string& /*sdp*/) {
  if (!running_) return true;
  {
    std::unique_lock lock(events_mutex_);
    pending_events_.push({EventType::SourceAdded, id});
  }
  events_cv_.notify_one();
  return true;
}

bool NmosManager::on_source_removed(uint8_t id, const std::string& /*name*/,
                                     const std::string& /*sdp*/) {
  if (!running_) return true;
  {
    std::unique_lock lock(events_mutex_);
    pending_events_.push({EventType::SourceRemoved, id});
  }
  events_cv_.notify_one();
  return true;
}

bool NmosManager::on_sink_added(uint8_t id, const std::string& /*name*/) {
  if (!running_) return true;
  {
    std::unique_lock lock(events_mutex_);
    pending_events_.push({EventType::SinkAdded, id});
  }
  events_cv_.notify_one();
  return true;
}

bool NmosManager::on_sink_removed(uint8_t id, const std::string& /*name*/) {
  if (!running_) return true;
  {
    std::unique_lock lock(events_mutex_);
    pending_events_.push({EventType::SinkRemoved, id});
  }
  events_cv_.notify_one();
  return true;
}

// ---------------------------------------------------------------------------
// Registration worker — processes events and heartbeats
// ---------------------------------------------------------------------------

bool NmosManager::registration_worker() {
  // Give the Node API server a moment to start listening
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  full_registration();

  using clock = std::chrono::steady_clock;
  auto next_hb = clock::now() + std::chrono::seconds(5);

  while (running_) {
    // Drain pending events (wait up to 1 s for the next one)
    {
      std::unique_lock lock(events_mutex_);
      events_cv_.wait_for(lock, std::chrono::seconds(1),
                          [this] { return !pending_events_.empty() || !running_; });

      while (!pending_events_.empty()) {
        Event ev = pending_events_.front();
        pending_events_.pop();
        lock.unlock();

        switch (ev.type) {
          case EventType::PtpStatusChange: register_node(); break;
          case EventType::SourceAdded:   register_source(ev.id);   break;
          case EventType::SourceRemoved: unregister_source(ev.id); break;
          case EventType::SinkAdded:     register_sink(ev.id);     break;
          case EventType::SinkRemoved:   unregister_sink(ev.id);   break;
        }

        lock.lock();
      }
    }

    // Process any scheduled IS-05 activations that have come due
    process_scheduled_activations();

    // Heartbeat when due
    if (running_ && clock::now() >= next_hb) {
      heartbeat();
      next_hb = clock::now() + std::chrono::seconds(5);
    }
  }

  // Unregister node on graceful shutdown (registry will GC the rest)
  unregister_resource("node", node_id_);
  BOOST_LOG_TRIVIAL(info) << "NmosManager:: registration worker stopped";
  return true;
}
