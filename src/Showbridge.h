#ifndef SHOWBRIDGE_H
#define SHOWBRIDGE_H

#include <vector>
#include <string>
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
};

class Showbridge {
public:
    Showbridge();
    ~Showbridge();

    bool initSdk(SocketLib::ipaddress p_addr);
    int scanForDevices(std::vector<std::string>& device_ips);
    bool selectDevice(int deviceIndex);
    bool sendFrame(frame_buffer& frame);

    std::string getSelectedDeviceIp() { return selectedDeviceIp; }
    int getSelectedDeviceIndex() { return selectedDeviceIndex; }
    std::vector<DiscoveredDACInfo> getDiscoveredDACs() { return discoveredDACs; }
    DameiSDK* getSdk() { return sdk; }

private:
#ifdef _WIN32
    SOCKET discovery_socket;
#else
    int discovery_socket;
#endif
    std::string selectedDeviceIp;
    int selectedDeviceIndex;
    std::vector<std::string> availableDeviceIps; // Stores only IPs for the UI dropdown
    std::vector<DiscoveredDACInfo> discoveredDACs; // Stores full info for internal use
    DameiSDK* sdk;

    // Helper function to initialize Winsock (Windows only)
    bool initWinsock();
    void cleanupWinsock();
};

#endif // SHOWBRIDGE_H
