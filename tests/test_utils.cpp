#include <gtest/gtest.h>
#include "../src/utils.hpp"
#include <cstdlib>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>

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
