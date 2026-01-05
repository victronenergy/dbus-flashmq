#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

bool HomeAssistantDiscovery::BatteryDevice::calcHasMidVoltage(const std::unordered_map<std::string, Item> &service_items)
{
    // Check if the service has mid voltage support
    return service_items.contains("/Dc/0/MidVoltage")
           && (!service_items.contains("/Settings/HasMidVoltage")
               || getItemValue(service_items, "/Settings/HasMidVoltage").as_int());
}

bool HomeAssistantDiscovery::BatteryDevice::calcHasTemperature(const std::unordered_map<std::string, Item> &service_items)
{
    // Check if the service has mid voltage support
    return service_items.contains("/Dc/0/Temperature")
           && (!service_items.contains("/Settings/HasTemperature")
               || getItemValue(service_items, "/Settings/HasTemperature").as_int());
}

bool HomeAssistantDiscovery::BatteryDevice::calcHasStarterVoltage(const std::unordered_map<std::string, Item> &service_items)
{
    // Check if the service has aux voltage support
    return service_items.contains("/Dc/1/Voltage")
           && (!service_items.contains("/Settings/HasStarterVoltage")
               || getItemValue(service_items, "/Settings/HasStarterVoltage").as_int());
}

void HomeAssistantDiscovery::BatteryDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    has_mid_voltage = calcHasMidVoltage(service_items);
    has_temperature = calcHasTemperature(service_items);
    has_starter_voltage = calcHasStarterVoltage(service_items);

    if (std::string path = "/Soc"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "State of Charge";
        entity.state_class = "measurement";
        entity.device_class = "battery";
        entity.unit_of_measurement = "%";
        entity.icon = "mdi:battery";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/0/Voltage"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Voltage";
        entity.state_class = "measurement";
        entity.device_class = "voltage";
        entity.unit_of_measurement = "V";
        entity.icon = "mdi:flash";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/0/Current"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Current";
        entity.device_class = "current";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "A";
        entity.icon = "mdi:current-dc";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/History/DischargedEnergy"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Energy Consumed";
        entity.device_class = "energy";
        entity.state_class = "total_increasing";
        entity.unit_of_measurement = "kWh";
        entity.icon = "mdi:battery-minus";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/History/ChargedEnergy"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Energy Charged";
        entity.device_class = "energy";
        entity.state_class = "total_increasing";
        entity.unit_of_measurement = "kWh";
        entity.icon = "mdi:battery-plus";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/0/Power"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Power";
        entity.device_class = "power";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "W";
        entity.icon = "mdi:flash";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }

    { // Battery Temperature - /Dc/0/Temperature
        std::string path = "/Dc/0/Temperature";
        HAEntityConfig entity;
        entity.name = "Battery Temperature";
        entity.device_class = "temperature";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "°C";
        entity.icon = "mdi:thermometer";
        entity.suggested_display_precision = 1;
        entity.enabled = has_temperature;
        entities.emplace(path, std::move(entity));
    }

    { // Mid Voltage - /Dc/0/MidVoltage
        std::string path = "/Dc/0/MidVoltage";
        HAEntityConfig entity;
        entity.name = "Mid Voltage";
        entity.device_class = "voltage";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "V";
        entity.icon = "mdi:flash";
        entity.suggested_display_precision = 2;
        entity.enabled = has_mid_voltage;
        entities.emplace(path, std::move(entity));
    }

    { // Mid Voltage Deviation - /Dc/0/MidVoltageDeviation
        std::string path = "/Dc/0/MidVoltageDeviation";
        HAEntityConfig entity;
        entity.name = "Mid Voltage Deviation";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "%";
        entity.icon = "mdi:battery-alert";
        entity.suggested_display_precision = 1;
        entity.enabled = has_mid_voltage;
        entities.emplace(path, std::move(entity));
    }

    { // Starter Voltage - /Dc/1/Voltage
        std::string path = "/Dc/1/Voltage";
        HAEntityConfig entity;
        entity.name = "Starter Voltage";
        entity.device_class = "voltage";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "V";
        entity.icon = "mdi:car-battery";
        entity.suggested_display_precision = 2;
        entity.enabled = has_starter_voltage;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/ConsumedAmphours"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Consumed Amp Hours";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "Ah";
        entity.icon = "mdi:battery-minus";
        entity.suggested_display_precision = 2;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/TimeToGo"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Time to Go";
        entity.device_class = "duration";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "s";
        entity.icon = "mdi:timer";
        entity.suggested_display_precision = 0;
        entities.emplace(path, std::move(entity));
    }
    addCommonDiagnostics(service_items);
}

std::pair<std::string, std::string> HomeAssistantDiscovery::BatteryDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = getItemText(items, {"/ProductName"});
        if (name.empty()) {
            name = "Battery Monitor";
        }

        // No custom name so add the device instance
        name.push_back(' ');
        name.append(short_service_name.instance());
    }

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "Battery Monitor";
    }

    return {name, model};
}

bool HomeAssistantDiscovery::BatteryDevice::update(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                                                   const std::unordered_map<std::string, Item> &changed_items)
{
    const auto &service_items = all_items.at(service);
    bool changed = DeviceData::update(all_items, changed_items);
    if (changed_items.contains("/Settings/HasMidVoltage")) {
        bool act_has_mid_voltage = calcHasMidVoltage(service_items);
        if (act_has_mid_voltage != has_mid_voltage) {
            has_mid_voltage = act_has_mid_voltage;
            entities["/Dc/0/MidVoltage"].enabled = has_mid_voltage;
            entities["/Dc/0/MidVoltageDeviation"].enabled = has_mid_voltage;
            changed = true;
        }
    }
    if (changed_items.contains("/Settings/HasTemperature")) {
        bool act_has_temperature = calcHasTemperature(service_items);
        if (act_has_temperature != has_temperature) {
            has_temperature = act_has_temperature;
            entities["/Dc/0/Temperature"].enabled = has_temperature;
            changed = true;
        }
    }
    if (changed_items.contains("/Settings/HasStarterVoltage")) {
        bool act_has_starter_voltage = calcHasStarterVoltage(service_items);
        if (act_has_starter_voltage != has_starter_voltage) {
            has_starter_voltage = act_has_starter_voltage;
            entities["/Dc/1/Voltage"].enabled = has_starter_voltage;
            changed = true;
        }
    }

    return changed;
}
