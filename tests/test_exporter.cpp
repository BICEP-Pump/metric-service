#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <memory>
#include "../src/exporter.hpp"
#include <vector>

TEST(ExporterTest, CorePushesEachScrapeImmediately) {
    int push_count = 0;
    size_t last_push_size = 0;
    Exporter exporter("http://localhost:8080");
    exporter.set_push_function([&](const std::vector<ContainerMetric>& metrics) {
        push_count++;
        last_push_size = metrics.size();
    });

    std::vector<ContainerMetric> m1 = {{"c1", 1.0, 100.0, 1000}};
    exporter.add_metrics(m1);
    EXPECT_EQ(push_count, 1);
    EXPECT_EQ(last_push_size, 1);
}

TEST(ExporterTest, EmptyMetrics) {
    int push_count = 0;
    Exporter exporter("http://localhost:8080");
    exporter.set_push_function([&](const std::vector<ContainerMetric>&) {
        push_count++;
    });

    exporter.add_metrics({});
    EXPECT_EQ(push_count, 0);
}

TEST(ExporterTest, PrometheusPushesEachScrapeImmediately) {
    int push_count = 0;
    size_t last_push_size = 0;
    Exporter exporter(
        "http://localhost:9091/metrics/job/test-job",
        MetricExportMode::PROMETHEUS
    );
    exporter.set_push_function([&](const std::vector<ContainerMetric>& metrics) {
        push_count++;
        last_push_size = metrics.size();
    });

    exporter.add_metrics({{"c1", 1.0, 100.0, 1000}, {"c2", 2.0, 200.0, 1001}});

    EXPECT_EQ(push_count, 1);
    EXPECT_EQ(last_push_size, 2);
}

TEST(ExporterTest, RealNetworkPush) {
    auto svr = std::make_shared<httplib::Server>();
    std::atomic<bool> received{false};
    std::string request_body;
    
    svr->Post("/metrics", [&](const httplib::Request& req, httplib::Response& res) {
        received = true;
        request_body = req.body;
        res.status = 200;
        res.set_content("OK", "text/plain");
    });

    std::thread server_thread([svr]() {
        svr->listen("0.0.0.0", 19092);
    });

    // Give time for the server to bind
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    Exporter exporter("http://127.0.0.1:19092/metrics");
    exporter.add_metrics({{"test", 10.5, 256.0, 123456789}});
    
    // Polling wait for the request to be processed
    for (int i = 0; i < 20 && !received; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    EXPECT_TRUE(received.load());
    EXPECT_NE(request_body.find("\"cpu_usage\":10.5"), std::string::npos);
    
    svr->stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}

TEST(ExporterTest, RealPrometheusPush) {
    auto svr = std::make_shared<httplib::Server>();
    std::atomic<bool> received{false};
    std::string request_body;

    svr->Post("/metrics/job/test-job", [&](const httplib::Request& req, httplib::Response& res) {
        received = true;
        request_body = req.body;
        res.status = 202;
        res.set_content("accepted", "text/plain");
    });

    std::thread server_thread([svr]() {
        svr->listen("0.0.0.0", 19093);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    Exporter exporter(
        "http://127.0.0.1:19093/metrics/job/test-job",
        MetricExportMode::PROMETHEUS
    );
    exporter.add_metrics({{"test-container", 10.5, 256.0, 123456789}});

    for (int i = 0; i < 20 && !received; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_TRUE(received.load());
    EXPECT_NE(request_body.find("container_cpu_usage{name=\"test-container\"} 10.5"), std::string::npos);
    EXPECT_NE(
        request_body.find("container_memory_usage_bytes{name=\"test-container\"}"),
        std::string::npos
    );

    svr->stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}
