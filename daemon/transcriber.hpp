//  Transcriber.hpp
//
//  Copyright (c) 2019 2024 Andrea Bondavalli. All rights reserved.
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

#ifndef _WHISPER_HPP_
#define _WHISPER_HPP_

#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>
#include <condition_variable>
#include <alsa/asoundlib.h>

#include "capture.hpp"
#include "session_manager.hpp"
#include "whisper.hpp"

struct TranscriberInfo {
  uint16_t file_duration{0};
  uint8_t files_num{0};
  uint8_t start_file_id{0};
  uint8_t current_file_id{0};
};

class Transcriber {
 public:
  static std::shared_ptr<Transcriber> create(
      std::shared_ptr<SessionManager> session_manager,
      std::shared_ptr<Config> config);
  Transcriber() = delete;
  Transcriber(const Browser&) = delete;
  Transcriber& operator=(const Browser&) = delete;

  bool init();
  bool terminate();

  std::error_code get_info(const StreamSink& sink, TranscriberInfo& info) const;
  std::error_code get_text(const StreamSink& sink, std::string& out);
  std::error_code clear_text(const StreamSink& sink);

 protected:
  explicit Transcriber(std::shared_ptr<SessionManager> session_manager,
                       std::shared_ptr<Config> config)
      : session_manager_(session_manager), config_(config){};

 private:
  bool on_ptp_status_change(const std::string& status);
  bool on_sink_add(uint8_t id);
  bool on_sink_remove(uint8_t id);
  bool start_capture();
  bool stop_capture();
  void open_files(uint8_t files_id);
  void close_files(uint8_t files_id);
  void save_files(uint8_t files_id);

  std::shared_ptr<SessionManager> session_manager_;
  std::shared_ptr<Config> config_;
  uint16_t file_duration_{5};
  uint8_t files_num_{4};
  uint8_t channels_{4};
  std::string model_;
  std::string language_;
  std::string openvino_device_;
  float silence_thresold_{1e-4};
  uint16_t keep_samples_{1600};
  snd_pcm_uframes_t chunk_samples_{0};
  size_t bytes_per_frame_{0};
  size_t buffer_samples_{0};
  uint32_t buffer_offset_{0};
  std::unordered_map<uint8_t, uint32_t> silence_samples_;
  // std::unordered_map<uint8_t, std::shared_mutex> streams_mutex_;
  std::unordered_map<uint8_t, std::vector<float>> tmp_buf_;
  std::map<std::pair<uint8_t, uint8_t>, std::vector<float>> output_bufs_;
  std::unordered_map<uint8_t, uint32_t> output_ids_;
  std::unordered_map<uint8_t, std::atomic<bool>> sink_restarted_;
  uint32_t file_counter_{0};
  std::atomic<uint8_t> file_id_{0};
  std::unique_ptr<uint8_t[]> buffer_;
  uint32_t rate_{16000};
  std::future<bool> res_capts_;
  std::future<bool> res_trans_;
  std::atomic_bool running_{false};
  Capture capture_;
  std::mutex whisper_mutex_;
  std::condition_variable whisper_cond_;
  std::unordered_map<uint8_t, Whisper> whispers_;
};

#endif
