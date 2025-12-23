#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

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
        addStringDiagnostic("/MppOperationMode", "MPP Operation Mode", "mdi:solar-panel", MPP_OPERATION_MODE_VALUE_TEMPLATE);
    if (service_items.contains("/Load/State"))
        addStringDiagnostic("/Load/State", "Load Output", "mdi:power-plug", ON_OFF_VALUE_TEMPLATE);
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
