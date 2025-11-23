//
//  main.cpp
//
//  Copyright (c) 2019 2025 Andrea Bondavalli. All rights reserved.
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

#include <boost/program_options.hpp>
#include <iostream>
#include <thread>

#include "browser.hpp"
#include "config.hpp"
#include "driver_interface.hpp"
#include "http_server.hpp"
#include "interface.hpp"
#include "log.hpp"
#include "mdns_server.hpp"
#include "rtsp_server.hpp"
#include "session_manager.hpp"

#ifdef _USE_STREAMER_
#include "streamer.hpp"
#endif

#ifdef _USE_SYSTEMD_
#include <systemd/sd-daemon.h>
#endif

namespace po = boost::program_options;
namespace postyle = boost::program_options::command_line_style;
namespace logging = boost::log;

static const std::string version("bondagit-2.2.0");
static std::atomic<bool> terminate = false;

void termination_handler(int signum) {
  BOOST_LOG_TRIVIAL(info) << "main:: got signal " << signum;
  // Terminate program
  terminate = true;
}

bool is_terminated() {
  return terminate.load();
}

const std::string& get_version() {
  return version;
}

int main(int argc, char* argv[]) {
  int rc(EXIT_SUCCESS);
  po::options_description desc("Options");
  desc.add_options()("version,v", "Print daemon version and exit")(
      "config,c", po::value<std::string>()->default_value("/etc/daemon.conf"),
      "daemon configuration file")("http_addr,a", po::value<std::string>(),
                                   "HTTP server addr")(
      "http_port,p", po::value<int>(), "HTTP server port")("help,h",
                                                           "Print this help "
                                                           "message");
  int unix_style = postyle::unix_style | postyle::short_allow_next;
  bool driver_restart(true);

#ifdef _USE_SYSTEMD_
  // with which interval we should pet the dog
  uint64_t current_watchdog_usec;
#endif

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv)
                  .options(desc)
                  .style(unix_style)
                  .run(),
              vm);

    po::notify(vm);

    if (vm.count("version")) {
      std::cout << version << '\n';
      return EXIT_SUCCESS;
    }
    if (vm.count("help")) {
      std::cout << "USAGE: " << argv[0] << '\n' << desc << '\n';
      return EXIT_SUCCESS;
    }

  } catch (po::error& poe) {
    std::cerr << poe.what() << '\n'
              << "USAGE: " << argv[0] << '\n'
              << desc << '\n';
    return EXIT_FAILURE;
  }

  signal(SIGINT, termination_handler);
  signal(SIGTERM, termination_handler);
  signal(SIGCHLD, SIG_IGN);

  std::srand(std::time(nullptr));

  std::string filename = vm["config"].as<std::string>();

#ifdef _USE_SYSTEMD_
  sd_watchdog_enabled(0, &current_watchdog_usec);

  if (current_watchdog_usec > 0) {
    // Inform systemd that if we're not petting the dog in 5s we're bust.
    sd_notify(0, "WATCHDOG_USEC=10000000");

    current_watchdog_usec = 10000000;
  }
#endif

  while (!is_terminated() && rc == EXIT_SUCCESS) {
    /* load configuration from file */
    auto config = Config::parse(filename, driver_restart);
    if (config == nullptr) {
      return EXIT_FAILURE;
    }

    if (vm.count("http_addr")) {
      config->set_http_addr_str(vm["http_addr"].as<std::string>());
    }
    /* override configuration according to command line args */
    if (vm.count("http_port")) {
      config->set_http_port(vm["http_port"].as<int>());
    }
    /* init logging */
    log_init(*config);

    if (config->get_ip_addr_str().empty()) {
#ifdef _USE_SYSTEMD_
      if (current_watchdog_usec > 0)
        sd_notify(0, "WATCHDOG=1");
      sd_notify(0, "STATUS=no IP address, waiting ...");
#endif
      BOOST_LOG_TRIVIAL(info) << "main:: no IP address, waiting ...";
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    BOOST_LOG_TRIVIAL(debug) << "main:: initializing daemon";
    try {
      auto driver = DriverManager::create();
      /* setup and init driver */
      if (driver == nullptr || !driver->init(*config)) {
        throw std::runtime_error(std::string("DriverManager:: init failed"));
      }

      /* start browser */
      auto browser = Browser::create(config);
      if (browser == nullptr || !browser->init()) {
        throw std::runtime_error(std::string("Browser:: init failed"));
      }

      /* start session manager */
      auto session_manager = SessionManager::create(driver, browser, config);
      if (session_manager == nullptr || !session_manager->init()) {
        throw std::runtime_error(std::string("SessionManager:: init failed"));
      }

      /* start mDNS server */
      MDNSServer mdns_server(session_manager, config);
      if (config->get_mdns_enabled() && !mdns_server.init()) {
        throw std::runtime_error(std::string("MDNSServer:: init failed"));
      }

      /* start rtsp server */
      RtspServer rtsp_server(session_manager, config);
      if (!rtsp_server.init()) {
        throw std::runtime_error(std::string("RtspServer:: init failed"));
      }

      /* start streamer */
#ifdef _USE_STREAMER_
      auto streamer = Streamer::create(session_manager, config);
      if (config->get_streamer_enabled() &&
          (streamer == nullptr || !streamer->init())) {
        throw std::runtime_error(std::string("Streamer:: init failed"));
      }

      /* start http server */
      HttpServer http_server(session_manager, browser, streamer, config);
#else
      HttpServer http_server(session_manager, browser, config);
#endif
      if (!http_server.init()) {
        throw std::runtime_error(std::string("HttpServer:: init failed"));
      }

      /* load session status from file */
      session_manager->load_status();

      BOOST_LOG_TRIVIAL(debug) << "main:: init done, entering loop...";

#ifdef _USE_SYSTEMD_
      // To be able to use sd_notify at all have to set service NotifyAccess
      // (e.g. to main)
      sd_notify(0, "READY=1");  // If service Type=notify the service is only
                                // considered ready once we send this (this is
                                // independent of watchdog capability)
      sd_notify(0, "STATUS=Working");
#endif

      while (!is_terminated()) {
#ifdef _USE_SYSTEMD_
        if (current_watchdog_usec > 0)
          sd_notify(0, "WATCHDOG=1");
#endif

        auto [ip_addr, ip_str, is_new] = get_new_interface_ip(
            config->get_interface_name(), config->get_ip_addr_str());
        if (is_new) {
          BOOST_LOG_TRIVIAL(warning)
              << "main:: IP address changed, restarting ...";
          break;
        }

        driver_restart = config->get_driver_restart();
        if (config->get_daemon_restart()) {
          BOOST_LOG_TRIVIAL(warning) << "main:: config changed, restarting ...";
          break;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
#ifdef _USE_SYSTEMD_
      if (is_terminated()) {
        sd_notify(0, "STOPPING=1");
        sd_notify(0, "STATUS=Stopping");
      } else {
        sd_notify(0, "RELOADING=1");
        sd_notify(0, "STATUS=Restarting");
      }
#endif

      /* save session status to file */
      session_manager->save_status();

      /* stop http server */
      if (!http_server.terminate()) {
        throw std::runtime_error(std::string("HttpServer:: terminate failed"));
      }

      /* stop streamer */
#ifdef _USE_STREAMER_
      if (config->get_streamer_enabled()) {
        if (!streamer->terminate()) {
          throw std::runtime_error(std::string("Streamer:: terminate failed"));
        }
      }
#endif

      /* stop rtsp server */
      if (!rtsp_server.terminate()) {
        throw std::runtime_error(std::string("RtspServer:: terminate failed"));
      }

      /* stop mDNS server */
      if (config->get_mdns_enabled()) {
        if (!mdns_server.terminate()) {
          throw std::runtime_error(std::string("MDNServer:: terminate failed"));
        }
      }

      /* stop session manager */
      if (!session_manager->terminate()) {
        throw std::runtime_error(
            std::string("SessionManager:: terminate failed"));
      }

      /* stop browser */
      if (!browser->terminate()) {
        throw std::runtime_error(std::string("Browser:: terminate failed"));
      }

      /* stop driver manager */
      if (!driver->terminate(*config)) {
        throw std::runtime_error(
            std::string("DriverManager:: terminate failed"));
      }

    } catch (std::exception& e) {
      BOOST_LOG_TRIVIAL(fatal) << "main:: fatal exception error: " << e.what();
      rc = EXIT_FAILURE;
    }

    BOOST_LOG_TRIVIAL(info) << "main:: end ";
  }

  std::cout << "daemon exiting with code: " << rc << std::endl;
  return rc;
}
