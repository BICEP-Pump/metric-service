#include "utils.hpp"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

constexpr int HTTP_TRANSPORT_ERROR_RESOLVE = 1;
constexpr int HTTP_TRANSPORT_ERROR_CONNECT = 2;
constexpr int HTTP_TRANSPORT_ERROR_WRITE = 3;
constexpr int HTTP_TRANSPORT_ERROR_READ = 4;
constexpr int HTTP_TRANSPORT_ERROR_PARSE = 5;

std::string format_http_host_header(const ParsedHttpUrl& parsed_url) {
    const bool is_ipv6 = parsed_url.host.find(':') != std::string::npos;
    const std::string host =
        is_ipv6 ? ("[" + parsed_url.host + "]") : parsed_url.host;

    const bool use_default_port =
        (parsed_url.scheme == "http" && parsed_url.port == 80);
    if (use_default_port) {
        return host;
    }

    return host + ":" + std::to_string(parsed_url.port);
}

bool send_all(int socket_fd, const std::string& data, HttpPostResult& result) {
    size_t total_sent = 0;
    while (total_sent < data.size()) {
        const ssize_t sent = send(
            socket_fd,
            data.data() + total_sent,
            data.size() - total_sent,
            0
        );
        if (sent <= 0) {
            result.transport_error_code = HTTP_TRANSPORT_ERROR_WRITE;
            result.transport_error_message = std::strerror(errno);
            return false;
        }
        total_sent += static_cast<size_t>(sent);
    }

    return true;
}

bool parse_http_response(
    const std::string& response,
    HttpPostResult& result
) {
    if (response.empty()) {
        result.transport_error_code = HTTP_TRANSPORT_ERROR_READ;
        result.transport_error_message = "Empty HTTP response.";
        return false;
    }

    const size_t first_line_end = response.find("\r\n");
    if (first_line_end == std::string::npos) {
        result.transport_error_code = HTTP_TRANSPORT_ERROR_PARSE;
        result.transport_error_message = "Invalid HTTP status line.";
        return false;
    }

    std::istringstream status_stream(response.substr(0, first_line_end));
    std::string http_version;
    int status_code = 0;
    status_stream >> http_version >> status_code;
    if (!status_stream || status_code <= 0) {
        result.transport_error_code = HTTP_TRANSPORT_ERROR_PARSE;
        result.transport_error_message = "Could not parse HTTP status code.";
        return false;
    }

    result.http_status = status_code;

    const size_t header_end = response.find("\r\n\r\n");
    if (header_end != std::string::npos) {
        result.response_body = response.substr(header_end + 4);
    } else {
        result.response_body.clear();
    }

    return true;
}

} // namespace

std::string metric_export_mode_to_string(MetricExportMode mode) {
    switch (mode) {
        case MetricExportMode::PROMETHEUS:
            return "prometheus";
        case MetricExportMode::CORE:
        default:
            return "core";
    }
}

std::string discover_ip() {
    struct ifaddrs *ifaddr;
    std::string ip = "127.0.0.1";

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return ip;
    }

    ip = parse_ifaddrs(ifaddr);

    freeifaddrs(ifaddr);
    return ip;
}

std::string parse_ifaddrs(struct ifaddrs* ifaddr) {
    std::string ip = "127.0.0.1";
    struct ifaddrs* ifa;

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) { // IPv4
            char host[NI_MAXHOST];
            int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s == 0) {
                std::string current_ip = host;
                if (current_ip != "127.0.0.1") {
                    ip = current_ip;
                    // If we find an IP that looks like a docker internal IP (172.x), we might prefer it
                    if (ip.find("172.") == 0) {
                        break; 
                    }
                }
            }
        }
    }

    return ip;
}

bool parse_http_url(
    const std::string& url,
    ParsedHttpUrl& parsed_url,
    std::string& error_message
) {
    if (url.empty()) {
        error_message = "URL is empty.";
        return false;
    }

    ParsedHttpUrl parsed;
    std::string remainder = url;

    const size_t scheme_pos = remainder.find("://");
    if (scheme_pos != std::string::npos) {
        parsed.scheme = remainder.substr(0, scheme_pos);
        remainder = remainder.substr(scheme_pos + 3);
    }

    if (parsed.scheme != "http") {
        error_message =
            "Unsupported URL scheme '" + parsed.scheme + "'. Only http is supported.";
        return false;
    }

    const size_t path_pos = remainder.find('/');
    std::string authority =
        (path_pos == std::string::npos) ? remainder : remainder.substr(0, path_pos);
    parsed.path =
        (path_pos == std::string::npos) ? "/" : remainder.substr(path_pos);

    if (authority.empty()) {
        error_message = "URL host is empty.";
        return false;
    }

    if (authority.front() == '[') {
        const size_t closing_bracket = authority.find(']');
        if (closing_bracket == std::string::npos) {
            error_message = "Invalid IPv6 host syntax in URL.";
            return false;
        }

        parsed.host = authority.substr(1, closing_bracket - 1);
        const std::string port_part = authority.substr(closing_bracket + 1);
        if (!port_part.empty()) {
            if (port_part.front() != ':') {
                error_message = "Invalid port separator in URL.";
                return false;
            }

            const std::string port_text = port_part.substr(1);
            if (port_text.empty()) {
                error_message = "URL port is empty.";
                return false;
            }

            for (char c : port_text) {
                if (!std::isdigit(static_cast<unsigned char>(c))) {
                    error_message = "URL port contains non-digit characters.";
                    return false;
                }
            }
            parsed.port = std::stoi(port_text);
        }
    } else {
        const size_t colon_pos = authority.rfind(':');
        if (colon_pos != std::string::npos) {
            parsed.host = authority.substr(0, colon_pos);
            const std::string port_text = authority.substr(colon_pos + 1);
            if (parsed.host.empty()) {
                error_message = "URL host is empty.";
                return false;
            }
            if (port_text.empty()) {
                error_message = "URL port is empty.";
                return false;
            }
            for (char c : port_text) {
                if (!std::isdigit(static_cast<unsigned char>(c))) {
                    error_message = "URL port contains non-digit characters.";
                    return false;
                }
            }
            parsed.port = std::stoi(port_text);
        } else {
            parsed.host = authority;
        }
    }

    if (parsed.host.empty()) {
        error_message = "URL host is empty.";
        return false;
    }

    if (parsed.port <= 0 || parsed.port > 65535) {
        error_message = "URL port is outside the valid range.";
        return false;
    }

    parsed_url = parsed;
    error_message.clear();
    return true;
}

bool perform_http_post(
    const ParsedHttpUrl& parsed_url,
    const std::string& request_body,
    const std::string& content_type,
    HttpPostResult& result
) {
    result = HttpPostResult{};

    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* addresses = nullptr;
    const std::string port_string = std::to_string(parsed_url.port);
    const int resolve_status = getaddrinfo(
        parsed_url.host.c_str(),
        port_string.c_str(),
        &hints,
        &addresses
    );
    if (resolve_status != 0) {
        result.transport_error_code = HTTP_TRANSPORT_ERROR_RESOLVE;
        result.transport_error_message = gai_strerror(resolve_status);
        return false;
    }

    int socket_fd = -1;
    for (struct addrinfo* addr = addresses; addr != nullptr; addr = addr->ai_next) {
        socket_fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (socket_fd < 0) {
            continue;
        }

        struct timeval timeout {};
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        if (connect(socket_fd, addr->ai_addr, addr->ai_addrlen) == 0) {
            break;
        }

        close(socket_fd);
        socket_fd = -1;
    }

    freeaddrinfo(addresses);

    if (socket_fd < 0) {
        result.transport_error_code = HTTP_TRANSPORT_ERROR_CONNECT;
        result.transport_error_message = std::strerror(errno);
        return false;
    }

    std::ostringstream request_stream;
    request_stream
        << "POST " << parsed_url.path << " HTTP/1.1\r\n"
        << "Host: " << format_http_host_header(parsed_url) << "\r\n"
        << "Content-Type: " << content_type << "\r\n"
        << "Accept: */*\r\n"
        << "Connection: close\r\n"
        << "Content-Length: " << request_body.size() << "\r\n\r\n"
        << request_body;
    const std::string request = request_stream.str();

    if (!send_all(socket_fd, request, result)) {
        close(socket_fd);
        return false;
    }

    std::string response;
    char buffer[4096];
    while (true) {
        const ssize_t received = recv(socket_fd, buffer, sizeof(buffer), 0);
        if (received > 0) {
            response.append(buffer, static_cast<size_t>(received));
            continue;
        }
        if (received == 0) {
            break;
        }

        result.transport_error_code = HTTP_TRANSPORT_ERROR_READ;
        result.transport_error_message = std::strerror(errno);
        close(socket_fd);
        return false;
    }

    close(socket_fd);
    return parse_http_response(response, result);
}

Config get_config() {
    Config config;
    const char* me = std::getenv("METRIC_ENDPOINT");
    if (me) config.metric_endpoint = me;

    const char* re = std::getenv("REGISTRATION_ENDPOINT");
    if (re) config.registration_endpoint = re;

    const char* si = std::getenv("SCRAPE_INTERVAL");
    if (si) config.scrape_interval = std::stoi(si);

    const char* mem = std::getenv("METRIC_EXPORT_MODE");
    if (mem) {
        std::string export_mode = mem;
        if (export_mode == "prometheus") {
            config.metric_export_mode = MetricExportMode::PROMETHEUS;
        } else if (export_mode == "core") {
            config.metric_export_mode = MetricExportMode::CORE;
        } else {
            std::cerr << "[Config] Unsupported METRIC_EXPORT_MODE '" << export_mode
                      << "'. Falling back to core." << std::endl;
        }
    }

    const char* sn = std::getenv("SERVICE_NAME");
    if (sn) config.service_name = sn;

    const char* sp = std::getenv("SERVICE_PORT");
    if (sp) config.service_port = std::stoi(sp);

    const char* sip = std::getenv("SERVICE_IP");
    if (sip) {
        config.service_ip = sip;
    } else {
        config.service_ip = discover_ip();
        std::cout << "[Config] Auto-discovered IP: " << config.service_ip << std::endl;
    }

    return config;
}

bool register_service(const Config& config) {
    if (config.registration_endpoint.empty()) {
        std::cout << "[Registration] No endpoint set. Skipping registration." << std::endl;
        return true;
    }

    const json j = {
        {"ip", config.service_ip},
        {"name", config.service_name},
        {"port", config.service_port}
    };
    const std::string payload = j.dump();

    ParsedHttpUrl endpoint;
    std::string parse_error;
    if (!parse_http_url(config.registration_endpoint, endpoint, parse_error)) {
        std::cerr << "[Registration] Invalid registration endpoint '" 
                  << config.registration_endpoint << "': " << parse_error << std::endl;
        return false;
    }

    // Hardened retry window for containers/CI (60s total)
    int max_retries = 30;
    int retry_delay = 2; // seconds

    // Shorter retries for ultra-fast localhost tests
    if (
        endpoint.host.find("127.0.0.1") != std::string::npos
        || endpoint.host.find("localhost") != std::string::npos
    ) {
        max_retries = 5;
        retry_delay = 1;
    }

    for (int i = 0; i < max_retries; ++i) {
        std::cout << "[Registration] Attempt " << (i + 1) << "/" << max_retries
                  << " POST " << config.registration_endpoint
                  << " payload=" << payload << std::endl;

        HttpPostResult response;
        const bool request_succeeded = perform_http_post(
            endpoint,
            payload,
            "application/json",
            response
        );

        if (
            request_succeeded
            && (response.http_status == 200
                || response.http_status == 201
                || response.http_status == 204)
        ) {
            std::cout << "[Registration] Successfully registered via "
                      << config.registration_endpoint
                      << ". HTTP status=" << response.http_status
                      << ", transport_error_code=" << response.transport_error_code
                      << std::endl;
            return true;
        }

        std::cerr << "[Registration] Attempt " << (i + 1) << "/" << max_retries
                  << " failed for POST " << config.registration_endpoint
                  << ". HTTP status="
                  << (request_succeeded ? std::to_string(response.http_status) : "none")
                  << ", transport_error_code=" << response.transport_error_code;
        if (!response.transport_error_message.empty()) {
            std::cerr << " (" << response.transport_error_message << ")";
        }
        if (!response.response_body.empty()) {
            std::cerr << ", response_body=" << response.response_body;
        }
        std::cerr << ". Retrying in " << retry_delay << "s..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(retry_delay));
    }

    std::cerr << "[Registration] Final failure after " << max_retries
              << " attempts for POST " << config.registration_endpoint
              << std::endl;
    return false;
}

void register_service_until_success(const Config& config) {
    if (config.registration_endpoint.empty()) {
        std::cout << "[Registration] No endpoint set. Background registration loop not started."
                  << std::endl;
        return;
    }

    while (!register_service(config)) {
        std::cerr << "[Registration] Registration is still pending. Waiting 10s before the next retry cycle."
                  << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}
