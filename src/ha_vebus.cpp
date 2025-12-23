#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

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
