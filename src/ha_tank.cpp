#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

void HomeAssistantDiscovery::TankDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    if (std::string path = "/Level"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Tank Level";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "%";
        entity.icon = "mdi:gauge";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Capacity"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Tank Capacity";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "L";
        entity.icon = "mdi:storage-tank";
        entity.suggested_display_precision = 0;
        entity.entity_category = "diagnostic";
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Remaining"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Tank Remaining Volume";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "L";
        entity.icon = "mdi:gauge";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
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
