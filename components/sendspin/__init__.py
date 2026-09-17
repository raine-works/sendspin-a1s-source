import esphome.codegen as cg
from esphome.components import esp32, microphone, network, psram, socket, wifi
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MICROPHONE,
    CONF_SOURCE,
    CONF_TASK_STACK_IN_PSRAM,
)
from esphome.types import ConfigType

# mdns for autodiscovery
AUTO_LOAD = ["mdns"]
CODEOWNERS = ["@raine-works"]
DEPENDENCIES = ["network"]
DOMAIN = "sendspin"

CONF_CAPTURE_BUFFER = "capture_buffer"
CONF_CHUNK_DURATION = "chunk_duration"
CONF_CODEC = "codec"
CONF_DEBOUNCE_DURATION = "debounce_duration"
CONF_LINE_SENSE = "line_sense"
CONF_OPUS_BITRATE = "opus_bitrate"
CONF_OPUS_COMPLEXITY = "opus_complexity"
CONF_SIGNAL_THRESHOLD = "signal_threshold"
CONF_SILENCE_TIMEOUT = "silence_timeout"

# sendspin-cpp build with Noise_KKpsk2 transport encryption and source-role support
SENDSPIN_CPP_REPO = "https://github.com/raine-works/sendspin-cpp.git"
SENDSPIN_CPP_REF = "9fbdbc42750d47d228f24bf050f04444f7cbef3e"

# sendspin-cpp library lives in the global `sendspin` namespace.
sendspin_library_ns = cg.global_ns.namespace("sendspin")

# Library Enums
SendspinCodecFormat = sendspin_library_ns.enum("SendspinCodecFormat", is_class=True)
CODEC_FORMAT_PCM = SendspinCodecFormat.enum("PCM")
CODEC_FORMAT_OPUS = SendspinCodecFormat.enum("OPUS")

# Library Structs
SourceRoleConfig = sendspin_library_ns.struct("SourceRoleConfig")

# Trailing underscore avoids clashing with sendspin-cpp's global `sendspin` namespace.
sendspin_ns = cg.esphome_ns.namespace("sendspin_")
SendspinHub = sendspin_ns.class_(
    "SendspinHub",
    cg.Component,
)
SendspinSource = sendspin_ns.class_(
    "SendspinSource",
    cg.Component,
)

SOURCE_CODECS = {
    "pcm": CODEC_FORMAT_PCM,
    "opus": CODEC_FORMAT_OPUS,
}


def _request_high_performance_networking(config: ConfigType) -> ConfigType:
    """Request high performance networking for Sendspin streaming."""
    network.require_high_performance_networking()
    socket.consume_sockets(
        1, "sendspin_websocket_server", socket.SocketType.TCP_LISTEN
    )(config)
    socket.consume_sockets(4, "sendspin_websocket_server")(config)
    socket.consume_sockets(1, "sendspin_websocket_client")(config)

    wifi.enable_runtime_power_save_control()
    wifi.enable_runtime_roaming_suppression()
    return config


def validate_signal_threshold(value):
    """Validates audio signal threshold in dBFS (e.g. -42dB), percentage (e.g. 1%), or raw amplitude."""
    if isinstance(value, (int, float)):
        if value < 0:
            amplitude = int(32767.0 * (10.0 ** (value / 20.0)))
            return max(1, min(32767, amplitude))
        elif value <= 1.0 and isinstance(value, float):
            return max(1, min(32767, int(32767.0 * value)))
        else:
            return max(1, min(32767, int(value)))
    s = str(value).strip()
    if s.endswith("%"):
        try:
            pct = float(s[:-1].strip()) / 100.0
            return max(1, min(32767, int(32767.0 * pct)))
        except ValueError:
            pass
    if s.lower().endswith("dbfs"):
        try:
            db = float(s[:-4].strip())
            return max(1, min(32767, int(32767.0 * (10.0 ** (db / 20.0)))))
        except ValueError:
            pass
    if s.lower().endswith("db"):
        try:
            db = float(s[:-2].strip())
            return max(1, min(32767, int(32767.0 * (10.0 ** (db / 20.0)))))
        except ValueError:
            pass
    try:
        val = float(s)
        if val < 0:
            return max(1, min(32767, int(32767.0 * (10.0 ** (val / 20.0)))))
        return max(1, min(32767, int(val)))
    except ValueError:
        raise cv.Invalid(
            f"Invalid signal threshold: {value}. Use e.g. -42dB, -40dBFS, 1%, or 300"
        )


SOURCE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SendspinSource),
        cv.Required(CONF_MICROPHONE): microphone.microphone_source_schema(
            min_bits_per_sample=16,
            max_bits_per_sample=32,
            min_channels=1,
            max_channels=2,
        ),
        cv.Optional(CONF_CODEC): cv.enum(SOURCE_CODECS, lower=True),
        cv.Optional(CONF_CHUNK_DURATION): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_CAPTURE_BUFFER): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_OPUS_BITRATE): cv.positive_int,
        cv.Optional(CONF_OPUS_COMPLEXITY): cv.uint8_t,
        cv.Optional(CONF_LINE_SENSE, default=False): cv.boolean,
        cv.Optional(CONF_SIGNAL_THRESHOLD, default="-42dB"): validate_signal_threshold,
        cv.Optional(
            CONF_SILENCE_TIMEOUT, default="20s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_DEBOUNCE_DURATION, default="200ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_TASK_STACK_IN_PSRAM): psram.validate_task_stack_in_psram,
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SendspinHub),
            cv.Optional(CONF_TASK_STACK_IN_PSRAM): psram.validate_task_stack_in_psram,
            cv.Optional(CONF_SOURCE): SOURCE_SCHEMA,
        }
    ),
    cv.only_on_esp32,
    _request_high_performance_networking,
)

FINAL_VALIDATE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SOURCE): cv.Schema(
            {
                cv.Required(
                    CONF_MICROPHONE
                ): microphone.final_validate_microphone_source_schema("sendspin"),
            },
            extra=cv.ALLOW_EXTRA,
        ),
    },
    extra=cv.ALLOW_EXTRA,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if config.get(CONF_TASK_STACK_IN_PSRAM):
        cg.add(var.set_task_stack_in_psram(True))
        psram.request_external_task_stack()

    # sendspin-cpp library
    esp32.add_idf_component(
        name="sendspin/sendspin-cpp", repo=SENDSPIN_CPP_REPO, ref=SENDSPIN_CPP_REF
    )

    cg.add_define("USE_SENDSPIN", True)  # for MDNS

    # Disable unused code paths in sendspin-cpp
    esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_COLOR", False)
    esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_ARTWORK", False)
    esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_CONTROLLER", False)
    esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_METADATA", False)
    esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_PLAYER", False)
    esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_VISUALIZER", False)

    if (source_config := config.get(CONF_SOURCE)) is not None:
        cg.add_define("USE_SENDSPIN_SOURCE", True)
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_SOURCE", True)
        # Block rather than drop when the httpd control queue fills during Wi-Fi jitter
        esp32.add_idf_sdkconfig_option("CONFIG_HTTPD_QUEUE_WORK_BLOCKING", True)
        # Increase the UDP control mailbox size (default 6) to 32 for higher queue capacity
        esp32.add_idf_sdkconfig_option("CONFIG_LWIP_UDP_RECVMBOX_SIZE", 32)
        # Route micro-opus pseudostack and codec state to internal RAM with PSRAM fallback for real-time encode speed
        esp32.add_idf_sdkconfig_option("CONFIG_OPUS_NONTHREADSAFE_PSEUDOSTACK", True)
        esp32.add_idf_sdkconfig_option("CONFIG_OPUS_THREADSAFE_PSEUDOSTACK", False)
        esp32.add_idf_sdkconfig_option("CONFIG_OPUS_USE_ALLOCA", False)
        esp32.add_idf_sdkconfig_option("CONFIG_OPUS_PSEUDOSTACK_PREFER_INTERNAL", True)
        esp32.add_idf_sdkconfig_option("CONFIG_OPUS_PSEUDOSTACK_PREFER_PSRAM", False)
        esp32.add_idf_sdkconfig_option("CONFIG_OPUS_PSEUDOSTACK_SIZE", 60000)
        esp32.add_idf_sdkconfig_option("CONFIG_OPUS_STATE_PREFER_INTERNAL", True)
        esp32.add_idf_sdkconfig_option("CONFIG_OPUS_STATE_PREFER_PSRAM", False)
        esp32.add_idf_sdkconfig_option("CONFIG_OPUS_ENABLE_XTENSA_OPTIMIZATIONS", True)
        esp32.add_idf_sdkconfig_option("CONFIG_COMPILER_OPTIMIZATION_PERF", True)

        mic_source = await microphone.microphone_source_to_code(
            source_config[CONF_MICROPHONE]
        )
        source = cg.new_Pvariable(source_config[CONF_ID], mic_source)
        await cg.register_component(source, source_config)
        await cg.register_parented(source, var)
        cg.add(var.set_source(source))

        source_fields = []
        if (chunk_duration := source_config.get(CONF_CHUNK_DURATION)) is not None:
            source_fields.append(
                ("chunk_duration_ms", chunk_duration.total_milliseconds)
            )
        if (capture_buffer := source_config.get(CONF_CAPTURE_BUFFER)) is not None:
            source_fields.append(
                ("capture_buffer_ms", capture_buffer.total_milliseconds)
            )
        if (opus_bitrate := source_config.get(CONF_OPUS_BITRATE)) is not None:
            source_fields.append(("opus_bitrate", opus_bitrate))
        if (codec := source_config.get(CONF_CODEC)) is not None:
            source_fields.append(("codec", codec))
        if (opus_complexity := source_config.get(CONF_OPUS_COMPLEXITY)) is not None:
            source_fields.append(("opus_complexity", opus_complexity))
        if (line_sense := source_config.get(CONF_LINE_SENSE)) is not None:
            source_fields.append(("line_sense", line_sense))
            cg.add(source.set_line_sense(line_sense))
        if source_config.get(CONF_TASK_STACK_IN_PSRAM):
            psram.request_external_task_stack()
            source_fields.append(("psram_stack", True))
        if (signal_threshold := source_config.get(CONF_SIGNAL_THRESHOLD)) is not None:
            cg.add(source.set_signal_threshold(signal_threshold))
        if (silence_timeout := source_config.get(CONF_SILENCE_TIMEOUT)) is not None:
            cg.add(source.set_silence_timeout_ms(silence_timeout.total_milliseconds))
        if (debounce_duration := source_config.get(CONF_DEBOUNCE_DURATION)) is not None:
            cg.add(source.set_debounce_duration_ms(debounce_duration.total_milliseconds))
        cg.add(
            source.set_role_config(
                cg.StructInitializer(SourceRoleConfig, *source_fields)
            )
        )
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_SOURCE", False)
