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

#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <iostream>
#include <string>

#include "http_server.hpp"
#include "log.hpp"
#include "json.hpp"

using namespace httplib;

static inline void set_headers(Response& res, const std::string content_type = "") {
  res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
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

static inline std::string get_http_error_message(
              const std::error_code& code) {
  std::stringstream ss;;
  ss << "(" << code.category().name() << ") " << code.message();
  return ss.str();
}

static inline void set_error(
              const std::error_code& code,
              const std::string& message,
              Response& res) {
  res.status = get_http_error_status(code);
  set_headers(res, "text/plain");
  res.body = message + " : " + get_http_error_message(code);
}

static inline void set_error(
              int status,
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

  svr_.set_base_dir(config_->get_http_base_dir().c_str());

  svr_.Get("(/|/Config|/PTP|/Sources|/Sinks|/Browser)", [&](const Request& req, Response& res) {
    std::ifstream file(config_->get_http_base_dir() + "/index.html");
    std::stringstream buffer;
    buffer << file.rdbuf();
    res.set_content(buffer.str(), "text/html");
  });

  /* allows cross-origin */
  svr_.Options("/api/(.*?)", [&](const Request & /*req*/, Response &res) {
    set_headers(res);
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
      if (!config_->save(config, false)) {
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
  svr_.Get("/api/source/sdp/([0-9]+)", [this](const Request& req, Response& res) {
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
  svr_.Get("/api/sink/status/([0-9]+)", [this](const Request& req,
                                               Response& res) {
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
        set_error(ret, "failed to get sink " + std::to_string(id) + 
                  " status", res);
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
        set_error(ret, "failed to add source " + std::to_string(source.id), res);
      } else {
        set_headers(res);
      }
    } catch (const std::runtime_error& e) {
      set_error(400, e.what(), res);
    }
  });

  /* remove a source */
  svr_.Delete("/api/source/([0-9]+)", [this](const Request& req, Response& res) {
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
  svr_.Get("/api/browse/sources", [this](const Request& req, Response& res) {
    auto const sources = browser_->get_remote_sources();
    set_headers(res, "application/json");
    res.body = remote_sources_to_json(sources);
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

  /* start http server on a separate thread */
  res_ = std::async(std::launch::async, [&]() {
    try {
      svr_.listen(config_->get_ip_addr_str().c_str(), config_->get_http_port());
    } catch (...) {
      BOOST_LOG_TRIVIAL(fatal)
          << "http_server:: "
          << "failed to listen to " << config_->get_ip_addr_str() << ":"
          << config_->get_http_port();
      return false;
    }
    return true;
  });

  /* wait for HTTP server to show up */
  httplib::Client cli(config_->get_ip_addr_str().c_str(), config_->get_http_port());
  int retry = 3;
  while (retry--) {
    auto res = cli.Get("/api/config");
    if (res && res->status == 200) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return retry;
}

bool HttpServer::terminate() {
  BOOST_LOG_TRIVIAL(info) << "http_server: stopping ... ";
  svr_.stop();
  return res_.get();
}
