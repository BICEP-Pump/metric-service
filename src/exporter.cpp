#include "exporter.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

Exporter::Exporter(const std::string& ep, MetricExportMode mode)
    : endpoint(ep), export_mode(mode) {
    // Default push function: the real HTTP push
    push_func = [this](const std::vector<ContainerMetric>& m) {
        this->push_to_endpoint(m);
    };
}

void Exporter::set_push_function(PushFunc func) {
    std::lock_guard<std::mutex> lock(mtx);
    push_func = func;
}

void Exporter::add_metrics(const std::vector<ContainerMetric>& metrics) {
    std::lock_guard<std::mutex> lock(mtx);
    if (!metrics.empty()) {
        push_func(metrics);
    }
}

void Exporter::push_to_endpoint(const std::vector<ContainerMetric>& metrics) {
    switch (export_mode) {
        case MetricExportMode::PROMETHEUS:
            push_to_prometheus_endpoint(metrics);
            return;
        case MetricExportMode::CORE:
        default:
            push_to_core_endpoint(metrics);
            return;
    }
}

void Exporter::push_to_core_endpoint(const std::vector<ContainerMetric>& metrics) {
    if (endpoint.empty()) {
        std::cout << "[Exporter] Dry run: Endpoint not set. Would have pushed " << metrics.size() << " metrics." << std::endl;
        return;
    }

    json j = json::array();
    for (const auto& m : metrics) {
        j.push_back({
            {"container_name", m.name},
            {"cpu_usage", m.cpu_cores},
            {"ram_mb", m.ram_mb},
            {"timestamp", m.timestamp}
        });
    }

    std::string body = j.dump();

    ParsedHttpUrl parsed_endpoint;
    std::string parse_error;
    if (!parse_http_url(endpoint, parsed_endpoint, parse_error)) {
        std::cerr << "[Exporter] Invalid metrics endpoint '" << endpoint
                  << "': " << parse_error << std::endl;
        return;
    }

    HttpPostResult response;
    const bool request_succeeded = perform_http_post(
        parsed_endpoint,
        body,
        "application/json",
        response
    );

    if (request_succeeded) {
        if (
            response.http_status == 200
            || response.http_status == 201
            || response.http_status == 204
        ) {
            std::cout << "[Exporter] Successfully pushed " << metrics.size() << " metrics." << std::endl;
        } else {
            std::cerr << "[Exporter] Failed to push metrics. Status: "
                      << response.http_status << std::endl;
            std::cerr << "[Exporter] Response: " << response.response_body << std::endl;
            std::cerr << "[Exporter] Transport error code: "
                      << response.transport_error_code;
            if (!response.transport_error_message.empty()) {
                std::cerr << " (" << response.transport_error_message << ")";
            }
            std::cerr << std::endl;
        }
    } else {
        std::cerr << "[Exporter] Failed to push metrics to " << endpoint
                  << ". Transport error code: " << response.transport_error_code;
        if (!response.transport_error_message.empty()) {
            std::cerr << " (" << response.transport_error_message << ")";
        }
        std::cerr
                  << std::endl;
    }
}

std::string Exporter::escape_prometheus_label_value(const std::string& value) const {
    std::string escaped = value;
    size_t pos = 0;
    while ((pos = escaped.find('\\', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\\\");
        pos += 2;
    }

    pos = 0;
    while ((pos = escaped.find('"', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\\"");
        pos += 2;
    }

    return escaped;
}

std::string Exporter::build_prometheus_payload(
    const std::vector<ContainerMetric>& metrics
) const {
    std::ostringstream payload;

    for (const auto& metric : metrics) {
        const std::string container_name =
            escape_prometheus_label_value(metric.name);
        payload << "container_cpu_usage{name=\"" << container_name
                << "\"} " << metric.cpu_cores << "\n";
        payload << "container_memory_usage_bytes{name=\"" << container_name
                << "\"} " << (metric.ram_mb * 1024 * 1024) << "\n";
    }

    return payload.str();
}

void Exporter::push_to_prometheus_endpoint(
    const std::vector<ContainerMetric>& metrics
) {
    if (endpoint.empty()) {
        std::cout << "[Exporter] Dry run: Prometheus endpoint not set. Would have pushed "
                  << metrics.size() << " metrics." << std::endl;
        return;
    }

    std::string body = build_prometheus_payload(metrics);

    ParsedHttpUrl parsed_endpoint;
    std::string parse_error;
    if (!parse_http_url(endpoint, parsed_endpoint, parse_error)) {
        std::cerr << "[Exporter] Invalid Prometheus endpoint '" << endpoint
                  << "': " << parse_error << std::endl;
        return;
    }

    HttpPostResult response;
    const bool request_succeeded = perform_http_post(
        parsed_endpoint,
        body,
        "text/plain; version=0.0.4; charset=utf-8",
        response
    );

    if (request_succeeded) {
        if (
            response.http_status == 200
            || response.http_status == 201
            || response.http_status == 202
            || response.http_status == 204
        ) {
            std::cout << "[Exporter] Successfully pushed " << metrics.size()
                      << " metrics to Prometheus." << std::endl;
        } else {
            std::cerr << "[Exporter] Failed to push Prometheus metrics. Status: "
                      << response.http_status << std::endl;
            std::cerr << "[Exporter] Response: " << response.response_body << std::endl;
            std::cerr << "[Exporter] Transport error code: "
                      << response.transport_error_code;
            if (!response.transport_error_message.empty()) {
                std::cerr << " (" << response.transport_error_message << ")";
            }
            std::cerr << std::endl;
        }
    } else {
        std::cerr << "[Exporter] Failed to push Prometheus metrics to " << endpoint
                  << ". Transport error code: " << response.transport_error_code;
        if (!response.transport_error_message.empty()) {
            std::cerr << " (" << response.transport_error_message << ")";
        }
        std::cerr
                  << std::endl;
    }
}
