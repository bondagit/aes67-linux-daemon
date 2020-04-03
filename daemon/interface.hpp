//
//  interface.hpp
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

#ifndef _INTERFACE_HPP_
#define _INTERFACE_HPP_

std::pair<uint32_t, std::string> get_interface_ip(
    const std::string& interface_name);
std::pair<std::array<uint8_t, 6>, std::string> get_interface_mac(
    const std::string& interface_name);
int get_interface_index(const std::string& interface_name);

#endif
