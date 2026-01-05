#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

void HomeAssistantDiscovery::SystemDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    for (int phase = 1; phase <= 3; ++phase) {
        // AC Load Lx - /Ac/Consumption/Lx/Power
        std::string path = "/Ac/Consumption/L" + std::to_string(phase) + "/Power";
        if (service_items.contains(path)) {
            HAEntityConfig entity;
            entity.name = "AC Load L" + std::to_string(phase);
            entity.device_class = "power";
            entity.state_class = "measurement";
            entity.unit_of_measurement = "W";
            entity.icon = "mdi:home-lightning-bolt";
            entity.suggested_display_precision = 1;
            entities.emplace(path, std::move(entity));
        }
    }

    if (std::string path = "/Ac/Consumption/Total/Power"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Total AC Consumption";
        entity.device_class = "power";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "W";
        entity.icon = "mdi:home-lightning-bolt";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/Battery/Power"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Power";
        entity.device_class = "power";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "W";
        entity.icon = "mdi:battery";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/Pv/Power"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "PV Power";
        entity.device_class = "power";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "W";
        entity.icon = "mdi:solar-panel";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }
    if (service_items.contains("/SystemState/State"))
        addNumericDiagnostic("/SystemState/State", "System State", "mdi:state-machine", 0);
    addCommonDiagnostics(service_items);
}

std::pair<std::string, std::string> HomeAssistantDiscovery::SystemDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = "GX System " + std::string(short_service_name.instance());

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "Venus GX";
    }

    return {name, model};
}
