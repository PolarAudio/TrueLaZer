#include "Showbridge.h"
#include "../sdk/DameiSDK.h"
#include <stdio.h>
#include <string.h>
#include <stdexcept>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#endif

// Helper function to initialize Winsock (Windows only)
bool Showbridge::initWinsock() {
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
    return true;
#endif
}

// Helper function to cleanup Winsock (Windows only)
void Showbridge::cleanupWinsock() {
#ifdef _WIN32
    WSACleanup();
#endif
}

Showbridge::Showbridge() : sdk(nullptr), discovery_socket(INVALID_SOCKET), selectedDeviceIndex(-1), is_scanning(false) {
    initWinsock();
}

Showbridge::~Showbridge() {
    stopScanning();
    delete sdk;
#ifdef _WIN32
    if (discovery_socket != INVALID_SOCKET) {
        closesocket(discovery_socket);
    }
#else
    if (discovery_socket != -1) {
        close(discovery_socket);
    }
#endif
    cleanupWinsock();
}

void Showbridge::startScanning(const std::string& interface_ip) {
    if (is_scanning) {
        stopScanning();
    }
    selectedInterfaceIp = interface_ip;
    is_scanning = true;
    discovery_thread = std::thread(&Showbridge::scanLoop, this);
}

void Showbridge::stopScanning() {
    is_scanning = false;
    if (discovery_thread.joinable()) {
        discovery_thread.join();
    }
}

void Showbridge::scanLoop() {
    while (is_scanning) {
        unsigned long local_ip_addr = inet_addr(selectedInterfaceIp.c_str());

#ifdef _WIN32
        if (discovery_socket != INVALID_SOCKET) {
            closesocket(discovery_socket);
        }
#else
        if (discovery_socket != -1) {
            close(discovery_socket);
        }
#endif

        // Create UDP socket
#ifdef _WIN32
        discovery_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (discovery_socket == INVALID_SOCKET) {
            printf("Showbridge::scanLoop - Error creating socket: %d\n", WSAGetLastError());
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
#else
        discovery_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (discovery_socket == -1) {
            perror("Showbridge::scanLoop - Error creating socket");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
#endif

        // Bind the socket to port 8099 and INADDR_ANY
        sockaddr_in bind_addr;
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_port = htons(8099);
        bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(discovery_socket, (sockaddr*)&bind_addr, sizeof(bind_addr)) == SOCKET_ERROR) {
#ifdef _WIN32
            printf("Showbridge::scanLoop - Error binding socket: %d\n", WSAGetLastError());
            closesocket(discovery_socket);
            discovery_socket = INVALID_SOCKET;
#else
            perror("Showbridge::scanLoop - Error binding socket");
            close(discovery_socket);
            discovery_socket = -1;
#endif
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        // Set IP_UNICAST_IF to ensure broadcast is sent from the selected interface
#ifdef _WIN32
        if (setsockopt(discovery_socket, IPPROTO_IP, IP_UNICAST_IF, (char*)&local_ip_addr, sizeof(local_ip_addr)) == SOCKET_ERROR) {
            printf("Showbridge::scanLoop - Error setting IP_UNICAST_IF: %d\n", WSAGetLastError());
        }
#endif

        // Enable broadcast option
        char broadcast_opt = 1;
        if (setsockopt(discovery_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_opt, sizeof(broadcast_opt)) == SOCKET_ERROR) {
#ifdef _WIN32
            printf("Showbridge::scanLoop - Error setting broadcast option: %d\n", WSAGetLastError());
#else
            perror("Showbridge::scanLoop - Error setting broadcast option");
#endif
            closesocket(discovery_socket);
            discovery_socket = INVALID_SOCKET;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        u_char discovery_payload[6];
        discovery_payload[0] = (local_ip_addr >> 0) & 0xff;
        discovery_payload[1] = (local_ip_addr >> 8) & 0xff;
        discovery_payload[2] = (local_ip_addr >> 16) & 0xff;
        discovery_payload[3] = (local_ip_addr >> 24) & 0xff;
        discovery_payload[4] = 163;
        discovery_payload[5] = 31;

        sockaddr_in broadcast_addr;
        broadcast_addr.sin_family = AF_INET;
        broadcast_addr.sin_port = htons(8089);
        broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

        sendto(discovery_socket, (const char*)discovery_payload, sizeof(discovery_payload), 0, (sockaddr*)&broadcast_addr, sizeof(broadcast_addr));

        // Set timeout for receiving responses
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(discovery_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        // Listen for responses
        char recv_buffer[16];
        sockaddr_in sender_addr;
        int sender_addr_len = sizeof(sender_addr);

        while (is_scanning) {
            int bytes_received = recvfrom(discovery_socket, recv_buffer, sizeof(recv_buffer), 0, (sockaddr*)&sender_addr, &sender_addr_len);
            if (bytes_received == 16) {
                dac_info dacInfo;
                memcpy(&dacInfo, recv_buffer, sizeof(dacInfo));

                DiscoveredDACInfo new_device;
                new_device.ip_address = inet_ntoa(sender_addr.sin_addr);
                new_device.dac_information = dacInfo;
                new_device.last_seen = time(nullptr);

                std::lock_guard<std::mutex> lock(dac_mutex);
                bool already_discovered = false;
                for (auto& dac : discoveredDACs) {
                    if (dac.ip_address == new_device.ip_address && dac.dac_information.channel == new_device.dac_information.channel) {
                        dac.last_seen = new_device.last_seen;
                        already_discovered = true;
                        break;
                    }
                }

                if (!already_discovered) {
                    discoveredDACs.push_back(new_device);
                }
            } else {
                // Timeout or error
                break;
            }
        }

        cleanupDeviceList();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Showbridge::cleanupDeviceList() {
    std::lock_guard<std::mutex> lock(dac_mutex);
    time_t now = time(nullptr);
    discoveredDACs.erase(std::remove_if(discoveredDACs.begin(), discoveredDACs.end(),
        [now](const DiscoveredDACInfo& dac) {
            return (now - dac.last_seen) > 5; // 5 second timeout
        }), discoveredDACs.end());
}

std::vector<DiscoveredDACInfo> Showbridge::getDiscoveredDACs() {
    std::lock_guard<std::mutex> lock(dac_mutex);
    return discoveredDACs;
}

bool Showbridge::selectDevice(int deviceIndex) {
    std::lock_guard<std::mutex> lock(dac_mutex);
    if (deviceIndex < 0 || deviceIndex >= discoveredDACs.size()) {
        return false;
    }

    selectedDeviceIndex = deviceIndex;
    selectedDeviceIp = discoveredDACs[deviceIndex].ip_address;

    // We don't initialize the SDK here anymore.
    // It will be initialized on demand.
    return true;
}

bool Showbridge::sendFrame(frame_buffer& frame) {
    if (selectedDeviceIndex == -1) {
        return false;
    }

    if (!sdk) {
        sdk = new DameiSDK();
        unsigned long selected_ip_addr = inet_addr(selectedDeviceIp.c_str());
        if (!sdk->Init(static_cast<SocketLib::ipaddress>(selected_ip_addr))) {
            delete sdk;
            sdk = nullptr;
            return false;
        }
    }

    // The DameiSDK doesn't seem to have a direct frame sending function
    // in the provided headers. Assuming a function like `SendFrame` exists.
    // This part needs to be adapted to the actual SDK API.
    // return sdk->SendFrame(g_selected_show_index, frame);
    return false; // Placeholder
}