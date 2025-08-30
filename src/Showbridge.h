#ifndef SHOWBRIDGE_H
#define SHOWBRIDGE_H

#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include "../sdk/Easysocket.h"
#include "../sdk/SDKSocket.h" // For dac_info, show_list, etc.

// Include platform-specific socket headers
#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib") // Link with ws2_32.lib
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

// Forward declaration
class DameiSDK;

struct DiscoveredDACInfo {
    std::string ip_address;
    dac_info dac_information;
    time_t last_seen;
};

class Showbridge {
public:
    Showbridge();
    ~Showbridge();

    void startScanning(const std::string& interface_ip);
    void stopScanning();
    std::vector<DiscoveredDACInfo> getDiscoveredDACs();
    bool selectDevice(int deviceIndex);
    bool sendFrame(frame_buffer& frame);

    std::string getSelectedDeviceIp() { return selectedDeviceIp; }
    int getSelectedDeviceIndex() { return selectedDeviceIndex; }
    DameiSDK* getSdk() { return sdk; }
    bool isScanning() { return is_scanning; }

private:
    void scanLoop();
    void cleanupDeviceList();

#ifdef _WIN32
    SOCKET discovery_socket;
#else
    int discovery_socket;
#endif
    std::string selectedDeviceIp;
    std::string selectedInterfaceIp;
    int selectedDeviceIndex;
    std::vector<DiscoveredDACInfo> discoveredDACs;
    DameiSDK* sdk;
    std::thread discovery_thread;
    std::mutex dac_mutex;
    std::atomic<bool> is_scanning;

    // Helper function to initialize Winsock (Windows only)
    bool initWinsock();
    void cleanupWinsock();
};

#endif // SHOWBRIDGE_H