/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** main.cpp
**
** -------------------------------------------------------------------------*/

#include "open3d/visualization/webrtc_server/WebRTCServer.h"

#include <p2p/base/basic_packet_socket_factory.h>
#include <p2p/base/stun_server.h>
#include <p2p/base/turn_server.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/thread.h>
#include <signal.h>

#include <fstream>
#include <iostream>

#include "open3d/visualization/webrtc_server/HttpServerRequestHandler.h"
#include "open3d/visualization/webrtc_server/PeerConnectionManager.h"

namespace open3d {
namespace visualization {
namespace webrtc_server {

struct WebRTCServer::Impl {
    std::string http_address_;
    std::string web_root_;
    std::function<void(int, double, double)> mouse_button_callback_;
    // TODO: make this and Impl unique_ptr?
    std::shared_ptr<PeerConnectionManager> peer_connection_manager_ = nullptr;
    void Run();
};

WebRTCServer::WebRTCServer(const std::string& http_address,
                           const std::string& web_root)
    : impl_(new WebRTCServer::Impl()) {
    impl_->http_address_ = http_address;
    impl_->web_root_ = web_root;
}

void WebRTCServer::Impl::Run() {
    std::cout << "WebRTCServer::Run()" << std::endl;

    const std::string web_root = web_root_;
    const std::string http_address = http_address_;
    const std::vector<std::string> stun_urls{"stun:stun.l.google.com:19302"};

    // Logging settings.
    // src/rtc_base/logging.h: LS_VERBOSE, LS_ERROR
    rtc::LogMessage::LogToDebug((rtc::LoggingSeverity)rtc::LS_ERROR);
    std::cout << "Logger level:" << rtc::LogMessage::GetLogToDebug()
              << std::endl;
    rtc::LogMessage::LogTimestamps();
    rtc::LogMessage::LogThreads();

    // WebRTC server (a PeerConnectionManager).
    rtc::Thread* thread = rtc::Thread::Current();
    rtc::InitializeSSL();
    std::list<std::string> ice_servers(stun_urls.begin(), stun_urls.end());
    Json::Value config;

    config["urls"]["Bunny"]["video"] =
            "file:///home/yixing/repo/webrtc-streamer/html/"
            "Big_Buck_Bunny_360_10s_1MB.webm";
    peer_connection_manager_ = std::make_shared<PeerConnectionManager>(
            ice_servers, config["urls"], ".*", "");
    if (peer_connection_manager_->InitializePeerConnection()) {
        std::cout << "InitializePeerConnection() succeeded." << std::endl;
    } else {
        throw std::runtime_error("InitializePeerConnection() failed.");
    }

    // TODO: fix me.
    // https://stackoverflow.com/a/20291676/1255535.
    // https://stackoverflow.com/q/7852101/1255535.
    // auto signal_handler = [this](int n) {
    //     printf("SIGINT\n");
    //     // delete need thread still running
    //     peer_connection_manager_ = nullptr;
    //     rtc::Thread::Current()->Quit();
    // };

    // CivetWeb http server.
    std::vector<std::string> options;
    options.push_back("document_root");
    options.push_back(web_root);
    options.push_back("enable_directory_listing");
    options.push_back("no");
    options.push_back("additional_header");
    options.push_back("X-Frame-Options: SAMEORIGIN");
    options.push_back("access_control_allow_origin");
    options.push_back("*");
    options.push_back("listening_ports");
    options.push_back(http_address);
    options.push_back("enable_keep_alive");
    options.push_back("yes");
    options.push_back("keep_alive_timeout_ms");
    options.push_back("1000");
    try {
        // PeerConnectionManager provides a set of callback functions for
        // HttpServerRequestHandler.
        std::map<std::string, HttpServerRequestHandler::httpFunction> func =
                peer_connection_manager_->getHttpApi();

        // Main loop.
        std::cout << "HTTP Listen at " << http_address << std::endl;
        HttpServerRequestHandler civet_server(func, options);
        // signal(SIGINT, &signal_handler);  // TODO: fix me
        thread->Run();
    } catch (const CivetException& ex) {
        std::cout << "Cannot Initialize start HTTP server exception:"
                  << ex.what() << std::endl;
    }

    rtc::CleanupSSL();
    std::cout << "Exit" << std::endl;
}

void WebRTCServer::Run() { impl_->Run(); }

}  // namespace webrtc_server
}  // namespace visualization
}  // namespace open3d
