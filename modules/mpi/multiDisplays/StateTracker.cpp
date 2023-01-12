#include "StateTracker.h"

#include <iostream>

StateTracker::StateTracker(std::string ipAddress, uint portNumber) {
    tcpSocket = nullptr;
    this->ipAddress = ipAddress;
    this->portNumber = portNumber;
    state = nullptr;
    updated = false;
}

StateTracker::~StateTracker() {
    this->close();
}

void StateTracker::start() {
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
        try {
            state = nlohmann::ordered_json::parse(message);
        } catch (nlohmann::json::exception& e) {
            std::cout << "Parse exception: " << e.what() << std::endl;
            state = nullptr;
        }
        updated = true;
    };
    
    // On socket closed:
    tcpSocket->onSocketClosed = [&](int errorCode){
        std::cout << "Connection closed: " << errorCode << std::endl;
        delete tcpSocket;
        tcpSocket = nullptr;
        state = nullptr;
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

void StateTracker::close() {
    if (tcpSocket == nullptr)
        return;

    tcpSocket->Close();
}

bool StateTracker::isRunning() {
    return tcpSocket != nullptr && !tcpSocket->isClosed;
}

bool StateTracker::isUpdated() {
    return updated;
}

nlohmann::ordered_json StateTracker::pollState() {
    std::lock_guard<std::mutex> guard(mtx);
    
    updated = false;
    return state;
}