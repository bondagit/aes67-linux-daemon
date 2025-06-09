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

#ifdef _USE_STREAMER_
#include "streamer.hpp"
#endif
#ifdef _USE_TRANSCRIBER_
#include "transcriber.hpp"
#endif

class HttpServer {
 public:
  HttpServer() = delete;
  explicit HttpServer(std::shared_ptr<SessionManager> session_manager,
                      std::shared_ptr<Browser> browser,
#ifdef _USE_STREAMER_
                      std::shared_ptr<Streamer> streamer,
#endif
#ifdef _USE_TRANSCRIBER_
                      std::shared_ptr<Transcriber> transcriber,
#endif
                      std::shared_ptr<Config> config)
      : session_manager_(session_manager),
        browser_(browser),
#ifdef _USE_STREAMER_
        streamer_(streamer),
#endif
#ifdef _USE_TRANSCRIBER_
        transcriber_(transcriber),
#endif
        config_(config){};
  bool init();
  bool terminate();

 private:
  std::shared_ptr<SessionManager> session_manager_;
  std::shared_ptr<Browser> browser_;
#ifdef _USE_STREAMER_
  std::shared_ptr<Streamer> streamer_;
#endif
#ifdef _USE_TRANSCRIBER_
  std::shared_ptr<Transcriber> transcriber_;
#endif
  std::shared_ptr<Config> config_;
  httplib::Server svr_;
  std::future<bool> res_;
};

#endif
