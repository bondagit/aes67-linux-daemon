//
//  whisper.cpp
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

#include <boost/algorithm/string.hpp>
#include <chrono>
#include <fstream>

#include <float.h>

#include "utils.hpp"
#include "whisper.hpp"

bool Whisper::init(int id,
                   const std::string& model,
                   const std::string& language,
                   const std::string& openvino_encode_device) {
  if (ctx_) {
    (void)terminate();
  }
  output_text_.str("");

  TimeElapsed ts{"whisper:: init id " + std::to_string(id_)};

  struct whisper_context_params cparams = whisper_context_default_params();
  cparams.use_gpu = true;
  ctx_ = whisper_init_from_file_with_params(model.c_str(), cparams);
  if (!ctx_) {
    BOOST_LOG_TRIVIAL(fatal) << "whisper:: id " << id
                             << " whisper_init_from_file_with_params() failed";
    return false;
  }

  language_ = language;
  if (!whisper_is_multilingual(ctx_)) {
    if (language_ != "en") {
      BOOST_LOG_TRIVIAL(warning)
          << "whisper:: id " << id
          << " model is not multilingual, ignoring language";
      language_ = "en";
    }
  }

  whisper_ctx_init_openvino_encoder(ctx_, nullptr,
                                    openvino_encode_device.c_str(), nullptr);
  id_ = id;
  return true;
}

std::string Whisper::to_timestamp(int64_t t, bool comma) {
  int64_t msec = t * 10;
  int64_t hr = msec / (1000 * 60 * 60);
  msec = msec - hr * (1000 * 60 * 60);
  int64_t min = msec / (1000 * 60);
  msec = msec - min * (1000 * 60);
  int64_t sec = msec / 1000;
  msec = msec - sec * 1000;

  char buf[32];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d%s%03d", (int)hr, (int)min,
           (int)sec, comma ? "," : ".", (int)msec);
  return std::string(buf);
}

void Whisper::process_result() {
  prompt_tokens_.clear();
  const int n_segments = whisper_full_n_segments(ctx_);
  for (int i = 0; i < n_segments; ++i) {
    const char* text = whisper_full_get_segment_text(ctx_, i);
    if (text) {
      auto t0 = whisper_full_get_segment_t0(ctx_, i);
      auto t1 = whisper_full_get_segment_t1(ctx_, i);

      const int n_tokens = whisper_full_n_tokens(ctx_, i);
      for (int j = 0; j < n_tokens; j++) {
        auto token_text = std::string(whisper_full_get_token_text(ctx_, i, j));
        whisper_token_data data = whisper_full_get_token_data(ctx_, i, j);

        prompt_tokens_.push_back(data.id);
        BOOST_LOG_TRIVIAL(debug)
            << "whisper:: id " << id_ << " [" << to_timestamp(data.t0) << " -> "
            << to_timestamp(data.t1) << "] token id " << data.id << " ["
            << token_text << "] prob " << data.p;
      }

      BOOST_LOG_TRIVIAL(info)
          << "whisper:: id " << id_ << " [" << to_timestamp(t0) << " -> "
          << to_timestamp(t1) << "] text [" << text << "] ";

      if (text[0] == ' ') {
        text++;
      }
      if (std::string(text) != "[BLANK_AUDIO]") {
        std::unique_lock text_lock(text_mutex_);
        output_text_ << text << std::endl;
      }
    }
  }
}

// #define _DEBUG_SAVE_RAW_AUDIO_

bool Whisper::transribe(const float* in, uint32_t samples_in) {
  TimeElapsed ts{"whisper:: transribe() id " + std::to_string(id_)};
  // run the inference
  whisper_full_params wparams =
      whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);

  wparams.duration_ms = 0;
  wparams.print_progress = false;
  wparams.print_special = false;
  wparams.print_realtime = false;
  wparams.translate = false;
  wparams.language = language_.c_str();
  auto hw_concurrency = std::thread::hardware_concurrency();
  /* dont't compete with the capture loop */
  wparams.n_threads = (hw_concurrency > 1) ? hw_concurrency - 1 : 1;
  wparams.single_segment = false;
  wparams.print_timestamps = true;
  wparams.no_context = true;
  wparams.prompt_tokens = prompt_tokens_.data();
  wparams.prompt_n_tokens = prompt_tokens_.size();
  wparams.token_timestamps = true;

  wparams.vad = false;
  wparams.vad_model_path =
      "../3rdparty/whisper.cpp/models/ggml-silero-v5.1.2.bin";
  wparams.vad_params.threshold = 0.1f;
  wparams.vad_params.min_speech_duration_ms = 250;
  wparams.vad_params.min_silence_duration_ms = 100;
  wparams.vad_params.max_speech_duration_s = FLT_MAX;
  wparams.vad_params.speech_pad_ms = 30;
  wparams.vad_params.samples_overlap = 0.1f;

#ifdef _DEBUG_SAVE_RAW_AUDIO_
  static int counter = 0;
  std::ofstream rawaudio(
      "./out" + std::to_string(id_) + std::to_string(counter++) + ".raw",
      std::ios::out | std::ios::binary);
  rawaudio.write((const char*)in, samples_in * sizeof(float));
  rawaudio.close();
#endif

  BOOST_LOG_TRIVIAL(debug) << "whisper:: transribe id " << id_
                           << " input samples " << samples_in;

  if (whisper_full(ctx_, wparams, in, samples_in) != 0) {
    BOOST_LOG_TRIVIAL(fatal)
        << "whisper:: whisper_full() id " << id_ << " failed";
    return false;
  }

  process_result();

  if (ts.elapsed() * 16 > samples_in) {
    BOOST_LOG_TRIVIAL(warning)
        << "whisper:: id " << id_
        << " processing took longer than the audio file duration";
  }

  return true;
}

const std::string Whisper::get_text() {
  std::shared_lock text_lock(text_mutex_);
  return output_text_.str();
}

void Whisper::clear_text() {
  std::unique_lock text_lock(text_mutex_);
  output_text_.str("");
}

void Whisper::segment() {
  prompt_tokens_.clear();
}

void Whisper::terminate() {
  BOOST_LOG_TRIVIAL(debug) << "whisper:: id " << id_ << " terminate";
  if (ctx_) {
    whisper_print_timings(ctx_);
    whisper_free(ctx_);
    prompt_tokens_.clear();
    ctx_ = 0;
  }
}
