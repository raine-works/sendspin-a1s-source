#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_SOURCE)

#include "sendspin_hub.h"

#include "esphome/components/audio/audio.h"
#include "esphome/components/microphone/microphone_source.h"

#include <sendspin/source_role.h>

#include <atomic>
#include <cstdint>

namespace esphome::sendspin_ {

/// @brief Streams a microphone to the Sendspin server through the source role.
///
/// The server gates streaming, so the microphone only runs between the role's started and stopped callbacks. The
/// capture format advertised to the server is whatever the MicrophoneSource delivers, since it cannot resample.
class SendspinSource final : public SendspinChild, public sendspin::SourceRoleListener {
 public:
  explicit SendspinSource(microphone::MicrophoneSource *microphone_source) : microphone_source_(microphone_source) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  /// @brief Sets the role config's non-format fields (codec, chunking, Opus tuning). Called from codegen.
  void set_role_config(const sendspin::SourceRoleConfig &config) { this->role_config_ = config; }

  /// @brief Returns the role config completed with the microphone's format.
  ///
  /// Called by the hub in its setup(), before it starts the client; the microphone has set its format by then.
  sendspin::SourceRoleConfig build_role_config() const;

  void set_line_sense(bool line_sense) { this->line_sense_ = line_sense; }
  void set_signal_threshold(int16_t threshold) { this->signal_threshold_ = threshold; }
  void set_silence_timeout_ms(uint32_t ms) { this->silence_timeout_ms_ = ms; }
  void set_debounce_duration_ms(uint32_t ms) { this->debounce_duration_ms_ = ms; }

  bool is_signal_present() const { return this->current_signal_ == sendspin::SourceSignal::PRESENT; }
  int16_t get_last_peak() const { return this->last_peak_; }

  template<typename F> void add_signal_callback(F &&callback) {
    this->signal_callbacks_.add(std::forward<F>(callback));
  }

 protected:
  // --- SourceRoleListener overrides ---
  void on_streaming_started() override;
  void on_streaming_stopped() override;

  microphone::MicrophoneSource *microphone_source_;
  sendspin::SourceRoleConfig role_config_{};
  sendspin::SourceRole *source_role_{nullptr};
  audio::AudioStreamInfo stream_info_;

  // Microphone task only: predicted capture time of the next block's first sample
  int64_t next_capture_time_us_{0};

  // Streaming statistics
  std::atomic<uint64_t> total_bytes_streamed_{0};
  std::atomic<uint32_t> dropped_writes_{0};
  uint32_t stream_start_ms_{0};
  uint32_t last_stats_log_ms_{0};

  // Line-in signal sensing (Sendspin line_sense)
  bool line_sense_{false};
  int16_t signal_threshold_{260};       // -42 dBFS default
  uint32_t debounce_duration_ms_{200};  // 200 ms sustained signal to transition to PRESENT
  uint32_t silence_timeout_ms_{20000};  // 20 s sustained silence to transition to ABSENT

  // Atomic peak value updated lock-free from the microphone FreeRTOS task
  std::atomic<int16_t> current_peak_{0};

  // Main-loop evaluation state
  int16_t last_peak_{0};
  uint32_t last_signal_check_ms_{0};
  uint32_t signal_above_start_ms_{0};
  uint32_t signal_below_start_ms_{0};
  sendspin::SourceSignal current_signal_{sendspin::SourceSignal::ABSENT};
  bool signal_state_reported_{false};
  CallbackManager<void(bool)> signal_callbacks_{};
};

}  // namespace esphome::sendspin_

#endif  // USE_ESP32 && USE_SENDSPIN_SOURCE
