/// @file
/// @brief The Sendspin source: streams a microphone to the server through sendspin-cpp's source role, with
/// sample-clock timestamps, streaming statistics and optional line sensing.

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

  /// @brief Enables line sensing: the microphone runs from setup() on, and the source reports signal present or
  /// absent to the server. Called from codegen, before setup().
  void set_line_sense(bool line_sense) { this->line_sense_ = line_sense; }

  /// @brief Sets the peak level, as a raw 16-bit sample amplitude (1–32767), at or above which audio counts as a
  /// signal. Codegen converts the YAML's dB or percentage value to this.
  void set_signal_threshold(int16_t threshold) { this->signal_threshold_ = threshold; }

  /// @brief Sets how long, in milliseconds, the peak must stay below the threshold before signal is reported absent.
  void set_silence_timeout_ms(uint32_t ms) { this->silence_timeout_ms_ = ms; }

  /// @brief Sets how long, in milliseconds, the peak must stay at or above the threshold before signal is reported
  /// present.
  void set_debounce_duration_ms(uint32_t ms) { this->debounce_duration_ms_ = ms; }

  /// @brief Returns true once line sensing has reported signal present, until it reports absent. Always false
  /// without line sensing. Main loop thread only.
  bool is_signal_present() const { return this->current_signal_ == sendspin::SourceSignal::PRESENT; }

  /// @brief Returns the highest absolute sample value (0–32767) seen in the last 100 ms line-sensing window; 0
  /// without line sensing. Main loop thread only.
  int16_t get_last_peak() const { return this->last_peak_; }

  /// @brief Registers @p callback (`void(bool present)`), called on the main loop thread each time line sensing
  /// reports signal present (true) or absent (false) to the server.
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

  // Microphone task only: sample-clock timeline. A block's capture time is anchor + frames counted since the anchor
  // + a slow, rate-limited slew; it never follows per-block arrival jitter.
  bool anchored_{false};
  int64_t anchor_us_{0};
  uint64_t frames_since_anchor_{0};
  int64_t slew_us_{0};
  int64_t filtered_error_us_{0};
  int64_t out_of_band_us_{0};
  // Set by the main loop when a stream starts; the microphone task re-anchors on its next block
  std::atomic<bool> reanchor_requested_{true};

  // Streaming statistics
  std::atomic<uint64_t> total_bytes_streamed_{0};
  std::atomic<uint32_t> dropped_writes_{0};
  std::atomic<uint32_t> reanchors_{0};
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
