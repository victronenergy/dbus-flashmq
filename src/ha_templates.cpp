#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

const std::string_view HAEntityConfig::DEFAULT_VALUE_TEMPLATE = "{{ value_json.value }}";
const std::string_view HAEntityConfig::DEFAULT_COMMAND_TEMPLATE = "{\"value\": {{ value }} }";

const std::string_view HomeAssistantDiscovery::CHARGER_MODE_VALUE_TEMPLATE =
    "{% set modes = "
    "{ 1: 'Charger Only'"
    ", 2: 'Inverter Only'"
    ", 3: 'On'"
    ", 4: 'Off'"
    "} %}{{ modes[value_json.value] | default('Unknown (' + value_json.value|string + ')') }}";

const std::string_view HomeAssistantDiscovery::CHARGER_STATE_VALUE_TEMPLATE =
    "{% set states = "
    "{ 0: 'Off'"
    ", 1: 'Low Power'"
    ", 2: 'Fault'"
    ", 3: 'Bulk'"
    ", 4: 'Absorption'"
    ", 5: 'Float'"
    ", 6: 'Storage'"
    ", 7: 'Equalize'"
    ", 8: 'Passthru'"
    ", 9: 'Inverting'"
    ", 10: 'Power assist'"
    ", 11: 'Power supply'"
    ", 252: 'Bulk protect'"
    "} %}{{ states[value_json.value] | default('Unknown (' + value_json.value|string + ')') }}";

const std::string_view HomeAssistantDiscovery::VICTRON_VERSION_VALUE_TEMPLATE =
    "{% if value_json.value is none %}"
    "Unknown"
    "{% else %}"
    "{% set ver = value_json.value | int(default=0) %}"
    "{% if ver == 0 %}"
    "Unknown"
    "{% else %}"
    "{% set major = (ver // 65536) % 65536 %}"
    "{% set minor = (ver // 256) % 256 %}"
    "{% set build = ver % 256 %}"
    "{% if major == 0 %}"
    "{% set major = minor %}"
    "{% set minor = build %}"
    "{% set build = 0 %}"
    "{% endif %}"
    "{% if build > 0 and build != 255 %}"
    "{{ 'v%x.%02x.%02x' | format(major, minor, build) }}"
    "{% else %}"
    "{{ 'v%x.%02x' | format(major, minor) }}"
    "{% endif %}"
    "{% endif %}"
    "{% endif %}";

const std::string_view HomeAssistantDiscovery::DEVICE_OFF_REASON_VALUE_TEMPLATE =
    "{% set value = value_json.value | int(0) %}"
    "{% set reasons = [] %}"
    "{% if value | bitwise_and(1) %}""{% set reasons = reasons + [\"No/Low input power\"] %}""{% endif %}"
    "{% if value | bitwise_and(2) %}""{% set reasons = reasons + [\"Disabled by physical switch\"] %}""{% endif %}"
    "{% if value | bitwise_and(4) %}""{% set reasons = reasons + [\"Remote via Device-mode or push-button\"] %}""{% endif %}"
    "{% if value | bitwise_and(8) %}""{% set reasons = reasons + [\"Remote input connector\"] %}""{% endif %}"
    "{% if value | bitwise_and(16) %}""{% set reasons = reasons + [\"Internal condition preventing startup\"] %}""{% endif %}"
    "{% if value | bitwise_and(32) %}""{% set reasons = reasons + [\"Need token for operation\"] %}""{% endif %}"
    "{% if value | bitwise_and(64) %}""{% set reasons = reasons + [\"Signal from BMS\"] %}""{% endif %}"
    "{% if value | bitwise_and(128) %}""{% set reasons = reasons + [\"Engine shutdown on low input voltage\"] %}""{% endif %}"
    "{% if value | bitwise_and(256) %}""{% set reasons = reasons + [\"Converter is off to read input voltage accurately\"] %}""{% endif %}"
    "{% if value | bitwise_and(512) %}""{% set reasons = reasons + [\"Low temperature\"] %}""{% endif %}"
    "{% if value | bitwise_and(1024) %}""{% set reasons = reasons + [\"no/low panel power\"] %}""{% endif %}"
    "{% if value | bitwise_and(2048) %}""{% set reasons = reasons + [\"no/low battery power\"] %}""{% endif %}"
    "{% if value | bitwise_and(4096) %}""{% set reasons = reasons + [\"Unknown (4096)\"] %}""{% endif %}"
    "{% if value | bitwise_and(8192) %}""{% set reasons = reasons + [\"Unknown (8192)\"] %}""{% endif %}"
    "{% if value | bitwise_and(16384) %}""{% set reasons = reasons + [\"Unknown (16384)\"] %}""{% endif %}"
    "{% if value | bitwise_and(32768) %}""{% set reasons = reasons + [\"Active alarm\"] %}""{% endif %}"
    "{{ reasons | join(\", \") if reasons else \"-\" }}";

const std::string_view HomeAssistantDiscovery::FLUID_TYPE_VALUE_TEMPLATE =
    "{% set types = "
    "{ 0: 'Fuel'"
    ", 1: 'Fresh water'"
    ", 2: 'Waste water'"
    ", 3: 'Live well'"
    ", 4: 'Oil'"
    ", 5: 'Black water'"
    ", 6: 'Gasoline'"
    ", 7: 'Diesel'"
    ", 8: 'LPG'"
    ", 9: 'LNG'"
    ", 10: 'Hydraulic oil'"
    ", 11: 'Raw water'"
    "} %}{{ types[value_json.value] | default('Unknown (' + value_json.value|string + ')') }}";

const std::string_view HomeAssistantDiscovery::ON_OFF_VALUE_TEMPLATE =
    "{% if value_json.value is none %}"
    "Unknown"
    "{% else %}"
    "{% set ver = value_json.value | int(default=0) %}"
    "{% if ver == 0 %}"
    "OFF"
    "{% else %}"
    "ON"
    "{% endif %}"
    "{% endif %}";

const std::string_view HomeAssistantDiscovery::DEVICEFUNCTION_VALUE_TEMPLATE =
    "{% if value_json.value is none %}"
    "Unknown"
    "{% elif value_json.value == 0 %}"
    "Charger"
    "{% elif value_json.value == 1 %}"
    "PSU"
    "{% else %}"
    "{{ value_json.value | string }}"
    "{% endif %}";

const std::string_view HomeAssistantDiscovery::GRID_METER_POSITION_VALUE_TEMPLATE =
    "{% set positions = "
    "{ 0: 'AC input 1'"
    ", 1: 'AC output'"
    ", 2: 'AC input 2'"
    "} %}{{ positions[value_json.value] | default('Unknown (' + value_json.value|string + ')') }}";

const std::string_view HomeAssistantDiscovery::MPP_OPERATION_MODE_VALUE_TEMPLATE =
    "{% set modes = "
    "{ 0: 'Off'"
    ", 1: 'Voltage/current limited'"
    ", 2: 'MPPT active'"
    ", 255: 'Not available'"
    "} %}{{ modes[value_json.value] | default('Unknown (' + value_json.value|string + ')') }}";

const std::string_view HomeAssistantDiscovery::SWITCH_STATE_VALUE_TEMPLATE =
    "{% set states = "
    "{ 256: 'Connected'"
    ", 257: 'Over temperature'"
    ", 258: 'Temperature warning'"
    ", 259: 'Channel fault'"
    ", 260: 'Channel Tripped'"
    ", 261: 'Under Voltage'"
    "} %}{{ states[value_json.value] | default('Unknown (' + value_json.value|string + ')') }}";

const std::string_view HomeAssistantDiscovery::AC_INPUT_TYPE_VALUE_TEMPLATE =
    "{% set types = "
    "{ 1: 'Grid'"
    ", 2: 'Genset'"
    ", 3: 'Shore'"
    "} %}{{ types[value_json.value] | default('Unknown (' + value_json.value|string + ')') }}";

const std::string_view HomeAssistantDiscovery::YES_NO_VALUE_TEMPLATE =
    "{% if value_json.value is none %}"
    "Unknown"
    "{% else %}"
    "{% set ver = value_json.value | int(default=0) %}"
    "{% if ver == 0 %}"
    "No"
    "{% else %}"
    "Yes"
    "{% endif %}"
    "{% endif %}";

const std::string_view HomeAssistantDiscovery::ESS_MODE_VALUE_TEMPLATE =
    "{% set types = "
    "{ 0: 'Optimised with BatteryLife'"
    ", 1: 'Optimised without BatteryLife'"
    ", 2: 'Keep Batteries Charged'"
    ", 3: 'External Control'"
    "} %}{{ types[value_json.value] | default('Unknown (' + value_json.value|string + ')') }}";
