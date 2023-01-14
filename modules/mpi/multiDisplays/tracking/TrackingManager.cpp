#include "TrackingManager.h"

#include <iostream>

TrackingManager::TrackingManager(nlohmann::ordered_json config) {
    tcpSocket = nullptr;
    updated = false;

     // initialize the tracking manager
    ipAddress = "localhost";
    if (config.contains("ipAddress"))
        ipAddress = config["ipAddress"];
    
    portNumber = 8888;
    if (config.contains("portNumber"))
      portNumber = config["portNumber"];

    positionOffset[0] = 0.0f;
    positionOffset[1] = 0.0f;
    positionOffset[2] = 0.0f;
    if (config.contains("positionOffset")) {
        std::vector<float> vals = config["positionOffset"];
        positionOffset[0] = vals[0];
        positionOffset[1] = vals[1];
        positionOffset[2] = vals[2];
    }

    // Kinect - right-hand, y-down, z-forward, in milli-meters
    // OSPRay - right-hand, y-up, z-forward, in meters
    multiplyBy[0] = -0.001f;
    multiplyBy[1] = -0.001f;
    multiplyBy[2] = 0.001f;
    if (config.contains("multiplyBy")) {
        std::vector<float> vals = config["multiplyBy"];
        multiplyBy[0] = vals[0];
        multiplyBy[1] = vals[1];
        multiplyBy[2] = vals[2];
    }
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
            pos.x = vals[0] * multiplyBy[0] + positionOffset[0];
            pos.y = vals[1] * multiplyBy[1] + positionOffset[1];
            pos.z = vals[2] * multiplyBy[2] + positionOffset[2];
        }
        if (j != nullptr && j.size() ==  K4ABT_JOINT_COUNT && j[i].contains("conf")) {
            conf = j[i]["conf"]; 
        }
        
        state.positions[i] = pos;
        state.confidences[i] = conf;
    }
}