#include <string>
#include <vector>
#include <fstream>
#include "BitfieldManager.h"
#include <sstream>
#include <thread>
#include <unistd.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma once

// information from PeerInfo.cfg
struct PeerInfo{
    int peerId;
    std::string hostName;
    int port;
    bool has;
};

// information from Common.cfg
struct Common{
    int numberOfPreferredNeighbors;
    int unchokingInterval;
    int optimisticUnchokingInterval;
    std::string fileName;
    int fileSize;
    int pieceSize;
};

class PeerProcess {
public:
    explicit PeerProcess(int peerId);
    void start();

    Common common;
    BitfieldManager bitfield;

private:
    int ID;
    PeerInfo selfInfo;
    std::vector<PeerInfo> allPeers;
    std::vector<PeerInfo> connectedPeers;

    void readCommon();
    void readPeerInfo();
    void bitfieldInit();
    size_t getNumPieces() const;

    void startListen();
    void handleIncomingConnection(SOCKET clientSocket, std::string clientIP) const;
    void connectToEarlierPeers();
    void performHandshakes();
    void startMessageLoop();

    bool allPeersHaveFile();
};

