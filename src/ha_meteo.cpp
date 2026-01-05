#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

void HomeAssistantDiscovery::MeteoDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    if (std::string path = "/CellTemperature"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Cell temperature";
        entity.state_class = "measurement";
        entity.device_class = "temperature";
        entity.unit_of_measurement = "°C";
        entity.icon = "mdi:thermometer";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Irradiance"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Solar irradiance";
        entity.state_class = "measurement";
        entity.device_class = "irradiance";
        entity.unit_of_measurement = "W/m²";
        entity.icon = "mdi:solar-power";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/TodaysYield"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Today's Yield";
        entity.state_class = "total_increasing";
        entity.device_class = "energy";
        entity.unit_of_measurement = "kWh";
        entity.icon = "mdi:solar-panel";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/TimeSinceLastSun"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Time Since Last Sun";
        entity.state_class = "measurement";
        entity.device_class = "duration";
        entity.unit_of_measurement = "s";
        entity.icon = "mdi:clock";
        entity.entity_category = "diagnostic";
        entities.emplace(path, std::move(entity));
    }
    if (service_items.contains("/BatteryVoltage"))
        addNumericDiagnostic("/BatteryVoltage", "Battery Voltage", "mdi:battery", 3, "voltage", "V");
    addCommonDiagnostics(service_items);
}

std::pair<std::string, std::string> HomeAssistantDiscovery::MeteoDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = getItemText(items, {"/DeviceName", "/ProductName"});
        if (name.empty()) {
            // Determine sensor type based on available measurements
            bool has_irradiance = items.find("/Irradiance") != items.end();
            bool has_temp = items.find("/CellTemperature") != items.end();
            bool has_yield = items.find("/TodaysYield") != items.end();

            if (has_irradiance && has_temp && has_yield) {
                name = "Solar Weather Station";
            } else if (has_irradiance && has_temp) {
                name = "Solar Irradiance Sensor";
            } else if (has_irradiance) {
                name = "Irradiance Sensor";
            } else if (has_temp) {
                name = "Meteorological Sensor";
            } else {
                name = "Weather Station";
            }
        }

        // No custom name so add the device instance
        name.push_back(' ');
        name.append(short_service_name.instance());
    }

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "Meteorological Sensor";
    }

    return {name, model};
}
