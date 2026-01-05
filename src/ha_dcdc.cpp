#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

void HomeAssistantDiscovery::DcDcDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    if (std::string path = "/Dc/0/Voltage"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Voltage (Out)";
        entity.device_class = "voltage";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "V";
        entity.icon = "mdi:battery-charging";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/0/Current"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Current (Out)";
        entity.device_class = "current";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "A";
        entity.icon = "mdi:battery-charging";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/0/Power"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Power (Out)";
        entity.device_class = "power";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "W";
        entity.icon = "mdi:flash";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/In/V"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Voltage (In)";
        entity.device_class = "voltage";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "V";
        entity.icon = "mdi:battery-charging";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/In/I"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Current (In)";
        entity.device_class = "current";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "A";
        entity.icon = "mdi:battery-charging";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/In/P"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Power (In)";
        entity.device_class = "power";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "W";
        entity.icon = "mdi:flash";
        entity.suggested_display_precision = 1;
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
    if (service_items.contains("/Mode"))
        addStringDiagnostic("/Mode", "Mode", "mdi:cog", CHARGER_MODE_VALUE_TEMPLATE);
    if (service_items.contains("/Settings/DeviceFunction"))
        addStringDiagnostic("/Settings/DeviceFunction", "Device function", "mdi:information", DEVICEFUNCTION_VALUE_TEMPLATE);
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
