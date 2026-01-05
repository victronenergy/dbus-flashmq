#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

void HomeAssistantDiscovery::EvChargerDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    if (std::string path = "/Ac/Energy/Forward"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Total energy";
        entity.device_class = "energy";
        entity.state_class = "total_increasing";
        entity.unit_of_measurement = "kWh";
        entity.icon = "mdi:car-electric";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Ac/Power"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "AC Power";
        entity.device_class = "power";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "W";
        entity.icon = "mdi:flash";
        entity.suggested_display_precision = 0;
        entities.emplace(path, std::move(entity));
    }

    for (int phase = 1; phase <= 3; ++phase) {
        std::string path = "/Ac/L" + std::to_string(phase) + "/Power";
        if (service_items.contains(path)) {
            HAEntityConfig entity;
            entity.name = "L" + std::to_string(phase) + " Power";
            entity.device_class = "power";
            entity.state_class = "measurement";
            entity.unit_of_measurement = "W";
            entity.icon = "mdi:flash";
            entity.suggested_display_precision = 0;
            entities.emplace(path, std::move(entity));
        }
    }

    if (std::string path = "/Session/Time"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Session charging time";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "s";
        entity.icon = "mdi:timer";
        entity.suggested_display_precision = 0;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Session/Energy"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Session charging energy";
        entity.device_class = "energy";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "kWh";
        entity.icon = "mdi:car-electric";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Current"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Current";
        entity.device_class = "current";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "A";
        entity.icon = "mdi:current-ac";
        entity.suggested_display_precision = 0;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/EnableDisplay"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Display Control";
        entity.platform = "switch";
        entity.state_class = "measurement";
        entity.device_class = "switch";
        entity.command_topic = path;
        entity.payload_on = "{\"value\": 1}";
        entity.payload_off = "{\"value\": 0}";
        entity.value_template = ON_OFF_VALUE_TEMPLATE;
        entity.state_on = "ON";
        entity.state_off = "OFF";
        entity.icon = "mdi:monitor-dashboard";
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/MaxCurrent"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Max Current";
        entity.device_class = "current";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "A";
        entity.icon = "mdi:current-ac";
        entity.suggested_display_precision = 0;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Mode"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Automatic Mode";
        entity.platform = "switch";
        entity.state_class = "measurement";
        entity.device_class = "switch";
        entity.command_topic = path;
        entity.payload_on = "{\"value\": 1}";
        entity.payload_off = "{\"value\": 0}";
        entity.value_template = ON_OFF_VALUE_TEMPLATE;
        entity.state_on = "ON";
        entity.state_off = "OFF";
        entity.icon = "mdi:flash-auto";
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/SetCurrent"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Set Current";
        entity.device_class = "current";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "A";
        entity.icon = "mdi:current-ac";
        entity.suggested_display_precision = 0;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Position"; service_items.contains(path))
        addStringDiagnostic(path, "Position", "mdi:import", EVCHARGER_POSITION_VALUE_TEMPLATE);

    if (std::string path = "/Status"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "State";
        entity.icon = "mdi:connection";
        // Custom value template for state enum
        entity.value_template = EVCHARGER_STATE_VALUE_TEMPLATE;
        entities.emplace(path, std::move(entity));
    }

    addCommonDiagnostics(service_items);
}


std::pair<std::string, std::string> HomeAssistantDiscovery::EvChargerDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string model = getItemText(items, {"/Model"});
    if (model.empty()) {
        model = getItemText(items, {"/ProductName"});
    }
    if (model.empty()) {
        model = "EV Charger";
    }

    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = "EV Charger " + std::string(short_service_name.instance());
    }

    return {name, model};
}
