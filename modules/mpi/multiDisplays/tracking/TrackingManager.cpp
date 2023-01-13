#include "TrackingManager.h"

#include <iostream>

TrackingManager::TrackingManager(std::string ipAddress, uint portNumber) {
    tcpSocket = nullptr;
    this->ipAddress = ipAddress;
    this->portNumber = portNumber;
    updated = false;
}

TrackingManager::~TrackingManager() {
    this->close();
}

void TrackingManager::start() {
    if (tcpSocket != nullptr) {
        std::cout << "Connection has already been set." << std::endl;
        return;
    }

    // Initialize socket.
    tcpSocket = new TCPSocket([](int errorCode, std::string errorMessage){
        std::cerr << "Socket creation error:" << errorCode << " : " << errorMessage << std::endl;
    });

    // Start receiving from the host.
    tcpSocket->onMessageReceived = [&](std::string message) {
        std::lock_guard<std::mutex> guard(mtx);

        // std::cout << "Message from the Server: " << message << std::endl << std::flush;
        updateState(message);
        updated = true;
    };
    
    // On socket closed:
    tcpSocket->onSocketClosed = [&](int errorCode){
        std::cout << "Connection closed: " << errorCode << std::endl;
        delete tcpSocket;
        tcpSocket = nullptr;
        updateState("{}");
        updated = true;
    };

    // Connect to the host.
    tcpSocket->Connect(ipAddress, portNumber, [] {
        std::cout << "Connected to the server successfully." << std::endl;
    },
    [&](int errorCode, std::string errorMessage){ // Connection failed
        std::cout << errorCode << " : " << errorMessage << std::endl;
        tcpSocket->Close();
    });
}

void TrackingManager::close() {
    if (tcpSocket == nullptr)
        return;

    tcpSocket->Close();
}

bool TrackingManager::isRunning() {
    return tcpSocket != nullptr && !tcpSocket->isClosed;
}

bool TrackingManager::isUpdated() {
    return updated;
}

TrackingState TrackingManager::pollState() {
    std::lock_guard<std::mutex> guard(mtx);
    
    updated = false;
    return state;
}

void TrackingManager::updateState(std::string message) {
    nlohmann::ordered_json j; 
    try {
        j = nlohmann::ordered_json::parse(message);
    } catch (nlohmann::json::exception& e) {
        std::cout << "Parse exception: " << e.what() << std::endl;
        j = nullptr;
    }
    
    for (int i = 0; i < K4ABT_JOINT_COUNT; i++) {
        rkcommon::math::vec3f pos(0.);
        int conf = K4ABT_JOINT_CONFIDENCE_NONE;

        if (j != nullptr && j.size() ==  K4ABT_JOINT_COUNT && j[i].contains("pos")) {
            std::vector<float> vals = j[i]["pos"];
            pos.x = vals[0];
            pos.y = vals[1];
            pos.z = vals[2];
        }
        if (j != nullptr && j.size() ==  K4ABT_JOINT_COUNT && j[i].contains("conf")) {
            conf = j[i]["conf"]; 
        }
        
        state.positions[i] = pos;
        state.confidences[i] = conf;
    }
}