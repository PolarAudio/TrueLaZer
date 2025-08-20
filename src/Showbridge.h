#pragma once

#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "../sdk/SDKSocket.h"
#include <thread>
#include <mutex>
#include <atomic>

#pragma comment(lib, "Ws2_32.lib")

// Definition for DiscoveredDACInfo
struct DiscoveredDACInfo {
    dac_info dac_information;
    std::string ip_address;
};

class Showbridge {
public:
    Showbridge();
    ~Showbridge();

    bool initialize();
    void shutdown();
    std::vector<std::string> getNetworkInterfaces(); // New: Get list of local network interfaces
    void discoverDACs(const std::string& local_ip_address = ""); // Modified: Optional local IP for binding
    std::vector<DiscoveredDACInfo> performDACDiscovery(const std::string& local_ip_address);
    void selectDAC(int index);
    bool sendFrame(const frame_buffer& fb);

    std::vector<DiscoveredDACInfo> discoveredDACs;
    std::string selectedDeviceIdentifier; // Stores IP_Channel for selected DAC
    int selectedDeviceIndex; // Updated based on selectedDeviceIdentifier

private:
    unsigned char globalSequenceCounter;

    // Threading for continuous discovery
    std::thread discoveryThread;
    std::atomic<bool> stopDiscoveryFlag;

    void discoveryThreadLoop(const std::string& local_ip_address);

public:
    std::mutex dacMutex;
    void startDiscovery(const std::string& local_ip_address = "");
    void stopDiscovery();
};
