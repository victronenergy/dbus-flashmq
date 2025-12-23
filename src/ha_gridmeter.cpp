#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

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
