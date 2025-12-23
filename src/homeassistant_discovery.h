#ifndef HOMEASSISTANT_DISCOVERY_H
#define HOMEASSISTANT_DISCOVERY_H

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include "types.h"
#include "shortservicename.h"
#include "vendor/json.hpp"

namespace dbus_flashmq
{

/**
 * @brief Information about a Home Assistant device
 */
struct HADevice
{
    std::string name;
    std::string manufacturer = "Victron Energy";
    std::string model;
    std::string identifiers;
    std::string hw_version;
    std::string sw_version;
    std::string serial_number;
    std::string configuration_url;
    std::string via_device;

    nlohmann::json toJson() const;
};

/**
 * @brief Home Assistant entity configuration
 */
struct HAEntityConfig
{
    static const std::string_view DEFAULT_VALUE_TEMPLATE;
    static const std::string_view DEFAULT_COMMAND_TEMPLATE;

    bool enabled = true;
    std::string unique_id;
    std::string platform = "sensor"; // sensor, switch, number, binary_sensor, etc.

    std::string name;
    std::string state_topic;
    std::string device_class; // temperature, voltage, current, etc.
    std::string state_class; // measurement, total, total_increasing
    std::string unit_of_measurement;
    std::string icon;
    std::string entity_category; // config, diagnostic
    bool enabled_by_default = true;
    int suggested_display_precision = -1;
    std::string value_template;
    std::string state_on;
    std::string state_off;

    std::string command_topic;
    std::string command_template;
    std::string payload_on;
    std::string payload_off;
    bool optimistic = false;
    int min_value = 0;
    int max_value = 0;
    std::string mode;

    nlohmann::json toJson() const;
};

/**
 * @brief Home Assistant Discovery with generic sensor support
 */
class HomeAssistantDiscovery
{
private:
    struct DeviceData {
        virtual ~DeviceData() = default;

        std::string service;
        ShortServiceName short_service_name;
        HADevice ha_device;
        size_t item_count = 0;
        std::unordered_map<std::string, HAEntityConfig> entities; // dbus_path -> entity config
        std::unordered_set<std::string> name_paths{"/CustomName"};
        std::string discovery_topic;
        std::string cached_payload;

        nlohmann::json toJson() const;

        void fillHADevice(const std::string &vrm_id,
                          const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items);
        void fillHAEntities(const std::string &vrm_id,
                            const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items);
        void processChangedItems(const std::string &vrm_id,
                                 const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                                 const std::string& service,
                                 const std::unordered_map<std::string, Item> &changed_items);

        virtual std::pair<std::string, std::string> getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) = 0;
        virtual void addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) = 0;
        virtual bool update(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                            const std::unordered_map<std::string, Item> &changed_items) { return false; };

        void addCommonDiagnostics(const std::unordered_map<std::string, Item> &service_items);
        void addNumericDiagnostic(std::string_view dbus_path,
                                  std::string_view name,
                                  std::string_view icon = "",
                                  int suggested_display_precision = -1,
                                  std::string_view device_class = "",
                                  std::string_view unit_of_measurement = "",
                                  std::string_view value_template = "");
        void addStringDiagnostic(std::string_view dbus_path,
                                 std::string_view name,
                                 std::string_view icon = "",
                                 std::string_view value_template = "");
    };
    static std::unordered_map<std::string_view, std::function<std::unique_ptr<DeviceData>()>> device_factory_functions;

    struct TemperatureDevice : DeviceData {
        std::pair<std::string, std::string> getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        void addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
    };
    struct BatteryDevice : DeviceData {
        bool has_mid_voltage = false;
        bool has_temperature = false;
        bool has_starter_voltage = false;

        static bool calcHasMidVoltage(const std::unordered_map<std::string, Item> &service_items);
        static bool calcHasTemperature(const std::unordered_map<std::string, Item> &service_items);
        static bool calcHasStarterVoltage(const std::unordered_map<std::string, Item> &service_items);

        std::pair<std::string, std::string> getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        void addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        bool update(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                    const std::unordered_map<std::string, Item> &changed_items) override;
    };
    struct SolarChargerDevice : DeviceData {
        std::pair<std::string, std::string> getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        void addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
    };
    struct VeBusDevice : DeviceData {
        std::pair<std::string, std::string> getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        void addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
    };
    struct SystemDevice : DeviceData {
        std::pair<std::string, std::string> getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        void addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
    };
    struct TankDevice : DeviceData {
        TankDevice() : DeviceData() { name_paths.insert("/FluidType"); }
        std::pair<std::string, std::string> getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        void addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
    };
    struct GridMeterDevice : DeviceData {
        static const int max_nr_of_phases = 3;
        int nr_of_phases = 0;
        void calcNrOfPhases(const std::unordered_map<std::string, Item> &service_items);
        std::pair<std::string, std::string> getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        void addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        bool update(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                    const std::unordered_map<std::string, Item> &changed_items) override;
    };
    struct SwitchDevice : DeviceData {
        struct CustomNameInfo {
            std::string custom_name_path;
            std::string name_path;
            HAEntityConfig *state_entity_config = nullptr;
            HAEntityConfig *dimming_entity_config = nullptr;
        };
        std::unordered_map<std::string, CustomNameInfo> customname_paths;
        std::pair<std::string, std::string> getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        void addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        bool update(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                    const std::unordered_map<std::string, Item> &changed_items) override;
    };
    struct MeteoDevice : DeviceData {
        std::pair<std::string, std::string> getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        void addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
    };
    struct ChargerDevice : DeviceData {
        std::pair<std::string, std::string> getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        void addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
    };
    struct DcDcDevice : DeviceData {
        std::pair<std::string, std::string> getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        void addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
    };
    struct DigitalInputDevice : DeviceData {
        std::pair<std::string, std::string> getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        void addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
    };
    struct AcSystemDevice : DeviceData {
        static const int max_nr_of_phases = 3;
        int nr_of_phases = 0;
        static const int max_nr_of_ac_inputs = 3;
        int nr_of_ac_inputs = 0;
        void calcNrOfPhases(const std::unordered_map<std::string, Item> &service_items);
        void calcNrOfAcInputs(const std::unordered_map<std::string, Item> &service_items);
        std::pair<std::string, std::string> getNameAndModel(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        void addEntities(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) override;
        bool update(const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                    const std::unordered_map<std::string, Item> &changed_items) override;
    };

    static const std::string_view CHARGER_MODE_VALUE_TEMPLATE;
    static const std::string_view CHARGER_STATE_VALUE_TEMPLATE;
    static const std::string_view VICTRON_VERSION_VALUE_TEMPLATE;
    static const std::string_view DEVICE_OFF_REASON_VALUE_TEMPLATE;
    static const std::string_view FLUID_TYPE_VALUE_TEMPLATE;
    static const std::string_view ON_OFF_VALUE_TEMPLATE;
    static const std::string_view DEVICEFUNCTION_VALUE_TEMPLATE;
    static const std::string_view GRID_METER_POSITION_VALUE_TEMPLATE;
    static const std::string_view MPP_OPERATION_MODE_VALUE_TEMPLATE;
    static const std::string_view SWITCH_STATE_VALUE_TEMPLATE;
    static const std::string_view AC_INPUT_TYPE_VALUE_TEMPLATE;
    static const std::string_view YES_NO_VALUE_TEMPLATE;
    static const std::string_view ESS_MODE_VALUE_TEMPLATE;

    std::string vrm_id;
    // std::unordered_set<std::string> enabled_services;
    std::unordered_map<std::string, std::unique_ptr<DeviceData>> devices; // service -> DeviceData

    // Helper methods
    bool isServiceEnabled(std::string_view service_type) const;
    std::string createDeviceDiscoveryTopic(const std::string &device_id) const;
    std::unique_ptr<HomeAssistantDiscovery::DeviceData> createDeviceData(const std::string &service,
                                                                         const ShortServiceName &short_service_name,
                                                                         const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items) const;
    static VeVariant getItemValue(const std::unordered_map<std::string, Item> &service_items, std::string_view dbus_path);
    static std::string getItemText(const std::unordered_map<std::string, Item> &service_items,
                                   std::initializer_list<std::string_view> dbus_paths);
    static std::vector<std::string_view> splitPath(std::string_view dbus_path);
    static std::string toIdentifier(std::string_view input);
public:
    HomeAssistantDiscovery();
    ~HomeAssistantDiscovery() = default;

    // Configuration
    void setVrmId(const std::string &vrm_id);

    // Core sensor support
    void publishSensorEntitiesWithItems(const std::string &service,
                                        const ShortServiceName &short_service_name,
                                        const std::unordered_map<std::string, std::unordered_map<std::string, Item>> &all_items,
                                        const std::unordered_map<std::string, Item> &changed_items);

    // Bulk operations for service lifecycle
    void publishAllConfigs() const;
    void removeAllSensorsForService(const std::string &service);
    void clearAll();
};

}

#endif // HOMEASSISTANT_DISCOVERY_H
