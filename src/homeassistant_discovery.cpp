#include "homeassistant_discovery.h"
#include "vendor/flashmq_plugin.h"
#include <algorithm>
#include <ranges>

using namespace dbus_flashmq;

std::unordered_map<std::string_view, std::function<std::unique_ptr<HomeAssistantDiscovery::DeviceData>()>> HomeAssistantDiscovery::device_factory_functions = {
    {"temperature", []() { return std::make_unique<HomeAssistantDiscovery::TemperatureDevice>(); }},
    {"battery", []() { return std::make_unique<HomeAssistantDiscovery::BatteryDevice>(); }},
    {"solarcharger", []() { return std::make_unique<HomeAssistantDiscovery::SolarChargerDevice>(); }},
    {"vebus", []() { return std::make_unique<HomeAssistantDiscovery::VeBusDevice>(); }},
    {"system", []() { return std::make_unique<HomeAssistantDiscovery::SystemDevice>(); }},
    {"tank", []() { return std::make_unique<HomeAssistantDiscovery::TankDevice>(); }},
    {"grid", []() { return std::make_unique<HomeAssistantDiscovery::GridMeterDevice>(); }},
    {"switch", []() { return std::make_unique<HomeAssistantDiscovery::SwitchDevice>(); }},
    {"meteo", []() { return std::make_unique<HomeAssistantDiscovery::MeteoDevice>(); }},
    {"charger", []() { return std::make_unique<HomeAssistantDiscovery::ChargerDevice>(); }},
    {"dcdc", []() { return std::make_unique<HomeAssistantDiscovery::DcDcDevice>(); }},
    {"digitalinput", []() { return std::make_unique<HomeAssistantDiscovery::DigitalInputDevice>(); }},
    {"acsystem", []() { return std::make_unique<HomeAssistantDiscovery::AcSystemDevice>(); }},
    {"evcharger", []() { return std::make_unique<HomeAssistantDiscovery::EvChargerDevice>(); }},
};

VeVariant HomeAssistantDiscovery::getItemValue(const std::unordered_map<std::string, Item> &service_items, std::string_view dbus_path)
{
    auto it = service_items.find(std::string(dbus_path));
    if (it != service_items.end()) {
        return it->second.get_value().value;
    }
    return VeVariant();
}

std::string HomeAssistantDiscovery::getItemText(const std::unordered_map<std::string, Item> &service_items,
                                                std::initializer_list<std::string_view> dbus_paths) {
    for (std::string_view dbus_path : dbus_paths) {
        auto it = service_items.find(std::string(dbus_path));
        if (it != service_items.end()) {
            const auto &value = it->second.get_value().value;
            if (value.get_type() == VeVariantType::String) {
                std::string result = value.as_text();
                if (!result.empty())
                    return result;
            }
        }
    }
    return std::string();
}

std::vector<std::string_view> HomeAssistantDiscovery::splitPath(std::string_view dbus_path)
{
    dbus_path.remove_prefix(1); // Remove the leading '/'
    auto parts_view = dbus_path
                      | std::ranges::views::split('/')
                      | std::ranges::views::transform([](auto&& part) { return std::string_view(&*part.begin(), std::ranges::distance(part)); }); // convert from range of characters to string_view
    return std::vector<std::string_view>(parts_view.begin(), parts_view.end());
}

std::string HomeAssistantDiscovery::toIdentifier(std::string_view input)
{
    std::string result(input);
    std::replace(result.begin(), result.end(), '/', '_');
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

void HomeAssistantDiscovery::DeviceData::addNumericDiagnostic(std::string_view dbus_path,
                                                              std::string_view name,
                                                              std::string_view icon,
                                                              int suggested_display_precision,
                                                              std::string_view device_class,
                                                              std::string_view unit_of_measurement,
                                                              std::string_view value_template)
{
    HAEntityConfig entity;
    entity.entity_category = "diagnostic";
    entity.state_class = "measurement";
    entity.name = name;
    entity.icon = icon;
    if (!device_class.empty()) entity.device_class = device_class;
    if (!unit_of_measurement.empty()) entity.unit_of_measurement = unit_of_measurement;
    if (suggested_display_precision != -1) entity.suggested_display_precision = suggested_display_precision;
    if (!value_template.empty()) entity.value_template = value_template;
    entities.emplace(std::string(dbus_path), std::move(entity));

}

void HomeAssistantDiscovery::DeviceData::addStringDiagnostic(std::string_view dbus_path,
                                                             std::string_view name,
                                                             std::string_view icon,
                                                             std::string_view value_template)
{
    HAEntityConfig entity;
    entity.name = name;
    entity.icon = icon;
    entity.entity_category = "diagnostic";
    if (!value_template.empty()) entity.value_template = value_template;
    entities.emplace(std::string(dbus_path), std::move(entity));
}

void HomeAssistantDiscovery::DeviceData::addCommonDiagnostics(const std::unordered_map<std::string, Item> &service_items)
{
    if (service_items.contains("/DeviceInstance"))
        addNumericDiagnostic("/DeviceInstance", "Device Instance", "mdi:numeric");
    if (service_items.contains("/ErrorCode"))
        addNumericDiagnostic("/ErrorCode", "Error Code", "mdi:alert-circle");
    if (service_items.contains("/Status"))
        addNumericDiagnostic("/Status", "Status", "mdi:information");
    if (service_items.contains("/State"))
        addNumericDiagnostic("/State", "Device State", "mdi:power-settings");
    if (service_items.contains("/FirmwareVersion"))
        addStringDiagnostic("/FirmwareVersion", "Firmware Version", "mdi:chip", VICTRON_VERSION_VALUE_TEMPLATE);
    if (service_items.contains("/HardwareVersion"))
        addStringDiagnostic("/HardwareVersion", "Hardware Version", "mdi:memory", VICTRON_VERSION_VALUE_TEMPLATE);
    if (service_items.contains("/DeviceOffReason"))
        addStringDiagnostic("/DeviceOffReason", "Off Reason", "mdi:information-outline", DEVICE_OFF_REASON_VALUE_TEMPLATE);
}

nlohmann::json HADevice::toJson() const
{
    nlohmann::json j;
    j["name"] = name;
    j["manufacturer"] = manufacturer;
    j["model"] = model;
    j["identifiers"] = nlohmann::json::array({identifiers});

    if (!hw_version.empty()) j["hw_version"] = hw_version;
    if (!sw_version.empty()) j["sw_version"] = sw_version;
    if (!serial_number.empty()) j["serial_number"] = serial_number;
    if (!configuration_url.empty()) j["configuration_url"] = configuration_url;
    if (!via_device.empty()) j["via_device"] = via_device;

    return j;
}

nlohmann::json HAEntityConfig::toJson() const
{
    nlohmann::json config_json;

    config_json["platform"] = platform;
    if (enabled) {
        config_json["unique_id"] = unique_id;
        config_json["name"] = name;
        config_json["state_topic"] = state_topic;
        config_json["value_template"] = value_template.empty() ? DEFAULT_VALUE_TEMPLATE : value_template;
        if (!state_on.empty()) { config_json["state_on"] = state_on; }
        if (!state_off.empty()) { config_json["state_off"] = state_off; }
        if (!unit_of_measurement.empty()) { config_json["unit_of_measurement"] = unit_of_measurement; }
        if (!device_class.empty()) { config_json["device_class"] = device_class; }
        if (!state_class.empty()) { config_json["state_class"] = state_class; }
        if (!icon.empty()) { config_json["icon"] = icon; }
        if (!entity_category.empty()) { config_json["entity_category"] = entity_category; }
        if (suggested_display_precision >= 0) { config_json["suggested_display_precision"] = suggested_display_precision; }
        if (!command_topic.empty()) {
            config_json["command_topic"] = command_topic;

            // For switch entities
            if (!payload_on.empty()) { config_json["payload_on"] = payload_on; }
            if (!payload_off.empty()) { config_json["payload_off"] = payload_off; }

            // For number entities (dimmers)
            if (min_value != 0 || max_value != 0) { config_json["min"] = min_value; config_json["max"] = max_value; }
            if (!mode.empty()) { config_json["mode"] = mode; }
            config_json["command_template"] = command_template.empty() ? DEFAULT_COMMAND_TEMPLATE : command_template;
        }
    }

    return config_json;
}

void HomeAssistantDiscovery::DeviceData::fillHADevice(const std::string &vrm_id,
                                                      const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items)
{
    const auto &service_items = all_items.at(service);
    std::string systemIdentifier = vrm_id + "_system";
    std::string deviceIdentifier = vrm_id + "_" + toIdentifier(short_service_name.total());

    bool isSystemService = short_service_name.service_type() == "system";
    auto [name, model] = getNameAndModel(all_items);

    ha_device.name = std::move(name);
    ha_device.model = std::move(model);
    ha_device.identifiers = isSystemService ? systemIdentifier : deviceIdentifier;
    ha_device.via_device = isSystemService ? "" : systemIdentifier;
    ha_device.serial_number = getItemText(service_items, {"/Serial"});
    ha_device.sw_version = getItemText(service_items, {"/FirmwareVersion"});
    ha_device.hw_version = getItemText(service_items, {"/HardwareVersion"});
}

void HomeAssistantDiscovery::DeviceData::fillHAEntities(const std::string &vrm_id,
                                                        const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items)
{
    entities.clear();
    addEntities(all_items);
    // Fill in the unique_ids, the state topics and the command topics
    for (auto & entity : entities) {
        const auto &dbus_path = entity.first;
        HAEntityConfig &entity_config = entity.second;

        entity_config.unique_id = ha_device.identifiers + toIdentifier(dbus_path);
        entity_config.state_topic = "N/" + vrm_id + "/" + short_service_name.total() + dbus_path;
        if (!entity_config.command_topic.empty())
            entity_config.command_topic = "W/" + vrm_id + "/" + short_service_name.total() + entity_config.command_topic;
    }
}

void HomeAssistantDiscovery::DeviceData::processChangedItems(const std::string &vrm_id,
                                                             const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                                                             const std::string& service,
                                                             const std::unordered_map<std::string, Item> &changed_items)
{
    bool changed = false;
    const auto &service_items = all_items.at(service);
    if (item_count != service_items.size()) {
        // The number of items for this service has changed. Just start over with a full discovery
        flashmq_logf(LOG_DEBUG, "The number of items of device %s has changed", service.c_str());
        item_count = service_items.size();
        fillHAEntities(vrm_id, all_items);
        changed = true;
    } else {
        // Check whether something has changed that enables or disables some entities
        if (update(all_items, changed_items)) {
            flashmq_logf(LOG_DEBUG, "Some entities for device %s have been enabled or disabled", service.c_str());
            changed = true;
        }
        // Check whether the name is still the same
        if (std::any_of(changed_items.begin(), changed_items.end(), [this](const std::pair<std::string, Item> &elem)->bool { return name_paths.contains(elem.first); })) {
            auto [name, model] = getNameAndModel(all_items);
            if (ha_device.name != name) {
                flashmq_logf(LOG_DEBUG, "The name of device %s has changed", service.c_str());
                ha_device.name = name;
                changed = true;
            }
            if (ha_device.model != model) {
                flashmq_logf(LOG_DEBUG, "The model of device %s has changed", service.c_str());
                ha_device.model = model;
                changed = true;
            }
        }
    }
    if (changed) {
        nlohmann::json json = toJson();
        std::string payload = json.dump();
        if (cached_payload != payload) {
            flashmq_publish_message(discovery_topic, 0, true, payload);
            cached_payload = payload;
        }
    } else {
        flashmq_logf(LOG_DEBUG, "No changes detected for device %s, skipping republish", service.c_str());
    }
}

nlohmann::json HomeAssistantDiscovery::DeviceData::toJson() const
{
    nlohmann::json json = nlohmann::json::object();
    json["device"] = ha_device.toJson();
    json["origin"] = nlohmann::json::object({ {"name", "Venus OS / flashmq"}});
    nlohmann::json components = nlohmann::json::object();
    for (const auto &entity : entities) {
        const HAEntityConfig &entity_config = entity.second;
        components[entity_config.unique_id] = entity_config.toJson();
    }
    json["components"] = components;
    return json;
}

// HomeAssistantDiscovery Implementation
HomeAssistantDiscovery::HomeAssistantDiscovery()
{
}


void HomeAssistantDiscovery::setVrmId(const std::string &vrm_id)
{
    this->vrm_id = vrm_id;
    flashmq_logf(LOG_NOTICE, "Home Assistant Discovery VRM ID set to: %s", vrm_id.c_str());
}

bool HomeAssistantDiscovery::isServiceEnabled(std::string_view service_type) const
{
    return device_factory_functions.contains(service_type);
}

std::string HomeAssistantDiscovery::createDeviceDiscoveryTopic(const std::string &device_id) const
{
    return "homeassistant/device/" + device_id + "/config";
}

std::unique_ptr<HomeAssistantDiscovery::DeviceData> HomeAssistantDiscovery::createDeviceData(const std::string &service,
                                                                                             const ShortServiceName &short_service_name,
                                                                                             const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) const
{
    std::unique_ptr<DeviceData> device = device_factory_functions.at(short_service_name.service_type())();
    const auto &service_items = all_items.at(service);

    device->service = service;
    device->short_service_name = short_service_name;
    device->item_count = service_items.size();

    device->fillHADevice(vrm_id, all_items);
    device->fillHAEntities(vrm_id, all_items);
    device->discovery_topic = createDeviceDiscoveryTopic(device->ha_device.identifiers);

    nlohmann::json json = device->toJson();
    device->cached_payload = json.dump();
    return device;
}

void HomeAssistantDiscovery::publishSensorEntitiesWithItems(const std::string &service,
                                                            const ShortServiceName &short_service_name,
                                                            const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                                                            const std::unordered_map<std::string, Item> &changed_items)
{
    auto dev_it = devices.find(service);
    // Check if device created
    if (dev_it == devices.end()) {
        if (!isServiceEnabled(short_service_name.service_type())) {
            return;
        }

        flashmq_logf(LOG_DEBUG, "No device found for service %s, creating new device", service.c_str());
        auto [device, result] = devices.insert({service, createDeviceData(service, short_service_name, all_items)});
        flashmq_publish_message(device->second->discovery_topic, 0, true, device->second->cached_payload);
    } else {
        flashmq_logf(LOG_DEBUG, "Updating existing device for service %s", service.c_str());
        DeviceData &device = *dev_it->second;
        device.processChangedItems(vrm_id, all_items, service, changed_items);
    }
}

void HomeAssistantDiscovery::publishAllConfigs() const
{
    for (const auto &entry : devices) {
        const DeviceData &device = *entry.second;
        flashmq_publish_message(device.discovery_topic, 0, true, device.cached_payload);
    }
}

void HomeAssistantDiscovery::removeAllSensorsForService(const std::string &service)
{
    flashmq_logf(LOG_DEBUG, "Removing all Home Assistant sensors for service: %s", service.c_str());

    auto device_entry = devices.find(service);
    if (device_entry == devices.end())
        return;

    flashmq_publish_message(device_entry->second->discovery_topic, 0, true, ""); // empty payload removes the entity

    devices.erase(device_entry);
}

void HomeAssistantDiscovery::clearAll()
{
    flashmq_logf(LOG_INFO, "Clearing all Home Assistant discovery entities");

    for (const auto &device_entry: devices) {
        flashmq_publish_message(device_entry.second->discovery_topic, 0, true, ""); // empty payload removes the entity
    }
    devices.clear();
}
