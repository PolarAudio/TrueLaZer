#include "Showbridge.h"
#include <iostream>
#include <algorithm>
#include <windows.h> // Required for Sleep() and GetAdaptersInfo
#include <iphlpapi.h> // Required for GetAdaptersInfo

// Constants for DAC communication
const int DAC_PORT = 8089;
const int CLIENT_PORT_1 = 54445; // Source port for Channel 1
const int CLIENT_PORT_2 = 49504; // Source port for Channel 2

// No padding at the end of application data
const int PADDING_SIZE = 0;

Showbridge::Showbridge() : selectedDeviceIndex(-1), globalSequenceCounter(0), selectedDeviceIdentifier("") {
    // Constructor
}

Showbridge::~Showbridge() {
    // Destructor
}

bool Showbridge::initialize() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed: %d\n", iResult);
        return false;
    }
    // Add a small delay to allow Winsock to fully initialize
    Sleep(100); // Sleep for 100 milliseconds
    return true;
}

void Showbridge::shutdown() {
    stopDiscovery(); // Ensure discovery thread is stopped
    WSACleanup();
}

std::vector<DiscoveredDACInfo> Showbridge::performDACDiscovery(const std::string& local_ip_address) {
    std::vector<DiscoveredDACInfo> foundDACs;

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        printf("Error creating socket: %d\n", WSAGetLastError());
        return foundDACs;
    }


    // Set socket to broadcast mode
    char broadcast = '1';
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == SOCKET_ERROR) {
        printf("setsockopt for SO_BROADCAST failed: %d\n", WSAGetLastError());
        closesocket(sock);
        return foundDACs;
    }

    // Bind to a specific local IP address if provided, otherwise bind to INADDR_ANY
    sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(8099); // Port for receiving DAC responses

    if (!local_ip_address.empty()) {
        inet_pton(AF_INET, local_ip_address.c_str(), &(local_addr.sin_addr));
        printf("Binding discovery socket to specific IP: %s\n", local_ip_address.c_str());
    } else {
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        printf("Binding discovery socket to INADDR_ANY\n");
    }

    if (bind(sock, (sockaddr*)&local_addr, sizeof(local_addr)) == SOCKET_ERROR) {
        printf("Bind failed: %d\n", WSAGetLastError());
        closesocket(sock);
        return foundDACs;
    }

    // Set a timeout for receiving responses
    int timeout = 1000; // 1 second
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        printf("setsockopt for SO_RCVTIMEO failed: %d\n", WSAGetLastError());
        closesocket(sock);
        return foundDACs;
    }

    // Broadcast discovery message
    sockaddr_in broadcast_addr;
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(DAC_PORT);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    // Discovery command: Target IP (169.254.25.104) + Flags (163, 31)
    // This is the specific command observed in Truwave for discovery
    unsigned char discovery_command[6] = {169, 254, 25, 104, 163, 31};

    printf("Sending discovery broadcast...\n");
    int bytes_sent = sendto(sock, (const char*)discovery_command, sizeof(discovery_command), 0, (sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    if (bytes_sent == SOCKET_ERROR) {
        printf("sendto failed: %d\n", WSAGetLastError());
        closesocket(sock);
        return foundDACs;
    }

    // Receive responses
    char buffer[512];
    sockaddr_in sender_addr;
    int sender_addr_size = sizeof(sender_addr);

    while (true) {
        int bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&sender_addr, &sender_addr_size);
        if (bytes_received == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                // Timeout, no more DACs found
                break;
            } else {
                printf("recvfrom failed: %d\n", error);
                break;
            }
        }

        // Process the response (16 bytes expected)
        if (bytes_received == 16) {
            DiscoveredDACInfo dac_info_obj;
            // Copy raw bytes to dac_info struct
            memcpy(&(dac_info_obj.dac_information), buffer, sizeof(dac_info));

            // Convert IP address to string
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(sender_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
            dac_info_obj.ip_address = ip_str;

            foundDACs.push_back(dac_info_obj);
            printf("Found DAC: IP %s, Channel %d\n", dac_info_obj.ip_address.c_str(), dac_info_obj.dac_information.channel);
        }
    }

    closesocket(sock);
    return foundDACs;
}

void Showbridge::startDiscovery(const std::string& local_ip_address) {
    printf("Showbridge::startDiscovery called with IP: %s\n", local_ip_address.c_str());

    if (discoveryThread.joinable()) {
        // Already running, stop it first
        stopDiscovery();
    }
    stopDiscoveryFlag = false;
    discoveryThread = std::thread(&Showbridge::discoveryThreadLoop, this, local_ip_address);
}

void Showbridge::stopDiscovery() {
    if (discoveryThread.joinable()) {
        stopDiscoveryFlag = true;
        discoveryThread.join();
    }
}

void Showbridge::discoveryThreadLoop(const std::string& local_ip_address) {
    printf("Showbridge::discoveryThreadLoop started for IP: %s\n", local_ip_address.c_str());
    while (!stopDiscoveryFlag) {

        std::vector<DiscoveredDACInfo> current_scan_results = performDACDiscovery(local_ip_address);

        std::lock_guard<std::mutex> lock(dacMutex);
        discoveredDACs.clear(); // Clear and repopulate for simplicity, or implement merge logic
        for (const auto& dac : current_scan_results) {
            discoveredDACs.push_back(dac);
        }

        // After updating discoveredDACs, re-validate selectedDeviceIndex based on selectedDeviceIdentifier
        if (!selectedDeviceIdentifier.empty()) {
            int found_idx = -1;
            for (size_t i = 0; i < discoveredDACs.size(); ++i) {
                char current_identifier_buffer[64];
                snprintf(current_identifier_buffer, sizeof(current_identifier_buffer), "%s_%d",
                         discoveredDACs[i].ip_address.c_str(),
                         discoveredDACs[i].dac_information.channel);
                if (selectedDeviceIdentifier == current_identifier_buffer) {
                    found_idx = i;
                    break;
                }
            }
            selectedDeviceIndex = found_idx; // Update selectedDeviceIndex
        } else {
            selectedDeviceIndex = -1; // No identifier, so no selection
        }

        // Sleep for 1 second, but check stop flag periodically
        for (int i = 0; i < 10; ++i) { // Check every 100ms for 1 second
            if (stopDiscoveryFlag) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

std::vector<std::string> Showbridge::getNetworkInterfaces() {
    std::vector<std::string> interfaces;
    PIP_ADAPTER_INFO pAdapterInfo;
    PIP_ADAPTER_INFO pAdapter = NULL;
    DWORD dwRetVal = 0;
    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);

    pAdapterInfo = (IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));
    if (pAdapterInfo == NULL) {
        printf("Error allocating memory for GetAdaptersInfo\n");
        return interfaces;
    }

    // Make an initial call to GetAdaptersInfo to get the necessary size into the ulOutBufLen variable
    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO*)malloc(ulOutBufLen);
        if (pAdapterInfo == NULL) {
            printf("Error allocating memory for GetAdaptersInfo (overflow)\n");
            return interfaces;
        }
    }

    if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR) {
        pAdapter = pAdapterInfo;
        while (pAdapter) {
            // Only consider IPv4 interfaces with an IP address
            if (pAdapter->IpAddressList.IpAddress.String[0] != '\0') {
                interfaces.push_back(pAdapter->IpAddressList.IpAddress.String);
            }
            pAdapter = pAdapter->Next;
        }
    } else {
        printf("GetAdaptersInfo failed with error: %d\n", dwRetVal);
    }

    if (pAdapterInfo) {
        free(pAdapterInfo);
    }
    return interfaces;
}



void Showbridge::selectDAC(int index) {
    std::lock_guard<std::mutex> lock(dacMutex); // Add mutex protection
    if (index >= 0 && index < discoveredDACs.size()) {
        selectedDeviceIndex = index;
        // Construct unique identifier for the selected DAC
        char identifier_buffer[64];
        snprintf(identifier_buffer, sizeof(identifier_buffer), "%s_%d",
                 discoveredDACs[selectedDeviceIndex].ip_address.c_str(),
                 discoveredDACs[selectedDeviceIndex].dac_information.channel);
        selectedDeviceIdentifier = identifier_buffer;
        printf("Selected DAC: %s (Ch: %d), Identifier: %s\n", discoveredDACs[selectedDeviceIndex].ip_address.c_str(), discoveredDACs[selectedDeviceIndex].dac_information.channel, selectedDeviceIdentifier.c_str());
    } else {
        printf("Invalid DAC selection index.\n");
        selectedDeviceIndex = -1;
        selectedDeviceIdentifier = ""; // Clear identifier on invalid selection
    }
}

bool Showbridge::sendFrame(const frame_buffer& fb) {
    std::lock_guard<std::mutex> lock(dacMutex); // Add mutex protection
    if (selectedDeviceIndex == -1) {
        printf("No DAC selected. Cannot send frame.\n");
        return false;
    }

    // Get selected DAC's channel for source port and header byte
    unsigned char channel_id_byte = 0;
    unsigned short source_port = 0;
    if (selectedDeviceIndex >= 0 && selectedDeviceIndex < discoveredDACs.size()) {
        unsigned char channel = discoveredDACs[selectedDeviceIndex].dac_information.channel;
        if (channel == 1) {
            source_port = CLIENT_PORT_1; // Port for Channel 1
            channel_id_byte = 0x01; // Channel ID byte for Channel 1
        } else if (channel == 2) {
            source_port = CLIENT_PORT_2; // Port for Channel 2
            channel_id_byte = 0x00; // Channel ID byte for Channel 2
        } else {
            printf("Showbridge::sendFrame - Warning: Unknown channel %d, using default ephemeral port.\n", channel);
            // If channel is unknown, do not bind to a specific port, let OS choose ephemeral.
            // The bind block below will be skipped if source_port is 0.
        }
    }

    sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DAC_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(discoveredDACs[selectedDeviceIndex].ip_address.c_str());

    // Calculate total data size (frame_buffer)
    int total_data_size = sizeof(frame_buffer);

    // Ensure the buffer is exactly 4604 bytes for the application data payload
    unsigned char app_data_buffer[4604];

    for (int sub_frame_id = 0; sub_frame_id < 3; ++sub_frame_id) {
        SOCKET send_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (send_socket == INVALID_SOCKET) {
            printf("Showbridge::sendFrame - Error creating socket for sub-frame %d: %d\n", sub_frame_id, WSAGetLastError());
            return false;
        }

        // Bind the socket to the dynamically selected local source port
        if (source_port != 0) { // Only bind if a specific port is determined
            sockaddr_in local_addr;
            local_addr.sin_family = AF_INET;
            local_addr.sin_port = htons(source_port);
            local_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Bind to any local IP

            if (bind(send_socket, (sockaddr*)&local_addr, sizeof(local_addr)) == SOCKET_ERROR) {
                printf("Showbridge::sendFrame - Error binding send socket to port %hu for sub-frame %d: %d\n", source_port, sub_frame_id, WSAGetLastError());
                closesocket(send_socket);
                return false;
            }
        }

        // Construct the 4-byte application header for this sub-frame
        unsigned char header[4];
        header[0] = 0x03; // Fixed
        header[1] = (unsigned char)sub_frame_id; // 0x00, 0x01, 0x02
        header[2] = globalSequenceCounter; // Global sequence counter
        header[3] = channel_id_byte; // Channel ID (0x01 for Ch1, 0x00 for Ch2)

        // Copy the 4-byte header
        memcpy(app_data_buffer, header, sizeof(header));

        // Copy the frame_buffer data after the header
        memcpy(app_data_buffer + sizeof(header), &fb, sizeof(frame_buffer));

        int bytes_sent = sendto(send_socket, (const char*)app_data_buffer, sizeof(app_data_buffer), 0, (sockaddr*)&dest_addr, sizeof(dest_addr));
        if (bytes_sent == SOCKET_ERROR) {
            printf("Showbridge::sendFrame - Error sending packet for sub-frame %d: %d\n", sub_frame_id, WSAGetLastError());
            closesocket(send_socket);
            return false;
        }

        closesocket(send_socket);
    }

    // Increment global sequence counter after sending all 3 packets for the frame
    globalSequenceCounter++;

    return true;
}