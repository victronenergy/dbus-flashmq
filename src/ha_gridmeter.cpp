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
    for (int phase = 1; phase <= max_nr_of_phases; ++phase) {
        {
            std::string power_path = "/Ac/L" + std::to_string(phase) + "/Power";
            HAEntityConfig power_sensor;
            power_sensor.name = "L" + std::to_string(phase) + " Power";
            power_sensor.device_class = "power";
            power_sensor.state_class = "measurement";
            power_sensor.unit_of_measurement = "W";
            power_sensor.icon = "mdi:transmission-tower";
            power_sensor.suggested_display_precision = 1;
            power_sensor.enabled = phase <= nr_of_phases && service_items.contains(power_path);
            entities.emplace(power_path, std::move(power_sensor));
        }
        {
            std::string voltage_path = "/Ac/L" + std::to_string(phase) + "/Voltage";
            HAEntityConfig voltage_sensor;
            voltage_sensor.name = "L" + std::to_string(phase) + " Voltage";
            voltage_sensor.device_class = "voltage";
            voltage_sensor.state_class = "measurement";
            voltage_sensor.unit_of_measurement = "V";
            voltage_sensor.icon = "mdi:flash";
            voltage_sensor.suggested_display_precision = 1;
            voltage_sensor.enabled = phase <= nr_of_phases && service_items.contains(voltage_path);
            entities.emplace(voltage_path, std::move(voltage_sensor));
        }
        {
            std::string current_path = "/Ac/L" + std::to_string(phase) + "/Current";
            HAEntityConfig current_sensor;
            current_sensor.name = "L" + std::to_string(phase) + " Current";
            current_sensor.device_class = "current";
            current_sensor.state_class = "measurement";
            current_sensor.unit_of_measurement = "A";
            current_sensor.icon = "mdi:current-ac";
            current_sensor.suggested_display_precision = 2;
            current_sensor.enabled = phase <= nr_of_phases && service_items.contains(current_path);
            entities.emplace(current_path, std::move(current_sensor));
        }
        {
            std::string energy_forward_path = "/Ac/L" + std::to_string(phase) + "/Energy/Forward";
            HAEntityConfig energy_forward_sensor;
            energy_forward_sensor.name = "L" + std::to_string(phase) + " Energy Import";
            energy_forward_sensor.device_class = "energy";
            energy_forward_sensor.state_class = "total_increasing";
            energy_forward_sensor.unit_of_measurement = "kWh";
            energy_forward_sensor.icon = "mdi:counter";
            energy_forward_sensor.suggested_display_precision = 2;
            energy_forward_sensor.enabled = phase <= nr_of_phases && service_items.contains(energy_forward_path);
            entities.emplace(energy_forward_path, std::move(energy_forward_sensor));
        }
        {
            std::string energy_reverse_path = "/Ac/L" + std::to_string(phase) + "/Energy/Reverse";
            HAEntityConfig energy_reverse_sensor;
            energy_reverse_sensor.name = "L" + std::to_string(phase) + " Energy Export";
            energy_reverse_sensor.device_class = "energy";
            energy_reverse_sensor.state_class = "total_increasing";
            energy_reverse_sensor.unit_of_measurement = "kWh";
            energy_reverse_sensor.icon = "mdi:counter";
            energy_reverse_sensor.suggested_display_precision = 2;
            energy_reverse_sensor.enabled = phase <= nr_of_phases && service_items.contains(energy_reverse_path);
            entities.emplace(energy_reverse_path, std::move(energy_reverse_sensor));
        }
    }
    if (service_items.contains("/Ac/Power")) {
        HAEntityConfig total_power;
        total_power.name = "Total Power";
        total_power.device_class = "power";
        total_power.state_class = "measurement";
        total_power.unit_of_measurement = "W";
        total_power.icon = "mdi:transmission-tower";
        total_power.suggested_display_precision = 1;
        entities.emplace("/Ac/Power", std::move(total_power));
    }
    if (service_items.contains("/Position"))
        addStringDiagnostic("/Position", "Position", "mdi:map-marker",
                            "{% set positions = {0: 'AC input 1', 1: 'AC output', 2: 'AC input 2'} %}{{ positions[value_json.value] | default('Unknown (' + value_json.value|string + ')') }}");
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
