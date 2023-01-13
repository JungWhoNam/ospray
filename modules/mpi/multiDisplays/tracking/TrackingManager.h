#pragma once

#include "tcpsocket.hpp"
#include "json.hpp"
#include "TrackingUtil.h"
#include "rkcommon/math/vec.h"

#include <string>
#include <mutex>


struct TrackingState {
    rkcommon::math::vec3f positions[K4ABT_JOINT_COUNT];
    int confidences[K4ABT_JOINT_COUNT];

    TrackingState() {
        for (int i = 0; i < K4ABT_JOINT_COUNT; i++) {
            positions[i] = rkcommon::math::vec3f(0.);
            confidences[i] = K4ABT_JOINT_CONFIDENCE_NONE;
        }
    }
};

class TrackingManager
{
public:
    TrackingManager(std::string ipAddress, uint portNumber);
    ~TrackingManager();

    void start();
    void close();
    bool isRunning();
    bool isUpdated();

    TrackingState pollState();

private:
    void updateState(std::string message);

    TCPSocket *tcpSocket;
    std::string ipAddress;
    uint portNumber;

    TrackingState state;
    bool updated;
    
    std::mutex mtx;
};