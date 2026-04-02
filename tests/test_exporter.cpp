#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <memory>
#include "../src/exporter.hpp"
#include <vector>

TEST(ExporterTest, BatchingLogic) {
    int push_count = 0;
    Exporter exporter("http://localhost:8080", 2);
    exporter.set_push_function([&](const std::vector<ContainerMetric>&) {
        push_count++;
    });

    std::vector<ContainerMetric> m1 = {{"c1", 1.0, 100.0, 1000}};
    exporter.add_metrics(m1);
    EXPECT_EQ(push_count, 0);

    exporter.add_metrics(m1);
    EXPECT_EQ(push_count, 1);
}

TEST(ExporterTest, EmptyMetrics) {
    int push_count = 0;
    Exporter exporter("http://localhost:8080", 2);
    exporter.set_push_function([&](const std::vector<ContainerMetric>&) {
        push_count++;
    });

    exporter.add_metrics({});
    EXPECT_EQ(push_count, 0);
    exporter.flush();
    EXPECT_EQ(push_count, 0);
}

TEST(ExporterTest, BatchSizeOne) {
    int push_count = 0;
    Exporter exporter("http://localhost:8080", 1);
    exporter.set_push_function([&](const std::vector<ContainerMetric>&) {
        push_count++;
    });

    exporter.add_metrics({{"c1", 1.0, 100.0, 1000}});
    EXPECT_EQ(push_count, 1);
}

TEST(ExporterTest, RealNetworkPush) {
    auto svr = std::make_shared<httplib::Server>();
    std::atomic<bool> received{false};
    
    svr->Post("/metrics", [&](const httplib::Request& req, httplib::Response& res) {
        received = true;
        res.status = 200;
        res.set_content("OK", "text/plain");
    });

    std::thread server_thread([svr]() {
        svr->listen("0.0.0.0", 19092);
    });

    // Give time for the server to bind
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    Exporter exporter("http://127.0.0.1:19092/metrics", 1);
    exporter.add_metrics({{"test", 10.5, 256.0, 123456789}});
    
    // Polling wait for the request to be processed
    for (int i = 0; i < 20 && !received; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    EXPECT_TRUE(received.load());
    
    svr->stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}
