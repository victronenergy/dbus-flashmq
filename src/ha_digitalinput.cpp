#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

void HomeAssistantDiscovery::DigitalInputDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);
    name_paths = {"/ProductName"};

    if (std::string path = "/InputState"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "State";
        entity.platform = "binary_sensor";
        entity.value_template = ON_OFF_VALUE_TEMPLATE;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Count"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Count";
        entity.state_class = "measurement";
        entity.icon = "mdi:counter";
        entity.unit_of_measurement = "rising edges";
        entity.suggested_display_precision = 0;
        entities.emplace(path, std::move(entity));
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
