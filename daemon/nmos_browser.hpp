//
//  nmos_browser.hpp
//
//  Copyright (c) 2019 2026 Andrea Bondavalli. All rights reserved.
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

#ifndef _NMOS_BROWSER_HPP_
#define _NMOS_BROWSER_HPP_

#include <cstdint>
#include <string>

#include <boost/property_tree/ptree.hpp>

#include "config.hpp"
#include "utils.hpp"

struct NmosSource {
  std::string id;
  std::string address;
  std::string name;
  SDPOrigin origin;
  std::string sdp;
};

class NmosBrowser {
public:
  explicit NmosBrowser(std::shared_ptr<Config> config) : config_(config) {};
  NmosBrowser() = delete;
  NmosBrowser(const NmosBrowser&) = delete;
  NmosBrowser& operator=(const NmosBrowser&) = delete;
  virtual ~NmosBrowser() = default;

  bool get_remote_sources(std::list<NmosSource>& sources);

 private:
  struct HttpUrl {
    std::string host;
    uint16_t port;
    std::string path;
  };

  bool parse_http_url(const std::string& url, HttpUrl& parsed);
  bool http_get(const std::string& host,
                uint16_t port,
                const std::string& path,
                std::string& body);
  bool get_json_resource(const std::string& host,
                         uint16_t port,
                         const std::string& path,
                         boost::property_tree::ptree& resource);
  bool is_audio_sdp(const std::string& sdp);

  std::shared_ptr<Config> config_;
};

#endif
