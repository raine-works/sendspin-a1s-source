#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

#include <sendspin/client.h>
#include <sendspin/config.h>
#include <sendspin/types.h>

#ifdef USE_SENDSPIN_SOURCE
#include <sendspin/source_role.h>
#endif

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace esphome::sendspin_ {

#ifdef USE_SENDSPIN_SOURCE
class SendspinSource;
#endif

/// @brief Setup priorities for the sendspin hub and its child components.
namespace sendspin_priority {
inline constexpr float HUB = esphome::setup_priority::AFTER_WIFI;
inline constexpr float CHILD = HUB - 1.0f;
}  // namespace sendspin_priority

/// @brief Sendspin Hub component managing client lifecycle, Noise encryption, and network providers.
class SendspinHub final : public Component,
                          public sendspin::SendspinClientListener,
                          public sendspin::SendspinNetworkProvider,
                          public sendspin::SendspinPersistenceProvider {
 public:
  float get_setup_priority() const override { return sendspin_priority::HUB; }
  void setup() override;
  void loop() override;
  void dump_config() override;

  void connect_to_server(const std::string &url);
  void disconnect_from_server(sendspin::SendspinGoodbyeReason reason);
  void update_state(sendspin::SendspinClientState state);

  std::optional<sendspin::ServerInformationObject> get_server_information() const;
  bool is_group_playing() const;

  std::string get_client_id() const;
  sendspin::ConnectionTrust get_trust() const;
  bool is_paired() const;
  std::string get_pairing_token() const;

  template<typename F> void add_group_update_callback(F &&callback) {
    this->group_update_callbacks_.add(std::forward<F>(callback));
  }

  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }

#ifdef USE_SENDSPIN_SOURCE
  void set_source(SendspinSource *source) { this->source_ = source; }
  sendspin::SourceRole *get_source_role();
#endif

 protected:
  sendspin::SendspinClientConfig build_client_config_();
  static const char *get_mac_address_into_buffer(std::span<char, MAC_ADDRESS_PRETTY_BUFFER_SIZE> buf);

  // --- SendspinClientListener overrides ---
  void on_group_update(const sendspin::GroupUpdateObject &group) override;
  void on_request_high_performance() override;
  void on_release_high_performance() override;

  // Encryption / pairing lifecycle callbacks
  void on_pairing_started(const std::string &server_id) override;
  void on_pairing_succeeded(const std::string &server_id) override;
  void on_pairing_failed(const std::string &server_id, sendspin::SendspinPairAbortReason reason) override;
  void on_trust_changed(sendspin::ConnectionTrust trust) override;
  void on_display_pairing_pin(const std::string &pin) override;
  void on_clear_pairing_pin() override;

  // --- SendspinNetworkProvider override ---
  bool is_network_ready() override;

  // --- SendspinPersistenceProvider overrides (ESP32 NVS) ---
  std::optional<std::vector<uint8_t>> load_blob(const std::string &key) override;
  bool save_blob(const std::string &key, const uint8_t *data, size_t len) override;
  bool erase_blob(const std::string &key) override;

#ifdef USE_SENDSPIN_SOURCE
  SendspinSource *source_{nullptr};
#endif

  std::unique_ptr<sendspin::SendspinClient> client_;
  CallbackManager<void(const sendspin::GroupUpdateObject &)> group_update_callbacks_{};
  bool task_stack_in_psram_{false};
};

/// @brief Base class for sendspin subcomponents.
class SendspinChild : public Component, public Parented<SendspinHub> {
 public:
  float get_setup_priority() const override { return sendspin_priority::CHILD; }
};

}  // namespace esphome::sendspin_

#endif  // USE_ESP32
