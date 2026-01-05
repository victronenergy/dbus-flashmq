#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

void HomeAssistantDiscovery::ChargerDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

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

    if (std::string path = "/Dc/1/Voltage"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Voltage (2)";
        entity.device_class = "voltage";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "V";
        entity.icon = "mdi:battery-charging";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/1/Current"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Current (2)";
        entity.device_class = "current";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "A";
        entity.icon = "mdi:battery-charging";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/2/Voltage"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Voltage (3)";
        entity.device_class = "voltage";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "V";
        entity.icon = "mdi:battery-charging";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/2/Current"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Current (3)";
        entity.device_class = "current";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "A";
        entity.icon = "mdi:battery-charging";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Ac/In/L1/P"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "AC Power";
        entity.device_class = "power";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "W";
        entity.icon = "mdi:transmission-tower";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Ac/In/L1/I"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "AC Current";
        entity.device_class = "current";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "A";
        entity.icon = "mdi:current-ac";
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
