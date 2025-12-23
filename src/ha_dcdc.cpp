#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

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
