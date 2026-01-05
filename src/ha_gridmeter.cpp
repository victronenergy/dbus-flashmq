#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

const int HomeAssistantDiscovery::GridMeterDevice::max_nr_of_phases;

void HomeAssistantDiscovery::GridMeterDevice::calcNrOfPhases(const std::unordered_map<std::string, Item> &service_items)
{
    if (!service_items.contains("/NrOfPhases")) {
        // We do not know so just assume 3
        nr_of_phases = 3;
    } else {
        nr_of_phases = getItemValue(service_items, "/NrOfPhases").as_int();
        // Limit the number of phases to 1 .. max_nr_of_phases
        nr_of_phases = std::clamp(nr_of_phases, 1, max_nr_of_phases);
    }
}

void HomeAssistantDiscovery::GridMeterDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    calcNrOfPhases(service_items);

    // Always loop over all possible phases. The enabled properties will be set according to the actual number of phases.
    // This way we know in the update function that the entities exist and only need to enable/disable them.
    for (int phase = 1; phase <= max_nr_of_phases; ++phase) {
        { // Power - /Ac/Lx/Power
            std::string path = "/Ac/L" + std::to_string(phase) + "/Power";
            HAEntityConfig entity;
            entity.name = "L" + std::to_string(phase) + " Power";
            entity.device_class = "power";
            entity.state_class = "measurement";
            entity.unit_of_measurement = "W";
            entity.icon = "mdi:transmission-tower";
            entity.suggested_display_precision = 1;
            entity.enabled = phase <= nr_of_phases && service_items.contains(path);
            entities.emplace(path, std::move(entity));
        }

        { // Voltage - /Ac/Lx/Voltage
            std::string path = "/Ac/L" + std::to_string(phase) + "/Voltage";
            HAEntityConfig entity;
            entity.name = "L" + std::to_string(phase) + " Voltage";
            entity.device_class = "voltage";
            entity.state_class = "measurement";
            entity.unit_of_measurement = "V";
            entity.icon = "mdi:flash";
            entity.suggested_display_precision = 1;
            entity.enabled = phase <= nr_of_phases && service_items.contains(path);
            entities.emplace(path, std::move(entity));
        }

        { // Current - /Ac/Lx/Current
            std::string path = "/Ac/L" + std::to_string(phase) + "/Current";
            HAEntityConfig entity;
            entity.name = "L" + std::to_string(phase) + " Current";
            entity.device_class = "current";
            entity.state_class = "measurement";
            entity.unit_of_measurement = "A";
            entity.icon = "mdi:current-ac";
            entity.suggested_display_precision = 2;
            entity.enabled = phase <= nr_of_phases && service_items.contains(path);
            entities.emplace(path, std::move(entity));
        }

        { // Energy Import - /Ac/Lx/Energy/Forward
            std::string path = "/Ac/L" + std::to_string(phase) + "/Energy/Forward";
            HAEntityConfig entity;
            entity.name = "L" + std::to_string(phase) + " Energy Import";
            entity.device_class = "energy";
            entity.state_class = "total_increasing";
            entity.unit_of_measurement = "kWh";
            entity.icon = "mdi:counter";
            entity.suggested_display_precision = 2;
            entity.enabled = phase <= nr_of_phases && service_items.contains(path);
            entities.emplace(path, std::move(entity));
        }

        { // Energy Export - /Ac/Lx/Energy/Reverse
            std::string path = "/Ac/L" + std::to_string(phase) + "/Energy/Reverse";
            HAEntityConfig entity;
            entity.name = "L" + std::to_string(phase) + " Energy Export";
            entity.device_class = "energy";
            entity.state_class = "total_increasing";
            entity.unit_of_measurement = "kWh";
            entity.icon = "mdi:counter";
            entity.suggested_display_precision = 2;
            entity.enabled = phase <= nr_of_phases && service_items.contains(path);
            entities.emplace(path, std::move(entity));
        }
    }

    if (std::string path = "/Ac/Power"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Total Power";
        entity.device_class = "power";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "W";
        entity.icon = "mdi:transmission-tower";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }
    if (service_items.contains("/Position"))
        addStringDiagnostic("/Position", "Position", "mdi:map-marker", GRID_METER_POSITION_VALUE_TEMPLATE);
    addCommonDiagnostics(service_items);
}

bool HomeAssistantDiscovery::GridMeterDevice::update(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                                                     const std::unordered_map<std::string, Item> &changed_items)
{
    static const std::string_view phaseItems[] = {
        "/Power",
        "/Voltage",
        "/Current",
        "/Energy/Forward",
        "/Energy/Reverse",
    };
    bool changed = false;
    if (changed_items.contains("/NrOfPhases")) {
        auto const& service_items = all_items.at(service);
        auto prev_nr_of_phases = nr_of_phases;
        calcNrOfPhases(service_items);
        if (prev_nr_of_phases != nr_of_phases) {
            // Update enabled state of entities
            // Always loop over all possible phases. The enabled properties will be set according to the actual number of phases.
            for (int phase = 1; phase <= max_nr_of_phases; ++phase) {
                for (auto phaseItem : phaseItems) {
                    std::string path = "/Ac/L" + std::to_string(phase) + std::string(phaseItem);
                    entities.at(path).enabled = phase <= nr_of_phases && service_items.contains(path);
                }
            }
            changed = true;
        }
    }
    return changed;
}

std::pair<std::string, std::string> HomeAssistantDiscovery::GridMeterDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = "Grid Meter " + std::string(short_service_name.instance());
    }

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "Energy Meter";
    }

    return {name, model};
}
