#include "guicustomizations.h"

#include <filesystem>
#include <fstream>
#include <chrono>
#include "vendor/json.hpp"
#include "vendor/flashmq_plugin.h"
#include "utils.h"

dbus_flashmq::GuiCustomizationEntry::GuiCustomizationEntry(const std::filesystem::path &dir_path) :
    m_app_basename(dir_path.filename()),
    m_blob_file_path(get_blob_file_path(dir_path, m_app_basename)),
    m_sha256_hex(dbus_flashmq::hash_file(m_blob_file_path)),
    m_chunks(calculate_chunks(m_blob_file_path))
{

}

std::filesystem::path dbus_flashmq::GuiCustomizationEntry::get_blob_file_path(const std::filesystem::path &dir, std::string app_basename)
{
    app_basename.append(".json");
    std::filesystem::path result(dir / "gui-v2" / app_basename);
    return result;
}

std::vector<dbus_flashmq::GuiCustomizationChunk> dbus_flashmq::GuiCustomizationEntry::calculate_chunks(const std::filesystem::path &blob_file_path)
{
    if (!(std::filesystem::exists(blob_file_path) && std::filesystem::is_regular_file(blob_file_path)))
    {
        std::string err("Can't calculate chuks for ");
        err.append(blob_file_path);
        throw std::runtime_error(err);
    }

    const uintmax_t total_size {std::filesystem::file_size(blob_file_path)};
    uintmax_t pos = 0;

    std::vector<GuiCustomizationChunk> result;

    while (pos < total_size)
    {
        const uintmax_t total_left { total_size - pos };
        const uintmax_t cur_chunk_size { std::min<uintmax_t>(total_left, 4096)};

        auto &x = result.emplace_back();
        x.blob_file_path = blob_file_path;
        x.full_file_size = total_size;
        x.offset = pos;
        x.size = cur_chunk_size;

        pos += cur_chunk_size;
    }

    return result;
}

void dbus_flashmq::GuiCustomizations::scan()
{
    try
    {
        scan_private();
    }
    catch (std::exception &ex)
    {
        flashmq_logf(LOG_ERROR, "Error in GuiCustomizations::scan: %s", ex.what());
    }
}

void dbus_flashmq::GuiCustomizations::scan_private()
{
    if (!std::filesystem::exists(apps_path))
    {
        m_apps_cache.reset();
        m_apps_cache_hash.reset();
        return;
    }

    const auto real_hash = get_cache_key();

    if (m_apps_cache_hash == real_hash)
        return;

    m_apps_cache.emplace();

    std::filesystem::path appdir = apps_path;

    for (const auto &entry : std::filesystem::directory_iterator(appdir))
    {
        try
        {
            GuiCustomizationEntry app(entry);
            m_apps_cache.value().try_emplace(app.m_app_basename, app);
        }
        catch(std::exception &ex)
        {
            flashmq_logf(LOG_ERROR, "Error indexing app '%s': %s", entry.path().filename().c_str(), ex.what());
        }
    }

    m_apps_cache_hash = real_hash;
}

std::optional<size_t> dbus_flashmq::GuiCustomizations::get_cache_key()
{
    std::optional<size_t> result;
    const std::string apps_path_string(apps_path);

    try
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(apps_path_string, std::filesystem::directory_options::follow_directory_symlink))
        {
            if (!std::filesystem::is_regular_file(entry.status()))
                continue;

            const auto current_time = std::filesystem::last_write_time(entry);
            auto current_time2 = std::chrono::clock_cast<std::chrono::system_clock>(current_time).time_since_epoch().count();

            const std::string path_as_string = entry.path().string();
            size_t addition = std::hash<std::string>()(path_as_string);
            addition ^= std::hash<decltype(current_time2)>()(current_time2);

            if (!result)
                result.emplace();

            result = result.value() ^ addition;
        }
    }
    catch (std::exception &ex)
    {
        flashmq_logf(LOG_WARNING, "Can't obtain most recent file time in '%s': %s", apps_path_string.c_str(), ex.what());
    }

    return result;
}

void dbus_flashmq::GuiCustomizations::publish_filtered(
        const std::string *read_request, const std::vector<std::string> *read_request_split, const std::string &topic, const std::string &payload)
{
    assert(!read_request_split || read_request_split->at(0) == "R");

    if (read_request && read_request_split && read_request_split->at(0) == "R")
    {
        std::string r_to_n_transform = *read_request;
        r_to_n_transform.at(0) = 'N';

        if (!topic.starts_with(r_to_n_transform))
            return;
    }

    flashmq_publish_message(topic, 0, false, payload);
}

void dbus_flashmq::GuiCustomizations::publish_chunk(const std::string &vrm_id, const std::string &app_name, const unsigned int chunk_no)
{
    const auto &app = m_apps_cache.value().at(app_name);

    if (chunk_no >= app.m_chunks.size())
        return;

    const auto &chunk = app.m_chunks.at(chunk_no);

    const std::string b64 = chunk.get_base64();

    std::ostringstream topic_oss;
    topic_oss << "N/" << vrm_id << "/GuiCustomizations/Apps/" << app_name << "/Chunks/" << chunk_no;
    const std::string topic(topic_oss.str());

    nlohmann::json chunk_info = nlohmann::json::object();
    chunk_info["base64"] = b64;

    flashmq_publish_message(topic, 0, false, chunk_info.dump());
}

void dbus_flashmq::GuiCustomizations::publish_customizations(
        const std::string &vrm_id, const std::string *read_request, const std::vector<std::string> *read_request_split)
{
    try
    {
        scan_private();

        if (!m_apps_cache)
            return;

        if (read_request_split && read_request_split->size() == 7 && read_request_split->at(2) == "GuiCustomizations" && read_request_split->at(5) == "Chunks")
        {
            const unsigned int chunkno{value_to_int_ranged<unsigned int>(read_request_split->at(6))};
            publish_chunk(vrm_id, read_request_split->at(4), chunkno);
            return;
        }

        {
            nlohmann::json apps_list = nlohmann::json::array({});

            for (auto &pair : m_apps_cache.value())
            {
                apps_list.push_back(pair.first);
            }

            std::ostringstream applist_oss;
            applist_oss << "N/" << vrm_id << "/GuiCustomizations/Applist";

            publish_filtered(read_request, read_request_split, applist_oss.str(), apps_list.dump());
        }

        {
            nlohmann::json info = nlohmann::json::object();

            for (auto &pair : m_apps_cache.value())
            {
                std::ostringstream base_path_oss;
                base_path_oss << "N/" << vrm_id << "/GuiCustomizations/Apps/" << pair.first;
                const std::string base_path(base_path_oss.str());
                std::string fetch_prefix(base_path + "/Chunks/");
                fetch_prefix.at(0) = 'R';

                info["sha256"] = pair.second.m_sha256_hex;
                info["chunk_count"] = pair.second.m_chunks.size();
                info["fetch_prefix"] = fetch_prefix;

                publish_filtered(read_request, read_request_split, base_path + "/info", info.dump());
            }
        }
    }
    catch (std::exception &ex)
    {
        flashmq_logf(LOG_ERROR, "Error in GuiCustomizations::publish_customizations: %s", ex.what());
    }
}

std::string dbus_flashmq::GuiCustomizationChunk::get_base64() const
{
    if (size > 1024*1024*10)
        throw std::runtime_error("get_base64 chunk size error");

    if (std::filesystem::file_size(blob_file_path) != full_file_size)
    {
        throw std::runtime_error("get_base64 chunk size error: file size changed on disk?");
    }

    std::ifstream f;
    f.exceptions(std::ios::badbit);
    f.open(blob_file_path, std::ios::binary);

    std::vector<char> buf(static_cast<size_t>(size));
    f.seekg(static_cast<size_t>(offset));
    f.read(buf.data(), static_cast<std::streamsize>(buf.size()));

    if (static_cast<size_t>(f.gcount()) != buf.size())
    {
        throw std::runtime_error("GuiCustomizationChunk::get_base64 error");
    }

    const auto chunk_view = make_string_view(buf, 0u, buf.size());
    const std::string b64 = base64_encode(chunk_view);
    return b64;
}
