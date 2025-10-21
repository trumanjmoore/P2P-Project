#include "logger.h"

std::string Logger::getTimestamp()
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::string time_str = std::ctime(&now_time);
    if (!time_str.empty() && time_str.back() == '\n')
    {
        time_str.pop_back();
    }

    return time_str;
}

void Logger::writeLog(const std::string& message)
{
    std::lock_guard<std::mutex> guard(logMutex);
    if (logFile.is_open())
    {
        logFile << message << std::endl;
        // immediately writes but to disk but might be bad for performance(?)
        // should leave for testing
        logFile.flush();
    }
}

Logger::Logger(int peerID) : peerID(peerID)
{
    // if the directory doesn't exist, we create it
    std::string dir = std::filesystem::current_path().string() + "/project";
    std::filesystem::create_directories(dir);
    
    // if the file doesn't exist, create it
    // if it does exist, clear it
    std::string filename = dir + "/log_peer_" + std::to_string(peerID) + ".log";
    logFile.open(filename, std::ios::trunc);

    if (!logFile.is_open())
    {
        std::cerr << "Unable to open log file for Peer " << peerID << std::endl;
    }
}

Logger::~Logger()
{
    if (logFile.is_open())
    {
        logFile.close();
    }
}

void Logger::logMakeConnection(int otherPeerID)
{
    std::string message = getTimestamp() + ": Peer " + std::to_string(peerID) +
    " makes a connection to Peer " + std::to_string(otherPeerID) + ".";
    writeLog(message);
}

void Logger::logConnectedFrom(int otherPeerID)
{
    std::string message = getTimestamp() + ": Peer " + std::to_string(peerID) +
    " is connected from Peer " + std::to_string(otherPeerID) + ".";
    writeLog(message);
}

void Logger::logChangePreferredNeighbors(const std::vector<int>& neighbors)
{
    std::string message = getTimestamp() + ": Peer " + std::to_string(peerID) + 
    " has the preferred neighbors ";
    if (neighbors.size() >= 1)
    {
        message = message + std::to_string(neighbors[0]);
        for (size_t i = 1; i < neighbors.size(); i++)
        {
            message = message + "," + std::to_string(neighbors[i]);
        }
    }
    message = message + ".";
    writeLog(message);
}

void Logger::logChangeOptimisticUnchoke(int neighborID)
{
    std::string message = getTimestamp() + ": Peer " + std::to_string(peerID) + 
    " has the optimistically unchoked neighbor " + std::to_string(neighborID) + ".";
    writeLog(message);
}

void Logger::logUnchokedBy(int otherPeerID)
{
    std::string message = getTimestamp() + ": Peer " + std::to_string(peerID) + 
    " is unchoked by " + std::to_string(otherPeerID) + ".";
    writeLog(message);
}

void Logger::logChokedBy(int otherPeerID)
{
    std::string message = getTimestamp() + ": Peer " + std::to_string(peerID) + 
    " is choked by " + std::to_string(otherPeerID) + ".";
    writeLog(message);
}

void Logger::logReceivedHave(int fromPeerID, int pieceIndex)
{
    std::string message = getTimestamp() + ": Peer " + std::to_string(peerID) + 
    " received the 'have' message from " + std::to_string(fromPeerID) + " for the piece " + std::to_string(pieceIndex) + ".";
    writeLog(message);
}

void Logger::logReceivedInterested(int fromPeerID)
{
    std::string message = getTimestamp() + ": Peer " + std::to_string(peerID) + 
    " received the 'interested' message from " + std::to_string(fromPeerID) + ".";
    writeLog(message);
}

void Logger::logReceivedNotInterested(int fromPeerID)
{
    std::string message = getTimestamp() + ": Peer " + std::to_string(peerID) + 
    " received the 'not interested' message from " + std::to_string(fromPeerID) + ".";
    writeLog(message);
}

void Logger::logDownloadedPiece(int fromPeerID, int pieceIndex, int totalPieces)
{
    std::string message = getTimestamp() + ": Peer " + std::to_string(peerID) + 
    " has downloaded the piece " + std::to_string(pieceIndex) + " from " + std::to_string(fromPeerID) +
    ". Now the number of pieces it has is " + std::to_string(totalPieces) + ".";
    writeLog(message);
}

void Logger::logCompletedDownload()
{
    std::string message = getTimestamp() + ": Peer " + std::to_string(peerID) + " has downloaded the complete file.";
}