#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

void HomeAssistantDiscovery::SolarChargerDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    if (std::string path = "/Pv/V"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "PV Voltage";
        entity.device_class = "voltage";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "V";
        entity.icon = "mdi:solar-panel";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Pv/I"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "PV Current";
        entity.device_class = "current";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "A";
        entity.icon = "mdi:solar-panel";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Yield/Power"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "PV Power";
        entity.device_class = "power";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "W";
        entity.icon = "mdi:solar-panel";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/0/Voltage"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Voltage";
        entity.device_class = "voltage";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "V";
        entity.icon = "mdi:battery-charging";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/0/Current"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Current";
        entity.device_class = "current";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "A";
        entity.icon = "mdi:battery-charging";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/State"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Charge State";
        entity.icon = "mdi:battery-charging";
        // Custom value template for state enum
        entity.value_template = CHARGER_STATE_VALUE_TEMPLATE;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/History/Daily/0/Yield"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Daily Yield";
        entity.device_class = "energy";
        entity.state_class = "total_increasing";
        entity.unit_of_measurement = "kWh";
        entity.icon = "mdi:solar-panel";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
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
