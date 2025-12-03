#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <filesystem>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <random>
#include "BitfieldManager.h"
#include "messageSender.h"
#include "FileHandling.h"
#include "logger.h"

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
    PeerRelationship(SOCKET ts, BitfieldManager tb, int ti, bool cm, bool ct, bool im, bool it):
    theirSocket(ts), theirBitfield(tb), theirID(ti), chokedMe(cm), chokedThem(ct), interestedInMe(im), interestedInThem(it) {}
    SOCKET theirSocket;
    BitfieldManager theirBitfield;
    int theirID;
    bool chokedMe;
    bool chokedThem;
    bool interestedInMe;
    bool interestedInThem;
    uint64_t bytesDownloaded = 0;
    uint64_t lastDownloaded = 0;
};

class PeerProcess {
public:
    explicit PeerProcess(int peerId);
    void start();

    Common common;
    BitfieldManager bitfield;
    FileHandling fileHandler;
    Logger logger;

private:
    int ID;
    PeerInfo selfInfo;
    std::vector<PeerInfo> allPeers;
    std::vector<PeerInfo> neighborPeers;
    std::unordered_map<int, PeerRelationship> relationships;
    std::unordered_map<int, int> requests;

    void readCommon();
    void readPeerInfo();
    void bitfieldInit();
    size_t getNumPieces() const;
    void fileHandlinitInit();
    void loggerInit();

    void startListen();
    void handleConnection(SOCKET clientSocket, bool receiver);
    void connectToEarlierPeers();
    void connectionMessageLoop(SOCKET sock, int remotePeerId);
    int getPieceToRequest(int peerId);

    void handleChoke(int peerId);
    void handleUnchoke(int peerId);
    void handleInterested(int peerId);
    void handleNotInterested(int peerId);
    void handleHave(int peerId, const std::vector<unsigned char>& payload);
    void handleBitfield(int peerId, const std::vector<unsigned char>& payload);
    void handleRequest(int peerId, const std::vector<unsigned char>& payload);
    void handlePiece(int peerId, const std::vector<unsigned char>& payload);

    std::mutex peersMutex;
    std::atomic<int> optimisticUnchokedPeer{-1};
    std::thread preferredNeighborThread;
    std::thread optimisticUnchokeThread;

    std::atomic<bool> schedulerStop{false};
    std::condition_variable_any schedulerCv;
    std::mutex schedulerMutex;

    // algorithms for choosing preferred neighbors and optimistic unchoking
    void findPreferredNeighbor();
    void startOptimisticUnchoke();

    // when all other peers have the complete file
    bool allPeersHave();
};

