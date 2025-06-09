//
//  capture.cpp
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
#include <sys/time.h>
#include "utils.hpp"
#include "capture.hpp"
#include "log.hpp"

#ifndef timersub
#define timersub(a, b, result)                       \
  do {                                               \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;    \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
    if ((result)->tv_usec < 0) {                     \
      --(result)->tv_sec;                            \
      (result)->tv_usec += 1000000;                  \
    }                                                \
  } while (0)
#endif

bool Capture::xrun() {
  snd_pcm_status_t* status;
  int res;
  snd_pcm_status_alloca(&status);
  if ((res = snd_pcm_status(capture_handle_, status)) < 0) {
    BOOST_LOG_TRIVIAL(warning)
        << "capture:: pcm_xrun status: " << snd_strerror(res);
    return false;
  }
  if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
    struct timeval now, diff, tstamp;
    gettimeofday(&now, 0);
    snd_pcm_status_get_trigger_tstamp(status, &tstamp);
    timersub(&now, &tstamp, &diff);
    BOOST_LOG_TRIVIAL(error)
        << "capture:: pcm_xrun overrun!!! (at least "
        << diff.tv_sec * 1000 + diff.tv_usec / 1000.0 << " ms long";

    if ((res = snd_pcm_prepare(capture_handle_)) < 0) {
      BOOST_LOG_TRIVIAL(error)
          << "capture:: pcm_xrun prepare error: " << snd_strerror(res);
      return false;
    }
    return true; /* ok, data should be accepted again */
  }
  if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
    BOOST_LOG_TRIVIAL(error)
        << "capture:: capture stream format change? attempting recover...";
    if ((res = snd_pcm_prepare(capture_handle_)) < 0) {
      BOOST_LOG_TRIVIAL(error)
          << "capture:: pcm_xrun xrun(DRAINING) error: " << snd_strerror(res);
      return false;
    }
    return true;
  }
  BOOST_LOG_TRIVIAL(error) << "capture:: read/write error, state = "
                           << snd_pcm_state_name(
                                  snd_pcm_status_get_state(status));
  return false;
}

/* I/O suspend handler */
bool Capture::suspend() {
  int res;
  BOOST_LOG_TRIVIAL(info) << "capture:: Suspended. Trying resume. ";
  while ((res = snd_pcm_resume(capture_handle_)) == -EAGAIN)
    sleep(1); /* wait until suspend flag is released */
  if (res < 0) {
    BOOST_LOG_TRIVIAL(error) << "capture:: Failed. Restarting stream. ";
    if ((res = snd_pcm_prepare(capture_handle_)) < 0) {
      BOOST_LOG_TRIVIAL(error)
          << "capture:: suspend: prepare error:  " << snd_strerror(res);
      return false;
    }
  }
  return true;
}

ssize_t Capture::read(uint8_t* data) {
  ssize_t r;
  size_t count = chunk_samples_;

  if (!is_open_) {
    return -1;
  }

  while (count > 0) {
    r = snd_pcm_readi(capture_handle_, data, count);
    if (r == -EAGAIN || (r >= 0 && (size_t)r < count)) {
      if (!is_open_)
        return -1;
      snd_pcm_wait(capture_handle_, 1000);
    } else if (r == -EPIPE) {
      if (!xrun())
        return -1;
    } else if (r == -ESTRPIPE) {
      if (!suspend())
        return -1;
    } else if (r < 0) {
      BOOST_LOG_TRIVIAL(error) << "capture:: read error: " << snd_strerror(r);
      return -1;
    }
    if (r > 0) {
      count -= r;
      data += r * bytes_per_frame_;
    }
  }
  return chunk_samples_;
}

bool Capture::open(uint32_t rate, uint8_t channels) {
  if (is_open_) {
    BOOST_LOG_TRIVIAL(error) << "capture:: audio device already open";
    return false;
  }
  int err;
  if ((err = snd_pcm_open(&capture_handle_, device_name, SND_PCM_STREAM_CAPTURE,
                          SND_PCM_NONBLOCK)) < 0) {
    BOOST_LOG_TRIVIAL(fatal) << "capture:: cannot open audio device "
                             << device_name << " : " << snd_strerror(err);
    return false;
  }

  snd_pcm_hw_params_t* hw_params{0};
  if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
    BOOST_LOG_TRIVIAL(fatal)
        << "capture:: cannot allocate hardware parameter structure: "
        << snd_strerror(err);
    goto fail;
  }

  if ((err = snd_pcm_hw_params_any(capture_handle_, hw_params)) < 0) {
    BOOST_LOG_TRIVIAL(fatal)
        << "capture:: cannot initialize hardware parameter structure: "
        << snd_strerror(err);
    goto fail;
  }

  if ((err = snd_pcm_hw_params_set_access(capture_handle_, hw_params,
                                          SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    BOOST_LOG_TRIVIAL(fatal)
        << "capture:: cannot set access type: " << snd_strerror(err);
    goto fail;
  }

  if ((err = snd_pcm_hw_params_set_format(capture_handle_, hw_params, format)) <
      0) {
    BOOST_LOG_TRIVIAL(fatal)
        << "capture:: cannot set sample format: " << snd_strerror(err);
    goto fail;
  }

  snd_pcm_hw_params_set_rate_resample(capture_handle_, hw_params, 1);
  if ((err = snd_pcm_hw_params_set_rate_near(capture_handle_, hw_params, &rate,
                                             0)) < 0) {
    BOOST_LOG_TRIVIAL(fatal)
        << "capture:: cannot set sample rate: " << snd_strerror(err);
    goto fail;
  }

  if ((err = snd_pcm_hw_params_set_channels(capture_handle_, hw_params,
                                            channels)) < 0) {
    BOOST_LOG_TRIVIAL(fatal)
        << "capture:: cannot set channel count: " << snd_strerror(err);
    goto fail;
  }

  if ((err = snd_pcm_hw_params(capture_handle_, hw_params)) < 0) {
    BOOST_LOG_TRIVIAL(fatal)
        << "capture:: cannot set parameters: " << snd_strerror(err);
    goto fail;
  }
  snd_pcm_hw_params_get_period_size(hw_params, &chunk_samples_, 0);
  snd_pcm_hw_params_get_periods(hw_params, &periods_, 0);
  BOOST_LOG_TRIVIAL(debug) << "capture:: period_size " << chunk_samples_
                           << " periods " << periods_;
  bytes_per_frame_ = snd_pcm_format_physical_width(format) * channels / 8;

  snd_pcm_hw_params_free(hw_params);

  if ((err = snd_pcm_prepare(capture_handle_)) < 0) {
    BOOST_LOG_TRIVIAL(fatal)
        << "capture:: cannot prepare audio interface for use: "
        << snd_strerror(err);
    goto fail;
  }

  is_open_ = true;
  return true;

fail:
  if (hw_params) {
    snd_pcm_hw_params_free(hw_params);
  }
  snd_pcm_close(capture_handle_);
  return false;
}

void Capture::close() {
  if (is_open_) {
    snd_pcm_close(capture_handle_);
    is_open_ = false;
  }
}
