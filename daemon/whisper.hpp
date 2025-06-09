//
//  whisper.hpp
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

#include <shared_mutex>
#include <whisper.h>

class Whisper {
 public:
  Whisper() = default;
  Whisper(const Whisper&) = delete;

  bool init(int sink_id,
            const std::string& model,
            const std::string& language,
            const std::string& openvino_encode_device);
  bool active() const { return ctx_ != 0; }
  const std::string get_text();
  void clear_text();
  void process_result();
  void terminate();
  void segment();
  bool transribe(const float* in, uint32_t samples_in);

 private:
  std::string to_timestamp(int64_t t, bool comma = false);

  int id_{0};
  std::string language_;
  std::vector<whisper_token> prompt_tokens_;
  std::stringstream output_text_;
  std::shared_mutex text_mutex_;
  struct whisper_state* state_;
  struct whisper_context* ctx_{0};
};
