//
//  nmos_manager.hpp
//
//  IS-04 Node API server, IS-05 Connection Management, and Registration client.
//  Sources → NMOS Senders (+ Source + Flow); Sinks → NMOS Receivers.
//

#ifndef _NMOS_MANAGER_HPP_
#define _NMOS_MANAGER_HPP_

#include <atomic>
#include <condition_variable>
#include <future>
#include <map>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <string>
#include <vector>

#include <httplib.h>

#include "config.hpp"
#include "session_manager.hpp"

class NmosManager {
 public:
  static std::shared_ptr<NmosManager> create(
      std::shared_ptr<SessionManager> session_manager,
      std::shared_ptr<Config> config);
  NmosManager() = delete;
  NmosManager(const NmosManager&) = delete;
  NmosManager& operator=(const NmosManager&) = delete;
  virtual ~NmosManager() = default;

  bool init();
  bool terminate();

 private:
  // IS-05 activation record
  struct Is05Activation {
    std::string mode;            // "" = null
    std::string requested_time; // "" = null
    std::string activation_time;// "" = null
    int64_t     deadline_ns{-1};// steady_clock ns; -1 = not scheduled
  };

  // IS-05 RTP transport parameters for a sender leg
  struct SenderTp {
    std::string source_ip;
    std::string destination_ip;
    uint16_t    source_port{5004};
    uint16_t    destination_port{5004};
    bool        rtp_enabled{false};
  };

  // IS-05 RTP transport parameters for a receiver leg
  struct ReceiverTp {
    std::string interface_ip;
    std::string multicast_ip;        // "" = null
    uint16_t    destination_port{5004};
    std::string source_ip{"auto"};
    bool        rtp_enabled{false};
  };

  struct SenderResources {
    // IS-04
    std::string source_id, flow_id, sender_id;
    std::string source_json, flow_json, sender_json;
    // IS-05 staged
    bool        staged_master_enable{true};
    std::string staged_receiver_id; // "" = null
    Is05Activation staged_act;
    SenderTp    staged_tp;
    // IS-05 active (copy of staged after activation)
    bool        active_master_enable{true};
    std::string active_receiver_id;
    Is05Activation active_act;
    SenderTp    active_tp;
  };

  struct ReceiverResources {
    // IS-04
    std::string receiver_id;
    std::string receiver_json;
    // IS-05 staged
    bool        staged_master_enable{false};
    std::string staged_sender_id;   // "" = null
    Is05Activation staged_act;
    ReceiverTp  staged_tp;
    // IS-05 active
    bool        active_master_enable{false};
    std::string active_sender_id;
    Is05Activation active_act;
    ReceiverTp  active_tp;
  };

  // Scheduled activation awaiting its deadline
  struct PendingActivation {
    bool    is_sender;
    uint8_t daemon_id;
    int64_t deadline_ns;
  };

  enum class EventType { SourceAdded, SourceRemoved, SinkAdded, SinkRemoved };
  struct Event { EventType type; uint8_t id; };

  explicit NmosManager(std::shared_ptr<SessionManager> session_manager,
                       std::shared_ptr<Config> config)
      : session_manager_(session_manager), config_(config) {}

  // ---- IS-04 ----
  void setup_node_api();
  void rebuild_device_json_locked();

  std::string make_resource_uuid(const std::string& type, uint8_t id) const;
  std::string build_node_json() const;
  std::string build_source_json(const StreamSource& src,
                                const std::string& source_id) const;
  std::string build_flow_json(const StreamSource& src,
                              const std::string& source_id,
                              const std::string& flow_id) const;
  std::string build_sender_json(const StreamSource& src,
                                uint8_t daemon_id,
                                const std::string& flow_id,
                                const std::string& sender_id,
                                const std::string& active_receiver_id) const;
  std::string build_receiver_json(const StreamSink& sink,
                                  const std::string& receiver_id,
                                  const std::string& active_sender_id) const;

  // ---- IS-05 ----
  void setup_connection_api();

  SenderTp   build_sender_tp(const StreamSource& src) const;
  ReceiverTp build_receiver_tp_from_sdp(const std::string& sdp) const;

  std::string tp_sender_json(const SenderTp& tp) const;
  std::string tp_receiver_json(const ReceiverTp& tp) const;
  std::string activation_json(const Is05Activation& act) const;
  std::string staged_sender_json(const SenderResources& sr) const;
  std::string active_sender_json(const SenderResources& sr) const;
  std::string staged_receiver_json(const ReceiverResources& rr) const;
  std::string active_receiver_json(const ReceiverResources& rr) const;

  // PATCH body parsing; returns false + error message on bad input.
  // staged_json_out receives the staged state JSON to return to the caller
  // (captured before activation fires observer events that may erase the entry).
  bool patch_sender_staged(uint8_t daemon_id,
                           const std::string& body,
                           std::string& error_out,
                           std::string& staged_json_out);
  bool patch_receiver_staged(uint8_t daemon_id,
                             const std::string& body,
                             std::string& error_out,
                             std::string& staged_json_out);

  // Immediate: apply staged → active, call session_manager if needed.
  // Must be called WITHOUT resources_mutex_ held (it acquires it internally).
  void apply_sender_activation(uint8_t daemon_id);
  void apply_receiver_activation(uint8_t daemon_id);

  // Fetch SDP for a remote sender via IS-04 registry + manifest_href.
  void fetch_remote_sender_sdp(const std::string& sender_uuid, std::string& sdp);

  void process_scheduled_activations();

  // ---- Registration ----
  bool register_resource(const std::string& type, const std::string& data_json);
  bool unregister_resource(const std::string& type, const std::string& id);
  bool heartbeat();

  bool full_registration();
  // Update senders_[id] / receivers_[id] from session_manager (no network I/O).
  bool register_source_local(uint8_t id);
  bool register_sink_local(uint8_t id);
  bool register_source(uint8_t id);
  bool unregister_source(uint8_t id);
  bool register_sink(uint8_t id);
  bool unregister_sink(uint8_t id);

  bool on_source_added(uint8_t id, const std::string& name, const std::string& sdp);
  bool on_source_removed(uint8_t id, const std::string& name, const std::string& sdp);
  bool on_sink_added(uint8_t id, const std::string& name);
  bool on_sink_removed(uint8_t id, const std::string& name);

  bool registration_worker();
  bool server_worker();

  // ---- Members ----
  std::shared_ptr<SessionManager> session_manager_;
  std::shared_ptr<Config>         config_;

  std::string node_id_;
  std::string device_id_;
  std::string node_json_;

  mutable std::shared_mutex       resources_mutex_;
  std::map<uint8_t, SenderResources>   senders_;
  std::map<uint8_t, ReceiverResources> receivers_;
  std::string device_json_;

  mutable std::mutex              pending_act_mutex_;
  std::vector<PendingActivation>  pending_activations_;

  // IS-05 active-sender preservation across unregister/register cycles
  // (session_manager::add_sink triggers remove+add observers for existing sinks)
  std::map<uint8_t, std::string>  preserved_active_sender_ids_; // guarded by resources_mutex_

  httplib::Server      node_api_svr_;
  std::atomic_bool     running_{false};
  std::future<bool>    reg_res_;
  std::future<bool>    svr_res_;

  std::mutex                  events_mutex_;
  std::condition_variable     events_cv_;
  std::queue<Event>           pending_events_;
};

#endif
