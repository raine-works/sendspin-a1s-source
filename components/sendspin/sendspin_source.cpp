#include "sendspin_source.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_SOURCE)

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <esp_heap_caps.h>
#include <esp_timer.h>

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace esphome::sendspin_ {

static const char *const TAG = "sendspin.source";

// Capture times run on a sample clock: each block starts where the previous one ended, and only a fraction of its
// disagreement with the block's arrival time is folded back in. Arrival times jitter with I2S DMA and task scheduling
// but are unbiased, so the slew tracks the ADC's real rate against esp_timer without putting the jitter on the wire.
static const int64_t CAPTURE_TIME_SLEW_DIVISOR = 32;
// A disagreement this large is a discontinuity (the microphone restarted or dropped audio), not jitter: re-anchor.
static const int64_t CAPTURE_TIME_REANCHOR_US = 20000;

void SendspinSource::setup() {
  this->source_role_ = this->parent_->get_source_role();
  if (this->source_role_ == nullptr) {
    ESP_LOGE(TAG, "Failed to get source role from hub");
    this->mark_failed();
    return;
  }

  this->stream_info_ = this->microphone_source_->get_audio_stream_info();

  this->microphone_source_->add_data_callback([this](const std::vector<uint8_t> &data) {
    // THREAD CONTEXT: Microphone task, the source role's single producer
    if (data.empty()) {
      return;  // A read timeout; the role rejects empty writes with a warning
    }

    if (this->line_sense_) {
      int32_t peak = 0;
      if (this->stream_info_.get_bits_per_sample() == 32) {
        const int32_t *samples32 = reinterpret_cast<const int32_t *>(data.data());
        const size_t count = data.size() / sizeof(int32_t);
        for (size_t i = 0; i < count; ++i) {
          int32_t val = std::abs(samples32[i] >> 16);
          if (val > peak) {
            peak = val;
          }
        }
      } else {
        const int16_t *samples = reinterpret_cast<const int16_t *>(data.data());
        const size_t count = data.size() / sizeof(int16_t);
        for (size_t i = 0; i < count; ++i) {
          int32_t val = std::abs(static_cast<int32_t>(samples[i]));
          if (val > peak) {
            peak = val;
          }
        }
      }
      const int16_t peak16 = static_cast<int16_t>(std::min<int32_t>(peak, 32767));
      int16_t prev = this->current_peak_.load(std::memory_order_relaxed);
      while (peak16 > prev && !this->current_peak_.compare_exchange_weak(prev, peak16, std::memory_order_relaxed)) {
      }
    }

    if (this->source_role_->is_streaming()) {
      const int64_t block_us = static_cast<int64_t>(this->stream_info_.bytes_to_frames(data.size())) * 1000000 /
                               this->stream_info_.get_sample_rate();
      // The block was just read, so its first sample was captured about one block ago
      const int64_t arrival_us = esp_timer_get_time() - block_us;
      const int64_t error_us = arrival_us - this->next_capture_time_us_;

      int64_t capture_time_us = arrival_us;
      if (std::llabs(error_us) < CAPTURE_TIME_REANCHOR_US) {
        capture_time_us = this->next_capture_time_us_ + error_us / CAPTURE_TIME_SLEW_DIVISOR;
      }
      this->next_capture_time_us_ = capture_time_us + block_us;

      if (this->source_role_->write_audio(data.data(), data.size(), capture_time_us)) {
        this->total_bytes_streamed_.fetch_add(data.size(), std::memory_order_relaxed);
      } else {
        this->dropped_writes_.fetch_add(1, std::memory_order_relaxed);
      }
    }
  });

  if (this->line_sense_) {
    ESP_LOGI(TAG,
             "Line sense enabled (threshold: %d, debounce: %" PRIu32 " ms, silence timeout: %" PRIu32 " ms); "
             "starting continuous microphone capture",
             this->signal_threshold_, this->debounce_duration_ms_, this->silence_timeout_ms_);
    this->microphone_source_->start();
  }
}

void SendspinSource::loop() {
  if (this->source_role_ != nullptr && this->source_role_->is_streaming()) {
    const uint32_t now = millis();
    if (now - this->last_stats_log_ms_ >= 5000) {
      this->last_stats_log_ms_ = now;
      const uint32_t duration_s = (now - this->stream_start_ms_) / 1000;
      const uint32_t kb_sent = static_cast<uint32_t>(this->total_bytes_streamed_.load(std::memory_order_relaxed) / 1024);
      const uint32_t drops = this->dropped_writes_.load(std::memory_order_relaxed);
      ESP_LOGI(TAG, "Streaming: %" PRIu32 "s active | %" PRIu32 " KB sent | %" PRIu32 " drops",
               duration_s, kb_sent, drops);
    }
  }

  if (!this->line_sense_ || this->source_role_ == nullptr) {
    return;
  }

  const uint32_t now = millis();
  if (now - this->last_signal_check_ms_ < 100) {
    return;
  }
  this->last_signal_check_ms_ = now;

  const int16_t peak = this->current_peak_.exchange(0, std::memory_order_relaxed);
  this->last_peak_ = peak;

  const bool is_above = (peak >= this->signal_threshold_);

  if (is_above) {
    this->signal_below_start_ms_ = 0;
    if (this->current_signal_ != sendspin::SourceSignal::PRESENT) {
      if (this->signal_above_start_ms_ == 0) {
        this->signal_above_start_ms_ = now;
      } else if (now - this->signal_above_start_ms_ >= this->debounce_duration_ms_) {
        this->current_signal_ = sendspin::SourceSignal::PRESENT;
        this->signal_state_reported_ = true;
        this->source_role_->set_signal(sendspin::SourceSignal::PRESENT);
        ESP_LOGI(TAG, "Audio signal detected (peak: %d >= %d); reported PRESENT to server",
                 peak, this->signal_threshold_);
        this->signal_callbacks_.call(true);
      }
    }
  } else {
    this->signal_above_start_ms_ = 0;
    // Initial report after boot: wait 1500 ms of baseline silence before publishing ABSENT
    const uint32_t timeout = this->signal_state_reported_ ? this->silence_timeout_ms_ : 1500;
    if (this->current_signal_ == sendspin::SourceSignal::PRESENT || !this->signal_state_reported_) {
      if (this->signal_below_start_ms_ == 0) {
        this->signal_below_start_ms_ = now;
      } else if (now - this->signal_below_start_ms_ >= timeout) {
        this->current_signal_ = sendspin::SourceSignal::ABSENT;
        this->signal_state_reported_ = true;
        this->source_role_->set_signal(sendspin::SourceSignal::ABSENT);
        if (timeout == this->silence_timeout_ms_) {
          ESP_LOGI(TAG, "Audio silence for %" PRIu32 "s (peak: %d < %d); reported ABSENT to server",
                   this->silence_timeout_ms_ / 1000, peak, this->signal_threshold_);
        } else {
          ESP_LOGI(TAG, "Initial line-in state: ABSENT (peak: %d < %d)", peak, this->signal_threshold_);
        }
        this->signal_callbacks_.call(false);
      }
    }
  }
}

void SendspinSource::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Sendspin Source:\n"
                "  Codec: %s\n"
                "  Format: %" PRIu32 " Hz, %u channel(s), %u bits per sample\n"
                "  Chunk duration: %" PRIu32 " ms\n"
                "  Opus complexity: %u\n"
                "  Line sense: %s\n"
                "  Signal threshold: %d (%.1f dBFS)\n"
                "  Silence timeout: %" PRIu32 " ms\n"
                "  Debounce duration: %" PRIu32 " ms",
                this->role_config_.codec == sendspin::SendspinCodecFormat::OPUS ? "Opus" : "PCM",
                this->stream_info_.get_sample_rate(), this->stream_info_.get_channels(),
                this->stream_info_.get_bits_per_sample(), this->role_config_.chunk_duration_ms,
                this->role_config_.opus_complexity,
                YESNO(this->line_sense_),
                this->signal_threshold_,
                this->signal_threshold_ > 0 ? 20.0f * log10f(static_cast<float>(this->signal_threshold_) / 32767.0f) : -96.0f,
                this->silence_timeout_ms_,
                this->debounce_duration_ms_);
}

sendspin::SourceRoleConfig SendspinSource::build_role_config() const {
  const audio::AudioStreamInfo info = this->microphone_source_->get_audio_stream_info();
  sendspin::SourceRoleConfig config = this->role_config_;
  config.sample_rate = info.get_sample_rate();
  config.channels = info.get_channels();
  config.bit_depth = info.get_bits_per_sample();
  config.line_sense = this->line_sense_;
  return config;
}

// --- SourceRoleListener overrides ---

// THREAD CONTEXT: Main loop (fired from the hub's client loop())
void SendspinSource::on_streaming_started() {
  ESP_LOGI(TAG,
           "Server started stream (codec: %s, %" PRIu32 " Hz, %u ch, %u bit); internal free: %u B, max block: %u B; "
           "%s microphone",
           this->role_config_.codec == sendspin::SendspinCodecFormat::OPUS ? "Opus" : "PCM",
           this->stream_info_.get_sample_rate(), this->stream_info_.get_channels(),
           this->stream_info_.get_bits_per_sample(),
           static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
           static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
           this->line_sense_ ? "keeping active" : "starting");
  this->next_capture_time_us_ = 0;
  this->total_bytes_streamed_.store(0, std::memory_order_relaxed);
  this->dropped_writes_.store(0, std::memory_order_relaxed);
  this->stream_start_ms_ = millis();
  this->last_stats_log_ms_ = millis();
  if (!this->line_sense_) {
    this->microphone_source_->start();
  }
}

// THREAD CONTEXT: Main loop (fired from the hub's client loop())
void SendspinSource::on_streaming_stopped() {
  const uint32_t duration_s = (millis() - this->stream_start_ms_) / 1000;
  const uint32_t kb_sent = static_cast<uint32_t>(this->total_bytes_streamed_.load(std::memory_order_relaxed) / 1024);
  const uint32_t drops = this->dropped_writes_.load(std::memory_order_relaxed);
  ESP_LOGI(TAG, "Server stopped stream; duration: %" PRIu32 "s, sent: %" PRIu32 " KB, drops: %" PRIu32 "; %s microphone",
           duration_s, kb_sent, drops,
           this->line_sense_ ? "keeping active for line sense" : "stopping");
  if (!this->line_sense_) {
    this->microphone_source_->stop();
  }
}

}  // namespace esphome::sendspin_

#endif  // USE_ESP32 && USE_SENDSPIN_SOURCE
