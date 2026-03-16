#ifndef GUICUSTOMIZATIONS_H
#define GUICUSTOMIZATIONS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <optional>

namespace dbus_flashmq
{

constexpr std::string_view apps_path("/data/apps/enabled/");

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

    static std::filesystem::path get_blob_file_path(const std::filesystem::path &dir, std::string app_basename);
    static std::vector<GuiCustomizationChunk> calculate_chunks(const std::filesystem::path &blob_file_path);

public:
    GuiCustomizationEntry(const std::filesystem::path &dir_path);


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
 */
class GuiCustomizations
{
    std::optional<std::unordered_map<std::string, GuiCustomizationEntry>> m_apps_cache;
    std::optional<size_t> m_apps_cache_hash;

    static std::optional<size_t> get_cache_key();
    void scan_private();
    static void publish_filtered(
            const std::string *read_request, const std::vector<std::string> *read_request_split, const std::string &topic, const std::string &payload);
    void publish_chunk(const std::string &vrm_id, const std::string &app_name, const unsigned int chunk_no);
public:
    void scan();
    void publish_customizations(
            const std::string &vrm_id, const std::string *read_request, const std::vector<std::string> *read_request_split);
};



}


#endif // GUICUSTOMIZATIONS_H
