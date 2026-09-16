#include "sendspin_source.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_SOURCE)

#include "esphome/core/log.h"

#include <esp_timer.h>

#include <cinttypes>
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

    this->source_role_->write_audio(data.data(), data.size(), capture_time_us);
  });
}

void SendspinSource::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Sendspin Source:\n"
                "  Codec: %s\n"
                "  Format: %" PRIu32 " Hz, %u channel(s), %u bits per sample\n"
                "  Chunk duration: %" PRIu32 " ms",
                this->role_config_.codec == sendspin::SendspinCodecFormat::OPUS ? "Opus" : "PCM",
                this->stream_info_.get_sample_rate(), this->stream_info_.get_channels(),
                this->stream_info_.get_bits_per_sample(), this->role_config_.chunk_duration_ms);
}

sendspin::SourceRoleConfig SendspinSource::build_role_config() const {
  const audio::AudioStreamInfo info = this->microphone_source_->get_audio_stream_info();
  sendspin::SourceRoleConfig config = this->role_config_;
  config.sample_rate = info.get_sample_rate();
  config.channels = info.get_channels();
  config.bit_depth = info.get_bits_per_sample();
  return config;
}

// --- SourceRoleListener overrides ---

// THREAD CONTEXT: Main loop (fired from the hub's client loop())
void SendspinSource::on_streaming_started() {
  ESP_LOGI(TAG, "Server started the stream; starting microphone");
  this->microphone_source_->start();
}

// THREAD CONTEXT: Main loop (fired from the hub's client loop())
void SendspinSource::on_streaming_stopped() {
  ESP_LOGI(TAG, "Server stopped the stream; stopping microphone");
  this->microphone_source_->stop();
}

}  // namespace esphome::sendspin_

#endif  // USE_ESP32 && USE_SENDSPIN_SOURCE
