#include <boost/filesystem.hpp>
#include "WebServer.hpp"
#include "utility/GetIp.hpp"
#include "utility/FileUnit.hpp"
#include "3rd_party/nlohmann/json.hpp"
#include <log_helper/LogHelper.h>

using nJson = nlohmann::json;
using namespace stark_power_manager;

WebServer::WebServer(uint16_t port)
    : m_port(port)
{
    ECO_INFO("[WebServer] starting on port %u", m_port);
    m_running.store(true);
    m_serverThread = std::thread(&WebServer::RunServer, this);
}

void
WebServer::BroadcastMessageToWebApp(const std::string& message)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    for (auto u : m_users)
    {
        u->send_text(message);
    }
}

void
WebServer::SetBatteryCtrlHandler(std::function<void(const std::string&)> handler)
{
    m_batteryCtrlHandler = std::move(handler);
}

void
WebServer::SetBatteryInfoHandler(std::function<void()> handler)
{
    m_batteryInfoHandler = std::move(handler);
}

void
WebServer::HandleWebAppMsg(const std::string& msg)
{
    try
    {
        nJson j = nJson::parse(msg, nullptr, false);
        if (!j.is_discarded() && j.contains("channel"))
        {
            std::string channel = j["channel"].get<std::string>();
            if (channel == "battery_ctrl" && m_batteryCtrlHandler)
            {
                m_batteryCtrlHandler(msg);
                return;
            }
            if (channel == "battery_get_info" && m_batteryInfoHandler)
            {
                m_batteryInfoHandler();
                return;
            }
        }
    }
    catch (...)
    {
    }
}

void
WebServer::Stop()
{
    if (!m_running.load()) return;
    ECO_DEBUG("[WebServer] stopping...");
    m_app.stop();
    m_running.store(false);
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
    ECO_INFO("[WebServer] stopped");
}

bool
WebServer::IsRunning() const
{
    return m_running.load();
}

void
WebServer::RunServer()
{
    ECO_DEBUG("[WebServer] RunServer");
    crow::mustache::set_base("/");

    /* WebSocket */
    CROW_ROUTE(m_app, "/ws")
        .websocket()
        .onopen([&](crow::websocket::connection& conn) {
            CROW_LOG_INFO << "new websocket connection";
            std::lock_guard<std::mutex> _(m_mtx);
            m_users.insert(&conn);
        })
        .onclose([&](crow::websocket::connection& conn, const std::string& reason) {
            CROW_LOG_INFO << "websocket connection closed: " << reason;
            std::lock_guard<std::mutex> _(m_mtx);
            m_users.erase(&conn);
        })
        .onmessage([&](crow::websocket::connection& /*conn*/, const std::string& data, bool is_binary) {
            if (!is_binary && !data.empty() && data[0] == '{')
            {
                HandleWebAppMsg(data);
            }
        });

    /* HTTP */
    CROW_ROUTE(m_app, "/")
    ([] {
        std::string index_html = WORK_DIR;
        index_html = index_html + "webapp/html/test_utility.html";
        crow::mustache::context ctx;
        return crow::mustache::load(index_html).render();
    });

    CROW_ROUTE(m_app, "/get_ip")
    ([] {
        crow::json::wvalue response;
        response["ip"] = GetIPAddress();
        return crow::response(response);
    });

    m_app.route_dynamic("/<path>")(
        [this](const crow::request& req, crow::response& resp, std::string path) {
            ResponseStaticResource(req, resp, path);
        });

    m_app.port(m_port).run();
    ECO_DEBUG("[WebServer] server thread exit");
    m_running.store(false);
}

void
WebServer::ResponseStaticResource(const crow::request& req, crow::response& resp, const std::string& path)
{
    std::smatch match;
    if (std::regex_match(path, match, std::regex("^.+\\.([A-Za-z][A-Za-z0-9]*)$")))
    {
        std::string fileType = match[1];
        std::transform(fileType.begin(), fileType.end(), fileType.begin(), ::tolower);
        std::string contentType = "text/plain";
        if ("html" == fileType || "htm" == fileType) contentType = "text/html";
        else if ("js" == fileType)  contentType = "application/javascript";
        else if ("css" == fileType) contentType = "text/css";
        else if ("gif" == fileType) contentType = "image/gif";
        else if ("png" == fileType) contentType = "image/png";
        else if ("jpg" == fileType) contentType = "image/jpeg";
        else if ("ico" == fileType) contentType = "image/x-icon";

        std::string fileName = WORK_DIR;
        fileName = fileName + "webapp/" + path;
        if (boost::filesystem::exists(boost::filesystem::path(fileName + ".gz")))
        {
            fileName += ".gz";
            resp.set_header("Content-Encoding", "gzip");
        }
        resp.set_header("Content-Type", contentType);
        std::ifstream in(fileName);
        if (in.is_open())
        {
            std::string buffer{ std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
            resp.write(buffer);
            resp.end();
            return;
        }
    }
    std::string error("Not found ");
    BuildAndSendResponse(resp, 404, error);
}

void
WebServer::BuildAndSendResponse(crow::response& resp, int code, const std::string& message)
{
    resp = crow::response(code);
    std::ostringstream os;
    os << message.length();
    resp.set_header("Content-Length", os.str());
    resp.end(message);
}
