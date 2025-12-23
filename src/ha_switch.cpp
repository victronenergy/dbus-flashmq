#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

void HomeAssistantDiscovery::SwitchDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    // Handle the dynamic entities
    // go over the service_items and create entities for each SwitchableOutput
    for (const auto &item : service_items) {
        const auto &dbus_path = item.first;
        auto parts = splitPath(dbus_path);
        if (parts.size() == 3
            && parts[0] == "SwitchableOutput"
            && std::count(parts[1].begin(), parts[1].end(), '_') == 1)
        {
            std::string_view output_type = parts[1].substr(0, parts[1].find('_'));

            if (parts[2] == "State") {
                HAEntityConfig switch_state;
                std::string name_path = "/SwitchableOutput/" + std::string(parts[1]) + "/Name";
                std::string custom_name_path = "/SwitchableOutput/" + std::string(parts[1]) + "/Settings/CustomName";
                switch_state.name = getItemText(service_items, {custom_name_path, name_path});
                switch_state.platform = "switch";
                switch_state.state_class = "measurement";
                switch_state.device_class = "switch";
                switch_state.command_topic = dbus_path;
                switch_state.payload_on = "{\"value\": 1}";
                switch_state.payload_off = "{\"value\": 0}";
                switch_state.optimistic = false; // Wait for state feedback
                switch_state.value_template = "{% if value_json.value == 1 %}ON{% else %}OFF{% endif %}";
                switch_state.state_on = "ON";
                switch_state.state_off = "OFF";

                if (output_type == "output") {
                    switch_state.icon = "mdi:electric-switch";
                } else if (output_type == "pwm") {
                    switch_state.icon = "mdi:sine-wave";
                } else if (output_type == "relay") {
                    switch_state.icon = "mdi:electric-switch";
                }
                auto result = entities.emplace(dbus_path, std::move(switch_state));
                if (result.second) {
                    // Successfully added, so store the path for custom name updates
                    customname_paths[custom_name_path].custom_name_path = custom_name_path;
                    customname_paths[custom_name_path].name_path = name_path;
                    customname_paths[custom_name_path].state_entity_config = &result.first->second;
                }
            } else if (parts[2] == "Dimming" && output_type == "pwm") {
                HAEntityConfig dimming_state;
                std::string name_path = "/SwitchableOutput/" + std::string(parts[1]) + "/Name";
                std::string custom_name_path = "/SwitchableOutput/" + std::string(parts[1]) + "/Settings/CustomName";
                dimming_state.name = getItemText(service_items, {custom_name_path, name_path}) + " Dimming";
                dimming_state.platform = "number";
                dimming_state.state_class = "measurement";
                dimming_state.icon = "mdi:brightness-percent";
                dimming_state.suggested_display_precision = 0;

                dimming_state.command_topic = dbus_path;
                dimming_state.command_template = "{\"value\": {{ value }} }";
                dimming_state.min_value = 0;
                dimming_state.max_value = 100;
                dimming_state.unit_of_measurement = "%";
                dimming_state.mode = "slider";
                dimming_state.optimistic = false; // Wait for state feedback
                auto result = entities.emplace(dbus_path, std::move(dimming_state));
                if (result.second) {
                    // Successfully added, so store the path for custom name updates
                    customname_paths[custom_name_path].custom_name_path = custom_name_path;
                    customname_paths[custom_name_path].name_path = name_path;
                    customname_paths[custom_name_path].dimming_entity_config = &result.first->second;
                }
            }
        }
    }

    if (service_items.contains("/State"))
        addStringDiagnostic("/State", "Device State", "mdi:power-settings", "{% set states = {256: 'Connected', 257: 'Over temperature', 258: 'Temperature warning', 259: 'Channel fault', 260: 'Channel Tripped', 261: 'Under Voltage'} %}{{ states[value_json.value] | default('Unknown (' + value_json.value|string + ')') }}");
    addCommonDiagnostics(service_items);
}

bool HomeAssistantDiscovery::SwitchDevice::update(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                                                  const std::unordered_map<std::string, Item> &changed_items)
{
    const auto &service_items = all_items.at(service);
    bool changed = DeviceData::update(all_items, changed_items);
    // Check if any of the SwitchableOutput names have changed
    for (const auto &item : customname_paths) {
        if (changed_items.contains(item.first)) {
            CustomNameInfo const & info = item.second;
            std::string base_name = getItemText(service_items, {info.custom_name_path, info.name_path});
            if (info.state_entity_config)
                info.state_entity_config->name = base_name;
            if (info.dimming_entity_config)
                info.dimming_entity_config->name = base_name + " Dimming";
            changed = true;
        }
    }
    return changed;
}

std::pair<std::string, std::string> HomeAssistantDiscovery::SwitchDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = getItemText(items, {"/ProductName"});
        if (name.empty()) {
            name = "Switch Device";
        }
        // No custom name so add the device instance
        name.push_back(' ');
        name.append(short_service_name.instance());
    }

    std::string model = getItemText(items, {"/ProductName"});
    if (model.empty()) {
        model = "Switch Device";
    }

    return {name, model};
}
