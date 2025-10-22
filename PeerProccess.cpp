#include "PeerProccess.h"

// Initiate with the peer id
PeerProcess::PeerProcess(int peerId) {
    ID = peerId;
}

// Start the peerProcess
void PeerProcess::start() {
    readCommon();
    readPeerInfo();
    bitfieldInit();
}
// Read the Common.cfg file and place the information in the common strut
void PeerProcess::readCommon() {
    std::ifstream commonFile("Common.cfg");
    std::string line;
    std::string key, value;

    while (std::getline(commonFile, line)) {
        std::istringstream stream(line);
        stream >> key;
        stream >> value;
        if (key == "NumberOfPreferredNeighbors")
            common.numberOfPreferredNeighbors = std::stoi(value);
        else if (key == "UnchokingInterval")
            common.unchokingInterval = std::stoi(value);
        else if (key == "OptimisticUnchokingInterval")
            common.optimisticUnchokingInterval = std::stoi(value);
        else if (key == "FileName")
            common.fileName = value;
        else if (key == "FileSize")
            common.fileSize = std::stoi(value);
        else if (key == "PieceSize")
            common.pieceSize = std::stoi(value);
    }

    commonFile.close();
}

// Read the PeerIndo.cfg file and find the info that matches the ID and fill in the selfInfo struct
void PeerProcess::readPeerInfo() {
    std::ifstream peerInfoFile("PeerInfo.cfg");
    std::string line;
    int id;

    while (std::getline(peerInfoFile, line)) {
        std::istringstream stream(line);
        stream >> id;
        if (id == ID) {
            selfInfo.peerId = id;
            stream >> selfInfo.hostName;
            stream >> selfInfo.port;
            stream >> selfInfo.has;
            break;
        }

        peerInfoFile.close();
    }
}

// Create the bitfield with the proper size
void PeerProcess::bitfieldInit() {
    bitfield = BitfieldManager(getNumPieces());
}

// Get the number of pieces from the common struct pieces
size_t PeerProcess::getNumPieces() const {
    return (common.fileSize + common.pieceSize - 1) / common.pieceSize;
}
