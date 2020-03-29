//
//  utils.cpp
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
//

#include "utils.hpp"

uint16_t crc16(const uint8_t* p, size_t len) {
  uint8_t x;
  uint16_t crc = 0xFFFF;

  while (len--) {
    x = crc >> 8 ^ *p++;
    x ^= x >> 4;
    crc = (crc << 8) ^ (static_cast<uint16_t>(x << 12)) ^
          (static_cast<uint16_t>(x << 5)) ^ (static_cast<uint16_t>(x));
  }
  return crc;
}
