//
//  main.cpp
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

namespace po = boost::program_options;
namespace postyle = boost::program_options::command_line_style;
namespace logging = boost::log;

static std::string version("bondagit-1.5.3");
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
      "daemon configuration file")("http_port,p", po::value<int>(),
                                   "HTTP server port")("help,h",
                                                       "Print this help "
                                                       "message");
  int unix_style = postyle::unix_style | postyle::short_allow_next;
  bool driver_restart(true);

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

  while (!is_terminated() && rc == EXIT_SUCCESS) {
    /* load configuration from file */
    auto config = Config::parse(filename, driver_restart);
    if (config == nullptr) {
      return EXIT_FAILURE;
    }
    /* override configuration according to command line args */
    if (vm.count("http_port")) {
      config->set_http_port(vm["http_port"].as<int>());
    }
    /* init logging */
    log_init(*config);

    if (config->get_ip_addr_str().empty()) {
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

      /* start session manager */
      auto session_manager = SessionManager::create(driver, config);
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

      /* start browser */
      auto browser = Browser::create(config);
      if (browser == nullptr || !browser->init()) {
        throw std::runtime_error(std::string("Browser:: init failed"));
      }

      /* start http server */
      HttpServer http_server(session_manager, browser, config);
      if (!http_server.init()) {
        throw std::runtime_error(std::string("HttpServer:: init failed"));
      }

      /* load session status from file */
      session_manager->load_status();

      BOOST_LOG_TRIVIAL(debug) << "main:: init done, entering loop...";
      while (!is_terminated()) {
        auto [ip_addr, ip_str] = get_interface_ip(config->get_interface_name());
        if (config->get_ip_addr_str() != ip_str) {
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

      /* save session status to file */
      session_manager->save_status();

      /* stop http server */
      if (!http_server.terminate()) {
        throw std::runtime_error(std::string("HttpServer:: terminate failed"));
      }

      /* stop browser */
      if (!browser->terminate()) {
        throw std::runtime_error(std::string("Browser:: terminate failed"));
      }

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
