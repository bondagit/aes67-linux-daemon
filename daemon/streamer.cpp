//  streamer.cpp
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
#include "streamer.hpp"

std::shared_ptr<Streamer> Streamer::create(
    std::shared_ptr<SessionManager> session_manager,
    std::shared_ptr<Config> config) {
  // no need to be thread-safe here
  static std::weak_ptr<Streamer> instance;
  if (auto ptr = instance.lock()) {
    return ptr;
  }
  auto ptr = std::shared_ptr<Streamer>(new Streamer(session_manager, config));
  instance = ptr;
  return ptr;
}

bool Streamer::init() {
  BOOST_LOG_TRIVIAL(info) << "Streamer: init";
  session_manager_->add_ptp_status_observer(
      std::bind(&Streamer::on_ptp_status_change, this, std::placeholders::_1));
  session_manager_->add_sink_observer(
      SessionManager::SinkObserverType::add_sink,
      std::bind(&Streamer::on_sink_add, this, std::placeholders::_1));
  session_manager_->add_sink_observer(
      SessionManager::SinkObserverType::remove_sink,
      std::bind(&Streamer::on_sink_remove, this, std::placeholders::_1));

  running_ = false;

  PTPStatus status;
  session_manager_->get_ptp_status(status);
  on_ptp_status_change(status.status);

  return true;
}

bool Streamer::on_sink_add(uint8_t id) {
  return true;
}

bool Streamer::on_sink_remove(uint8_t id) {
  if (faac_[id]) {
    std::unique_lock faac_lock(faac_mutex_[id]);
    faacEncClose(faac_[id]);
    faac_[id] = 0;
  }
  total_sink_samples_[id] = 0;
  return true;
}

bool Streamer::on_ptp_status_change(const std::string& status) {
  BOOST_LOG_TRIVIAL(info) << "Streamer: new ptp status " << status;
  if (status == "locked") {
    return start_capture();
  }
  if (status == "unlocked") {
    return stop_capture();
  }
  return true;
}

bool Streamer::start_capture() {
  if (running_)
    return true;

  BOOST_LOG_TRIVIAL(info) << "streamer: starting audio capture ... ";

  channels_ = config_->get_streamer_channels();
  files_num_ = config_->get_streamer_files_num();
  file_duration_ = config_->get_streamer_file_duration();
  rate_ = config_->get_sample_rate();

  if (!capture_.open(rate_, channels_)) {
    BOOST_LOG_TRIVIAL(fatal) << "streamer:: cannot open capture";
    return false;
  }

  bytes_per_frame_ = capture_.get_bytes_per_frame();
  capture_.set_chunk_samples(6144);  // AAC 6 channels input
  chunk_samples_ = capture_.get_chunk_samples();

  buffer_samples_ = rate_ * file_duration_ / chunk_samples_ * chunk_samples_;
  BOOST_LOG_TRIVIAL(info) << "streamer: buffer_samples " << buffer_samples_;
  buffer_.reset(new uint8_t[buffer_samples_ * bytes_per_frame_]);
  if (buffer_ == nullptr) {
    BOOST_LOG_TRIVIAL(fatal) << "streamer: cannot allocate audio buffer";
    return false;
  }

  buffer_offset_ = 0;
  total_sink_samples_.clear();
  file_id_ = 0;
  file_counter_ = 0;
  running_ = true;

  open_files(file_id_);

  /* start capturing on a separate thread */
  res_ = std::async(std::launch::async, [&]() {
    BOOST_LOG_TRIVIAL(debug)
        << "streamer: audio capture loop start, chunk_samples = "
        << chunk_samples_;
    while (running_) {
      if ((capture_.read(buffer_.get() + buffer_offset_ * bytes_per_frame_)) <
          0) {
        break;
      }

      save_files(file_id_);
      buffer_offset_ += chunk_samples_;

      /* check id buffer is full */
      if (buffer_offset_ + chunk_samples_ > buffer_samples_) {
        close_files(file_id_);
        /* increase file id */
        file_id_ = (file_id_ + 1) % files_num_;
        file_counter_++;
        buffer_offset_ = 0;

        open_files(file_id_);
      }
    }
    BOOST_LOG_TRIVIAL(debug) << "streamer: audio capture loop end";
    return true;
  });

  return true;
}

void Streamer::open_files(uint8_t files_id) {
  BOOST_LOG_TRIVIAL(debug) << "streamer: opening files with id "
                           << std::to_string(files_id) << " ...";
  for (const auto& sink : session_manager_->get_sinks()) {
    tmp_streams_[sink.id].str("");
    std::unique_lock faac_lock(faac_mutex_[sink.id]);
    if (!faac_[sink.id]) {
      setup_codec(sink);
    }
  }
}

void Streamer::save_files(uint8_t files_id) {
  auto sample_size = bytes_per_frame_ / channels_;

  for (const auto& sink : session_manager_->get_sinks()) {
    total_sink_samples_[sink.id] += chunk_samples_;
    for (size_t offset = 0; offset < chunk_samples_; offset++) {
      for (uint16_t ch : sink.map) {
        auto in = buffer_.get() + (buffer_offset_ + offset) * bytes_per_frame_ +
                  ch * sample_size;
        std::copy(in, in + sample_size,
                  std::ostream_iterator<uint8_t>(tmp_streams_[sink.id]));
      }
    }
  }
}

bool Streamer::setup_codec(const StreamSink& sink) {
  /* open and setup the encoder */
  faac_[sink.id] = faacEncOpen(config_->get_sample_rate(), sink.map.size(),
                               &codec_in_samples_[sink.id],
                               &codec_out_buffer_size_[sink.id]);
  if (!faac_[sink.id]) {
    BOOST_LOG_TRIVIAL(fatal) << "streamer:: cannot open codec";
    return false;
  }
  BOOST_LOG_TRIVIAL(debug) << "streamer: codec samples in "
                           << codec_in_samples_[sink.id] << " out buffer size "
                           << codec_out_buffer_size_[sink.id];
  faacEncConfigurationPtr faac_cfg;
  /* check faac version */
  faac_cfg = faacEncGetCurrentConfiguration(faac_[sink.id]);
  if (!faac_cfg) {
    BOOST_LOG_TRIVIAL(fatal) << "streamer:: cannot get codec configuration";
    return false;
  }

  faac_cfg->aacObjectType = LOW;
  faac_cfg->mpegVersion = MPEG4;
  faac_cfg->useTns = 0;
  faac_cfg->useLfe = sink.map.size() > 6 ? 1 : 0;
  faac_cfg->shortctl = SHORTCTL_NORMAL;
  faac_cfg->allowMidside = 2;
  faac_cfg->bitRate = 64000 / sink.map.size();
  // faac_cfg->bandWidth = 18000;
  // faac_cfg->quantqual = 50;
  // faac_cfg->pnslevel = 4;
  // faac_cfg->jointmode = JOINT_MS;
  faac_cfg->outputFormat = 1;
  faac_cfg->inputFormat = FAAC_INPUT_16BIT;

  if (!faacEncSetConfiguration(faac_[sink.id], faac_cfg)) {
    BOOST_LOG_TRIVIAL(fatal) << "streamer:: cannot set codec configuration";
    return false;
  }

  out_buffer_size_[sink.id] = 0;
  return true;
}

void Streamer::close_files(uint8_t files_id) {
  uint16_t sample_size = bytes_per_frame_ / channels_;

  std::list<std::future<bool> > ress;
  for (const auto& sink : session_manager_->get_sinks()) {
    ress.emplace_back(std::async(std::launch::async, [=]() {
      uint32_t out_len = 0;
      {
        std::unique_lock faac_lock(faac_mutex_[sink.id]);
        if (!faac_[sink.id])
          return false;

        auto codec_in_samples = codec_in_samples_[sink.id];
        uint32_t out_size = codec_out_buffer_size_[sink.id] * buffer_samples_ *
                            sink.map.size() / codec_in_samples;

        if (out_size > out_buffer_size_[sink.id]) {
          out_buffer_[sink.id].reset(new uint8_t[out_size]);
          if (out_buffer_[sink.id] == nullptr) {
            BOOST_LOG_TRIVIAL(fatal)
                << "streamer: cannot allocate output buffer";
            return false;
          }
          out_buffer_size_[sink.id] = out_size;
        }

        uint32_t in_samples = 0;
        bool end = false;
        while (!end) {
          if (in_samples + codec_in_samples >=
              buffer_samples_ * sink.map.size()) {
            uint16_t diff = buffer_samples_ * sink.map.size() - in_samples;
            codec_in_samples = diff;
            end = true;
          }

          auto ret = faacEncEncode(
              faac_[sink.id],
              (int32_t*)(tmp_streams_[sink.id].str().c_str() +
                         in_samples * sample_size),
              codec_in_samples, out_buffer_[sink.id].get() + out_len,
              codec_out_buffer_size_[sink.id]);
          if (ret < 0) {
            BOOST_LOG_TRIVIAL(error)
                << "streamer: cannot encode file id "
                << std::to_string(files_id) << " for sink id "
                << std::to_string(sink.id);
            return false;
          }

          in_samples += codec_in_samples;
          out_len += ret;
        }
      }
      std::unique_lock streams_lock(streams_mutex_[sink.id]);
      output_streams_[std::make_pair(sink.id, files_id)].str("");
      std::copy(out_buffer_[sink.id].get(),
                out_buffer_[sink.id].get() + out_len,
                std::ostream_iterator<uint8_t>(
                    output_streams_[std::make_pair(sink.id, files_id)]));
      output_ids_[files_id] = file_counter_;
      return true;
    }));
  }

  for (auto& res : ress) {
    (void)res.get();
  }
}

bool Streamer::stop_capture() {
  if (!running_)
    return true;

  BOOST_LOG_TRIVIAL(info) << "streamer: stopping audio capture ... ";
  running_ = false;
  bool ret = res_.get();
  for (const auto& sink : session_manager_->get_sinks()) {
    if (faac_[sink.id]) {
      faacEncClose(faac_[sink.id]);
      faac_[sink.id] = 0;
    }
  }
  capture_.close();
  return ret;
}

bool Streamer::terminate() {
  BOOST_LOG_TRIVIAL(info) << "streamer: terminating ... ";
  return stop_capture();
}

std::error_code Streamer::get_info(const StreamSink& sink, StreamerInfo& info) {
  if (!running_) {
    BOOST_LOG_TRIVIAL(warning) << "streamer:: not running";
    return std::error_code{DaemonErrc::streamer_not_running};
  }

  for (uint16_t ch : sink.map) {
    if (ch >= channels_) {
      BOOST_LOG_TRIVIAL(error) << "streamer:: channel is not captured for sink "
                               << std::to_string(sink.id);
      return std::error_code{DaemonErrc::streamer_invalid_ch};
    }
  }

  if (total_sink_samples_[sink.id] < buffer_samples_ * (files_num_ - 1)) {
    BOOST_LOG_TRIVIAL(warning)
        << "streamer:: not enough samples buffered for sink "
        << std::to_string(sink.id);
    return std::error_code{DaemonErrc::streamer_retry_later};
  }

  auto file_id = file_id_.load();
  uint8_t start_file_id = (file_id + files_num_ / 2) % files_num_;

  switch (capture_.get_format()) {
    case SND_PCM_FORMAT_S16_LE:
      info.format = "s16";
      break;
    case SND_PCM_FORMAT_S24_3LE:
      info.format = "s24";
      break;
    case SND_PCM_FORMAT_S32_LE:
      info.format = "s32";
      break;
    default:
      info.format = "invalid";
      break;
  }

  info.files_num = files_num_;
  info.file_duration = file_duration_;
  info.player_buffer_files_num = player_buffer_files_num_;
  info.rate = config_->get_sample_rate();
  info.channels = sink.map.size();
  info.start_file_id = start_file_id;
  info.current_file_id = file_id;

  BOOST_LOG_TRIVIAL(debug) << "streamer:: returning position "
                           << std::to_string(file_id);
  return std::error_code{};
}

std::error_code Streamer::get_stream(const StreamSink& sink,
                                     uint8_t files_id,
                                     uint8_t& current_file_id,
                                     uint8_t& start_file_id,
                                     uint32_t& file_counter,
                                     std::string& out) {
  if (!running_) {
    BOOST_LOG_TRIVIAL(warning) << "streamer:: not running";
    return std::error_code{DaemonErrc::streamer_not_running};
  }

  for (uint16_t ch : sink.map) {
    if (ch >= channels_) {
      BOOST_LOG_TRIVIAL(error) << "streamer:: channel is not captured for sink "
                               << std::to_string(sink.id);
      return std::error_code{DaemonErrc::streamer_invalid_ch};
    }
  }

  if (total_sink_samples_[sink.id] < buffer_samples_ * (files_num_ - 1)) {
    BOOST_LOG_TRIVIAL(warning)
        << "streamer:: not enough samples buffered for sink "
        << std::to_string(sink.id);
    return std::error_code{DaemonErrc::streamer_retry_later};
  }

  current_file_id = file_id_.load();
  start_file_id = (current_file_id + files_num_ / 2) % files_num_;
  if (files_id == current_file_id) {
    BOOST_LOG_TRIVIAL(error)
        << "streamer: requesting current file id " << std::to_string(files_id);
  }
  std::shared_lock streams_lock(streams_mutex_[sink.id]);
  out = output_streams_[std::make_pair(sink.id, files_id)].str();
  file_counter = output_ids_[files_id];
  return std::error_code{};
}
