#include "homeassistant_discovery.h"

using namespace dbus_flashmq;

const int HomeAssistantDiscovery::AcSystemDevice::max_nr_of_phases;
const int HomeAssistantDiscovery::AcSystemDevice::max_nr_of_ac_inputs;

void HomeAssistantDiscovery::AcSystemDevice::calcNrOfPhases(const std::unordered_map<std::string, Item> &service_items)
{
    if (!service_items.contains("/Ac/NumberOfPhases")) {
        // We do not know so just assume 3
        nr_of_phases = 3;
    } else {
        nr_of_phases = getItemValue(service_items, "/Ac/NumberOfPhases").as_int();
        // Limit the number of phases to 1 .. max_nr_of_phases
        nr_of_phases = std::clamp(nr_of_phases, 1, max_nr_of_phases);
    }
}

void HomeAssistantDiscovery::AcSystemDevice::calcNrOfAcInputs(const std::unordered_map<std::string, Item> &service_items)
{
    if (!service_items.contains("/Ac/NumberOfAcInputs")) {
        nr_of_ac_inputs = 1;
    } else {
        nr_of_ac_inputs = getItemValue(service_items, "/Ac/NumberOfAcInputs").as_int();
        // Limit the number of AC inputs to 1 .. max_nr_of_ac_inputs
        nr_of_ac_inputs = std::clamp(nr_of_ac_inputs, 1, max_nr_of_ac_inputs);
    }
}

// https://github.com/victronenergy/venus/wiki/dbus#acsystem
void HomeAssistantDiscovery::AcSystemDevice::addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& service_items = all_items.at(service);

    calcNrOfPhases(service_items);
    calcNrOfAcInputs(service_items);

    // Always loop over all possible phases and all possible number of ac inputs.
    // The enabled properties will be set according to the actual number of phases and nr of inputs.
    // This way we know in the update function that the entities exist and only need to enable/disable them.
    for (int input = 1; input <= max_nr_of_ac_inputs; ++input) { // /Ac/In/Lx/{P,V,I,F}, /Ac/In/{Type,CurrentLimitIsAdjustable,CurrentLimit}
        std::string input_path_prefix = "/Ac/In/" + std::to_string(input);
        std::string input_name_prefix = "AC In " + std::to_string(input);
        for (int phase = 1; phase <= max_nr_of_phases; ++phase) { // /Ac/In/Lx/{P,V,I,F}
            std::string phase_path_prefix = input_path_prefix + "/L" + std::to_string(phase);
            std::string phase_name_prefix = input_name_prefix + " L" + std::to_string(phase);
            { // Power - /Ac/In/Lx/P
                std::string path = phase_path_prefix + "/P";
                HAEntityConfig entity;
                entity.name = phase_name_prefix + " Power";
                entity.device_class = "power";
                entity.state_class = "measurement";
                entity.unit_of_measurement = "W";
                entity.icon = "mdi:transmission-tower";
                entity.suggested_display_precision = 1;
                entity.enabled = input <= nr_of_ac_inputs && phase <= nr_of_phases && service_items.contains(path);
                entities.emplace(path, std::move(entity));
            }

            { // Voltage - /Ac/In/Lx/V
                std::string path = phase_path_prefix + "/V";
                HAEntityConfig entity;
                entity.name = phase_name_prefix + " Voltage";
                entity.device_class = "voltage";
                entity.state_class = "measurement";
                entity.unit_of_measurement = "V";
                entity.icon = "mdi:flash";
                entity.suggested_display_precision = 1;
                entity.enabled = input <= nr_of_ac_inputs && phase <= nr_of_phases && service_items.contains(path);
                entities.emplace(path, std::move(entity));
            }

            { // Current - /Ac/In/Lx/I
                std::string path = phase_path_prefix + "/I";
                HAEntityConfig entity;
                entity.name = phase_name_prefix + " Current";
                entity.device_class = "current";
                entity.state_class = "measurement";
                entity.unit_of_measurement = "A";
                entity.icon = "mdi:current-ac";
                entity.suggested_display_precision = 2;
                entity.enabled = input <= nr_of_ac_inputs && phase <= nr_of_phases && service_items.contains(path);
                entities.emplace(path, std::move(entity));
            }

            { // Frequency - /Ac/In/Lx/F
                std::string path = phase_path_prefix + "/F";
                HAEntityConfig entity;
                entity.name = phase_name_prefix + " Frequency";
                entity.device_class = "frequency";
                entity.state_class = "measurement";
                entity.unit_of_measurement = "Hz";
                entity.icon = "mdi:sine-wave";
                entity.suggested_display_precision = 2;
                entity.enabled = input <= nr_of_ac_inputs && phase <= nr_of_phases && service_items.contains(path);
                entities.emplace(path, std::move(entity));
            }
        }

        if (std::string path = input_path_prefix + "/Type"; service_items.contains(path))
            addStringDiagnostic(path, input_name_prefix + " Type", "mdi:import", AC_INPUT_TYPE_VALUE_TEMPLATE);

        if (std::string path = input_path_prefix + "/CurrentLimitIsAdjustable"; service_items.contains(path))
            addStringDiagnostic(path, input_name_prefix + " Current Limit Adjustable", "mdi:speedometer", YES_NO_VALUE_TEMPLATE);

        { // Current Limit - /Ac/In/CurrentLimit
            std::string path = input_path_prefix + "/CurrentLimit";
            HAEntityConfig entity;
            entity.name = input_name_prefix + " Current Limit";
            entity.platform = "number";
            entity.device_class = "current";
            entity.state_class = "measurement";
            entity.unit_of_measurement = "A";
            entity.icon = "mdi:speedometer";
            entity.suggested_display_precision = 2;
            entity.command_topic = path;
            entity.mode = "box";
            entity.enabled = input <= nr_of_ac_inputs && service_items.contains(path);
            entities.emplace(path, std::move(entity));
        }
    }

    for (int phase = 1; phase <= max_nr_of_phases; ++phase) { // /Ac/Out/Lx/{P,V,I,F}, /Ac/Out/NominalInverterPower
        std::string phase_path_prefix = "/Ac/Out/L" + std::to_string(phase);
        std::string phase_name_prefix = "AC Out L" + std::to_string(phase);
        { // Power - /Ac/Out/Lx/P
            std::string path = phase_path_prefix + "/P";
            HAEntityConfig entity;
            entity.name = phase_name_prefix + " Power";
            entity.device_class = "power";
            entity.state_class = "measurement";
            entity.unit_of_measurement = "W";
            entity.icon = "mdi:home-lightning-bolt";
            entity.suggested_display_precision = 1;
            entity.enabled = phase <= nr_of_phases && service_items.contains(path);
            entities.emplace(path, std::move(entity));
        }

        { // Voltage - /Ac/Out/Lx/V
            std::string path = phase_path_prefix + "/V";
            HAEntityConfig entity;
            entity.name = phase_name_prefix + " Voltage";
            entity.device_class = "voltage";
            entity.state_class = "measurement";
            entity.unit_of_measurement = "V";
            entity.icon = "mdi:home-lightning-bolt";
            entity.suggested_display_precision = 1;
            entity.enabled = phase <= nr_of_phases && service_items.contains(path);
            entities.emplace(path, std::move(entity));
        }

        { // Current - /Ac/Out/Lx/I
            std::string current_path = phase_path_prefix + "/I";
            HAEntityConfig entity;
            entity.name = phase_name_prefix + " Current";
            entity.device_class = "current";
            entity.state_class = "measurement";
            entity.unit_of_measurement = "A";
            entity.icon = "mdi:home-lightning-bolt";
            entity.suggested_display_precision = 2;
            entity.enabled = phase <= nr_of_phases && service_items.contains(current_path);
            entities.emplace(current_path, std::move(entity));
        }

        { // Frequency - /Ac/Out/Lx/F
            std::string path = phase_path_prefix + "/F";
            HAEntityConfig entity;
            entity.name = phase_name_prefix + " Frequency";
            entity.device_class = "frequency";
            entity.state_class = "measurement";
            entity.unit_of_measurement = "Hz";
            entity.icon = "mdi:sine-wave";
            entity.suggested_display_precision = 2;
            entity.enabled = phase <= nr_of_phases && service_items.contains(path);
            entities.emplace(path, std::move(entity));
        }

        addNumericDiagnostic(phase_path_prefix + "/NominalInverterPower", phase_name_prefix + " Nominal inverter power", "mdi:home-lightning-bolt", 0);
    }

    if (std::string path = "/Pv/Disable"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "PV Disable";
        entity.platform = "switch";
        entity.state_class = "measurement";
        entity.device_class = "switch";
        entity.command_topic = path;
        entity.payload_on = "{\"value\": 1}";
        entity.payload_off = "{\"value\": 0}";
        entity.value_template = ON_OFF_VALUE_TEMPLATE;
        entity.state_on = "ON";
        entity.state_off = "OFF";
        entity.icon = "mdi:solar-power";
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Ess/AcPowerSetpoint"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "ESS AC Power Setpoint";
        entity.device_class = "power";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "W";
        entity.icon = "mdi:transmission-tower";
        entity.suggested_display_precision = 0;
        entities.emplace(path, std::move(entity));
    }

    if (std::string ess_disablefeedin_path = "/Ess/DisableFeedIn"; service_items.contains(ess_disablefeedin_path)) {
        HAEntityConfig entity;
        entity.name = "ESS Disable Feed-In";
        entity.platform = "switch";
        entity.state_class = "measurement";
        entity.device_class = "switch";
        entity.command_topic = ess_disablefeedin_path;
        entity.payload_on = "{\"value\": 1}";
        entity.payload_off = "{\"value\": 0}";
        entity.value_template = ON_OFF_VALUE_TEMPLATE;
        entity.state_on = "ON";
        entity.state_off = "OFF";
        entity.icon = "mdi:transmission-tower-off";
        entities.emplace(ess_disablefeedin_path, std::move(entity));
    }

    if (std::string path = "/Ess/UseInverterPowerSetpoint"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "ESS Use Inverter Power Setpoint";
        entity.platform = "switch";
        entity.state_class = "measurement";
        entity.device_class = "switch";
        entity.command_topic = path;
        entity.payload_on = "{\"value\": 1}";
        entity.payload_off = "{\"value\": 0}";
        entity.value_template = ON_OFF_VALUE_TEMPLATE;
        entity.state_on = "ON";
        entity.state_off = "OFF";
        entity.icon = "mdi:speedometer";
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Ess/InverterPowerSetpPoint"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "ESS Inverter Power Setpoint";
        entity.device_class = "power";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "W";
        entity.icon = "mdi:battery-charging-60";
        entity.suggested_display_precision = 0;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Settings/Ess/MinimumSocLimit"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "ESS Minimum SoC Limit";
        entity.platform = "number";
        entity.device_class = "percentage";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "%";
        entity.icon = "mdi:battery-minus";
        entity.suggested_display_precision = 0;
        entity.command_topic = path;
        entity.min_value = 0;
        entity.max_value = 100;
        entity.mode = "slider";
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Settings/Ess/Mode"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "ESS Mode";
        entity.icon = "mdi:information-outline";
        entity.value_template = ESS_MODE_VALUE_TEMPLATE;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/State"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "AC System State";
        entity.icon = "mdi:power-settings";
        entity.value_template = CHARGER_STATE_VALUE_TEMPLATE;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Ac/ActiveIn/ActiveInput"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Active AC Input";
        entity.icon = "mdi:import";
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/0/Current"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Current";
        entity.device_class = "current";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "A";
        entity.icon = "mdi:current-dc";
        entity.suggested_display_precision = 1;
        entities.emplace(path, std::move(entity));
    }

    if (std::string path = "/Dc/0/Voltage"; service_items.contains(path)) {
        HAEntityConfig entity;
        entity.name = "Battery Voltage";
        entity.device_class = "voltage";
        entity.state_class = "measurement";
        entity.unit_of_measurement = "V";
        entity.icon = "mdi:flash";
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
        entity.suggested_display_precision = 0;
        entities.emplace(path, std::move(entity));
    }

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

    addNumericDiagnostic("/Ac/NumberOfAcInputs", "Number of AC Inputs", "mdi:numeric", 0);
    addNumericDiagnostic("/Ac/NumberOfPhases", "Number of Phases", "mdi:numeric", 0);
    addCommonDiagnostics(service_items);
}

// In the update function, check the number of ac_inputs and phases for changes
bool HomeAssistantDiscovery::AcSystemDevice::update(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                                                    const std::unordered_map<std::string, Item> &changed_items)
{
    static const std::string_view acInPhaseItems[] = {
        "/P",
        "/V",
        "/I",
        "/F",
    };
    static const std::string_view acInItems[] = {
        "/CurrentLimit",
    };
    static const std::string_view acOutPhaseItems[] = {
        "/P",
        "/V",
        "/I",
    };
    bool changed = false;
    int old_nr_of_phases = nr_of_phases;
    int old_nr_of_ac_inputs = nr_of_ac_inputs;
    if (changed_items.contains("/Ac/NumberOfPhases") || changed_items.contains("/Ac/NumberOfAcInputs")) {
        const auto& service_items = all_items.at(service);
        calcNrOfPhases(service_items);
        calcNrOfAcInputs(service_items);

        changed = old_nr_of_phases != nr_of_phases || old_nr_of_ac_inputs != nr_of_ac_inputs;

        if (changed) {
            // Go over all phase items and update enabled
            for (int input = 1; input <= max_nr_of_ac_inputs; input++) {
                std::string input_path_prefix = "/Ac/In/" + std::to_string(input);
                for (int phase = 1; phase <= max_nr_of_phases; phase++) {
                    std::string phase_path_prefix = input_path_prefix + "/L" + std::to_string(phase);
                    for (auto phaseItem : acInPhaseItems) {
                        std::string path = phase_path_prefix + std::string(phaseItem);
                        entities.at(path).enabled = input <= nr_of_ac_inputs && phase <= nr_of_phases && service_items.contains(path);
                    }
                }
                for (auto inputItem : acInItems) {
                    std::string path = input_path_prefix + std::string(inputItem);
                    entities.at(path).enabled = input <= nr_of_ac_inputs && service_items.contains(path);
                }
            }
            for (int phase = 1; phase <= max_nr_of_phases; phase++) {
                std::string phase_path_prefix = "/Ac/Out/L" + std::to_string(phase);
                for (auto phaseItem : acOutPhaseItems) {
                    std::string path = phase_path_prefix + std::string(phaseItem);
                    entities.at(path).enabled = phase <= nr_of_phases && service_items.contains(path);
                }
            }
        }
    }
    return changed;
}

std::pair<std::string, std::string> HomeAssistantDiscovery::AcSystemDevice::getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>>& all_items)
{
    const auto& items = all_items.at(service);
    std::string name = getItemText(items, {"/CustomName"});
    if (name.empty()) {
        name = "AC System " + std::string(short_service_name.instance());
    }

    return {name, "AC System service"};
}
