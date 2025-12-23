#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

void HomeAssistantDiscovery::SystemDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    for (int phase = 1; phase <= 3; ++phase) {
        std::string path = "/Ac/Consumption/L" + std::to_string(phase) + "/Power";
        if (service_items.contains(path)) {
            HAEntityConfig ac_loads;
            ac_loads.name = "AC Load L" + std::to_string(phase);
            ac_loads.device_class = "power";
            ac_loads.state_class = "measurement";
            ac_loads.unit_of_measurement = "W";
            ac_loads.icon = "mdi:home-lightning-bolt";
            ac_loads.suggested_display_precision = 1;
            entities.emplace(path, std::move(ac_loads));
        }
    }
    if (service_items.contains("/Ac/Consumption/Total/Power")) {
        HAEntityConfig total_consumption;
        total_consumption.name = "Total AC Consumption";
        total_consumption.device_class = "power";
        total_consumption.state_class = "measurement";
        total_consumption.unit_of_measurement = "W";
        total_consumption.icon = "mdi:home-lightning-bolt";
        total_consumption.suggested_display_precision = 1;
        entities.emplace("/Ac/Consumption/Total/Power", std::move(total_consumption));
    }
    if (service_items.contains("/Dc/Battery/Power")) {
        HAEntityConfig bat_power;
        bat_power.name = "Battery Power";
        bat_power.device_class = "power";
        bat_power.state_class = "measurement";
        bat_power.unit_of_measurement = "W";
        bat_power.icon = "mdi:battery";
        bat_power.suggested_display_precision = 1;
        entities.emplace("/Dc/Battery/Power", std::move(bat_power));
    }
    if (service_items.contains("/Dc/Pv/Power")) {
        HAEntityConfig pv_power;
        pv_power.name = "PV Power";
        pv_power.device_class = "power";
        pv_power.state_class = "measurement";
        pv_power.unit_of_measurement = "W";
        pv_power.icon = "mdi:solar-panel";
        pv_power.suggested_display_precision = 1;
        entities.emplace("/Dc/Pv/Power", std::move(pv_power));
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
