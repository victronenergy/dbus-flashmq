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

    if (service_items.contains("/Soc")) {
        HAEntityConfig soc_sensor;
        soc_sensor.name = "State of Charge";
        soc_sensor.state_class = "measurement";
        soc_sensor.device_class = "battery";
        soc_sensor.unit_of_measurement = "%";
        soc_sensor.icon = "mdi:battery";
        soc_sensor.suggested_display_precision = 1;
        entities.emplace("/Soc", std::move(soc_sensor));
    }

    if (service_items.contains("/Dc/0/Voltage")) {
        HAEntityConfig voltage_sensor;
        voltage_sensor.name = "Battery Voltage";
        voltage_sensor.state_class = "measurement";
        voltage_sensor.device_class = "voltage";
        voltage_sensor.unit_of_measurement = "V";
        voltage_sensor.icon = "mdi:flash";
        voltage_sensor.suggested_display_precision = 2;
        entities.emplace("/Dc/0/Voltage", std::move(voltage_sensor));
    }

    if (service_items.contains("/Dc/0/Current")) {
        HAEntityConfig current_sensor;
        current_sensor.name = "Battery Current";
        current_sensor.device_class = "current";
        current_sensor.state_class = "measurement";
        current_sensor.unit_of_measurement = "A";
        current_sensor.icon = "mdi:current-dc";
        current_sensor.suggested_display_precision = 2;
        entities.emplace("/Dc/0/Current", std::move(current_sensor));
    }

    if (service_items.contains("/History/DischargedEnergy")) {
        HAEntityConfig energy_consumed;
        energy_consumed.name = "Energy Consumed";
        energy_consumed.device_class = "energy";
        energy_consumed.state_class = "total_increasing";
        energy_consumed.unit_of_measurement = "kWh";
        energy_consumed.icon = "mdi:battery-minus";
        energy_consumed.suggested_display_precision = 2;
        entities.emplace("/History/DischargedEnergy", std::move(energy_consumed));
    }

    if (service_items.contains("/History/ChargedEnergy")) {
        HAEntityConfig energy_charged;
        energy_charged.name = "Energy Charged";
        energy_charged.device_class = "energy";
        energy_charged.state_class = "total_increasing";
        energy_charged.unit_of_measurement = "kWh";
        energy_charged.icon = "mdi:battery-plus";
        energy_charged.suggested_display_precision = 2;
        entities.emplace("/History/ChargedEnergy", std::move(energy_charged));
    }
    if (service_items.contains("/Dc/0/Power")) {
        HAEntityConfig power_sensor;
        power_sensor.name = "Battery Power";
        power_sensor.device_class = "power";
        power_sensor.state_class = "measurement";
        power_sensor.unit_of_measurement = "W";
        power_sensor.icon = "mdi:flash";
        power_sensor.suggested_display_precision = 1;
        entities.emplace("/Dc/0/Power", std::move(power_sensor));
    }
    {
        // Always add and calculate enabled
        HAEntityConfig temp_sensor;
        temp_sensor.name = "Battery Temperature";
        temp_sensor.device_class = "temperature";
        temp_sensor.state_class = "measurement";
        temp_sensor.unit_of_measurement = "°C";
        temp_sensor.icon = "mdi:thermometer";
        temp_sensor.suggested_display_precision = 1;
        temp_sensor.enabled = has_temperature;
        entities.emplace("/Dc/0/Temperature", std::move(temp_sensor));
    }
    {
        // Always add and calculate enabled
        HAEntityConfig mid_voltage_sensor;
        mid_voltage_sensor.name = "Mid Voltage";
        mid_voltage_sensor.device_class = "voltage";
        mid_voltage_sensor.state_class = "measurement";
        mid_voltage_sensor.unit_of_measurement = "V";
        mid_voltage_sensor.icon = "mdi:flash";
        mid_voltage_sensor.suggested_display_precision = 2;
        mid_voltage_sensor.enabled = has_mid_voltage;
        entities.emplace("/Dc/0/MidVoltage", std::move(mid_voltage_sensor));
    }
    {
        // Always add and calculate enabled
        HAEntityConfig mid_deviation_sensor;
        mid_deviation_sensor.name = "Mid Voltage Deviation";
        mid_deviation_sensor.state_class = "measurement";
        mid_deviation_sensor.unit_of_measurement = "%";
        mid_deviation_sensor.icon = "mdi:battery-alert";
        mid_deviation_sensor.suggested_display_precision = 1;
        mid_deviation_sensor.enabled = has_mid_voltage;
        entities.emplace("/Dc/0/MidVoltageDeviation", std::move(mid_deviation_sensor));
    }
    {
        // Always add and calculate enabled
        HAEntityConfig starter_voltage_sensor;
        starter_voltage_sensor.name = "Starter Voltage";
        starter_voltage_sensor.device_class = "voltage";
        starter_voltage_sensor.state_class = "measurement";
        starter_voltage_sensor.unit_of_measurement = "V";
        starter_voltage_sensor.icon = "mdi:car-battery";
        starter_voltage_sensor.suggested_display_precision = 2;
        starter_voltage_sensor.enabled = has_starter_voltage;
        entities.emplace("/Dc/1/Voltage", std::move(starter_voltage_sensor));
    }
    if (service_items.contains("/ConsumedAmphours")) {
        HAEntityConfig consumed_ah_sensor;
        consumed_ah_sensor.name = "Consumed Amp Hours";
        consumed_ah_sensor.state_class = "measurement";
        consumed_ah_sensor.unit_of_measurement = "Ah";
        consumed_ah_sensor.icon = "mdi:battery-minus";
        consumed_ah_sensor.suggested_display_precision = 2;
        entities.emplace("/ConsumedAmphours", std::move(consumed_ah_sensor));
    }
    if (service_items.contains("/TimeToGo")) {
        HAEntityConfig ttg_sensor;
        ttg_sensor.name = "Time to Go";
        ttg_sensor.device_class = "duration";
        ttg_sensor.state_class = "measurement";
        ttg_sensor.unit_of_measurement = "s";
        ttg_sensor.icon = "mdi:timer";
        ttg_sensor.suggested_display_precision = 0;
        entities.emplace("/TimeToGo", std::move(ttg_sensor));
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
