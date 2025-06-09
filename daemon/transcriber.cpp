//  Transcriber.cpp
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

#include <boost/algorithm/string.hpp>

#include "utils.hpp"
#include "transcriber.hpp"

std::shared_ptr<Transcriber> Transcriber::create(
    std::shared_ptr<SessionManager> session_manager,
    std::shared_ptr<Config> config) {
  // no need to be thread-safe here
  static std::weak_ptr<Transcriber> instance;
  if (auto ptr = instance.lock()) {
    return ptr;
  }
  auto ptr =
      std::shared_ptr<Transcriber>(new Transcriber(session_manager, config));
  instance = ptr;
  return ptr;
}

bool Transcriber::init() {
  BOOST_LOG_TRIVIAL(info) << "transcriber:: init";
  session_manager_->add_ptp_status_observer(std::bind(
      &Transcriber::on_ptp_status_change, this, std::placeholders::_1));
  session_manager_->add_sink_observer(
      SessionManager::SinkObserverType::add_sink,
      std::bind(&Transcriber::on_sink_add, this, std::placeholders::_1));
  session_manager_->add_sink_observer(
      SessionManager::SinkObserverType::remove_sink,
      std::bind(&Transcriber::on_sink_remove, this, std::placeholders::_1));

  running_ = false;

  PTPStatus status;
  session_manager_->get_ptp_status(status);
  on_ptp_status_change(status.status);

  return true;
}

bool Transcriber::on_sink_add(uint8_t id) {
  sink_restarted_[id] = true;
  return true;
}

bool Transcriber::on_sink_remove(uint8_t id) {
  return true;
}

bool Transcriber::on_ptp_status_change(const std::string& status) {
  BOOST_LOG_TRIVIAL(info) << "transcriber:: new ptp status " << status;
  if (status == "locked") {
    return start_capture();
  }
  if (status == "unlocked") {
    return stop_capture();
  }
  return true;
}

bool Transcriber::start_capture() {
  if (running_)
    return true;

  BOOST_LOG_TRIVIAL(info) << "transcriber:: starting audio capture ... ";

  channels_ = config_->get_transcriber_channels();
  files_num_ = config_->get_transcriber_files_num();
  file_duration_ = config_->get_transcriber_file_duration();
  model_ = config_->get_transcriber_model();
  language_ = config_->get_transcriber_language();
  openvino_device_ = config_->get_transcriber_openvino_device();

  if (!capture_.open(rate_, channels_)) {
    BOOST_LOG_TRIVIAL(fatal) << "transcriber:: cannot open capture";
    return false;
  }

  bytes_per_frame_ = capture_.get_bytes_per_frame();
  capture_.set_chunk_samples(8000);  // 500 ms
  chunk_samples_ = capture_.get_chunk_samples();
  buffer_samples_ = rate_ * file_duration_ / chunk_samples_ * chunk_samples_;
  BOOST_LOG_TRIVIAL(debug) << "transcriber:: buffer_samples "
                           << buffer_samples_;

  buffer_.reset(new uint8_t[buffer_samples_ * bytes_per_frame_]);
  if (buffer_ == nullptr) {
    BOOST_LOG_TRIVIAL(fatal) << "transcriber:: cannot allocate audio buffer";
    return false;
  }

  buffer_offset_ = 0;
  file_id_ = 0;
  file_counter_ = 0;
  running_ = true;

  open_files(file_id_);

  /* start capturing on a separate thread */
  res_capts_ = std::async(std::launch::async, [&]() {
    BOOST_LOG_TRIVIAL(debug)
        << "transcriber:: audio capture loop start, chunk_samples = "
        << chunk_samples_;
    while (running_) {
      if ((capture_.read(buffer_.get() + buffer_offset_ * bytes_per_frame_)) <
          0) {
        break;
      }

      save_files(file_id_);
      buffer_offset_ += chunk_samples_;

      /* check if buffer is full */
      if ((buffer_offset_ + chunk_samples_) > buffer_samples_) {
        close_files(file_id_);

        std::lock_guard<std::mutex> lock(whisper_mutex_);
        /* increase file id */
        file_id_ = (file_id_ + 1) % files_num_;
        file_counter_++;
        whisper_cond_.notify_one();

        buffer_offset_ = 0;

        open_files(file_id_);
      }
    }
    BOOST_LOG_TRIVIAL(debug) << "transcriber:: audio capture loop end";

    /* notify transcription for termination*/
    std::lock_guard<std::mutex> lock(whisper_mutex_);
    whisper_cond_.notify_one();

    return true;
  });

  /* start transcribing on a separate thread */
  res_trans_ = std::async(std::launch::async, [&]() {
    BOOST_LOG_TRIVIAL(debug) << "transcriber:: transcriptions loop start";
    for (const auto& sink : session_manager_->get_sinks()) {
      if (!whispers_[sink.id].init(sink.id, model_, language_,
                                   openvino_device_)) {
        BOOST_LOG_TRIVIAL(fatal) << "transcriber:: cannot open whisper";
        return false;
      }
    }

    uint32_t current_file_conter = 0;
    uint8_t file_id = 0;
    while (1) {
      std::unique_lock whispers_lock(whisper_mutex_);
      /* wait for a new file to complete */
      whisper_cond_.wait(whispers_lock, [&] {
        return !running_ ||
               (file_counter_ > 0 && current_file_conter != file_counter_);
      });
      current_file_conter++;
      whispers_lock.unlock();

      if (!running_)
        break;

      if (file_id == file_id_.load()) {
        BOOST_LOG_TRIVIAL(error)
            << "transcriber:: requesting current capture file, "
            << "probably running to slow, skipping file "
            << std::to_string(file_id);
      } else {
        for (const auto& sink : session_manager_->get_sinks()) {
          if (sink_restarted_[sink.id]) {
            continue;
          }
          if (!whispers_[sink.id].active()) {
            /* a new sink was added */
            if (!whispers_[sink.id].init(sink.id, model_, language_,
                                         openvino_device_)) {
              BOOST_LOG_TRIVIAL(fatal) << "transcriber:: cannot open whisper";
            }
          }

          if (whispers_[sink.id].active()) {
            auto samples_num =
                output_bufs_[std::make_pair(sink.id, file_id)].size();
            BOOST_LOG_TRIVIAL(info)
                << "transcriber:: id " << (int)sink.id << " file "
                << (int)file_id << " samples " << samples_num
                << " capturing file " << (int)file_id_.load();

            if (samples_num > keep_samples_) {
              whispers_[sink.id].transribe(
                  output_bufs_[std::make_pair(sink.id, file_id)].data(),
                  samples_num);
            } else {
              whispers_[sink.id].segment();
            }
          }
        }
      }
      /* increase file id to process */
      file_id = (file_id + 1) % files_num_;
    }

    /* close all the opened Whispers*/
    for (auto& whisper : whispers_) {
      if (whisper.second.active()) {
        whisper.second.terminate();
      }
    }

    BOOST_LOG_TRIVIAL(debug) << "transcriber:: transcriptions loop end";
    return true;
  });

  return true;
}

void Transcriber::open_files(uint8_t file_id) {
  BOOST_LOG_TRIVIAL(debug) << "transcriber:: opening files with id "
                           << std::to_string(file_id) << " ...";
  for (const auto& sink : session_manager_->get_sinks()) {
    if (sink_restarted_[sink.id]) {
      BOOST_LOG_TRIVIAL(info) << "transcriber:: sink id "
                              << std::to_string(sink.id) << " restarting";
      /* sink restarted, clear buffer  */
      sink_restarted_[sink.id] = false;
      /* clear text */
      whispers_[sink.id].clear_text();
    }
    tmp_buf_[sink.id].clear();
    silence_samples_[sink.id] = 0;
  }
}

void Transcriber::save_files(uint8_t file_id) {
  auto sample_size = bytes_per_frame_ / channels_;
  for (const auto& sink : session_manager_->get_sinks()) {
    if (sink_restarted_[sink.id]) {
      continue;
    }
    for (size_t offset = 0; offset < chunk_samples_; offset++) {
      float pcmFloat{0};
      for (uint16_t ch : sink.map) {
        if (ch > channels_) {
          /* we are not capturing this channel, skip */
          continue;
        }
        /* extracted mapped channels and converted pcm from int to float*/
        uint8_t* in = buffer_.get() +
                      (buffer_offset_ + offset) * bytes_per_frame_ +
                      ch * sample_size;
        switch (sample_size) {
          case 2: {
            int16_t pcm = *in | (*(in + 1) << 8);
            pcmFloat += static_cast<float>(pcm) / 32768.0f;
          } break;
          case 3: {
            int32_t pcm = *in | (*(in + 1) << 8) | (*(in + 2) << 16);
            // If the most significant bit of the 24th bit is set
            if (*(in + 2) & 0x80) {
              pcm |= (0xFF << 24);  // Fill the upper 8 bits with 1s
            }
            pcmFloat += static_cast<float>(pcm) / 8388608.0f;
          } break;
          case 4: {
            int32_t pcm =
                *in | (*(in + 1) << 8) | (*(in + 2) << 16) | (*(in + 3) << 24);
            pcmFloat += static_cast<float>(pcm) / 2147483648.0f;
          } break;
        }
      }

      if (std::fabs(pcmFloat) < silence_thresold_) {
        silence_samples_[sink.id]++;
      }
      tmp_buf_[sink.id].push_back(pcmFloat);
    }
  }
}

void Transcriber::close_files(uint8_t file_id) {
  for (const auto& sink : session_manager_->get_sinks()) {
    if (sink_restarted_[sink.id]) {
      output_bufs_[std::make_pair(sink.id, file_id)].clear();
      continue;
    }
    BOOST_LOG_TRIVIAL(debug)
        << "transcriber:: id " << (int)sink.id << " silence samples "
        << silence_samples_[sink.id];
    // std::unique_lock streams_lock(streams_mutex_[sink.id]);
    output_bufs_[std::make_pair(sink.id, file_id)].clear();
    if (buffer_samples_ - silence_samples_[sink.id] > keep_samples_) {
      std::copy(tmp_buf_[sink.id].begin(), tmp_buf_[sink.id].end(),
                back_inserter(output_bufs_[std::make_pair(sink.id, file_id)]));
    }
    output_ids_[file_id] = file_counter_;
  }
}

bool Transcriber::stop_capture() {
  if (!running_)
    return true;

  BOOST_LOG_TRIVIAL(info) << "transcriber:: stopping audio capture ... ";
  running_ = false;
  bool ret = res_trans_.get();
  ret = res_capts_.get();
  capture_.close();
  return ret;
}

bool Transcriber::terminate() {
  BOOST_LOG_TRIVIAL(info) << "transcriber:: terminating ... ";
  return stop_capture();
}

std::error_code Transcriber::get_info(const StreamSink& sink,
                                      TranscriberInfo& info) const {
  if (!running_) {
    BOOST_LOG_TRIVIAL(warning) << "Transcriber:: not running";
    return std::error_code{DaemonErrc::transcriber_not_running};
  }

  for (uint16_t ch : sink.map) {
    if (ch >= channels_) {
      BOOST_LOG_TRIVIAL(error)
          << "Transcriber:: channel is not captured for sink "
          << std::to_string(sink.id);
      return std::error_code{DaemonErrc::transcriber_invalid_ch};
    }
  }

  auto file_id = file_id_.load();
  uint8_t start_file_id = (file_id + files_num_ / 2) % files_num_;

  info.files_num = files_num_;
  info.file_duration = file_duration_;
  info.start_file_id = start_file_id;
  info.current_file_id = file_id;

  BOOST_LOG_TRIVIAL(debug) << "Transcriber:: returning position "
                           << std::to_string(file_id);
  return std::error_code{};
}

std::error_code Transcriber::get_text(const StreamSink& sink,
                                      std::string& out) {
  if (!running_) {
    BOOST_LOG_TRIVIAL(warning) << "Transcriber:: not running";
    return std::error_code{DaemonErrc::transcriber_not_running};
  }

  for (uint16_t ch : sink.map) {
    if (ch >= channels_) {
      BOOST_LOG_TRIVIAL(error)
          << "Transcriber:: channel is not captured for sink "
          << std::to_string(sink.id);
      return std::error_code{DaemonErrc::transcriber_invalid_ch};
    }
  }

  out = whispers_[sink.id].get_text();
  return std::error_code{};
}

std::error_code Transcriber::clear_text(const StreamSink& sink) {
  if (!running_) {
    BOOST_LOG_TRIVIAL(warning) << "Transcriber:: not running";
    return std::error_code{DaemonErrc::transcriber_not_running};
  }

  for (uint16_t ch : sink.map) {
    if (ch >= channels_) {
      BOOST_LOG_TRIVIAL(error)
          << "Transcriber:: channel is not captured for sink "
          << std::to_string(sink.id);
      return std::error_code{DaemonErrc::transcriber_invalid_ch};
    }
  }
  whispers_[sink.id].clear_text();
  return std::error_code{};
}
