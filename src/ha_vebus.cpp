#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

void HomeAssistantDiscovery::VeBusDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    for (int phase = 1; phase <= 3; ++phase) {
        std::string in_path_prefix = "/Ac/ActiveIn/L" + std::to_string(phase);
        std::string in_name_prefix = "AC In L" + std::to_string(phase);
        std::string out_path_prefix = "/Ac/Out/L" + std::to_string(phase);
        std::string out_name_prefix = "AC Out L" + std::to_string(phase);
        if (std::string path = in_path_prefix + "/P"; service_items.contains(path)) {
            HAEntityConfig entity;
            entity.name = in_name_prefix + " Power";
            entity.device_class = "power";
            entity.state_class = "measurement";
            entity.unit_of_measurement = "W";
            entity.icon = "mdi:transmission-tower";
            entity.suggested_display_precision = 1;
            entities.emplace(path, std::move(entity));
        }

        if (std::string path = out_path_prefix + "/P"; service_items.contains(path)) {
            HAEntityConfig entity;
            entity.name = out_name_prefix + " Power";
            entity.device_class = "power";
            entity.state_class = "measurement";
            entity.unit_of_measurement = "W";
            entity.icon = "mdi:home-lightning-bolt";
            entity.suggested_display_precision = 1;
            entities.emplace(path, std::move(entity));
        }

        if (std::string path = in_path_prefix + "/V"; service_items.contains(path)) {
            HAEntityConfig entity;
            entity.name = in_name_prefix + " Voltage";
            entity.device_class = "voltage";
            entity.state_class = "measurement";
            entity.unit_of_measurement = "V";
            entity.icon = "mdi:transmission-tower";
            entity.suggested_display_precision = 1;
            entities.emplace(path, std::move(entity));
        }

        if (std::string path = out_path_prefix + "/V"; service_items.contains(path)) {
            HAEntityConfig entity;
            entity.name = in_name_prefix + " Voltage";
            entity.device_class = "voltage";
            entity.state_class = "measurement";
            entity.unit_of_measurement = "V";
            entity.icon = "mdi:home-lightning-bolt";
            entity.suggested_display_precision = 1;
            entities.emplace(path, std::move(entity));
        }
    }

    if (std::string path = "/Dc/0/Power"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "DC Power";
        entity.device_class = "power";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "W";
        entity.icon = "mdi:battery-charging";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/0/Voltage"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "DC Voltage";
        entity.device_class = "voltage";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "V";
        entity.icon = "mdi:battery-charging";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/State"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "State";
        entity.icon = "mdi:power-settings";
        entity.value_template = CHARGER_STATE_VALUE_TEMPLATE;
        entities.emplace(path, std::move(entity));
    }
    if (service_items.contains("/Mode"))
        addStringDiagnostic("/Mode", "Mode", "mdi:cog", CHARGER_MODE_VALUE_TEMPLATE);
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
