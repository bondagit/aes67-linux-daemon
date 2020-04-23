//
//  utils.hpp
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

#ifndef _UTILS_HPP_
#define _UTILS_HPP_

#include <iostream>
#include <cstddef>

#include <httplib.h>

uint16_t crc16(const uint8_t* p, size_t len);

std::tuple<bool /* res */,
           std::string /* protocol */,
           std::string /* host */,
           std::string /* port */,
           std::string /* path */>
parse_url(const std::string& _url);

std::string get_node_id();

#endif
