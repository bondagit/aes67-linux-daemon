//
//  log.cpp
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

#include <boost/algorithm/string.hpp>
#include <boost/log/core.hpp>
#include <boost/log/sinks/syslog_backend.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <vector>

#include "config.hpp"
#include "log.hpp"

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;

using sink_t = sinks::synchronous_sink<sinks::syslog_backend>;

void log_init(const Config& config) {
  boost::shared_ptr<logging::core> core = logging::core::get();

  // remove all sink in case of re-configuration
  core->remove_all_sinks();

  // set log level
  core->set_filter(logging::trivial::severity >= config.get_log_severity());

  /* if proto is none, syslog is disable, logging on stdout */
  if (config.get_syslog_proto() != "none") {
    boost::shared_ptr<sinks::syslog_backend> backend;

    /* if proto is udp, syslog_server address is used */
    if (config.get_syslog_proto() == "udp") {
      backend = boost::make_shared<sinks::syslog_backend>(
          keywords::facility = sinks::syslog::local0,
          keywords::use_impl = sinks::syslog::udp_socket_based);

      std::vector<std::string> address;
      boost::split(address, config.get_syslog_server(), boost::is_any_of(":"));

      if (address.size() == 2) {
        // Setup the target address and port to send syslog messages to
        backend->set_target_address(address[0],
                                    boost::lexical_cast<int>(address[1]));
      }
    } else {
      /* if proto is local, local syslog is used */
      backend = boost::make_shared<sinks::syslog_backend>(
          keywords::facility = sinks::syslog::user,
          keywords::use_impl = sinks::syslog::native);
    }

    // Create and fill in another level translator for "MyLevel" attribute of
    // type string
    sinks::syslog::custom_severity_mapping<std::string> mapping("MyLevel");
    mapping["debug"] = sinks::syslog::debug;
    mapping["normal"] = sinks::syslog::info;
    mapping["warning"] = sinks::syslog::warning;
    mapping["failure"] = sinks::syslog::critical;
    backend->set_severity_mapper(mapping);

    // Wrap it into the frontend and register in the core.
    core->add_sink(boost::make_shared<sink_t>(backend));
  }
}
