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
    for (int input = 1; input <= max_nr_of_ac_inputs; ++input) {
        std::string input_path_prefix = "/Ac/In/" + std::to_string(input);
        std::string input_name_prefix = "AC In " + std::to_string(input);
        for (int phase = 1; phase <= max_nr_of_phases; ++phase) {
            std::string phase_path_prefix = input_path_prefix + "/L" + std::to_string(phase);
            std::string phase_name_prefix = input_name_prefix + " L" + std::to_string(phase);
            {
                std::string power_path = phase_path_prefix + "/P";
                HAEntityConfig power_sensor;
                power_sensor.name = phase_name_prefix + " Power";
                power_sensor.device_class = "power";
                power_sensor.state_class = "measurement";
                power_sensor.unit_of_measurement = "W";
                power_sensor.icon = "mdi:transmission-tower";
                power_sensor.suggested_display_precision = 1;
                power_sensor.enabled = input <= nr_of_ac_inputs && phase <= nr_of_phases && service_items.contains(power_path);
                entities.emplace(power_path, std::move(power_sensor));
            }
            {
                std::string voltage_path = phase_path_prefix + "/V";
                HAEntityConfig voltage_sensor;
                voltage_sensor.name = phase_name_prefix + " Voltage";
                voltage_sensor.device_class = "voltage";
                voltage_sensor.state_class = "measurement";
                voltage_sensor.unit_of_measurement = "V";
                voltage_sensor.icon = "mdi:flash";
                voltage_sensor.suggested_display_precision = 1;
                voltage_sensor.enabled = input <= nr_of_ac_inputs && phase <= nr_of_phases && service_items.contains(voltage_path);
                entities.emplace(voltage_path, std::move(voltage_sensor));
            }
            {
                std::string current_path = phase_path_prefix + "/I";
                HAEntityConfig current_sensor;
                current_sensor.name = phase_name_prefix + " Current";
                current_sensor.device_class = "current";
                current_sensor.state_class = "measurement";
                current_sensor.unit_of_measurement = "A";
                current_sensor.icon = "mdi:current-ac";
                current_sensor.suggested_display_precision = 2;
                current_sensor.enabled = input <= nr_of_ac_inputs && phase <= nr_of_phases && service_items.contains(current_path);
                entities.emplace(current_path, std::move(current_sensor));
            }
            {
                std::string frequency_path = phase_path_prefix + "/F";
                HAEntityConfig frequency_sensor;
                frequency_sensor.name = phase_name_prefix + " Frequency";
                frequency_sensor.device_class = "frequency";
                frequency_sensor.state_class = "measurement";
                frequency_sensor.unit_of_measurement = "Hz";
                frequency_sensor.icon = "mdi:sine-wave";
                frequency_sensor.suggested_display_precision = 2;
                frequency_sensor.enabled = input <= nr_of_ac_inputs && phase <= nr_of_phases && service_items.contains(frequency_path);
                entities.emplace(frequency_path, std::move(frequency_sensor));
            }
        }
        {
            std::string type_path = input_path_prefix + "/Type";
            if (service_items.contains(type_path))
                addStringDiagnostic(type_path, input_name_prefix + " Type", "mdi:import", AC_INPUT_TYPE_VALUE_TEMPLATE);
        }
        {
            std::string current_limit_is_adjustable_path = input_path_prefix + "/CurrentLimitIsAdjustable";
            if (service_items.contains(current_limit_is_adjustable_path))
                addStringDiagnostic(current_limit_is_adjustable_path, input_name_prefix + " Current Limit Adjustable", "mdi:speedometer", YES_NO_VALUE_TEMPLATE);
        }
        {
            std::string current_limit_path = input_path_prefix + "/CurrentLimit";
            HAEntityConfig current_limit_sensor;
            current_limit_sensor.name = input_name_prefix + " Current Limit";
            current_limit_sensor.platform = "number";
            current_limit_sensor.device_class = "current";
            current_limit_sensor.state_class = "measurement";
            current_limit_sensor.unit_of_measurement = "A";
            current_limit_sensor.icon = "mdi:speedometer";
            current_limit_sensor.suggested_display_precision = 2;
            current_limit_sensor.command_topic = current_limit_path;
            current_limit_sensor.mode = "box";
            current_limit_sensor.enabled = input <= nr_of_ac_inputs && service_items.contains(current_limit_path);
            entities.emplace(current_limit_path, std::move(current_limit_sensor));
        }
    }

    for (int phase = 1; phase <= max_nr_of_phases; ++phase) {
        std::string phase_path_prefix = "/Ac/Out/L" + std::to_string(phase);
        std::string phase_name_prefix = "AC Out L" + std::to_string(phase);
        {
            std::string power_path = phase_path_prefix + "/P";
            HAEntityConfig power_sensor;
            power_sensor.name = phase_name_prefix + " Power";
            power_sensor.device_class = "power";
            power_sensor.state_class = "measurement";
            power_sensor.unit_of_measurement = "W";
            power_sensor.icon = "mdi:home-lightning-bolt";
            power_sensor.suggested_display_precision = 1;
            power_sensor.enabled = phase <= nr_of_phases && service_items.contains(power_path);
            entities.emplace(power_path, std::move(power_sensor));
        }
        {
            std::string voltage_path = phase_path_prefix + "/V";
            HAEntityConfig voltage_sensor;
            voltage_sensor.name = phase_name_prefix + " Voltage";
            voltage_sensor.device_class = "voltage";
            voltage_sensor.state_class = "measurement";
            voltage_sensor.unit_of_measurement = "V";
            voltage_sensor.icon = "mdi:home-lightning-bolt";
            voltage_sensor.suggested_display_precision = 1;
            voltage_sensor.enabled = phase <= nr_of_phases && service_items.contains(voltage_path);
            entities.emplace(voltage_path, std::move(voltage_sensor));
        }
        {
            std::string current_path = phase_path_prefix + "/I";
            HAEntityConfig current_sensor;
            current_sensor.name = phase_name_prefix + " Current";
            current_sensor.device_class = "current";
            current_sensor.state_class = "measurement";
            current_sensor.unit_of_measurement = "A";
            current_sensor.icon = "mdi:home-lightning-bolt";
            current_sensor.suggested_display_precision = 2;
            current_sensor.enabled = phase <= nr_of_phases && service_items.contains(current_path);
            entities.emplace(current_path, std::move(current_sensor));
        }
        {
            std::string frequency_path = phase_path_prefix + "/F";
            HAEntityConfig frequency_sensor;
            frequency_sensor.name = phase_name_prefix + " Frequency";
            frequency_sensor.device_class = "frequency";
            frequency_sensor.state_class = "measurement";
            frequency_sensor.unit_of_measurement = "Hz";
            frequency_sensor.icon = "mdi:sine-wave";
            frequency_sensor.suggested_display_precision = 2;
            frequency_sensor.enabled = phase <= nr_of_phases && service_items.contains(frequency_path);
            entities.emplace(frequency_path, std::move(frequency_sensor));
        }
        addNumericDiagnostic(phase_path_prefix + "/NominalInverterPower", phase_name_prefix + " Nominal inverter power", "mdi:home-lightning-bolt", 0);
    }

    {
        std::string pv_disable_path = "/Pv/Disable";
        HAEntityConfig pv_disable_switch;
        pv_disable_switch.name = "PV Disable";
        pv_disable_switch.platform = "switch";
        pv_disable_switch.state_class = "measurement";
        pv_disable_switch.device_class = "switch";
        pv_disable_switch.command_topic = pv_disable_path;
        pv_disable_switch.payload_on = "{\"value\": 1}";
        pv_disable_switch.payload_off = "{\"value\": 0}";
        pv_disable_switch.value_template = ON_OFF_VALUE_TEMPLATE;
        pv_disable_switch.state_on = "ON";
        pv_disable_switch.state_off = "OFF";
        pv_disable_switch.icon = "mdi:solar-power";
        pv_disable_switch.enabled = service_items.contains(pv_disable_path);
        entities.emplace(pv_disable_path, std::move(pv_disable_switch));
    }
    {
        std::string ess_acpowersetpoint_path = "/Ess/AcPowerSetpoint";
        HAEntityConfig ess_acpowersetpoint_sensor;
        ess_acpowersetpoint_sensor.name = "ESS AC Power Setpoint";
        ess_acpowersetpoint_sensor.device_class = "power";
        ess_acpowersetpoint_sensor.state_class = "measurement";
        ess_acpowersetpoint_sensor.unit_of_measurement = "W";
        ess_acpowersetpoint_sensor.icon = "mdi:transmission-tower";
        ess_acpowersetpoint_sensor.suggested_display_precision = 0;
        ess_acpowersetpoint_sensor.enabled = service_items.contains(ess_acpowersetpoint_path);
        entities.emplace(ess_acpowersetpoint_path, std::move(ess_acpowersetpoint_sensor));
    }
    {
        std::string ess_disablefeedin_path = "/Ess/DisableFeedIn";
        HAEntityConfig ess_disablefeedin_switch;
        ess_disablefeedin_switch.name = "ESS Disable Feed-In";
        ess_disablefeedin_switch.platform = "switch";
        ess_disablefeedin_switch.state_class = "measurement";
        ess_disablefeedin_switch.device_class = "switch";
        ess_disablefeedin_switch.command_topic = ess_disablefeedin_path;
        ess_disablefeedin_switch.payload_on = "{\"value\": 1}";
        ess_disablefeedin_switch.payload_off = "{\"value\": 0}";
        ess_disablefeedin_switch.value_template = ON_OFF_VALUE_TEMPLATE;
        ess_disablefeedin_switch.state_on = "ON";
        ess_disablefeedin_switch.state_off = "OFF";
        ess_disablefeedin_switch.icon = "mdi:transmission-tower-off";
        ess_disablefeedin_switch.enabled = service_items.contains(ess_disablefeedin_path);
        entities.emplace(ess_disablefeedin_path, std::move(ess_disablefeedin_switch));
    }
    {
        std::string ess_useinverterpowersetpoint_path = "/Ess/UseInverterPowerSetpoint";
        HAEntityConfig ess_useinverterpowersetpoint_switch;
        ess_useinverterpowersetpoint_switch.name = "ESS Use Inverter Power Setpoint";
        ess_useinverterpowersetpoint_switch.platform = "switch";
        ess_useinverterpowersetpoint_switch.state_class = "measurement";
        ess_useinverterpowersetpoint_switch.device_class = "switch";
        ess_useinverterpowersetpoint_switch.command_topic = ess_useinverterpowersetpoint_path;
        ess_useinverterpowersetpoint_switch.payload_on = "{\"value\": 1}";
        ess_useinverterpowersetpoint_switch.payload_off = "{\"value\": 0}";
        ess_useinverterpowersetpoint_switch.value_template = ON_OFF_VALUE_TEMPLATE;
        ess_useinverterpowersetpoint_switch.state_on = "ON";
        ess_useinverterpowersetpoint_switch.state_off = "OFF";
        ess_useinverterpowersetpoint_switch.icon = "mdi:speedometer";
        ess_useinverterpowersetpoint_switch.enabled = service_items.contains(ess_useinverterpowersetpoint_path);
        entities.emplace(ess_useinverterpowersetpoint_path, std::move(ess_useinverterpowersetpoint_switch));
    }
    {
        std::string ess_inverterpowersetpoint_path = "/Ess/InverterPowerSetpPoint";
        HAEntityConfig ess_inverterpowersetpoint_sensor;
        ess_inverterpowersetpoint_sensor.name = "ESS Inverter Power Setpoint";
        ess_inverterpowersetpoint_sensor.device_class = "power";
        ess_inverterpowersetpoint_sensor.state_class = "measurement";
        ess_inverterpowersetpoint_sensor.unit_of_measurement = "W";
        ess_inverterpowersetpoint_sensor.icon = "mdi:battery-charging-60";
        ess_inverterpowersetpoint_sensor.suggested_display_precision = 0;
        ess_inverterpowersetpoint_sensor.enabled = service_items.contains(ess_inverterpowersetpoint_path);
        entities.emplace(ess_inverterpowersetpoint_path, std::move(ess_inverterpowersetpoint_sensor));
    }
    {
        std::string settings_ess_minimumsoclimit_path = "/Settings/Ess/MinimumSocLimit";
        HAEntityConfig settings_ess_minimumsoclimit_sensor;
        settings_ess_minimumsoclimit_sensor.name = "ESS Minimum SoC Limit";
        settings_ess_minimumsoclimit_sensor.platform = "number";
        settings_ess_minimumsoclimit_sensor.device_class = "percentage";
        settings_ess_minimumsoclimit_sensor.state_class = "measurement";
        settings_ess_minimumsoclimit_sensor.unit_of_measurement = "%";
        settings_ess_minimumsoclimit_sensor.icon = "mdi:battery-minus";
        settings_ess_minimumsoclimit_sensor.suggested_display_precision = 0;
        settings_ess_minimumsoclimit_sensor.command_topic = settings_ess_minimumsoclimit_path;
        settings_ess_minimumsoclimit_sensor.min_value = 0;
        settings_ess_minimumsoclimit_sensor.max_value = 100;
        settings_ess_minimumsoclimit_sensor.mode = "slider";
        settings_ess_minimumsoclimit_sensor.enabled = service_items.contains(settings_ess_minimumsoclimit_path);
        entities.emplace(settings_ess_minimumsoclimit_path, std::move(settings_ess_minimumsoclimit_sensor));
    }
    {
        std::string settings_ess_mode_path = "/Settings/Ess/Mode";
        HAEntityConfig settings_ess_mode_sensor;
        settings_ess_mode_sensor.name = "ESS Mode";
        settings_ess_mode_sensor.icon = "mdi:information-outline";
        settings_ess_mode_sensor.value_template = ESS_MODE_VALUE_TEMPLATE;
        settings_ess_mode_sensor.enabled = service_items.contains(settings_ess_mode_path);
        entities.emplace(settings_ess_mode_path, std::move(settings_ess_mode_sensor));
    }
    {
        std::string state_path = "/State";
        HAEntityConfig state_sensor;
        state_sensor.name = "AC System State";
        state_sensor.icon = "mdi:power-settings";
        state_sensor.value_template = CHARGER_STATE_VALUE_TEMPLATE;
        state_sensor.enabled = service_items.contains(state_path);
        entities.emplace(state_path, std::move(state_sensor));
    }
    {
        std::string ac_activein_activeinput_path = "/Ac/ActiveIn/ActiveInput";
        HAEntityConfig ac_activein_activeinput_sensor;
        ac_activein_activeinput_sensor.name = "Active AC Input";
        ac_activein_activeinput_sensor.icon = "mdi:import";
        ac_activein_activeinput_sensor.enabled = service_items.contains(ac_activein_activeinput_path);
        entities.emplace(ac_activein_activeinput_path, std::move(ac_activein_activeinput_sensor));
    }
    {
        std::string current_path = "/Dc/0/Current";
        HAEntityConfig current_sensor;
        current_sensor.name = "Battery Current";
        current_sensor.device_class = "current";
        current_sensor.state_class = "measurement";
        current_sensor.unit_of_measurement = "A";
        current_sensor.icon = "mdi:current-dc";
        current_sensor.suggested_display_precision = 1;
        current_sensor.enabled = service_items.contains(current_path);
        entities.emplace(current_path, std::move(current_sensor));
    }
    {
        std::string voltage_path = "/Dc/0/Voltage";
        HAEntityConfig voltage_sensor;
        voltage_sensor.name = "Battery Voltage";
        voltage_sensor.device_class = "voltage";
        voltage_sensor.state_class = "measurement";
        voltage_sensor.unit_of_measurement = "V";
        voltage_sensor.icon = "mdi:flash";
        voltage_sensor.suggested_display_precision = 2;
        voltage_sensor.enabled = service_items.contains(voltage_path);
        entities.emplace(voltage_path, std::move(voltage_sensor));
    }
    {
        std::string power_path = "/Dc/0/Power";
        HAEntityConfig power_sensor;
        power_sensor.name = "Battery Power";
        power_sensor.device_class = "power";
        power_sensor.state_class = "measurement";
        power_sensor.unit_of_measurement = "W";
        power_sensor.icon = "mdi:flash";
        power_sensor.suggested_display_precision = 0;
        power_sensor.enabled = service_items.contains(power_path);
        entities.emplace(power_path, std::move(power_sensor));
    }
    {
        std::string soc_path = "/Soc";
        HAEntityConfig soc_sensor;
        soc_sensor.name = "State of Charge";
        soc_sensor.state_class = "measurement";
        soc_sensor.device_class = "battery";
        soc_sensor.unit_of_measurement = "%";
        soc_sensor.icon = "mdi:battery";
        soc_sensor.suggested_display_precision = 1;
        soc_sensor.enabled = service_items.contains(soc_path);
        entities.emplace(soc_path, std::move(soc_sensor));
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
