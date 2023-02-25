//
//  http_server.hpp
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

#ifndef _HTTP_SERVER_HPP_
#define _HTTP_SERVER_HPP_

#include <httplib.h>

#include "browser.hpp"
#include "config.hpp"
#include "session_manager.hpp"

class HttpServer {
 public:
  HttpServer() = delete;
  explicit HttpServer(std::shared_ptr<SessionManager> session_manager,
                      std::shared_ptr<Browser> browser,
                      std::shared_ptr<Config> config)
      : session_manager_(session_manager), browser_(browser), config_(config){};
  bool init();
  bool terminate();

 private:
  std::shared_ptr<SessionManager> session_manager_;
  std::shared_ptr<Browser> browser_;
  std::shared_ptr<Config> config_;
  httplib::Server svr_;
  std::future<bool> res_;
};

#endif
