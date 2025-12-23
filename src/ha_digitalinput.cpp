#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

void HomeAssistantDiscovery::DigitalInputDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);
    name_paths = {"/ProductName"};
    if (service_items.contains("/InputState")) {
        HAEntityConfig input_state;
        input_state.name = "State";
        input_state.platform = "binary_sensor";
        input_state.value_template = ON_OFF_VALUE_TEMPLATE;
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
