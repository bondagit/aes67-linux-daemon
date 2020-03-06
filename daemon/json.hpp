//
//  json.hpp
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

#ifndef _JSON_HPP_
#define _JSON_HPP_

#include <list>
#include "session_manager.hpp"
#include "browser.hpp"

/* JSON serializers */
std::string config_to_json(const Config& config);
std::string source_to_json(const StreamSource& source);
std::string sink_to_json(const StreamSink& sink);
std::string sink_status_to_json(const SinkStreamStatus& status);
std::string ptp_config_to_json(const PTPConfig& config);
std::string ptp_status_to_json(const PTPStatus& status);
std::string sources_to_json(const std::list<StreamSource>& sources);
std::string sinks_to_json(const std::list<StreamSink>& sinks);
std::string streams_to_json(const std::list<StreamSource>& sources,
                            const std::list<StreamSink>& sinks);
std::string remote_source_to_json(const RemoteSource& source);
std::string remote_sources_to_json(const std::list<RemoteSource>& sources);

/* JSON deserializers */
Config json_to_config(std::istream& jstream, const Config& curCconfig);
Config json_to_config(std::istream& jstream);
Config json_to_config(const std::string& json, const Config& curConfig);
Config json_to_config(const std::string& json);
StreamSource json_to_source(const std::string& id, const std::string& json);
StreamSink json_to_sink(const std::string& id, const std::string& json);
PTPConfig json_to_ptp_config(const std::string& json);
void json_to_sources(std::istream& jstream,
                     std::list<StreamSource>& sources);
void json_to_sources(const std::string& json,
                     std::list<StreamSource>& sources);
void json_to_sinks(std::istream& jstream,
                     std::list<StreamSink>& sinks);
void json_to_sinks(const std::string& json,
                     std::list<StreamSink>& sinks);
void json_to_streams(std::istream& jstream,
                     std::list<StreamSource>& sources,
                     std::list<StreamSink>& sinks);
void json_to_streams(const std::string& json,
                     std::list<StreamSource>& sources,
                     std::list<StreamSink>& sinks);
#endif
