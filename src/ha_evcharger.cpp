#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

void HomeAssistantDiscovery::EvChargerDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    { // Total energy - /Ac/Energy/Forward
        HAEntityConfig entity;
        std::string path = "/Ac/Energy/Forward";
        entity.name = "Total energy";
        entity.device_class = "energy";
        entity.state_class = "total_increasing";
        entity.unit_of_measurement = "kWh";
        entity.icon = "mdi:car-electric";
        entity.suggested_display_precision = 2;
        entity.enabled = service_items.contains(path);
        entities.emplace(path, std::move(entity));
    }

    { // AC Power - /Ac/Power
        HAEntityConfig entity;
        std::string path = "/Ac/Power";
        entity.name = "AC Power";
        entity.device_class = "power";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "W";
        entity.icon = "mdi:flash";
        entity.suggested_display_precision = 0;
        entity.enabled = service_items.contains(path);
        entities.emplace(path, std::move(entity));
    }

    for (int phase = 1; phase <= 3; ++phase) { // /Ac/L1/Power, /Ac/L2/Power, /Ac/L3/Power
        HAEntityConfig entity;
        std::string path = "/Ac/L" + std::to_string(phase) + "/Power";
        entity.name = "L" + std::to_string(phase) + " Power";
        entity.device_class = "power";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "W";
        entity.icon = "mdi:flash";
        entity.suggested_display_precision = 0;
        entity.enabled = service_items.contains(path);
        entities.emplace(path, std::move(entity));
    }

    { // Session charging time - /Session/Time
        HAEntityConfig entity;
        std::string path = "/Session/Time";
        entity.name = "Session charging time";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "s";
        entity.icon = "mdi:timer";
        entity.suggested_display_precision = 0;
        entity.enabled = service_items.contains(path);
        entities.emplace(path, std::move(entity));
    }

    { // Session charging energy - /Session/Energy
        HAEntityConfig entity;
        std::string path = "/Session/Energy";
        entity.name = "Session charging energy";
        entity.device_class = "energy";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "kWh";
        entity.icon = "mdi:car-electric";
        entity.suggested_display_precision = 2;
        entity.enabled = service_items.contains(path);
        entities.emplace(path, std::move(entity));
    }

    { // Current - /Current
        HAEntityConfig entity;
        std::string path = "/Current";
        entity.name = "Current";
        entity.device_class = "current";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "A";
        entity.icon = "mdi:current-ac";
        entity.suggested_display_precision = 0;
        entity.enabled = service_items.contains(path);
        entities.emplace(path, std::move(entity));
    }

    { // Enable display - /EnableDisplay
        HAEntityConfig entity;
        std::string path = "/EnableDisplay";
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
        entity.enabled = service_items.contains(path);
        entities.emplace(path, std::move(entity));
    }

    { // Max current - /MaxCurrent
        std::string path = "/MaxCurrent";
        HAEntityConfig entity;
        entity.name = "Max Current";
        entity.device_class = "current";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "A";
        entity.icon = "mdi:current-ac";
        entity.suggested_display_precision = 0;
        entity.enabled = service_items.contains(path);
        entities.emplace(path, std::move(entity));
    }

    { // Mode - /Mode
        HAEntityConfig entity;
        std::string path = "/Mode";
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
        entity.enabled = service_items.contains(path);
        entities.emplace(path, std::move(entity));
    }

    { // Set Current - /SetCurrent
        std::string path = "/SetCurrent";
        HAEntityConfig entity;
        entity.name = "Set Current";
        entity.device_class = "current";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "A";
        entity.icon = "mdi:current-ac";
        entity.suggested_display_precision = 0;
        entity.enabled = service_items.contains(path);
        entities.emplace(path, std::move(entity));
    }

    { // Position - /Position
        std::string path = "/Position";
        if (service_items.contains(path))
            addStringDiagnostic(path, "Position", "mdi:import", EVCHARGER_POSITION_VALUE_TEMPLATE);
    }

    { // Status - /Status
        std::string path = "/Status";
        HAEntityConfig entity;
        entity.name = "State";
        entity.icon = "mdi:connection";
        // Custom value template for state enum
        entity.value_template = EVCHARGER_STATE_VALUE_TEMPLATE;
        entity.enabled = service_items.contains(path);
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
