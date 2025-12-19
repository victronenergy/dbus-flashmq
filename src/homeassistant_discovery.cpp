#include "homeassistant_discovery.h"
#include "vendor/flashmq_plugin.h"
#include "utils.h"
#include <algorithm>
#include <ranges>

using namespace dbus_flashmq;

static constexpr std::string_view CHARGER_STATE_VALUE_TEMPLATE =
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


static constexpr std::string_view DEVICE_OFF_REASON_VALUE_TEMPLATE =
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

static constexpr std::string_view FLUID_TYPE_VALUE_TEMPLATE =
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

static VeVariant getItemValue(const std::unordered_map<std::string, Item> &service_items, std::string_view dbus_path)
{
    auto it = service_items.find(std::string(dbus_path));
    if (it != service_items.end()) {
        return it->second.get_value().value;
    }
    return VeVariant();
}

static std::string getItemText(const std::unordered_map<std::string, Item> &service_items,
                               std::initializer_list<std::string_view> dbus_paths) {
    for (std::string_view dbus_path : dbus_paths) {
        auto it = service_items.find(std::string(dbus_path));
        if (it != service_items.end()) {
            const auto &value = it->second.get_value().value;
            if (value.get_type() == VeVariantType::String) {
                std::string result = value.as_text();
                if (!result.empty())
                    return result;
            }
        }
    }
    return std::string();
}

static std::vector<std::string_view> splitPath(std::string_view dbus_path)
{
    dbus_path.remove_prefix(1); // Remove the leading '/'
    auto parts_view = dbus_path
                      | std::ranges::views::split('/')
                      | std::ranges::views::transform([](auto&& part) { return std::string_view(&*part.begin(), std::ranges::distance(part)); }); // convert from range of characters to string_view
    return std::vector<std::string_view>(parts_view.begin(), parts_view.end());
}

static std::string toIdentifier(std::string_view input)
{
    std::string result(input);
    std::replace(result.begin(), result.end(), '/', '_');
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::unordered_map<std::string_view, std::function<std::unique_ptr<HomeAssistantDiscovery::DeviceData>()>> HomeAssistantDiscovery::device_factory_functions = {
    {"temperature", []() { return std::make_unique<HomeAssistantDiscovery::TemperatureDevice>(); }},
    {"battery", []() { return std::make_unique<HomeAssistantDiscovery::BatteryDevice>(); }},
    {"solarcharger", []() { return std::make_unique<HomeAssistantDiscovery::SolarChargerDevice>(); }},
    {"vebus", []() { return std::make_unique<HomeAssistantDiscovery::VeBusDevice>(); }},
    {"system", []() { return std::make_unique<HomeAssistantDiscovery::SystemDevice>(); }},
    {"tank", []() { return std::make_unique<HomeAssistantDiscovery::TankDevice>(); }},
    {"grid", []() { return std::make_unique<HomeAssistantDiscovery::GridMeterDevice>(); }},
    {"switch", []() { return std::make_unique<HomeAssistantDiscovery::SwitchDevice>(); }},
    {"meteo", []() { return std::make_unique<HomeAssistantDiscovery::MeteoDevice>(); }},
    {"charger", []() { return std::make_unique<HomeAssistantDiscovery::ChargerDevice>(); }},
    {"dcdc", []() { return std::make_unique<HomeAssistantDiscovery::DcDcDevice>(); }},
    {"digitalinput", []() { return std::make_unique<HomeAssistantDiscovery::DigitalInputDevice>(); }},
};

void HomeAssistantDiscovery::DeviceData::addNumericDiagnostic(std::string_view dbus_path,
                                                              std::string_view name,
                                                              std::string_view icon,
                                                              int suggested_display_precision,
                                                              std::string_view device_class,
                                                              std::string_view unit_of_measurement,
                                                              std::string_view value_template)
{
    HAEntityConfig entity;
    entity.entity_category = "diagnostic";
    entity.state_class = "measurement";
    entity.name = name;
    entity.icon = icon;
    if (!device_class.empty()) entity.device_class = device_class;
    if (!unit_of_measurement.empty()) entity.unit_of_measurement = unit_of_measurement;
    if (suggested_display_precision != -1) entity.suggested_display_precision = suggested_display_precision;
    if (!value_template.empty()) entity.value_template = value_template;
    entities.emplace(std::string(dbus_path), std::move(entity));

}

void HomeAssistantDiscovery::DeviceData::addStringDiagnostic(std::string_view dbus_path,
                                                             std::string_view name,
                                                             std::string_view icon,
                                                             std::string_view value_template)
{
    HAEntityConfig entity;
    entity.name = name;
    entity.icon = icon;
    entity.entity_category = "diagnostic";
    if (!value_template.empty()) entity.value_template = value_template;
    entities.emplace(std::string(dbus_path), std::move(entity));
}

void HomeAssistantDiscovery::DeviceData::addCommonDiagnostics(const std::unordered_map<std::string, Item> &service_items)
{
    if (service_items.contains("/DeviceInstance"))
        addNumericDiagnostic("/DeviceInstance", "Device Instance", "mdi:numeric");
    if (service_items.contains("/ErrorCode"))
        addNumericDiagnostic("/ErrorCode", "Error Code", "mdi:alert-circle");
    if (service_items.contains("/Status"))
        addNumericDiagnostic("/Status", "Status", "mdi:information");
    if (service_items.contains("/State"))
        addNumericDiagnostic("/State", "Device State", "mdi:power-settings");
    if (service_items.contains("/FirmwareVersion"))
        addStringDiagnostic("/FirmwareVersion", "Firmware Version", "mdi:chip",
                            "{% if value_json.value is none %}"
                            "Unknown"
                            "{% else %}"
                            "{{ 'v%x' | format(value_json.value | int(default=0)) }}"
                            "{% endif %}"
                            );
    if (service_items.contains("/HardwareVersion"))
        addStringDiagnostic("/HardwareVersion", "Hardware Version", "mdi:memory",
                            "{% if value_json.value is none %}"
                            "Unknown"
                            "{% else %}"
                            "{{ 'v%x' | format(value_json.value | int(default=0)) }}"
                            "{% endif %}"
                            );
    if (service_items.contains("/DeviceOffReason"))
        addStringDiagnostic("/DeviceOffReason", "Off Reason", "mdi:information-outline", DEVICE_OFF_REASON_VALUE_TEMPLATE);
}

void HomeAssistantDiscovery::MeteoDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);
    if (service_items.contains("/CellTemperature")) {
        HAEntityConfig cell_temp_sensor;
        cell_temp_sensor.name = "Cell temperature";
        cell_temp_sensor.state_class = "measurement";
        cell_temp_sensor.device_class = "temperature";
        cell_temp_sensor.unit_of_measurement = "°C";
        cell_temp_sensor.icon = "mdi:thermometer";
        cell_temp_sensor.suggested_display_precision = 1;
        entities.emplace("/CellTemperature", std::move(cell_temp_sensor));
    }
    if (service_items.contains("/Irradiance")) {
        HAEntityConfig irradiance_sensor;
        irradiance_sensor.name = "Solar irradiance";
        irradiance_sensor.state_class = "measurement";
        irradiance_sensor.device_class = "irradiance";
        irradiance_sensor.unit_of_measurement = "W/m²";
        irradiance_sensor.icon = "mdi:solar-power";
        irradiance_sensor.suggested_display_precision = 1;
        entities.emplace("/Irradiance", std::move(irradiance_sensor));
    }
    if (service_items.contains("/TodaysYield")) {
        HAEntityConfig todays_yield_sensor;
        todays_yield_sensor.name = "Today's Yield";
        todays_yield_sensor.state_class = "total_increasing";
        todays_yield_sensor.device_class = "energy";
        todays_yield_sensor.unit_of_measurement = "kWh";
        todays_yield_sensor.icon = "mdi:solar-panel";
        todays_yield_sensor.suggested_display_precision = 2;
        entities.emplace("/TodaysYield", std::move(todays_yield_sensor));
    }
    if (service_items.contains("/TimeSinceLastSun")) {
        HAEntityConfig time_since_sun_sensor;
        time_since_sun_sensor.name = "Time Since Last Sun";
        time_since_sun_sensor.state_class = "measurement";
        time_since_sun_sensor.device_class = "duration";
        time_since_sun_sensor.unit_of_measurement = "s";
        time_since_sun_sensor.icon = "mdi:clock";
        time_since_sun_sensor.entity_category = "diagnostic";
        entities.emplace("/TimeSinceLastSun", std::move(time_since_sun_sensor));
    }
    if (service_items.contains("/BatteryVoltage"))
        addNumericDiagnostic("/BatteryVoltage", "Battery Voltage", "mdi:battery", 3, "voltage", "V");
    addCommonDiagnostics(service_items);
}
std::pair<std::string, std::string> HomeAssistantDiscovery::MeteoDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = getItemText(items, {"/DeviceName", "/ProductName"});
        if (name.empty()) {
            // Determine sensor type based on available measurements
            bool has_irradiance = items.find("/Irradiance") != items.end();
            bool has_temp = items.find("/CellTemperature") != items.end();
            bool has_yield = items.find("/TodaysYield") != items.end();

            if (has_irradiance && has_temp && has_yield) {
                name = "Solar Weather Station";
            } else if (has_irradiance && has_temp) {
                name = "Solar Irradiance Sensor";
            } else if (has_irradiance) {
                name = "Irradiance Sensor";
            } else if (has_temp) {
                name = "Meteorological Sensor";
            } else {
                name = "Weather Station";
            }
        }

        // No custom name so add the device instance
        name.push_back(' ');
        name.append(short_service_name.instance());
    }

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "Meteorological Sensor";
    }

    return {name, model};
}

void HomeAssistantDiscovery::TemperatureDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    if (service_items.contains("/Temperature")) {
        HAEntityConfig temp_sensor;
        temp_sensor.name = "Temperature";
        temp_sensor.state_class = "measurement";
        temp_sensor.device_class = "temperature";
        temp_sensor.unit_of_measurement = "°C";
        temp_sensor.icon = "mdi:thermometer";
        temp_sensor.suggested_display_precision = 1;
        entities.emplace("/Temperature", std::move(temp_sensor));
    }

    if (service_items.contains("/Humidity")) {
        HAEntityConfig humidity_sensor;
        humidity_sensor.name = "Humidity";
        humidity_sensor.state_class = "measurement";
        humidity_sensor.device_class = "humidity";
        humidity_sensor.unit_of_measurement = "%";
        humidity_sensor.icon = "mdi:water-percent";
        humidity_sensor.suggested_display_precision = 1;
        entities.emplace("/Humidity", std::move(humidity_sensor));
    }
    if (service_items.contains("/Pressure")) {
        HAEntityConfig pressure_sensor;
        pressure_sensor.name = "Pressure";
        pressure_sensor.state_class = "measurement";
        pressure_sensor.device_class = "atmospheric_pressure";
        pressure_sensor.unit_of_measurement = "hPa";
        pressure_sensor.icon = "mdi:gauge";
        pressure_sensor.suggested_display_precision = 1;
        entities.emplace("/Pressure", std::move(pressure_sensor));
    }
    if (service_items.contains("/BatteryVoltage"))
        addNumericDiagnostic("/BatteryVoltage", "Battery Voltage", "mdi:battery", 3, "voltage", "V");
    addCommonDiagnostics(service_items);
}
std::pair<std::string, std::string> HomeAssistantDiscovery::TemperatureDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = getItemText(items, {"/ProductName"});
        if (name.empty()) {
            // Determine sensor type based on available measurements
            bool has_temp = items.find("/Temperature") != items.end();
            bool has_humidity = items.find("/Humidity") != items.end();
            bool has_pressure = items.find("/Pressure") != items.end();

            if (has_temp && has_humidity && has_pressure) {
                name = "Environmental Sensor";
            } else if (has_temp && has_humidity) {
                name = "Temperature & Humidity Sensor";
            } else if (has_temp && has_pressure) {
                name = "Temperature & Pressure Sensor";
            } else if (has_humidity && has_pressure) {
                name = "Humidity & Pressure Sensor";
            } else if (has_humidity) {
                name = "Humidity Sensor";
            } else if (has_pressure) {
                name = "Pressure Sensor";
            } else {
                name = "Temperature Sensor";
            }
        }

        // No custom name so add the device instance
        name.push_back(' ');
        name.append(short_service_name.instance());
    }

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "Environmental Sensor";
    }

    return {name, model};
}

bool HomeAssistantDiscovery::BatteryDevice::calcHasMidVoltage(const std::unordered_map<std::string, Item> &service_items)
{
    // Check if the service has mid voltage support
    return service_items.contains("/Dc/0/MidVoltage")
           && (!service_items.contains("/Settings/HasMidVoltage")
               || getItemValue(service_items, "/Settings/HasMidVoltage").as_int());
}

bool HomeAssistantDiscovery::BatteryDevice::calcHasTemperature(const std::unordered_map<std::string, Item> &service_items)
{
    // Check if the service has mid voltage support
    return service_items.contains("/Dc/0/Temperature")
           && (!service_items.contains("/Settings/HasTemperature")
               || getItemValue(service_items, "/Settings/HasTemperature").as_int());
}

bool HomeAssistantDiscovery::BatteryDevice::calcHasStarterVoltage(const std::unordered_map<std::string, Item> &service_items)
{
    // Check if the service has aux voltage support
    return service_items.contains("/Dc/1/Voltage")
           && (!service_items.contains("/Settings/HasStarterVoltage")
               || getItemValue(service_items, "/Settings/HasStarterVoltage").as_int());
}

void HomeAssistantDiscovery::BatteryDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    has_mid_voltage = calcHasMidVoltage(service_items);
    has_temperature = calcHasTemperature(service_items);
    has_starter_voltage = calcHasStarterVoltage(service_items);

    if (service_items.contains("/Soc")) {
        HAEntityConfig soc_sensor;
        soc_sensor.name = "State of Charge";
        soc_sensor.state_class = "measurement";
        soc_sensor.device_class = "battery";
        soc_sensor.unit_of_measurement = "%";
        soc_sensor.icon = "mdi:battery";
        soc_sensor.suggested_display_precision = 1;
        entities.emplace("/Soc", std::move(soc_sensor));
    }

    if (service_items.contains("/Dc/0/Voltage")) {
        HAEntityConfig voltage_sensor;
        voltage_sensor.name = "Battery Voltage";
        voltage_sensor.state_class = "measurement";
        voltage_sensor.device_class = "voltage";
        voltage_sensor.unit_of_measurement = "V";
        voltage_sensor.icon = "mdi:flash";
        voltage_sensor.suggested_display_precision = 2;
        entities.emplace("/Dc/0/Voltage", std::move(voltage_sensor));
    }

    if (service_items.contains("/Dc/0/Current")) {
        HAEntityConfig current_sensor;
        current_sensor.name = "Battery Current";
        current_sensor.device_class = "current";
        current_sensor.state_class = "measurement";
        current_sensor.unit_of_measurement = "A";
        current_sensor.icon = "mdi:current-dc";
        current_sensor.suggested_display_precision = 2;
        entities.emplace("/Dc/0/Current", std::move(current_sensor));
    }

    if (service_items.contains("/History/DischargedEnergy")) {
        HAEntityConfig energy_consumed;
        energy_consumed.name = "Energy Consumed";
        energy_consumed.device_class = "energy";
        energy_consumed.state_class = "total_increasing";
        energy_consumed.unit_of_measurement = "kWh";
        energy_consumed.icon = "mdi:battery-minus";
        energy_consumed.suggested_display_precision = 2;
        entities.emplace("/History/DischargedEnergy", std::move(energy_consumed));
    }

    if (service_items.contains("/History/ChargedEnergy")) {
        HAEntityConfig energy_charged;
        energy_charged.name = "Energy Charged";
        energy_charged.device_class = "energy";
        energy_charged.state_class = "total_increasing";
        energy_charged.unit_of_measurement = "kWh";
        energy_charged.icon = "mdi:battery-plus";
        energy_charged.suggested_display_precision = 2;
        entities.emplace("/History/ChargedEnergy", std::move(energy_charged));
    }
    if (service_items.contains("/Dc/0/Power")) {
        HAEntityConfig power_sensor;
        power_sensor.name = "Battery Power";
        power_sensor.device_class = "power";
        power_sensor.state_class = "measurement";
        power_sensor.unit_of_measurement = "W";
        power_sensor.icon = "mdi:flash";
        power_sensor.suggested_display_precision = 1;
        entities.emplace("/Dc/0/Power", std::move(power_sensor));
    }
    {
        // Always add and calculate enabled
        HAEntityConfig temp_sensor;
        temp_sensor.name = "Battery Temperature";
        temp_sensor.device_class = "temperature";
        temp_sensor.state_class = "measurement";
        temp_sensor.unit_of_measurement = "°C";
        temp_sensor.icon = "mdi:thermometer";
        temp_sensor.suggested_display_precision = 1;
        temp_sensor.enabled = has_temperature;
        entities.emplace("/Dc/0/Temperature", std::move(temp_sensor));
    }
    {
        // Always add and calculate enabled
        HAEntityConfig mid_voltage_sensor;
        mid_voltage_sensor.name = "Mid Voltage";
        mid_voltage_sensor.device_class = "voltage";
        mid_voltage_sensor.state_class = "measurement";
        mid_voltage_sensor.unit_of_measurement = "V";
        mid_voltage_sensor.icon = "mdi:flash";
        mid_voltage_sensor.suggested_display_precision = 2;
        mid_voltage_sensor.enabled = has_mid_voltage;
        entities.emplace("/Dc/0/MidVoltage", std::move(mid_voltage_sensor));
    }
    {
        // Always add and calculate enabled
        HAEntityConfig mid_deviation_sensor;
        mid_deviation_sensor.name = "Mid Voltage Deviation";
        mid_deviation_sensor.state_class = "measurement";
        mid_deviation_sensor.unit_of_measurement = "%";
        mid_deviation_sensor.icon = "mdi:battery-alert";
        mid_deviation_sensor.suggested_display_precision = 1;
        mid_deviation_sensor.enabled = has_mid_voltage;
        entities.emplace("/Dc/0/MidVoltageDeviation", std::move(mid_deviation_sensor));
    }
    {
        // Always add and calculate enabled
        HAEntityConfig starter_voltage_sensor;
        starter_voltage_sensor.name = "Starter Voltage";
        starter_voltage_sensor.device_class = "voltage";
        starter_voltage_sensor.state_class = "measurement";
        starter_voltage_sensor.unit_of_measurement = "V";
        starter_voltage_sensor.icon = "mdi:car-battery";
        starter_voltage_sensor.suggested_display_precision = 2;
        starter_voltage_sensor.enabled = has_starter_voltage;
        entities.emplace("/Dc/1/Voltage", std::move(starter_voltage_sensor));
    }
    if (service_items.contains("/ConsumedAmphours")) {
        HAEntityConfig consumed_ah_sensor;
        consumed_ah_sensor.name = "Consumed Amp Hours";
        consumed_ah_sensor.state_class = "measurement";
        consumed_ah_sensor.unit_of_measurement = "Ah";
        consumed_ah_sensor.icon = "mdi:battery-minus";
        consumed_ah_sensor.suggested_display_precision = 2;
        entities.emplace("/ConsumedAmphours", std::move(consumed_ah_sensor));
    }
    if (service_items.contains("/TimeToGo")) {
        HAEntityConfig ttg_sensor;
        ttg_sensor.name = "Time to Go";
        ttg_sensor.device_class = "duration";
        ttg_sensor.state_class = "measurement";
        ttg_sensor.unit_of_measurement = "s";
        ttg_sensor.icon = "mdi:timer";
        ttg_sensor.suggested_display_precision = 0;
        entities.emplace("/TimeToGo", std::move(ttg_sensor));
    }
    addCommonDiagnostics(service_items);
}
std::pair<std::string, std::string> HomeAssistantDiscovery::BatteryDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = getItemText(items, {"/ProductName"});
        if (name.empty()) {
            name = "Battery Monitor";
        }

        // No custom name so add the device instance
        name.push_back(' ');
        name.append(short_service_name.instance());
    }

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "Battery Monitor";
    }

    return {name, model};
}
bool HomeAssistantDiscovery::BatteryDevice::update(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                                                   const std::unordered_map<std::string, Item> &changed_items)
{
    const auto &service_items = all_items.at(service);
    bool changed = DeviceData::update(all_items, changed_items);
    if (changed_items.contains("/Settings/HasMidvoltage")) {
        bool act_has_mid_voltage = calcHasMidVoltage(service_items);
        if (act_has_mid_voltage != has_mid_voltage) {
            has_mid_voltage = act_has_mid_voltage;
            entities["/Dc/0/MidVoltage"].enabled = has_mid_voltage;
            entities["/Dc/0/MidVoltageDeviation"].enabled = has_mid_voltage;
            changed = true;
        }
    }
    if (changed_items.contains("/Settings/HasTemperature")) {
        bool act_has_temperature = calcHasTemperature(service_items);
        if (act_has_temperature != has_temperature) {
            has_temperature = act_has_temperature;
            entities["/Dc/0/Temperature"].enabled = has_temperature;
            changed = true;
        }
    }
    if (changed_items.contains("/Settings/HasStarterVoltage")) {
        bool act_has_starter_voltage = calcHasStarterVoltage(service_items);
        if (act_has_starter_voltage != has_starter_voltage) {
            has_starter_voltage = act_has_starter_voltage;
            entities["/Dc/1/Voltage"].enabled = has_starter_voltage;
            changed = true;
        }
    }

    return changed;
}

void HomeAssistantDiscovery::SolarChargerDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    if (service_items.contains("/Pv/V")) {
        HAEntityConfig pv_voltage;
        pv_voltage.name = "PV Voltage";
        pv_voltage.device_class = "voltage";
        pv_voltage.state_class = "measurement";
        pv_voltage.unit_of_measurement = "V";
        pv_voltage.icon = "mdi:solar-panel";
        pv_voltage.suggested_display_precision = 2;
        entities.emplace("/Pv/V", std::move(pv_voltage));
    }

    if (service_items.contains("/Pv/I")) {
        HAEntityConfig pv_current;
        pv_current.name = "PV Current";
        pv_current.device_class = "current";
        pv_current.state_class = "measurement";
        pv_current.unit_of_measurement = "A";
        pv_current.icon = "mdi:solar-panel";
        pv_current.suggested_display_precision = 2;
        entities.emplace("/Pv/I", std::move(pv_current));
    }

    if (service_items.contains("/Yield/Power")) {
        HAEntityConfig pv_power;
        pv_power.name = "PV Power";
        pv_power.device_class = "power";
        pv_power.state_class = "measurement";
        pv_power.unit_of_measurement = "W";
        pv_power.icon = "mdi:solar-panel";
        pv_power.suggested_display_precision = 1;
        entities.emplace("/Yield/Power", std::move(pv_power));
    }

    if (service_items.contains("/Dc/0/Voltage")) {
        HAEntityConfig bat_voltage;
        bat_voltage.name = "Battery Voltage";
        bat_voltage.device_class = "voltage";
        bat_voltage.state_class = "measurement";
        bat_voltage.unit_of_measurement = "V";
        bat_voltage.icon = "mdi:battery-charging";
        bat_voltage.suggested_display_precision = 2;
        entities.emplace("/Dc/0/Voltage", std::move(bat_voltage));
    }

    if (service_items.contains("/Dc/0/Current")) {
        HAEntityConfig bat_current;
        bat_current.name = "Battery Current";
        bat_current.device_class = "current";
        bat_current.state_class = "measurement";
        bat_current.unit_of_measurement = "A";
        bat_current.icon = "mdi:battery-charging";
        bat_current.suggested_display_precision = 2;
        entities.emplace("/Dc/0/Current", std::move(bat_current));
    }
    if (service_items.contains("/State")) {
        HAEntityConfig state_sensor;
        state_sensor.name = "Charge State";
        state_sensor.icon = "mdi:battery-charging";
        // Custom value template for state enum
        state_sensor.value_template = CHARGER_STATE_VALUE_TEMPLATE;
        entities.emplace("/State", std::move(state_sensor));
    }
    if (service_items.contains("/History/Daily/0/Yield")) {
        HAEntityConfig daily_yield;
        daily_yield.name = "Daily Yield";
        daily_yield.device_class = "energy";
        daily_yield.state_class = "total_increasing";
        daily_yield.unit_of_measurement = "kWh";
        daily_yield.icon = "mdi:solar-panel";
        daily_yield.suggested_display_precision = 2;
        entities.emplace("/History/Daily/0/Yield", std::move(daily_yield));
    }
    if (service_items.contains("/MppOperationMode"))
        addStringDiagnostic("/MppOperationMode", "MPP Operation Mode", "mdi:solar-panel",
                            "{% set modes = {0: 'Off', 1: 'Voltage/current limited', 2: 'MPPT active', 255: 'Not available'} %}{{ modes[value_json.value] | default('Unknown (' + value_json.value|string + ')') }}");
    if (service_items.contains("/Load/State"))
        addStringDiagnostic("/Load/State", "Load Output", "mdi:power-plug",
                            "{% if value_json.value == 1 %}On{% else %}Off{% endif %}");
    addCommonDiagnostics(service_items);
}
std::pair<std::string, std::string> HomeAssistantDiscovery::SolarChargerDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = "Solar Charger " + std::string(short_service_name.instance());
    }

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "MPPT Solar Charger";
    }

    return {name, model};
}

void HomeAssistantDiscovery::ChargerDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    if (service_items.contains("/Dc/0/Voltage")) {
        HAEntityConfig bat_voltage;
        bat_voltage.name = "Battery Voltage";
        bat_voltage.device_class = "voltage";
        bat_voltage.state_class = "measurement";
        bat_voltage.unit_of_measurement = "V";
        bat_voltage.icon = "mdi:battery-charging";
        bat_voltage.suggested_display_precision = 2;
        entities.emplace("/Dc/0/Voltage", std::move(bat_voltage));
    }
    if (service_items.contains("/Dc/0/Current")) {
        HAEntityConfig bat_current;
        bat_current.name = "Battery Current";
        bat_current.device_class = "current";
        bat_current.state_class = "measurement";
        bat_current.unit_of_measurement = "A";
        bat_current.icon = "mdi:battery-charging";
        bat_current.suggested_display_precision = 2;
        entities.emplace("/Dc/0/Current", std::move(bat_current));
    }
    if (service_items.contains("/Dc/1/Voltage")) {
        HAEntityConfig bat_voltage;
        bat_voltage.name = "Battery Voltage (2)";
        bat_voltage.device_class = "voltage";
        bat_voltage.state_class = "measurement";
        bat_voltage.unit_of_measurement = "V";
        bat_voltage.icon = "mdi:battery-charging";
        bat_voltage.suggested_display_precision = 2;
        entities.emplace("/Dc/1/Voltage", std::move(bat_voltage));
    }
    if (service_items.contains("/Dc/1/Current")) {
        HAEntityConfig bat_current;
        bat_current.name = "Battery Current (2)";
        bat_current.device_class = "current";
        bat_current.state_class = "measurement";
        bat_current.unit_of_measurement = "A";
        bat_current.icon = "mdi:battery-charging";
        bat_current.suggested_display_precision = 2;
        entities.emplace("/Dc/1/Current", std::move(bat_current));
    }
    if (service_items.contains("/Dc/2/Voltage")) {
        HAEntityConfig bat_voltage;
        bat_voltage.name = "Battery Voltage (3)";
        bat_voltage.device_class = "voltage";
        bat_voltage.state_class = "measurement";
        bat_voltage.unit_of_measurement = "V";
        bat_voltage.icon = "mdi:battery-charging";
        bat_voltage.suggested_display_precision = 2;
        entities.emplace("/Dc/2/Voltage", std::move(bat_voltage));
    }
    if (service_items.contains("/Dc/2/Current")) {
        HAEntityConfig bat_current;
        bat_current.name = "Battery Current (3)";
        bat_current.device_class = "current";
        bat_current.state_class = "measurement";
        bat_current.unit_of_measurement = "A";
        bat_current.icon = "mdi:battery-charging";
        bat_current.suggested_display_precision = 2;
        entities.emplace("/Dc/2/Current", std::move(bat_current));
    }
    if (service_items.contains("/Ac/In/L1/P")) {
        HAEntityConfig power_sensor;
        power_sensor.name = "AC Power";
        power_sensor.device_class = "power";
        power_sensor.state_class = "measurement";
        power_sensor.unit_of_measurement = "W";
        power_sensor.icon = "mdi:transmission-tower";
        power_sensor.suggested_display_precision = 1;
        entities.emplace("/Ac/In/L1/P", std::move(power_sensor));
    }
    if (service_items.contains("/Ac/In/L1/I")) {
        HAEntityConfig current_sensor;
        current_sensor.name = "AC Current";
        current_sensor.device_class = "current";
        current_sensor.state_class = "measurement";
        current_sensor.unit_of_measurement = "A";
        current_sensor.icon = "mdi:current-ac";
        current_sensor.suggested_display_precision = 2;
        entities.emplace("/Ac/In/L1/I", std::move(current_sensor));
    }
    if (service_items.contains("/State")) {
        HAEntityConfig state_sensor;
        state_sensor.name = "Charge State";
        state_sensor.icon = "mdi:battery-charging";
        // Custom value template for state enum
        state_sensor.value_template = CHARGER_STATE_VALUE_TEMPLATE;
        entities.emplace("/State", std::move(state_sensor));
    }
    addCommonDiagnostics(service_items);
}
std::pair<std::string, std::string> HomeAssistantDiscovery::ChargerDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = "AC Charger " + std::string(short_service_name.instance());
    }

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "AC Charger";
    }

    return {name, model};
}

void HomeAssistantDiscovery::DcDcDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    if (service_items.contains("/Dc/0/Voltage")) {
        HAEntityConfig bat_voltage;
        bat_voltage.name = "Battery Voltage (Out)";
        bat_voltage.device_class = "voltage";
        bat_voltage.state_class = "measurement";
        bat_voltage.unit_of_measurement = "V";
        bat_voltage.icon = "mdi:battery-charging";
        bat_voltage.suggested_display_precision = 2;
        entities.emplace("/Dc/0/Voltage", std::move(bat_voltage));
    }
    if (service_items.contains("/Dc/0/Current")) {
        HAEntityConfig bat_current;
        bat_current.name = "Battery Current (Out)";
        bat_current.device_class = "current";
        bat_current.state_class = "measurement";
        bat_current.unit_of_measurement = "A";
        bat_current.icon = "mdi:battery-charging";
        bat_current.suggested_display_precision = 2;
        entities.emplace("/Dc/0/Current", std::move(bat_current));
    }
    if (service_items.contains("/Dc/0/Power")) {
        HAEntityConfig power_sensor;
        power_sensor.name = "Battery Power (Out)";
        power_sensor.device_class = "power";
        power_sensor.state_class = "measurement";
        power_sensor.unit_of_measurement = "W";
        power_sensor.icon = "mdi:flash";
        power_sensor.suggested_display_precision = 1;
        entities.emplace("/Dc/0/Power", std::move(power_sensor));
    }
    if (service_items.contains("/Dc/In/V")) {
        HAEntityConfig bat_voltage;
        bat_voltage.name = "Battery Voltage (In)";
        bat_voltage.device_class = "voltage";
        bat_voltage.state_class = "measurement";
        bat_voltage.unit_of_measurement = "V";
        bat_voltage.icon = "mdi:battery-charging";
        bat_voltage.suggested_display_precision = 2;
        entities.emplace("/Dc/In/V", std::move(bat_voltage));
    }
    if (service_items.contains("/Dc/In/I")) {
        HAEntityConfig bat_current;
        bat_current.name = "Battery Current (In)";
        bat_current.device_class = "current";
        bat_current.state_class = "measurement";
        bat_current.unit_of_measurement = "A";
        bat_current.icon = "mdi:battery-charging";
        bat_current.suggested_display_precision = 2;
        entities.emplace("/Dc/In/I", std::move(bat_current));
    }
    if (service_items.contains("/Dc/In/P")) {
        HAEntityConfig power_sensor;
        power_sensor.name = "Battery Power (In)";
        power_sensor.device_class = "power";
        power_sensor.state_class = "measurement";
        power_sensor.unit_of_measurement = "W";
        power_sensor.icon = "mdi:flash";
        power_sensor.suggested_display_precision = 1;
        entities.emplace("/Dc/In/P", std::move(power_sensor));
    }
    if (service_items.contains("/State")) {
        HAEntityConfig state_sensor;
        state_sensor.name = "Charge State";
        state_sensor.icon = "mdi:battery-charging";
        // Custom value template for state enum
        state_sensor.value_template = CHARGER_STATE_VALUE_TEMPLATE;
        entities.emplace("/State", std::move(state_sensor));
    }
    if (service_items.contains("/Mode"))
        addStringDiagnostic("/Mode", "Mode", "mdi:cog",
                            "{% set modes = {1: 'Charger Only', 2: 'Inverter Only', 3: 'On', 4: 'Off'} %}{{ modes[value_json.value] | default('Unknown (' + value_json.value|string + ')') }}");
    if (service_items.contains("/Settings/DeviceFunction"))
        addStringDiagnostic("/Settings/DeviceFunction", "Device function", "mdi:information",
                            "{% if value_json.value is none %}"
                            "Unknown"
                            "{% elif value_json.value == 0 %}"
                            "Charger"
                            "{% elif value_json.value == 1 %}"
                            "PSU"
                            "{% else %}"
                            "{{ value_json.value | string }}"
                            "{% endif %}"
                            );
    addCommonDiagnostics(service_items);
}
std::pair<std::string, std::string> HomeAssistantDiscovery::DcDcDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = "DcDc Charger " + std::string(short_service_name.instance());
    }

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "DcDc Charger";
    }

    return {name, model};
}

void HomeAssistantDiscovery::VeBusDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    for (int phase = 1; phase <= 3; ++phase) {
        std::string path = "/Ac/ActiveIn/L" + std::to_string(phase) + "/P";
        if (service_items.contains(path)) {
            HAEntityConfig ac_in_power;
            ac_in_power.name = "AC In L" + std::to_string(phase) + " Power";
            ac_in_power.device_class = "power";
            ac_in_power.state_class = "measurement";
            ac_in_power.unit_of_measurement = "W";
            ac_in_power.icon = "mdi:transmission-tower";
            ac_in_power.suggested_display_precision = 1;
            entities.emplace(path, std::move(ac_in_power));
        }

        path = "/Ac/Out/L" + std::to_string(phase) + "/P";
        if (service_items.contains(path)) {
            HAEntityConfig ac_out_power;
            ac_out_power.name = "AC Out L" + std::to_string(phase) + " Power";
            ac_out_power.device_class = "power";
            ac_out_power.state_class = "measurement";
            ac_out_power.unit_of_measurement = "W";
            ac_out_power.icon = "mdi:home-lightning-bolt";
            ac_out_power.suggested_display_precision = 1;
            entities.emplace(path, std::move(ac_out_power));
        }

        path = "/Ac/ActiveIn/L" + std::to_string(phase) + "/V";
        if (service_items.contains(path)) {
            HAEntityConfig ac_in_voltage;
            ac_in_voltage.name = "AC In L" + std::to_string(phase) + " Voltage";
            ac_in_voltage.device_class = "voltage";
            ac_in_voltage.state_class = "measurement";
            ac_in_voltage.unit_of_measurement = "V";
            ac_in_voltage.icon = "mdi:transmission-tower";
            ac_in_voltage.suggested_display_precision = 1;
            entities.emplace(path, std::move(ac_in_voltage));
        }

        path = "/Ac/Out/L" + std::to_string(phase) + "/V";
        if (service_items.contains(path)) {
            HAEntityConfig ac_out_voltage;
            ac_out_voltage.name = "AC Out L" + std::to_string(phase) + " Voltage";
            ac_out_voltage.device_class = "voltage";
            ac_out_voltage.state_class = "measurement";
            ac_out_voltage.unit_of_measurement = "V";
            ac_out_voltage.icon = "mdi:home-lightning-bolt";
            ac_out_voltage.suggested_display_precision = 1;
            entities.emplace(path, std::move(ac_out_voltage));
        }
    }
    if (service_items.contains("/Dc/0/Power")) {
        HAEntityConfig dc_power;
        dc_power.name = "DC Power";
        dc_power.device_class = "power";
        dc_power.state_class = "measurement";
        dc_power.unit_of_measurement = "W";
        dc_power.icon = "mdi:battery-charging";
        dc_power.suggested_display_precision = 1;
        entities.emplace("/Dc/0/Power", std::move(dc_power));
    }
    if (service_items.contains("/Dc/0/Voltage")) {
        HAEntityConfig dc_voltage;
        dc_voltage.name = "DC Voltage";
        dc_voltage.device_class = "voltage";
        dc_voltage.state_class = "measurement";
        dc_voltage.unit_of_measurement = "V";
        dc_voltage.icon = "mdi:battery-charging";
        dc_voltage.suggested_display_precision = 2;
        entities.emplace("/Dc/0/Voltage", std::move(dc_voltage));
    }
    if (service_items.contains("/State")) {
        HAEntityConfig state_sensor;
        state_sensor.name = "State";
        state_sensor.icon = "mdi:power-settings";
        state_sensor.value_template = CHARGER_STATE_VALUE_TEMPLATE;
        entities.emplace("/State", std::move(state_sensor));
    }
    if (service_items.contains("/Mode"))
        addStringDiagnostic("/Mode", "Mode", "mdi:cog",
                            "{% set modes = {1: 'Charger Only', 2: 'Inverter Only', 3: 'On', 4: 'Off'} %}{{ modes[value_json.value] | default('Unknown (' + value_json.value|string + ')') }}");
    if (service_items.contains("/VebusError"))
        addNumericDiagnostic("/VebusError", "VE.Bus Error", "mdi:alert-circle", 0);
    addCommonDiagnostics(service_items);
}
std::pair<std::string, std::string> HomeAssistantDiscovery::VeBusDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = "MultiPlus " + std::string(short_service_name.instance());
    }

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "MultiPlus/Quattro";
    }

    return {name, model};
}

void HomeAssistantDiscovery::SystemDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    for (int phase = 1; phase <= 3; ++phase) {
        std::string path = "/Ac/Consumption/L" + std::to_string(phase) + "/Power";
        if (service_items.contains(path)) {
            HAEntityConfig ac_loads;
            ac_loads.name = "AC Load L" + std::to_string(phase);
            ac_loads.device_class = "power";
            ac_loads.state_class = "measurement";
            ac_loads.unit_of_measurement = "W";
            ac_loads.icon = "mdi:home-lightning-bolt";
            ac_loads.suggested_display_precision = 1;
            entities.emplace(path, std::move(ac_loads));
        }
    }
    if (service_items.contains("/Ac/Consumption/Total/Power")) {
        HAEntityConfig total_consumption;
        total_consumption.name = "Total AC Consumption";
        total_consumption.device_class = "power";
        total_consumption.state_class = "measurement";
        total_consumption.unit_of_measurement = "W";
        total_consumption.icon = "mdi:home-lightning-bolt";
        total_consumption.suggested_display_precision = 1;
        entities.emplace("/Ac/Consumption/Total/Power", std::move(total_consumption));
    }
    if (service_items.contains("/Dc/Battery/Power")) {
        HAEntityConfig bat_power;
        bat_power.name = "Battery Power";
        bat_power.device_class = "power";
        bat_power.state_class = "measurement";
        bat_power.unit_of_measurement = "W";
        bat_power.icon = "mdi:battery";
        bat_power.suggested_display_precision = 1;
        entities.emplace("/Dc/Battery/Power", std::move(bat_power));
    }
    if (service_items.contains("/Dc/Pv/Power")) {
        HAEntityConfig pv_power;
        pv_power.name = "PV Power";
        pv_power.device_class = "power";
        pv_power.state_class = "measurement";
        pv_power.unit_of_measurement = "W";
        pv_power.icon = "mdi:solar-panel";
        pv_power.suggested_display_precision = 1;
        entities.emplace("/Dc/Pv/Power", std::move(pv_power));
    }
    if (service_items.contains("/SystemState/State"))
        addNumericDiagnostic("/SystemState/State", "System State", "mdi:state-machine", 0);
    addCommonDiagnostics(service_items);
}
std::pair<std::string, std::string> HomeAssistantDiscovery::SystemDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = "GX System " + std::string(short_service_name.instance());

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "Venus GX";
    }

    return {name, model};
}

void HomeAssistantDiscovery::TankDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    if (service_items.contains("/Level")) {
        HAEntityConfig level_sensor;
        level_sensor.name = "Tank Level";
        level_sensor.state_class = "measurement";
        level_sensor.unit_of_measurement = "%";
        level_sensor.icon = "mdi:gauge";
        level_sensor.suggested_display_precision = 1;
        entities.emplace("/Level", std::move(level_sensor));
    }
    if (service_items.contains("/Capacity")) {
        HAEntityConfig capacity_sensor;
        capacity_sensor.name = "Tank Capacity";
        capacity_sensor.state_class = "measurement";
        capacity_sensor.unit_of_measurement = "L";
        capacity_sensor.icon = "mdi:storage-tank";
        capacity_sensor.suggested_display_precision = 0;
        capacity_sensor.entity_category = "diagnostic";
        entities.emplace("/Capacity", std::move(capacity_sensor));
    }
    if (service_items.contains("/Remaining")) {
        HAEntityConfig remaining_sensor;
        remaining_sensor.name = "Tank Remaining Volume";
        remaining_sensor.state_class = "measurement";
        remaining_sensor.unit_of_measurement = "L";
        remaining_sensor.icon = "mdi:gauge";
        remaining_sensor.suggested_display_precision = 1;
        entities.emplace("/Remaining", std::move(remaining_sensor));
    }
    if (service_items.contains("/FluidType"))
        addStringDiagnostic("/FluidType", "Fluid Type", "mdi:waves", FLUID_TYPE_VALUE_TEMPLATE);
    addCommonDiagnostics(service_items);
}
std::pair<std::string, std::string> HomeAssistantDiscovery::TankDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = getItemText(items, {"/ProductName"});
        if (name.empty()) {
            // Try to determine tank type from FluidType
            auto fluid_type = items.find("/FluidType");
            if (fluid_type != items.end()) {
                int type = fluid_type->second.get_value().value.as_int();
                switch (type) {
                case 0: name = "Fuel Tank"; break;
                case 1: name = "Fresh Water Tank"; break;
                case 2: name = "Waste Water Tank"; break;
                case 3: name = "Live Well"; break;
                case 4: name = "Oil Tank"; break;
                case 5: name = "Black Water Tank"; break;
                case 6: name = "Gasoline Tank"; break;
                case 7: name = "Diesel Tank"; break;
                case 8: name = "LPG Tank"; break;
                case 9: name = "LNG Tank"; break;
                case 10: name = "Hydraulic Oil Tank"; break;
                case 11: name = "Raw Water Tank"; break;
                default: name = "Tank Sensor"; break;
                }
            } else {
                name = "Tank Sensor";
            }
        }

        // No custom name so add the device instance
        name.push_back(' ');
        name.append(short_service_name.instance());
    }

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "Tank Level Sensor";
    }

    return {name, model};
}

void HomeAssistantDiscovery::GridMeterDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    for (int phase = 1; phase <= 3; ++phase) {
        std::string power_path = "/Ac/L" + std::to_string(phase) + "/Power";
        if (service_items.contains(power_path)) {
            HAEntityConfig power_sensor;
            power_sensor.name = "L" + std::to_string(phase) + " Power";
            power_sensor.device_class = "power";
            power_sensor.state_class = "measurement";
            power_sensor.unit_of_measurement = "W";
            power_sensor.icon = "mdi:transmission-tower";
            power_sensor.suggested_display_precision = 1;
            entities.emplace(power_path, std::move(power_sensor));
        }

        std::string voltage_path = "/Ac/L" + std::to_string(phase) + "/Voltage";
        if (service_items.contains(voltage_path)) {
            HAEntityConfig voltage_sensor;
            voltage_sensor.name = "L" + std::to_string(phase) + " Voltage";
            voltage_sensor.device_class = "voltage";
            voltage_sensor.state_class = "measurement";
            voltage_sensor.unit_of_measurement = "V";
            voltage_sensor.icon = "mdi:flash";
            voltage_sensor.suggested_display_precision = 1;
            entities.emplace(voltage_path, std::move(voltage_sensor));
        }

        std::string current_path = "/Ac/L" + std::to_string(phase) + "/Current";
        if (service_items.contains(current_path)) {
            HAEntityConfig current_sensor;
            current_sensor.name = "L" + std::to_string(phase) + " Current";
            current_sensor.device_class = "current";
            current_sensor.state_class = "measurement";
            current_sensor.unit_of_measurement = "A";
            current_sensor.icon = "mdi:current-ac";
            current_sensor.suggested_display_precision = 2;
            entities.emplace(current_path, std::move(current_sensor));
        }

        std::string energy_forward_path = "/Ac/L" + std::to_string(phase) + "/Energy/Forward";
        if (service_items.contains(energy_forward_path)) {
            HAEntityConfig energy_forward_sensor;
            energy_forward_sensor.name = "L" + std::to_string(phase) + " Energy Import";
            energy_forward_sensor.device_class = "energy";
            energy_forward_sensor.state_class = "total_increasing";
            energy_forward_sensor.unit_of_measurement = "kWh";
            energy_forward_sensor.icon = "mdi:counter";
            energy_forward_sensor.suggested_display_precision = 2;
            entities.emplace(energy_forward_path, std::move(energy_forward_sensor));
        }
        std::string energy_reverse_path = "/Ac/L" + std::to_string(phase) + "/Energy/Reverse";
        if (service_items.contains(energy_reverse_path)) {
            HAEntityConfig energy_reverse_sensor;
            energy_reverse_sensor.name = "L" + std::to_string(phase) + " Energy Export";
            energy_reverse_sensor.device_class = "energy";
            energy_reverse_sensor.state_class = "total_increasing";
            energy_reverse_sensor.unit_of_measurement = "kWh";
            energy_reverse_sensor.icon = "mdi:counter";
            energy_reverse_sensor.suggested_display_precision = 2;
            entities.emplace(energy_reverse_path, std::move(energy_reverse_sensor));
        }
    }
    if (service_items.contains("/Ac/Power")) {
        HAEntityConfig total_power;
        total_power.name = "Total Power";
        total_power.device_class = "power";
        total_power.state_class = "measurement";
        total_power.unit_of_measurement = "W";
        total_power.icon = "mdi:transmission-tower";
        total_power.suggested_display_precision = 1;
        entities.emplace("/Ac/Power", std::move(total_power));
    }
    if (service_items.contains("/Position"))
        addStringDiagnostic("/Position", "Position", "mdi:map-marker",
                            "{% set positions = {0: 'AC input 1', 1: 'AC output', 2: 'AC input 2'} %}{{ positions[value_json.value] | default('Unknown (' + value_json.value|string + ')') }}");
    addCommonDiagnostics(service_items);
}
std::pair<std::string, std::string> HomeAssistantDiscovery::GridMeterDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = "Grid Meter " + std::string(short_service_name.instance());
    }

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "Energy Meter";
    }

    return {name, model};
}

void HomeAssistantDiscovery::SwitchDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    // Handle the dynamic entities
    // go over the service_items and create entities for each SwitchableOutput
    for (const auto &item : service_items) {
        const auto &dbus_path = item.first;
        auto parts = splitPath(dbus_path);
        if (parts.size() == 3
            && parts[0] == "SwitchableOutput"
            && std::count(parts[1].begin(), parts[1].end(), '_') == 1)
        {
            std::string_view output_type = parts[1].substr(0, parts[1].find('_'));

            if (parts[2] == "State") {
                HAEntityConfig switch_state;
                std::string name_path = "/SwitchableOutput/" + std::string(parts[1]) + "/Name";
                std::string custom_name_path = "/SwitchableOutput/" + std::string(parts[1]) + "/Settings/CustomName";
                switch_state.name = getItemText(service_items, {custom_name_path, name_path});
                switch_state.platform = "switch";
                switch_state.state_class = "measurement";
                switch_state.device_class = "switch";
                switch_state.command_topic = dbus_path;
                switch_state.payload_on = "{\"value\": 1}";
                switch_state.payload_off = "{\"value\": 0}";
                switch_state.optimistic = false; // Wait for state feedback
                switch_state.value_template = "{% if value_json.value == 1 %}ON{% else %}OFF{% endif %}";
                switch_state.state_on = "ON";
                switch_state.state_off = "OFF";

                if (output_type == "output") {
                    switch_state.icon = "mdi:electric-switch";
                } else if (output_type == "pwm") {
                    switch_state.icon = "mdi:sine-wave";
                } else if (output_type == "relay") {
                    switch_state.icon = "mdi:electric-switch";
                }
                auto result = entities.emplace(dbus_path, std::move(switch_state));
                if (result.second) {
                    // Successfully added, so store the path for custom name updates
                    customname_paths[custom_name_path].custom_name_path = custom_name_path;
                    customname_paths[custom_name_path].name_path = name_path;
                    customname_paths[custom_name_path].state_entity_config = &result.first->second;
                }
            } else if (parts[2] == "Dimming" && output_type == "pwm") {
                HAEntityConfig dimming_state;
                std::string name_path = "/SwitchableOutput/" + std::string(parts[1]) + "/Name";
                std::string custom_name_path = "/SwitchableOutput/" + std::string(parts[1]) + "/Settings/CustomName";
                dimming_state.name = getItemText(service_items, {custom_name_path, name_path}) + " Dimming";
                dimming_state.platform = "number";
                dimming_state.state_class = "measurement";
                dimming_state.icon = "mdi:brightness-percent";
                dimming_state.suggested_display_precision = 0;

                dimming_state.command_topic = dbus_path;
                dimming_state.command_template = "{\"value\": {{ value }} }";
                dimming_state.min_value = 0;
                dimming_state.max_value = 100;
                dimming_state.unit_of_measurement = "%";
                dimming_state.mode = "slider";
                dimming_state.optimistic = false; // Wait for state feedback
                auto result = entities.emplace(dbus_path, std::move(dimming_state));
                if (result.second) {
                    // Successfully added, so store the path for custom name updates
                    customname_paths[custom_name_path].custom_name_path = custom_name_path;
                    customname_paths[custom_name_path].name_path = name_path;
                    customname_paths[custom_name_path].dimming_entity_config = &result.first->second;
                }
            }
        }
    }

    if (service_items.contains("/State"))
        addStringDiagnostic("/State", "Device State", "mdi:power-settings", "{% set states = {256: 'Connected', 257: 'Over temperature', 258: 'Temperature warning', 259: 'Channel fault', 260: 'Channel Tripped', 261: 'Under Voltage'} %}{{ states[value_json.value] | default('Unknown (' + value_json.value|string + ')') }}");
    addCommonDiagnostics(service_items);
}
bool HomeAssistantDiscovery::SwitchDevice::update(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                                                  const std::unordered_map<std::string, Item> &changed_items)
{
    const auto &service_items = all_items.at(service);
    bool changed = DeviceData::update(all_items, changed_items);
    // Check if any of the SwitchableOutput names have changed
    for (const auto &item : customname_paths) {
        if (changed_items.contains(item.first)) {
            CustomNameInfo const & info = item.second;
            std::string base_name = getItemText(service_items, {info.custom_name_path, info.name_path});
            if (info.state_entity_config)
                info.state_entity_config->name = base_name;
            if (info.dimming_entity_config)
                info.dimming_entity_config->name = base_name + " Dimming";
            changed = true;
        }
    }
    return changed;
}
std::pair<std::string, std::string> HomeAssistantDiscovery::SwitchDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = getItemText(items, {"/ProductName"});
        if (name.empty()) {
            name = "Switch Device";
        }
        // No custom name so add the device instance
        name.push_back(' ');
        name.append(short_service_name.instance());
    }

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "Energy Meter";
    }

    return {name, model};
}

void HomeAssistantDiscovery::DigitalInputDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);
    name_paths = {"/ProductName"};
    if (service_items.contains("/InputState")) {
        HAEntityConfig input_state;
        input_state.name = "State";
        input_state.platform = "binary_sensor";
        input_state.value_template = "{% if value_json.value == 1 %}ON{% else %}OFF{% endif %}";
        entities.emplace("/InputState", std::move(input_state));
    }
    if (service_items.contains("/Count")) {
        HAEntityConfig count;
        count.name = "Count";
        count.state_class = "measurement";
        count.icon = "mdi:counter";
        count.unit_of_measurement = "rising edges";
        count.suggested_display_precision = 0;
        entities.emplace("/Count", std::move(count));
    }
    addCommonDiagnostics(service_items);
}
std::pair<std::string, std::string> HomeAssistantDiscovery::DigitalInputDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/ProductName"});
    if (name.empty()) {
        name = "Generic I/O";
    }
    return {name, name};
}

nlohmann::json HADevice::toJson() const
{
    nlohmann::json j;
    j["name"] = name;
    j["manufacturer"] = manufacturer;
    j["model"] = model;
    j["identifiers"] = nlohmann::json::array({identifiers});

    if (!hw_version.empty()) j["hw_version"] = hw_version;
    if (!sw_version.empty()) j["sw_version"] = sw_version;
    if (!serial_number.empty()) j["serial_number"] = serial_number;
    if (!configuration_url.empty()) j["configuration_url"] = configuration_url;
    if (!via_device.empty()) j["via_device"] = via_device;

    return j;
}

nlohmann::json HAEntityConfig::toJson() const
{
    nlohmann::json config_json;

    config_json["platform"] = platform;
    if (enabled) {
        config_json["unique_id"] = unique_id;
        config_json["name"] = name;
        config_json["state_topic"] = state_topic;
        config_json["enabled_by_default"] = enabled_by_default;
        if (!value_template.empty()) { config_json["value_template"] = value_template; }
        if (!state_on.empty()) { config_json["state_on"] = state_on; }
        if (!state_off.empty()) { config_json["state_off"] = state_off; }
        if (!unit_of_measurement.empty()) { config_json["unit_of_measurement"] = unit_of_measurement; }
        if (!device_class.empty()) { config_json["device_class"] = device_class; }
        if (!state_class.empty()) { config_json["state_class"] = state_class; }
        if (!icon.empty()) { config_json["icon"] = icon; }
        if (!entity_category.empty()) { config_json["entity_category"] = entity_category; }
        if (suggested_display_precision >= 0) { config_json["suggested_display_precision"] = suggested_display_precision; }
        if (!command_topic.empty()) {
            config_json["command_topic"] = command_topic;

            // For switch entities
            if (!payload_on.empty()) { config_json["payload_on"] = payload_on; }
            if (!payload_off.empty()) { config_json["payload_off"] = payload_off; }

            // For number entities (dimmers)
            if (min_value != 0 || max_value != 0) { config_json["min"] = min_value; config_json["max"] = max_value; }
            if (!command_template.empty()) { config_json["command_template"] = command_template; }
            if (!mode.empty()) { config_json["mode"] = mode; }

            config_json["optimistic"] = optimistic;
        }
    }

    return config_json;
}

bool HomeAssistantDiscovery::DeviceData::update(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                                                const std::unordered_map<std::string, Item> &changed_items)
{
    bool changed = false;
    if (std::any_of(changed_items.begin(), changed_items.end(), [this](const std::pair<std::string, Item> &elem)->bool { return name_paths.contains(elem.first); })) {
        auto [name, model] = getNameAndModel(all_items);
        if (ha_device.name != name) {
            ha_device.name = name;
            changed = true;
        }
        if (ha_device.model != model) {
            ha_device.model = model;
            changed = true;
        }
    }
    return changed;
}

void HomeAssistantDiscovery::DeviceData::fillHADevice(const std::string &vrm_id,
                                                      const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items)
{
    const auto &service_items = all_items.at(service);
    std::string systemIdentifier = vrm_id + "_system";
    std::string deviceIdentifier = vrm_id + "_" + toIdentifier(short_service_name.total());

    bool isSystemService = short_service_name.service_type() == "system";
    auto [name, model] = getNameAndModel(all_items);

    ha_device.name = std::move(name);
    ha_device.model = std::move(model);
    ha_device.identifiers = isSystemService ? systemIdentifier : deviceIdentifier;
    ha_device.via_device = isSystemService ? "" : systemIdentifier;
    ha_device.serial_number = getItemText(service_items, {"/Serial"});
    ha_device.sw_version = getItemText(service_items, {"/FirmwareVersion"});
    ha_device.hw_version = getItemText(service_items, {"/HardwareVersion"});
}

void HomeAssistantDiscovery::DeviceData::fillHAEntities(const std::string &vrm_id,
                                                        const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items)
{
    addEntities(all_items);
    // Fill in the unique_ids, the state topics and the command topics
    for (auto & entity : entities) {
        const auto &dbus_path = entity.first;
        HAEntityConfig &entity_config = entity.second;

        entity_config.unique_id = ha_device.identifiers + toIdentifier(dbus_path);
        entity_config.state_topic = "N/" + vrm_id + "/" + short_service_name.total() + dbus_path;
        if (!entity_config.command_topic.empty())
            entity_config.command_topic = "W/" + vrm_id + "/" + short_service_name.total() + entity_config.command_topic;
    }
}

nlohmann::json HomeAssistantDiscovery::DeviceData::toJson() const
{
    nlohmann::json json = nlohmann::json::object();
    json["device"] = ha_device.toJson();
    json["origin"] = nlohmann::json::object({ {"name", "Venus OS / flashmq"}});
    nlohmann::json components = nlohmann::json::object();
    for (const auto &entity : entities) {
        const HAEntityConfig &entity_config = entity.second;
        components[entity_config.unique_id] = entity_config.toJson();
    }
    json["components"] = components;
    return json;
}

// HomeAssistantDiscovery Implementation
HomeAssistantDiscovery::HomeAssistantDiscovery()
{
}


void HomeAssistantDiscovery::setVrmId(const std::string &vrm_id)
{
    this->vrm_id = vrm_id;
    flashmq_logf(LOG_NOTICE, "Home Assistant Discovery VRM ID set to: %s", vrm_id.c_str());
}

bool HomeAssistantDiscovery::isServiceEnabled(std::string_view service_type) const
{
    return device_factory_functions.contains(service_type);
}

std::string HomeAssistantDiscovery::createDeviceDiscoveryTopic(const std::string &device_id) const
{
    return "homeassistant/device/" + device_id + "/config";
}

std::unique_ptr<HomeAssistantDiscovery::DeviceData> HomeAssistantDiscovery::createDeviceData(const std::string &service,
                                                                                             const ShortServiceName &short_service_name,
                                                                                             const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) const
{
    std::unique_ptr<DeviceData> device = device_factory_functions.at(short_service_name.service_type())();

    device->service = service;
    device->short_service_name = short_service_name;

    device->fillHADevice(vrm_id, all_items);
    device->fillHAEntities(vrm_id, all_items);
    device->discovery_topic = createDeviceDiscoveryTopic(device->ha_device.identifiers);

    nlohmann::json json = device->toJson();
    device->cached_payload = json.dump();
    return device;
}

void HomeAssistantDiscovery::publishSensorEntitiesWithItems(const std::string &service,
                                                            const ShortServiceName &short_service_name,
                                                            const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                                                            const std::unordered_map<std::string, Item> &changed_items)
{
    auto dev_it = devices.find(service);
    // Check if device created
    if (dev_it == devices.end()) {
        if (!isServiceEnabled(short_service_name.service_type())) {
            return;
        }

        flashmq_logf(LOG_DEBUG, "No device found for service %s, creating new device", service.c_str());
        auto [device, result] = devices.insert({service, createDeviceData(service, short_service_name, all_items)});
        flashmq_publish_message(device->second->discovery_topic, 0, true, device->second->cached_payload);
    } else {
        flashmq_logf(LOG_DEBUG, "Updating existing device for service %s", service.c_str());
        DeviceData &device = *dev_it->second;
        if (device.update(all_items, changed_items)) {
            nlohmann::json json = device.toJson();
            std::string payload = json.dump();
            if (device.cached_payload != payload) {
                flashmq_publish_message(device.discovery_topic, 0, true, payload);
                device.cached_payload = payload; // Update cached payload
            }
        } else {
            flashmq_logf(LOG_DEBUG, "No changes detected for device %s, skipping republish", service.c_str());
        }
    }
}

void HomeAssistantDiscovery::publishAllConfigs() const
{
    for (const auto &entry : devices) {
        const DeviceData &device = *entry.second;
        flashmq_publish_message(device.discovery_topic, 0, true, device.cached_payload);
    }
}

void HomeAssistantDiscovery::removeAllSensorsForService(const std::string &service)
{
    flashmq_logf(LOG_DEBUG, "Removing all Home Assistant sensors for service: %s", service.c_str());

    auto device_entry = devices.find(service);
    if (device_entry == devices.end())
        return;

    flashmq_publish_message(device_entry->second->discovery_topic, 0, true, ""); // empty payload removes the entity

    devices.erase(device_entry);
}

void HomeAssistantDiscovery::clearAll()
{
    flashmq_logf(LOG_INFO, "Clearing all Home Assistant discovery entities");

    for (const auto &device_entry: devices) {
        flashmq_publish_message(device_entry.second->discovery_topic, 0, true, ""); // empty payload removes the entity
    }
    devices.clear();
}
