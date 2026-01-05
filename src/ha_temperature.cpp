#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

void HomeAssistantDiscovery::TemperatureDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    if (std::string path = "/Temperature"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Temperature";
        entity.state_class = "measurement";
        entity.device_class = "temperature";
        entity.unit_of_measurement = "°C";
        entity.icon = "mdi:thermometer";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Humidity"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Humidity";
        entity.state_class = "measurement";
        entity.device_class = "humidity";
        entity.unit_of_measurement = "%";
        entity.icon = "mdi:water-percent";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Pressure"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Pressure";
        entity.state_class = "measurement";
        entity.device_class = "atmospheric_pressure";
        entity.unit_of_measurement = "hPa";
        entity.icon = "mdi:gauge";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }
    if (service_items.contains("/BatteryVoltage"))
        addNumericDiagnostic("/BatteryVoltage", "Battery Voltage", "mdi:battery", 3, "voltage", "V");
    addCommonDiagnostics(service_items);
}

std::pair<std::string, std::string> HomeAssistantDiscovery::TemperatureDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = getItemText(items, {"/ProductName"});
        if (name.empty()) {
            // Determine sensor type based on available measurements
            bool has_temp = items.find("/Temperature") != items.end();
            bool has_humidity = items.find("/Humidity") != items.end();
            bool has_pressure = items.find("/Pressure") != items.end();

            if (has_temp && has_humidity && has_pressure) {
                name = "Environmental Sensor";
            } else if (has_temp && has_humidity) {
                name = "Temperature & Humidity Sensor";
            } else if (has_temp && has_pressure) {
                name = "Temperature & Pressure Sensor";
            } else if (has_humidity && has_pressure) {
                name = "Humidity & Pressure Sensor";
            } else if (has_humidity) {
                name = "Humidity Sensor";
            } else if (has_pressure) {
                name = "Pressure Sensor";
            } else {
                name = "Temperature Sensor";
            }
        }

        // No custom name so add the device instance
        name.push_back(' ');
        name.append(short_service_name.instance());
    }

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "Environmental Sensor";
    }

    return {name, model};
}
