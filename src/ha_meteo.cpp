#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

void HomeAssistantDiscovery::MeteoDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);
    if (service_items.contains("/CellTemperature")) {
        HAEntityConfig cell_temp_sensor;
        cell_temp_sensor.name = "Cell temperature";
        cell_temp_sensor.state_class = "measurement";
        cell_temp_sensor.device_class = "temperature";
        cell_temp_sensor.unit_of_measurement = "°C";
        cell_temp_sensor.icon = "mdi:thermometer";
        cell_temp_sensor.suggested_display_precision = 1;
        entities.emplace("/CellTemperature", std::move(cell_temp_sensor));
    }
    if (service_items.contains("/Irradiance")) {
        HAEntityConfig irradiance_sensor;
        irradiance_sensor.name = "Solar irradiance";
        irradiance_sensor.state_class = "measurement";
        irradiance_sensor.device_class = "irradiance";
        irradiance_sensor.unit_of_measurement = "W/m²";
        irradiance_sensor.icon = "mdi:solar-power";
        irradiance_sensor.suggested_display_precision = 1;
        entities.emplace("/Irradiance", std::move(irradiance_sensor));
    }
    if (service_items.contains("/TodaysYield")) {
        HAEntityConfig todays_yield_sensor;
        todays_yield_sensor.name = "Today's Yield";
        todays_yield_sensor.state_class = "total_increasing";
        todays_yield_sensor.device_class = "energy";
        todays_yield_sensor.unit_of_measurement = "kWh";
        todays_yield_sensor.icon = "mdi:solar-panel";
        todays_yield_sensor.suggested_display_precision = 2;
        entities.emplace("/TodaysYield", std::move(todays_yield_sensor));
    }
    if (service_items.contains("/TimeSinceLastSun")) {
        HAEntityConfig time_since_sun_sensor;
        time_since_sun_sensor.name = "Time Since Last Sun";
        time_since_sun_sensor.state_class = "measurement";
        time_since_sun_sensor.device_class = "duration";
        time_since_sun_sensor.unit_of_measurement = "s";
        time_since_sun_sensor.icon = "mdi:clock";
        time_since_sun_sensor.entity_category = "diagnostic";
        entities.emplace("/TimeSinceLastSun", std::move(time_since_sun_sensor));
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
