#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

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
