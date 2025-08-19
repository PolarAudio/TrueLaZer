#include "Showbridge.h"
#include "../sdk/DameiSDK.h"
#include <stdio.h>
#include <string.h>
#include <stdexcept>

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

Showbridge::Showbridge() {
    selectedDeviceIndex = -1;
    selectedDeviceIp = "";
    sdk = nullptr;
#ifdef _WIN32
    discovery_socket = INVALID_SOCKET;
    initWinsock();
#else
    discovery_socket = -1;
#endif
}

bool Showbridge::initSdk(SocketLib::ipaddress p_addr) {
    if (sdk) {
        delete sdk;
        sdk = nullptr;
    }
    sdk = new DameiSDK();
    return sdk->Init(p_addr);
}

Showbridge::~Showbridge() {
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

int Showbridge::scanForDevices(std::vector<std::string>& device_ips) {
    device_ips.clear();
    discoveredDACs.clear();

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
        printf("Error creating socket: %d\n", WSAGetLastError());
        return 0;
    }
    // Allow socket to reuse address
    BOOL optval = TRUE;
    if (setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval)) == SOCKET_ERROR) {
        printf("Error setting SO_REUSEADDR: %d\n", WSAGetLastError());
        closesocket(discovery_socket);
        discovery_socket = INVALID_SOCKET;
        return 0;
    }
#else
    discovery_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (discovery_socket == -1) {
        perror("Error creating socket");
        return 0;
    }
    // Allow socket to reuse address
    int optval = 1;
    if (setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("Error setting SO_REUSEADDR");
        close(discovery_socket);
        discovery_socket = -1;
        return 0;
    }
#endif

    // Bind the socket to port 8099
    sockaddr_in bind_addr;
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(8099);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(discovery_socket, (sockaddr*)&bind_addr, sizeof(bind_addr)) == SOCKET_ERROR) {
#ifdef _WIN32
        printf("Error binding socket: %d\n", WSAGetLastError());
        closesocket(discovery_socket);
        discovery_socket = INVALID_SOCKET;
#else
        perror("Error binding socket");
        close(discovery_socket);
        discovery_socket = -1;
#endif
        return 0;
    }

    // Enable broadcast option
    char broadcast_opt = 1;
    if (setsockopt(discovery_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_opt, sizeof(broadcast_opt)) == SOCKET_ERROR) {
#ifdef _WIN32
        printf("Error setting broadcast option: %d\n", WSAGetLastError());
        closesocket(discovery_socket);
        discovery_socket = INVALID_SOCKET;
#else
        perror("Error setting broadcast option");
        close(discovery_socket);
        discovery_socket = -1;
#endif
        return 0;
    }

    // Construct the 6-byte discovery payload dynamically using the local IP address.
    // For simplicity, we'll use the first non-loopback IP found.
    unsigned long local_ip_addr = 0;
#ifdef _WIN32
    PIP_ADAPTER_INFO pAdapterInfo;
    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
    pAdapterInfo = (PIP_ADAPTER_INFO) malloc(ulOutBufLen);
    if (pAdapterInfo == NULL) { return 0; }

    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterInfo);
        pAdapterInfo = (PIP_ADAPTER_INFO) malloc(ulOutBufLen);
        if (pAdapterInfo == NULL) { return 0; }
    }

    if ((GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR) {
        PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
        while (pAdapter) {
            unsigned long ip = inet_addr(pAdapter->IpAddressList.IpAddress.String);
            if (ip != 0 && ip != 0x0100007f) { // Exclude loopback
                local_ip_addr = ip;
                break;
            }
            pAdapter = pAdapter->Next;
        }
    }
    if (pAdapterInfo) { free(pAdapterInfo); }
#endif

    if (local_ip_addr == 0) {
        printf("Could not determine local IP address for broadcast.\n");
        return 0;
    }

    u_char discovery_payload[6];
    discovery_payload[0] = (local_ip_addr >> 0) & 0xff;
    discovery_payload[1] = (local_ip_addr >> 8) & 0xff;
    discovery_payload[2] = (local_ip_addr >> 16) & 0xff;
    discovery_payload[3] = (local_ip_addr >> 24) & 0xff;
    discovery_payload[4] = 163;
    discovery_payload[5] = 31;

    // Send broadcast packet
    sockaddr_in broadcast_addr;
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(8089);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    printf("Sending discovery broadcast to 255.255.255.255:8089 from %s:8099...\n", inet_ntoa(*(in_addr*)&local_ip_addr));
    sendto(discovery_socket, (const char*)discovery_payload, sizeof(discovery_payload), 0, (sockaddr*)&broadcast_addr, sizeof(broadcast_addr));

    // Set timeout for receiving responses
    struct timeval tv;
    tv.tv_sec = 5; // 5 seconds timeout
    tv.tv_usec = 0;
    setsockopt(discovery_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    // Listen for responses
    char recv_buffer[16];
    sockaddr_in sender_addr;
    int sender_addr_len = sizeof(sender_addr);

    printf("Listening for responses...\n");
    while (true) {
        int bytes_received = recvfrom(discovery_socket, recv_buffer, sizeof(recv_buffer), 0, (sockaddr*)&sender_addr, &sender_addr_len);
        if (bytes_received == SOCKET_ERROR) {
#ifdef _WIN32
            int error_code = WSAGetLastError();
            if (error_code == WSAETIMEDOUT) {
                printf("Receive timeout.\n");
                break; // Timeout, stop listening
            } else {
                printf("Socket error during receive: %d\n", error_code);
                break;
            }
#else
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                printf("Receive timeout.\n");
                break; // Timeout, stop listening
            } else {
                perror("Socket error during receive");
                break;
            }
#endif
        }

        if (bytes_received == 16) {
            dac_info dacInfo;
            memcpy(&dacInfo, recv_buffer, sizeof(dacInfo));

            DiscoveredDACInfo new_device;
            new_device.ip_address = inet_ntoa(sender_addr.sin_addr);
            new_device.dac_information = dacInfo;

            // Check if already discovered (based on IP and channel)
            bool already_discovered = false;
            for (const auto& dac : discoveredDACs) {
                if (dac.ip_address == new_device.ip_address && dac.dac_information.channel == new_device.dac_information.channel) {
                    already_discovered = true;
                    break;
                }
            }

            if (!already_discovered) {
                discoveredDACs.push_back(new_device);
                device_ips.push_back(new_device.ip_address); // For UI dropdown
                printf("Discovered DAC: %s (Channel: %d, Type: %d, Version: %d.%d)\n", 
                       new_device.ip_address.c_str(), 
                       new_device.dac_information.channel,
                       new_device.dac_information.type,
                       new_device.dac_information.version[0], new_device.dac_information.version[1]);
            }
        }
    }

    return discoveredDACs.size();
}

bool Showbridge::selectDevice(int deviceIndex) {
    if (deviceIndex < 0 || deviceIndex >= discoveredDACs.size()) {
        return false; // Invalid deviceIndex
    }

    selectedDeviceIndex = deviceIndex;
    selectedDeviceIp = discoveredDACs[deviceIndex].ip_address;

    // Convert string IP to ipaddress (unsigned long)
    unsigned long selected_ip_addr = inet_addr(selectedDeviceIp.c_str());

    if (sdk->Init(static_cast<SocketLib::ipaddress>(selected_ip_addr))) {
        return true;
    } else {
        return false;
    }
}

bool Showbridge::sendFrame(frame_buffer& frame) {
    // This function will be implemented later using the raw socket.
    // For now, it just returns false.
    return false;
}
