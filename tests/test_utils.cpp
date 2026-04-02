#include <gtest/gtest.h>
#include "../src/utils.hpp"
#include <cstdlib>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>

TEST(UtilsTest, ConfigParsingDefaults) {
    unsetenv("METRIC_ENDPOINT");
    Config cfg = get_config();
    EXPECT_EQ(cfg.scrape_interval, 10);
}

TEST(UtilsTest, ConfigParsingEnv) {
    setenv("METRIC_ENDPOINT", "http://test:9000", 1);
    Config cfg = get_config();
    EXPECT_EQ(cfg.metric_endpoint, "http://test:9000");
    unsetenv("METRIC_ENDPOINT");
}

TEST(UtilsTest, ParseIfaddrsLoopbackOnly) {
    struct ifaddrs ifa;
    struct sockaddr_in addr;
    
    ifa.ifa_next = NULL;
    ifa.ifa_name = (char*)"lo";
    ifa.ifa_addr = (struct sockaddr*)&addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    std::string ip = parse_ifaddrs(&ifa);
    EXPECT_EQ(ip, "127.0.0.1");
}

TEST(UtilsTest, ParseIfaddrsPriority) {
    struct ifaddrs ifa1, ifa2;
    struct sockaddr_in addr1, addr2;

    // eth0: 192.168.1.5
    ifa1.ifa_next = &ifa2;
    ifa1.ifa_name = (char*)"eth0";
    ifa1.ifa_addr = (struct sockaddr*)&addr1;
    addr1.sin_family = AF_INET;
    inet_pton(AF_INET, "192.168.1.5", &addr1.sin_addr);

    // docker0: 172.17.0.1 (Preferred)
    ifa2.ifa_next = NULL;
    ifa2.ifa_name = (char*)"docker0";
    ifa2.ifa_addr = (struct sockaddr*)&addr2;
    addr2.sin_family = AF_INET;
    inet_pton(AF_INET, "172.17.0.1", &addr2.sin_addr);

    std::string ip = parse_ifaddrs(&ifa1);
    EXPECT_EQ(ip, "172.17.0.1");
}

TEST(UtilsTest, RegisterServiceSuccess) {
    auto svr = std::make_shared<httplib::Server>();
    std::atomic<bool> request_received{false};
    
    svr->Post("/register", [&](const httplib::Request& req, httplib::Response& res) {
        request_received = true;
        res.status = 200;
        res.set_content("OK", "text/plain");
    });

    std::thread server_thread([svr]() {
        svr->listen("0.0.0.0", 19091);
    });
    
    // Give more time and wait for server to be ready
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    Config cfg;
    cfg.registration_endpoint = "http://127.0.0.1:19091/register";
    cfg.service_name = "test-service";
    cfg.service_ip = "1.2.3.4";
    cfg.service_port = 8888;

    register_service(cfg);
    
    EXPECT_TRUE(request_received.load());
    
    svr->stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}
