#pragma once

#include "tcpsocket.hpp"
#include "json.hpp"

#include <string>
#include <mutex>


class StateTracker
{
public:
    StateTracker(std::string ipAddress, uint portNumber);
    ~StateTracker();

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