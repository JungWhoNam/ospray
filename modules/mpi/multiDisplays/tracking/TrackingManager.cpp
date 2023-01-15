#include "TrackingManager.h"

#include <iostream>

TrackingManager::TrackingManager(nlohmann::ordered_json config) {
    tcpSocket = nullptr;
    updated = false;

     // initialize the tracking manager
    if (config.contains("ipAddress"))
        ipAddress = config["ipAddress"];
    
    if (config.contains("portNumber"))
      portNumber = config["portNumber"];

    if (config.contains("positionOffset")) {
        std::vector<float> vals = config["positionOffset"];
        positionOffset[0] = vals[0];
        positionOffset[1] = vals[1];
        positionOffset[2] = vals[2];
    }

    if (config.contains("multiplyBy")) {
        std::vector<float> vals = config["multiplyBy"];
        multiplyBy[0] = vals[0];
        multiplyBy[1] = vals[1];
        multiplyBy[2] = vals[2];
    }

    if (config.contains("leaningAngleThreshold")) {
        leaningAngleThreshold = config["leaningAngleThreshold"];
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
    // set the default state
    for (int i = 0; i < K4ABT_JOINT_COUNT; i++) {
        state.positions[i] = vec3f(0.f);
        state.confidences[i] = K4ABT_JOINT_CONFIDENCE_NONE;
    }
    state.mode = INTERACTION_NONE;
    state.leaningAngle = 0.0f;
    state.leaningDir = vec3f(0.0f);

    // parse the message (which is supposed to be in a JSON format)
    nlohmann::ordered_json j; 
    try {
        j = nlohmann::ordered_json::parse(message);
    } catch (nlohmann::json::exception& e) {
        std::cout << "Parse exception: " << e.what() << std::endl;
        j = nullptr;
    }

    // check if the tracking data is reliable.
    if (j == nullptr || j.size() != K4ABT_JOINT_COUNT) {
        return;
    }

    // update positions and confidence levels
    for (int i = 0; i < K4ABT_JOINT_COUNT; i++) {
        if (j[i].contains("pos")) {
            std::vector<float> vals = j[i]["pos"];
            state.positions[i].x = vals[0] * multiplyBy[0] + positionOffset[0];
            state.positions[i].y = vals[1] * multiplyBy[1] + positionOffset[1];
            state.positions[i].z = vals[2] * multiplyBy[2] + positionOffset[2];
        }
        if (j[i].contains("conf")) {
            state.confidences[i] = j[i]["conf"]; 
        }
    }

    // check if the tracking data is reliable for further detections.
    if (state.confidences[K4ABT_JOINT_SPINE_NAVEL] < K4ABT_JOINT_CONFIDENCE_LOW ||
        state.confidences[K4ABT_JOINT_HAND_LEFT] < K4ABT_JOINT_CONFIDENCE_LOW ||
        state.confidences[K4ABT_JOINT_HAND_RIGHT] < K4ABT_JOINT_CONFIDENCE_LOW ||
        state.confidences[K4ABT_JOINT_NECK] < K4ABT_JOINT_CONFIDENCE_LOW) {
        return;
    }

    // compute additional states (angle and direction)
    vec3f spine = state.positions[K4ABT_JOINT_NECK] - state.positions[K4ABT_JOINT_SPINE_NAVEL];
    vec3f normal (0.0f, 1.0f, 0.0f);
    state.leaningAngle = acos(dot(spine, normal) / (length(spine) * length(normal))) / M_PI * 180.f;
    state.leaningDir = spine - dot(spine, normal) / length(normal) * normal;
    state.leaningDir.x *= leaningDirScaleFactor[0];
    state.leaningDir.y *= leaningDirScaleFactor[1];
    state.leaningDir.z *= leaningDirScaleFactor[2];

    // compute additional states (mode)
    bool leftHandUp = state.positions[K4ABT_JOINT_SPINE_NAVEL].y < state.positions[K4ABT_JOINT_HAND_LEFT].y;
    bool rightHandUp = state.positions[K4ABT_JOINT_SPINE_NAVEL].y < state.positions[K4ABT_JOINT_HAND_RIGHT].y;
    state.mode = (leftHandUp && rightHandUp && state.leaningAngle > leaningAngleThreshold) ? INTERACTION_FLYING : INTERACTION_IDLE;
}

// show "important" tracking information in a readable format
std::string TrackingManager::getResultsInReadableForm() {
    std::string result;

    result += "headPos.x: " + std::to_string(state.positions[K4ABT_JOINT_HEAD].x) + "\n";
    result += "headPos.y: " + std::to_string(state.positions[K4ABT_JOINT_HEAD].y) + "\n";
    result += "headPos.z: " + std::to_string(state.positions[K4ABT_JOINT_HEAD].z) + "\n";

    if (state.mode == INTERACTION_NONE) result += "mode: NONE\n";
    else if (state.mode == INTERACTION_IDLE) result += "mode: IDLE\n";
    else if (state.mode == INTERACTION_FLYING) result += "mode: FLYING\n";

    result += "leaningAngle: " + std::to_string(state.leaningAngle) + " °\n";

    result += "leaningDir.x: " + std::to_string(state.leaningDir.x) + "\n";
    result += "leaningDir.y: " + std::to_string(state.leaningDir.y) + " (proj. to x-z plane)\n";
    result += "leaningDir.z: " + std::to_string(state.leaningDir.z);
    
    return result;
}