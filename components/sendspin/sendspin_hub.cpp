#include "sendspin_hub.h"

#ifdef USE_ESP32

#ifdef USE_SENDSPIN_SOURCE
#include "sendspin_source.h"
#endif

#include "esphome/components/network/util.h"
#ifdef USE_ETHERNET
#include "esphome/components/ethernet/ethernet_component.h"
#endif
#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"

#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>

namespace esphome::sendspin_ {

static const char *const TAG = "sendspin.hub";

void SendspinHub::setup() {
  auto config = this->build_client_config_();
  this->client_ = std::make_unique<sendspin::SendspinClient>(std::move(config));

  // Wire providers and client listener
  this->client_->set_listener(this);
  this->client_->set_network_provider(this);
  this->client_->set_persistence_provider(this);

#ifdef USE_SENDSPIN_SOURCE
  this->client_->add_source(this->source_->build_role_config()).set_listener(this->source_);
#endif

  if (!this->client_->start_server()) {
    ESP_LOGE(TAG, "Failed to start Sendspin server");
    this->mark_failed();
    return;
  }

  auto token = this->client_->pairing_token();
  if (token.has_value()) {
    ESP_LOGI(TAG, "Pairing Token: %s", token->c_str());
  }
}

void SendspinHub::loop() { this->client_->loop(); }

void SendspinHub::dump_config() {
  char mac_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  auto token = this->client_ != nullptr ? this->client_->pairing_token() : std::nullopt;
  ESP_LOGCONFIG(TAG,
                "Sendspin Hub:\n"
                "  Client ID: %s\n"
                "  MAC Address: %s\n"
                "  Pairing Token: %s\n"
                "  Task stack in PSRAM: %s",
                this->client_ != nullptr ? this->client_->client_id().c_str() : "pending",
                get_mac_address_into_buffer(mac_buf),
                token.has_value() ? token->c_str() : "none",
                YESNO(this->task_stack_in_psram_));
}

void SendspinHub::connect_to_server(const std::string &url) {
  if (this->is_ready()) {
    this->client_->connect_to(url);
  }
}

void SendspinHub::disconnect_from_server(sendspin::SendspinGoodbyeReason reason) {
  if (this->is_ready()) {
    this->client_->disconnect(reason);
  }
}

void SendspinHub::update_state(sendspin::SendspinClientState state) {
  if (this->is_ready()) {
    this->client_->update_state(state);
  }
}

std::optional<sendspin::ServerInformationObject> SendspinHub::get_server_information() const {
  if (this->is_ready()) {
    return this->client_->get_server_information();
  }
  return std::nullopt;
}

bool SendspinHub::is_group_playing() const {
  if (!this->is_ready() || !this->client_->is_connected()) {
    return false;
  }
  const auto &group = this->client_->get_group_state();
  return group.playback_state.has_value() &&
         group.playback_state.value() == sendspin::SendspinPlaybackState::PLAYING;
}

std::string SendspinHub::get_client_id() const {
  if (this->is_ready() && this->client_ != nullptr) {
    return this->client_->client_id();
  }
  return "";
}

sendspin::ConnectionTrust SendspinHub::get_trust() const {
  if (this->is_ready() && this->client_ != nullptr) {
    return this->client_->get_current_trust();
  }
  return sendspin::ConnectionTrust::NONE;
}

bool SendspinHub::is_paired() const {
  return this->get_trust() == sendspin::ConnectionTrust::USER;
}

std::string SendspinHub::get_pairing_token() const {
  if (this->is_ready() && this->client_ != nullptr) {
    auto token = this->client_->pairing_token();
    if (token.has_value()) {
      return *token;
    }
  }
  return "";
}

const char *SendspinHub::get_mac_address_into_buffer(std::span<char, MAC_ADDRESS_PRETTY_BUFFER_SIZE> buf) {
#ifdef USE_ETHERNET
  if (ethernet::global_eth_component != nullptr) {
    return ethernet::global_eth_component->get_eth_mac_address_pretty_into_buffer(buf);
  }
#endif
  return get_mac_address_pretty_into_buffer(buf);
}

sendspin::SendspinClientConfig SendspinHub::build_client_config_() {
  sendspin::SendspinClientConfig config;

  char mac_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  config.mac_address = SendspinHub::get_mac_address_into_buffer(mac_buf);
  config.name = App.get_friendly_name();
  config.product_name = App.get_name();
  config.manufacturer = "ESPHome";
  config.software_version = ESPHOME_VERSION;
  config.httpd_psram_stack = this->task_stack_in_psram_;

  return config;
}

void SendspinHub::on_group_update(const sendspin::GroupUpdateObject &group) {
  this->group_update_callbacks_.call(group);
}

void SendspinHub::on_request_high_performance() {
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr) {
    wifi::global_wifi_component->request_high_performance();
    wifi::global_wifi_component->request_roaming_suppression();
  }
#endif
}

void SendspinHub::on_release_high_performance() {
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr) {
    wifi::global_wifi_component->release_high_performance();
    wifi::global_wifi_component->release_roaming_suppression();
  }
#endif
}

void SendspinHub::on_pairing_started(const std::string &server_id) {
  ESP_LOGI(TAG, "Pairing started with server: %s", server_id.c_str());
}

void SendspinHub::on_pairing_succeeded(const std::string &server_id) {
  ESP_LOGI(TAG, "Pairing succeeded with server: %s", server_id.c_str());
}

void SendspinHub::on_pairing_failed(const std::string &server_id, sendspin::SendspinPairAbortReason reason) {
  ESP_LOGW(TAG, "Pairing failed with server: %s (reason: %u)", server_id.c_str(), static_cast<unsigned>(reason));
}

void SendspinHub::on_trust_changed(sendspin::ConnectionTrust trust) {
  ESP_LOGI(TAG, "Connection trust level: %s",
           trust == sendspin::ConnectionTrust::USER ? "USER (Paired)" : "NONE (Unpaired/Sentinel)");
}

void SendspinHub::on_display_pairing_pin(const std::string &pin) {
  ESP_LOGI(TAG, "Pairing PIN: %s", pin.c_str());
}

void SendspinHub::on_clear_pairing_pin() {
  ESP_LOGD(TAG, "Pairing PIN cleared");
}

bool SendspinHub::is_network_ready() { return network::is_connected(); }

std::optional<std::vector<uint8_t>> SendspinHub::load_blob(const std::string &key) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open("sendspin", NVS_READONLY, &handle);
  if (err != ESP_OK) {
    return std::nullopt;
  }
  size_t required_size = 0;
  err = nvs_get_blob(handle, key.c_str(), nullptr, &required_size);
  if (err != ESP_OK || required_size == 0) {
    nvs_close(handle);
    return std::nullopt;
  }
  std::vector<uint8_t> data(required_size);
  err = nvs_get_blob(handle, key.c_str(), data.data(), &required_size);
  nvs_close(handle);
  if (err == ESP_OK) {
    ESP_LOGD(TAG, "Loaded persistence blob '%s' (%u bytes)", key.c_str(), static_cast<unsigned>(required_size));
    return data;
  }
  return std::nullopt;
}

bool SendspinHub::save_blob(const std::string &key, const uint8_t *data, size_t len) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open("sendspin", NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS to save '%s': %s", key.c_str(), esp_err_to_name(err));
    return false;
  }
  err = nvs_set_blob(handle, key.c_str(), data, len);
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }
  nvs_close(handle);
  if (err == ESP_OK) {
    ESP_LOGD(TAG, "Persisted blob '%s' (%u bytes)", key.c_str(), static_cast<unsigned>(len));
    return true;
  }
  ESP_LOGW(TAG, "Failed to persist blob '%s': %s", key.c_str(), esp_err_to_name(err));
  return false;
}

bool SendspinHub::erase_blob(const std::string &key) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open("sendspin", NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return false;
  }
  err = nvs_erase_key(handle, key.c_str());
  if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) {
    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGD(TAG, "Erased persistence key '%s'", key.c_str());
    return true;
  }
  nvs_close(handle);
  ESP_LOGW(TAG, "Failed to erase persistence key '%s': %s", key.c_str(), esp_err_to_name(err));
  return false;
}

#ifdef USE_SENDSPIN_SOURCE
sendspin::SourceRole *SendspinHub::get_source_role() {
  if (this->is_ready()) {
    return this->client_->source();
  }
  return nullptr;
}
#endif

}  // namespace esphome::sendspin_

#endif  // USE_ESP32
