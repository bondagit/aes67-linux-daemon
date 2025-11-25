//
//  http_server.cpp
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
#include <string>

#include "main.hpp"
#include "json.hpp"
#include "log.hpp"
#include "http_server.hpp"

using namespace httplib;

static inline void set_headers(Response& res,
                               const std::string& content_type = "") {
  res.set_header("Access-Control-Allow-Methods",
                 "GET, POST, PUT, DELETE, OPTIONS");
  res.set_header("Access-Control-Allow-Origin", "*");
  res.set_header("Access-Control-Allow-Headers", "x-user-id");
  if (!content_type.empty()) {
    res.set_header("Content-Type", content_type);
  }
}

static inline int get_http_error_status(const std::error_code& code) {
  if (std::string(code.category().name()) == "daemon") {
    if (code.value() < static_cast<int>(DaemonErrc::send_invalid_size)) {
      return 400;
    }
  }
  if (std::string(code.category().name()) == "driver") {
    if (code.value() == static_cast<int>(DriverErrc::command_failed)) {
      return 400;
    }
  }
  return 500;
}

static inline std::string get_http_error_message(const std::error_code& code) {
  std::stringstream ss;
  ss << "(" << code.category().name() << ") " << code.message();
  return ss.str();
}

static inline void set_error(const std::error_code& code,
                             const std::string& message,
                             Response& res) {
  res.status = get_http_error_status(code);
  set_headers(res, "text/plain");
  res.body = message + " : " + get_http_error_message(code);
}

static inline void set_error(int status,
                             const std::string& message,
                             Response& res) {
  res.status = status;
  set_headers(res, "text/plain");
  res.body = message;
}

bool HttpServer::init() {
  /* setup http operations */
  if (!svr_.is_valid()) {
    return false;
  }

  svr_.set_mount_point("/", config_->get_http_base_dir().c_str());

  svr_.Get("(/|/Config|/PTP|/Sources|/Sinks|/Browser)",
           [&](const Request& req, Response& res) {
             std::ifstream file(config_->get_http_base_dir() + "/index.html");
             std::stringstream buffer;
             buffer << file.rdbuf();
             res.set_content(buffer.str(), "text/html");
           });

  /* allows cross-origin */
  svr_.Options("/api/(.*?)", [&](const Request& /*req*/, Response& res) {
    set_headers(res);
  });

  /* get version */
  svr_.Get("/api/version", [&](const Request& req, Response& res) {
    set_headers(res, "application/json");
    res.body = "{ \"version\": \"" + get_version() + "\" }";
  });

  /* get config */
  svr_.Get("/api/config", [&](const Request& req, Response& res) {
    set_headers(res, "application/json");
    res.body = config_to_json(*config_);
  });

  /* set config */
  svr_.Post("/api/config", [this](const Request& req, Response& res) {
    try {
      Config config = json_to_config(req.body, *config_);

      if (config_->get_syslog_proto() != config.get_syslog_proto() ||
          config_->get_syslog_server() != config.get_syslog_server() ||
          config_->get_log_severity() != config.get_log_severity()) {
        log_init(config);
      }
      std::error_code ret;
      if (config_->get_playout_delay() != config.get_playout_delay()) {
        ret = session_manager_->set_driver_config("playout_delay",
                                                  config.get_playout_delay());
      }
      if (config_->get_sample_rate() != config.get_sample_rate()) {
        ret = session_manager_->set_driver_config("sample_rate",
                                                  config.get_sample_rate());
      }
      if (config_->get_ptp_domain() != config.get_ptp_domain() ||
          config_->get_ptp_dscp() != config.get_ptp_dscp()) {
        PTPConfig ptpConfig{config.get_ptp_domain(), config.get_ptp_dscp()};
        ret = session_manager_->set_ptp_config(ptpConfig);
      }
      if (ret) {
        set_error(ret, "failed to set config", res);
        return;
      }
      if (!config_->save(config)) {
        set_error(500, "failed to save config", res);
        return;
      }
      set_headers(res);
    } catch (const std::runtime_error& e) {
      set_error(400, e.what(), res);
    }
  });

  /* get ptp status */
  svr_.Get("/api/ptp/status", [this](const Request& req, Response& res) {
    PTPStatus status;
    session_manager_->get_ptp_status(status);
    set_headers(res, "application/json");
    res.body = ptp_status_to_json(status);
  });

  /* get ptp config */
  svr_.Get("/api/ptp/config", [this](const Request& req, Response& res) {
    PTPConfig ptpConfig;
    session_manager_->get_ptp_config(ptpConfig);
    set_headers(res, "application/json");
    res.body = ptp_config_to_json(ptpConfig);
  });

  /* set ptp config */
  svr_.Post("/api/ptp/config", [this](const Request& req, Response& res) {
    try {
      PTPConfig ptpConfig = json_to_ptp_config(req.body);
      auto ret = session_manager_->set_ptp_config(ptpConfig);
      if (ret) {
        set_error(ret, "failed to set ptp config", res);
        return;
      }
      Config config(*config_);
      config.set_ptp_domain(ptpConfig.domain);
      config.set_ptp_dscp(ptpConfig.dscp);
      if (!config_->save(config)) {
        set_error(500, "failed to save config", res);
        return;
      }
      set_headers(res);
    } catch (const std::runtime_error& e) {
      set_error(400, e.what(), res);
    }
  });

  /* get all sources */
  svr_.Get("/api/sources", [this](const Request& req, Response& res) {
    auto const sources = session_manager_->get_sources();
    set_headers(res, "application/json");
    res.body = sources_to_json(sources);
  });

  /* get all sinks */
  svr_.Get("/api/sinks", [this](const Request& req, Response& res) {
    auto const sinks = session_manager_->get_sinks();
    set_headers(res, "application/json");
    res.body = sinks_to_json(sinks);
  });

  /* get all sources and sinks */
  svr_.Get("/api/streams", [this](const Request& req, Response& res) {
    auto const sources = session_manager_->get_sources();
    auto const sinks = session_manager_->get_sinks();
    set_headers(res, "application/json");
    res.body = streams_to_json(sources, sinks);
  });

  /* get a source SDP */
  svr_.Get(
      "/api/source/sdp/([0-9]+)", [this](const Request& req, Response& res) {
        uint32_t id;
        try {
          id = std::stoi(req.matches[1]);
        } catch (...) {
          set_error(400, "failed to convert id", res);
          return;
        }

        auto ret = session_manager_->get_source_sdp(id, res.body);
        if (ret) {
          set_error(ret, "get source " + std::to_string(id) + " failed", res);
        } else {
          set_headers(res, "application/sdp");
        }
      });

  /* get stream status */
  svr_.Get(
      "/api/sink/status/([0-9]+)", [this](const Request& req, Response& res) {
        uint32_t id;
        try {
          id = std::stoi(req.matches[1]);
        } catch (...) {
          set_error(400, "failed to convert id", res);
          return;
        }
        SinkStreamStatus status;
        auto ret = session_manager_->get_sink_status(id, status);
        if (ret) {
          set_error(ret, "failed to get sink " + std::to_string(id) + " status",
                    res);
        } else {
          set_headers(res, "application/json");
          res.body = sink_status_to_json(status);
        }
      });

  /* add a source */
  svr_.Put("/api/source/([0-9]+)", [this](const Request& req, Response& res) {
    try {
      StreamSource source = json_to_source(req.matches[1], req.body);
      auto ret = session_manager_->add_source(source);
      if (ret) {
        set_error(ret, "failed to add source " + std::to_string(source.id),
                  res);
      } else {
        set_headers(res);
      }
    } catch (const std::runtime_error& e) {
      set_error(400, e.what(), res);
    }
  });

  /* remove a source */
  svr_.Delete(
      "/api/source/([0-9]+)", [this](const Request& req, Response& res) {
        uint32_t id;
        try {
          id = std::stoi(req.matches[1]);
        } catch (...) {
          set_error(400, "failed to convert id", res);
          return;
        }
        auto ret = session_manager_->remove_source(id);
        if (ret) {
          set_error(ret, "failed to remove source " + std::to_string(id), res);
        } else {
          set_headers(res);
        }
      });

  /* add a sink */
  svr_.Put("/api/sink/([0-9]+)", [this](const Request& req, Response& res) {
    try {
      StreamSink sink = json_to_sink(req.matches[1], req.body);
      auto ret = session_manager_->add_sink(sink);
      if (ret) {
        set_error(ret, "failed to add sink " + std::to_string(sink.id), res);
      } else {
        set_headers(res);
      }
    } catch (const std::runtime_error& e) {
      set_error(400, e.what(), res);
    }
  });

  /* remove a sink */
  svr_.Delete("/api/sink/([0-9]+)", [this](const Request& req, Response& res) {
    uint32_t id;
    try {
      id = std::stoi(req.matches[1]);
    } catch (...) {
      set_error(400, "failed to convert id", res);
      return;
    }
    auto ret = session_manager_->remove_sink(id);
    if (ret) {
      set_error(ret, "failed to remove sink " + std::to_string(id), res);
    } else {
      set_headers(res);
    }
  });

  /* get remote sources */
  svr_.Get("/api/browse/sources/(all|mdns|sap)",
           [this](const Request& req, Response& res) {
             auto const sources = browser_->get_remote_sources(req.matches[1]);
             set_headers(res, "application/json");
             res.body = remote_sources_to_json(sources);
           });

  /* retrieve streamer info and position */
#ifdef _USE_STREAMER_
  svr_.Get("/api/streamer/info/([0-9]+)", [this](const Request& req,
                                                 Response& res) {
    uint32_t id;
    StreamerInfo info;
    enum class streamer_info_status {
      ok,
      ptp_not_locked,
      invalid_channels,
      buffering,
      not_enabled,
      invalid_sink,
      cannot_retrieve
    };

    info.status = static_cast<uint8_t>(streamer_info_status::ok);
    if (!config_->get_streamer_enabled()) {
      info.status = static_cast<uint8_t>(streamer_info_status::not_enabled);
    } else {
      try {
        id = std::stoi(req.matches[1]);
      } catch (...) {
        info.status = static_cast<uint8_t>(streamer_info_status::invalid_sink);
      }
    }

    if (info.status == static_cast<uint8_t>(streamer_info_status::ok)) {
      StreamSink sink;
      auto ret = session_manager_->get_sink(id, sink);
      if (ret) {
        info.status =
            static_cast<uint8_t>(streamer_info_status::cannot_retrieve);
      } else {
        ret = streamer_->get_info(sink, info);
        switch (ret.value()) {
          case static_cast<int>(DaemonErrc::streamer_not_running):
            info.status =
                static_cast<uint8_t>(streamer_info_status::ptp_not_locked);
            break;
          case static_cast<int>(DaemonErrc::streamer_invalid_ch):
            info.status =
                static_cast<uint8_t>(streamer_info_status::invalid_channels);
            break;
          case static_cast<int>(DaemonErrc::streamer_retry_later):
            info.status = static_cast<uint8_t>(streamer_info_status::buffering);
            break;
          default:
            info.status = static_cast<uint8_t>(streamer_info_status::ok);
            break;
        }
      }
    }
    set_headers(res, "application/json");
    res.body = streamer_info_to_json(info);
  });
#endif
  /* retrieve live streamer */
  svr_.Get("/api/streamer/stream/([0-9]+)", [this](const Request& req,
                                                   Response& res) {
#ifdef _USE_STREAMER_
    if (!config_->get_streamer_enabled()) {
      set_error(400, "streamer not enabled", res);
      return;
    }
    uint8_t sinkId;
    try {
      sinkId = std::stoi(req.matches[1]);
    } catch (...) {
      set_error(400, "failed to convert id", res);
      return;
    }
    StreamSink sink;
    auto ret = session_manager_->get_sink(sinkId, sink);
    if (ret) {
      set_error(ret, "failed to retrieve sink " + std::to_string(sinkId), res);
      return;
    }

    ret = streamer_->live_stream_init(sink, req.remote_addr, req.remote_port);
    if (ret) {
      set_error(ret, "failed to init streamer " + std::to_string(sinkId), res);
      return;
    }

    res.set_content_provider("audio/aac",
                             [&](size_t /*offset*/, DataSink& httpSync) {
                               return streamer_->live_stream_wait(
                                   httpSync, req.remote_addr, req.remote_port);
                             });
#else
    set_error(400, "streamer support not compiled-in", res);
#endif
  });

  /* retrieve streamer file */
  svr_.Get("/api/streamer/stream/([0-9]+)/([0-9]+)", [this](const Request& req,
                                                            Response& res) {
#ifdef _USE_STREAMER_
    if (!config_->get_streamer_enabled()) {
      set_error(400, "streamer not enabled", res);
      return;
    }
    uint8_t sinkId, fileId;
    try {
      sinkId = std::stoi(req.matches[1]);
      fileId = std::stoi(req.matches[2]);
    } catch (...) {
      set_error(400, "failed to convert id", res);
      return;
    }
    StreamSink sink;
    auto ret = session_manager_->get_sink(sinkId, sink);
    if (ret) {
      set_error(ret, "failed to retrieve sink " + std::to_string(sinkId), res);
      return;
    }
    uint8_t currentFileId, startFileId;
    uint32_t fileCount;
    ret = streamer_->get_stream(sink, fileId, currentFileId, startFileId,
                                fileCount, res.body);
    if (ret) {
      set_error(ret, "failed to fetch stream " + std::to_string(sinkId), res);
      return;
    }
    set_headers(res, "audio/aac");
    res.set_header("X-File-Count", std::to_string(fileCount));
    res.set_header("X-File-Current-Id", std::to_string(currentFileId));
    res.set_header("X-File-Start-Id", std::to_string(startFileId));
#else
    set_error(400, "streamer support not compiled-in", res);
#endif
  });

  svr_.set_logger([](const Request& req, const Response& res) {
    if (res.status == 200) {
      BOOST_LOG_TRIVIAL(info) << "http_server:: " << req.method << " "
                              << req.path << " response " << res.status;
    } else {
      BOOST_LOG_TRIVIAL(error)
          << "http_server:: " << req.method << " " << req.path << " response "
          << res.status << " " << res.body;
    }
  });

  std::string http_addr = config_->get_http_addr_str();
  if (http_addr.empty())
    http_addr = config_->get_ip_addr_str();
  BOOST_LOG_TRIVIAL(info) << "http_server:: binding to " << http_addr << ":"
                          << config_->get_http_port();

  /* start http server on a separate thread */
  res_ = std::async(std::launch::async, [&]() {
    try {
      svr_.listen(http_addr.c_str(), config_->get_http_port());
    } catch (...) {
      BOOST_LOG_TRIVIAL(fatal) << "http_server:: " << "failed to listen to "
                               << http_addr << ":" << config_->get_http_port();
      return false;
    }
    return true;
  });

  /* wait for HTTP server to show up */
  httplib::Client cli(config_->get_ip_addr_str().c_str(),
                      config_->get_http_port());
  int retry = 3;
  while (retry) {
    auto res = cli.Get("/api/config");
    if (res && res->status == 200) {
      break;
    }
    --retry;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return retry;
}

bool HttpServer::terminate() {
  BOOST_LOG_TRIVIAL(info) << "http_server: stopping ... ";
  svr_.stop();
  return res_.get();
}
