#ifndef GUICUSTOMIZATIONS_H
#define GUICUSTOMIZATIONS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <optional>
#include <set>

namespace dbus_flashmq
{

constexpr std::string_view apps_path("/run/gx-apps/apps/");

struct GuiCustomizationChunk
{
    std::string blob_file_path;
    uintmax_t full_file_size {};
    uintmax_t offset {};
    uintmax_t size {};

    std::string get_base64() const;
};

struct GuiCustomizationEntry
{
    const std::string m_app_basename;
    const std::string m_blob_file_path;
    const std::string m_sha256_hex {};
    const std::vector<GuiCustomizationChunk> m_chunks;

    static std::vector<GuiCustomizationChunk> calculate_chunks(const std::filesystem::path &blob_file_path);

public:
    GuiCustomizationEntry(const std::filesystem::path &blob_file_path);


};

/**
 * @brief The GuiCustomizations class is an interface for loading GUIv2 customizations
 *
 * As an example, see:
 *
 * https://github.com/victronenergy/gui-v2/tree/main/examples/DeviceList
 *
 * It publishes:
 *
 * N/<portalid>/GuiCustomizations/Applist
 *     ["DeviceListExample"]
 * N/<portalid>/GuiCustomizations/Apps/DeviceListExample/info
 *     {
 *         "chunk_count": 2,
 *         "fetch_prefix": "R/<portalid>/GuiCustomizations/Apps/DeviceListExample/Chunks/",
 *         "sha256": "a3de5fdc0975d3d28fec938a3513d4a241b149d5539be354b45f22a29a800909"
 *     }
 *
 *
 * Publishing to the fetch prefix with a chunk numer, like:
 *
 * R/<portalid>/GuiCustomizations/DeviceListExample/Apps/Chunks/0
 *
 * will cause the GX to publish:
 *
 * N/<portalid>/GuiCustomizations/DeviceListExample/Apps/Chunks/0
 *     {"base64":"dV9Ig....p9Cg=="}
 *
 * It also publishes the Custom Branding selected in com.victronenergy.appmanager's /CustomBranding, from
 * <apps_path>/<name>/custom-branding/<name>.json, apart from the plugins. Its chunks are fetched the same way:
 *
 * N/<portalid>/GuiCustomizations/CustomBranding/info
 *     {
 *         "name": "<name>",
 *         "chunk_count": 1,
 *         "fetch_prefix": "R/<portalid>/GuiCustomizations/CustomBranding/Chunks/",
 *         "sha256": "..."
 *     }
 *
 * Nothing is published for it while no Custom Branding is selected. When the Custom Branding stops being available,
 * CustomBranding/info is published once with an empty payload.
 */
class GuiCustomizations
{
    std::optional<std::unordered_map<std::string, GuiCustomizationEntry>> m_apps_cache;
    std::optional<size_t> m_apps_cache_hash;
    std::set<std::string> m_enabled_apps;
    std::string m_custom_branding; // the selected Custom Branding, from the app manager
    std::optional<GuiCustomizationEntry> m_custom_branding_cache;
    std::optional<size_t> m_custom_branding_cache_hash;
    bool m_custom_branding_published = false;

    std::optional<size_t> get_cache_key() const;
    std::optional<std::filesystem::path> get_custom_branding_path() const;
    std::optional<size_t> get_custom_branding_cache_key() const;
    void publish_custom_branding_chunk(const std::string &vrm_id, const unsigned int chunk_no);
    void scan_private();
    static void publish_filtered(
            const std::string *read_request, const std::vector<std::string> *read_request_split, const std::string &topic, const std::string &payload);
    void publish_chunk(const std::string &vrm_id, const std::string &app_name, const unsigned int chunk_no);
public:
    bool set_enabled_apps(const std::set<std::string> &apps);
    bool set_custom_branding(const std::string &name);
    bool scan_custom_branding();
    void publish_custom_branding(const std::string &vrm_id, const std::string *read_request, const std::vector<std::string> *read_request_split);
    void scan();
    void publish_customizations(
            const std::string &vrm_id, const std::string *read_request, const std::vector<std::string> *read_request_split);
};



}


#endif // GUICUSTOMIZATIONS_H
