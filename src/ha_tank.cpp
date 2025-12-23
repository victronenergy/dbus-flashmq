#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

void HomeAssistantDiscovery::TankDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    if (service_items.contains("/Level")) {
        HAEntityConfig level_sensor;
        level_sensor.name = "Tank Level";
        level_sensor.state_class = "measurement";
        level_sensor.unit_of_measurement = "%";
        level_sensor.icon = "mdi:gauge";
        level_sensor.suggested_display_precision = 1;
        entities.emplace("/Level", std::move(level_sensor));
    }
    if (service_items.contains("/Capacity")) {
        HAEntityConfig capacity_sensor;
        capacity_sensor.name = "Tank Capacity";
        capacity_sensor.state_class = "measurement";
        capacity_sensor.unit_of_measurement = "L";
        capacity_sensor.icon = "mdi:storage-tank";
        capacity_sensor.suggested_display_precision = 0;
        capacity_sensor.entity_category = "diagnostic";
        entities.emplace("/Capacity", std::move(capacity_sensor));
    }
    if (service_items.contains("/Remaining")) {
        HAEntityConfig remaining_sensor;
        remaining_sensor.name = "Tank Remaining Volume";
        remaining_sensor.state_class = "measurement";
        remaining_sensor.unit_of_measurement = "L";
        remaining_sensor.icon = "mdi:gauge";
        remaining_sensor.suggested_display_precision = 1;
        entities.emplace("/Remaining", std::move(remaining_sensor));
    }
    if (service_items.contains("/FluidType"))
        addStringDiagnostic("/FluidType", "Fluid Type", "mdi:waves", FLUID_TYPE_VALUE_TEMPLATE);
    addCommonDiagnostics(service_items);
}

std::pair<std::string, std::string> HomeAssistantDiscovery::TankDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = getItemText(items, {"/ProductName"});
        if (name.empty()) {
            // Try to determine tank type from FluidType
            auto fluid_type = items.find("/FluidType");
            if (fluid_type != items.end()) {
                int type = fluid_type->second.get_value().value.as_int();
                switch (type) {
                case 0: name = "Fuel Tank"; break;
                case 1: name = "Fresh Water Tank"; break;
                case 2: name = "Waste Water Tank"; break;
                case 3: name = "Live Well"; break;
                case 4: name = "Oil Tank"; break;
                case 5: name = "Black Water Tank"; break;
                case 6: name = "Gasoline Tank"; break;
                case 7: name = "Diesel Tank"; break;
                case 8: name = "LPG Tank"; break;
                case 9: name = "LNG Tank"; break;
                case 10: name = "Hydraulic Oil Tank"; break;
                case 11: name = "Raw Water Tank"; break;
                default: name = "Tank Sensor"; break;
                }
            } else {
                name = "Tank Sensor";
            }
        }

        // No custom name so add the device instance
        name.push_back(' ');
        name.append(short_service_name.instance());
    }

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "Tank Level Sensor";
    }

    return {name, model};
}
