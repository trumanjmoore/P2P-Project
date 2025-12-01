#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "BitfieldManager.h"
#include "messageSender.h"

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

struct PeerRelationship {
    PeerRelationship(SOCKET ts, BitfieldManager tb, bool cm, bool ct, bool im, bool it):
        theirSocket(ts), theirBitfield(tb), chokedMe(cm), chokedThem(ct), interestedInMe(im), interestedInThem(it) {}
    SOCKET theirSocket;
    BitfieldManager theirBitfield;
    bool chokedMe;
    bool chokedThem;
    bool interestedInMe;
    bool interestedInThem;
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
    std::vector<PeerInfo> neighborPeers;
    std::unordered_map<int, PeerRelationship> relationships;

    void readCommon();
    void readPeerInfo();
    void bitfieldInit();
    size_t getNumPieces() const;

    void startListen();
    void handleConnection(SOCKET clientSocket, bool receiver);
    void connectToEarlierPeers();
    void connectionMessageLoop(SOCKET sock, int remotePeerId);

    void handleChoke(int peerId);
    void handleUnchoke(int peerId);
    void handleInterested(int peerId);
    void handleNotInterested(int peerId);
    void handleHave(int peerId, const std::vector<unsigned char>& payload);
    void handleBitfield(int peerId, const std::vector<unsigned char>& payload);
    void handleRequest(int peerId, const std::vector<unsigned char>& payload);
    void handlePiece(int peerId, const std::vector<unsigned char>& payload);

    bool allPeersHave();
};

