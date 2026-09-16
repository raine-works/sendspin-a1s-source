#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_SOURCE)

#include "sendspin_hub.h"

#include "esphome/components/audio/audio.h"
#include "esphome/components/microphone/microphone_source.h"

#include <sendspin/source_role.h>

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
  void dump_config() override;

  /// @brief Sets the role config's non-format fields (codec, chunking, Opus tuning). Called from codegen.
  void set_role_config(const sendspin::SourceRoleConfig &config) { this->role_config_ = config; }

  /// @brief Returns the role config completed with the microphone's format.
  ///
  /// Called by the hub in its setup(), before it starts the client; the microphone has set its format by then.
  sendspin::SourceRoleConfig build_role_config() const;

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
};

}  // namespace esphome::sendspin_

#endif  // USE_ESP32 && USE_SENDSPIN_SOURCE
