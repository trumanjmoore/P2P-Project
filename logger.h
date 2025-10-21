#pragma once

#include <string>
#include <vector>
#include <fstream>
// using a mutex to make sure only a single log can be done at a given point
#include <mutex>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sstream>
#include <filesystem>

class Logger
{
    private:
    int peerID;
    std::ofstream logFile;
    std::mutex logMutex;
    std::string getTimestamp();

    void writeLog(const std::string& message);

    public:
    explicit Logger(int peerID);
    ~Logger();

    void logMakeConnection(int otherPeerID);
    void logConnectedFrom(int otherPeerID);

    void logChangePreferredNeighbors(const std::vector<int>& neighbors);
    void logChangeOptimisticUnchoke(int neighborID);

    void logUnchokedBy(int otherPeerID);
    void logChokedBy(int otherPeerID);

    void logReceivedHave(int fromPeerID, int pieceIndex);
    void logReceivedInterested(int fromPeerID);
    void logReceivedNotInterested(int fromPeerID);

    void logDownloadedPiece(int fromPeerID, int pieceIndex, int totalPieces);
    void logCompletedDownload();
};