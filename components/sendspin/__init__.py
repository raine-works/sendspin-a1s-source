from dataclasses import dataclass, field

from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32, microphone, network, psram, socket, wifi
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_FORMAT,
    CONF_HEIGHT,
    CONF_ID,
    CONF_MICROPHONE,
    CONF_SAMPLE_RATE,
    CONF_SOURCE,
    CONF_TASK_STACK_IN_PSRAM,
    CONF_WIDTH,
)
from esphome.core import CORE, ID
from esphome.cpp_generator import TemplateArgsType
from esphome.types import ConfigType

# mdns for autodiscovery
AUTO_LOAD = ["mdns"]
CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["network"]
DOMAIN = "sendspin"

CONF_DISPLAY_OFFSET = "display_offset"
CONF_SENDSPIN_ID = "sendspin_id"

CONF_INITIAL_STATIC_DELAY = "initial_static_delay"
CONF_FIXED_DELAY = "fixed_delay"
CONF_DECODE_MEMORY = "decode_memory"

CONF_CAPTURE_BUFFER = "capture_buffer"
CONF_CHUNK_DURATION = "chunk_duration"
CONF_CODEC = "codec"
CONF_OPUS_BITRATE = "opus_bitrate"
CONF_OPUS_COMPLEXITY = "opus_complexity"

# sendspin-cpp build with Noise_KKpsk2 transport encryption and source-role support (brandenc77/sendspin-cpp@fix/source-pairing).
# The IDF component manager pins the resolved commit in dependencies.lock, so clean the build files to pick up a new push.
SENDSPIN_CPP_REPO = "https://github.com/brandenc77/sendspin-cpp.git"
SENDSPIN_CPP_REF = "fix/source-pairing"

# Matches ARTWORK_MAX_SLOTS in sendspin-cpp.
MAX_ARTWORK_SLOTS = 4

# sendspin-cpp library lives in the global `sendspin` namespace.
sendspin_library_ns = cg.global_ns.namespace("sendspin")

# Library Enums
SendspinCodecFormat = sendspin_library_ns.enum("SendspinCodecFormat", is_class=True)
CODEC_FORMAT_FLAC = SendspinCodecFormat.enum("FLAC")
CODEC_FORMAT_OPUS = SendspinCodecFormat.enum("OPUS")
CODEC_FORMAT_PCM = SendspinCodecFormat.enum("PCM")
CODEC_FORMAT_UNSUPPORTED = SendspinCodecFormat.enum("UNSUPPORTED")

SendspinImageFormat = sendspin_library_ns.enum("SendspinImageFormat", is_class=True)
IMAGE_FORMAT_JPEG = SendspinImageFormat.enum("JPEG")
IMAGE_FORMAT_PNG = SendspinImageFormat.enum("PNG")
IMAGE_FORMAT_BMP = SendspinImageFormat.enum("BMP")

SendspinImageSource = sendspin_library_ns.enum("SendspinImageSource", is_class=True)
IMAGE_SOURCE_ALBUM = SendspinImageSource.enum("ALBUM")
IMAGE_SOURCE_ARTIST = SendspinImageSource.enum("ARTIST")

# Library Structs
AudioSupportedFormatObject = sendspin_library_ns.struct("AudioSupportedFormatObject")
PlayerRoleConfig = sendspin_library_ns.struct("PlayerRoleConfig")
SourceRoleConfig = sendspin_library_ns.struct("SourceRoleConfig")
ArtworkRoleConfig = sendspin_library_ns.struct("ArtworkRoleConfig")
ImageSlotPreference = sendspin_library_ns.struct("ImageSlotPreference")

# MemoryLocation enum (from sendspin/types.h) controls SPIRAM-vs-internal-RAM placement
# preference for the player role's transfer buffers.
SendspinMemoryLocation = sendspin_library_ns.enum("MemoryLocation", is_class=True)

MEMORY_PSRAM = "psram"
MEMORY_INTERNAL = "internal"
MEMORY_LOCATIONS = [MEMORY_PSRAM, MEMORY_INTERNAL]
MEMORY_LOCATION_ENUM = {
    MEMORY_PSRAM: SendspinMemoryLocation.PREFER_EXTERNAL,
    MEMORY_INTERNAL: SendspinMemoryLocation.PREFER_INTERNAL,
}

# Trailing underscore avoids clashing with sendspin-cpp's global `sendspin` namespace.
# Analysis tools strip the trailing underscore (same pattern as `template_`).
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


SendspinSwitchCommandAction = sendspin_ns.class_(
    "SendspinSwitchCommandAction",
    automation.Action,
    cg.Parented.template(SendspinHub),
)


@dataclass
class SendspinConfiguration:
    artwork_support: bool = False
    controller_support: bool = False
    metadata_support: bool = False
    player_support: bool = False
    visualizer_support: bool = False

    artwork_preferences: list[ConfigType] = field(default_factory=list)
    player_config: ConfigType | None = None


def _get_data() -> SendspinConfiguration:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = SendspinConfiguration()
    return CORE.data[DOMAIN]


def request_artwork_support() -> None:
    """Request artwork role support for Sendspin."""
    _get_data().artwork_support = True


def request_controller_support() -> None:
    """Request controller role support for Sendspin."""
    _get_data().controller_support = True


def request_metadata_support() -> None:
    """Request metadata role support for Sendspin."""
    _get_data().metadata_support = True


def request_player_support() -> None:
    """Request player role support for Sendspin."""
    _get_data().player_support = True


def request_visualizer_support() -> None:
    """Request visualizer role support for Sendspin."""
    _get_data().visualizer_support = True


def register_artwork_preference(config: ConfigType) -> int:
    """Register an artwork slot preference and return the slot it was given.

    A slot is a preference's position in the list, which is also the order the roles are
    advertised to the server in.
    """
    request_artwork_support()
    preferences = _get_data().artwork_preferences
    if len(preferences) >= MAX_ARTWORK_SLOTS:
        raise cv.Invalid(
            f"Too many Sendspin image slots. Maximum is {MAX_ARTWORK_SLOTS}."
        )
    preferences.append(config)
    return len(preferences) - 1


def register_player_config(config: ConfigType) -> None:
    """Register the player role config from the media source subcomponent."""
    data = _get_data()
    request_player_support()
    if data.player_config is not None:
        raise cv.Invalid(
            "Only one sendspin media_source player configuration is supported"
        )
    data.player_config = config


def _request_high_performance_networking(config: ConfigType) -> ConfigType:
    """Request high performance networking for Sendspin streaming.

    Also enables wake_loop_threadsafe support for fast defer() callbacks
    from background threads (WebSocket handler, image decoder).
    """
    network.require_high_performance_networking()
    # Socket consumption varies by mode:
    # - Server mode: 1 listening socket + 4 client connections (established connection, unproven connections, and a spare)
    # - Client mode: 1 outbound connection
    socket.consume_sockets(
        1, "sendspin_websocket_server", socket.SocketType.TCP_LISTEN
    )(config)
    socket.consume_sockets(4, "sendspin_websocket_server")(config)
    socket.consume_sockets(1, "sendspin_websocket_client")(config)

    wifi.enable_runtime_power_save_control()
    wifi.enable_runtime_roaming_suppression()
    return config


# The capture format (sample rate, channels, bit depth) is whatever the microphone source delivers. Unset fields keep
# sendspin-cpp's defaults, and value rules are left to the library's fail-closed add_source() validation.
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


def _request_controller_role(config: ConfigType) -> ConfigType:
    """Request the controller role for the sendspin.switch action."""
    request_controller_support()
    return config


SENDSPIN_SIMPLE_ACTION_SCHEMA = cv.All(
    automation.maybe_simple_id(
        cv.Schema(
            {
                cv.GenerateID(): cv.use_id(SendspinHub),
            }
        )
    ),
    _request_controller_role,
)


@automation.register_action(
    "sendspin.switch",
    SendspinSwitchCommandAction,
    SENDSPIN_SIMPLE_ACTION_SCHEMA,
    synchronous=True,
)
async def sendspin_switch_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


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

    data = _get_data()

    # The color role is not yet wired up in ESPHome; disable it in the library for now.
    esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_COLOR", False)

    # Configure Sendspin roles based on requested features (ESPHome internally via USE_SENDSPIN_*)
    # and disable building unused code paths in the sendspin-cpp library (IDF SDKConfig via CONFIG_SENDSPIN_ENABLE_*).
    if data.artwork_support:
        cg.add_define("USE_SENDSPIN_ARTWORK", True)

        # require_frame_done is always on: SendspinImageSlot always acks a delivery, either
        # immediately or from the transition_finished action.
        preference_structs = [
            cg.StructInitializer(
                ImageSlotPreference,
                ("source", pref[CONF_SOURCE]),
                ("format", pref[CONF_FORMAT]),
                ("width", pref[CONF_WIDTH]),
                ("height", pref[CONF_HEIGHT]),
                ("require_frame_done", True),
                ("display_offset_ms", pref[CONF_DISPLAY_OFFSET]),
            )
            for pref in data.artwork_preferences
        ]

        artwork_psram_stack = bool(config.get(CONF_TASK_STACK_IN_PSRAM))
        artwork_config = cg.StructInitializer(
            ArtworkRoleConfig,
            ("preferred_formats", preference_structs),
            ("psram_stack", artwork_psram_stack),
        )
        cg.add(var.set_artwork_config(artwork_config))
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_ARTWORK", False)

    if data.controller_support:
        cg.add_define("USE_SENDSPIN_CONTROLLER", True)
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_CONTROLLER", False)

    if data.metadata_support:
        cg.add_define("USE_SENDSPIN_METADATA", True)
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_METADATA", False)

    if data.player_support:
        cg.add_define("USE_SENDSPIN_PLAYER", True)

        # Configures the player role. We always assume support for 16 bits per sample mono and stereo FLAC, Opus, and PCM at the configured sample rate
        # (with Opus only supported at 48 kHz since that's the only sample rate it supports). Users can configure the specific formats via the Sendspin server
        player_cfg = data.player_config
        sample_rate = player_cfg[CONF_SAMPLE_RATE]

        # OPUS only supports 48 kHz audio
        codecs = [CODEC_FORMAT_FLAC]
        if sample_rate == 48000:
            codecs.append(CODEC_FORMAT_OPUS)
        codecs.append(CODEC_FORMAT_PCM)

        def _audio_format(codec, channels):
            return cg.StructInitializer(
                AudioSupportedFormatObject,
                ("codec", codec),
                ("channels", channels),
                ("sample_rate", sample_rate),
                ("bit_depth", 16),
            )

        audio_format_structs = [
            _audio_format(codec, channels) for codec in codecs for channels in (2, 1)
        ]

        psram_stack = player_cfg.get(CONF_TASK_STACK_IN_PSRAM, False)
        if psram_stack:
            psram.request_external_task_stack()

        player_struct_fields = [
            ("audio_formats", audio_format_structs),
            ("audio_buffer_capacity", player_cfg[CONF_BUFFER_SIZE]),
            ("fixed_delay_us", player_cfg[CONF_FIXED_DELAY]),
            ("initial_static_delay_ms", player_cfg[CONF_INITIAL_STATIC_DELAY]),
            ("psram_stack", psram_stack),
        ]
        if (decode_memory := player_cfg.get(CONF_DECODE_MEMORY)) is not None:
            player_struct_fields.append(
                ("decode_buffer_location", MEMORY_LOCATION_ENUM[decode_memory])
            )
        player_config_struct = cg.StructInitializer(
            PlayerRoleConfig,
            *player_struct_fields,
        )
        cg.add(var.set_player_config(player_config_struct))
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_PLAYER", False)

    if data.visualizer_support:
        cg.add_define("USE_SENDSPIN_VISUALIZER", True)
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_VISUALIZER", False)

    if (source_config := config.get(CONF_SOURCE)) is not None:
        cg.add_define("USE_SENDSPIN_SOURCE", True)
        # Set explicitly: the library's idf_component.yml gates its micro-opus dependency on this option.
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_SOURCE", True)

        mic_source = await microphone.microphone_source_to_code(
            source_config[CONF_MICROPHONE]
        )
        source = cg.new_Pvariable(source_config[CONF_ID], mic_source)
        await cg.register_component(source, source_config)
        await cg.register_parented(source, var)
        cg.add(var.set_source(source))

        # Designated initializers, so these must follow SourceRoleConfig's declaration order.
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
        if source_config.get(CONF_TASK_STACK_IN_PSRAM):
            psram.request_external_task_stack()
            source_fields.append(("psram_stack", True))
        cg.add(
            source.set_role_config(
                cg.StructInitializer(SourceRoleConfig, *source_fields)
            )
        )
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_SOURCE", False)
