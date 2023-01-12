#pragma once

#include "tcpsocket.hpp"
#include "json.hpp"

#include <string>
#include <mutex>


class TrackingManager
{
public:
    TrackingManager(std::string ipAddress, uint portNumber);
    ~TrackingManager();

    void start();
    void close();
    bool isRunning();
    bool isUpdated();

    nlohmann::ordered_json pollState();

private:
    TCPSocket *tcpSocket;
    std::string ipAddress;
    uint portNumber;

    nlohmann::ordered_json state;
    bool updated;
    
    std::mutex mtx;
};