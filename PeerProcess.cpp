#include "PeerProcess.h"
#include "messageSender.h"

// initiate with the peer id
PeerProcess::PeerProcess(int peerId) {
    ID = peerId;
}

// start the peerProcess
void PeerProcess::start() {
    readCommon();
    readPeerInfo();
    bitfieldInit();
}
// read the Common.cfg file and place the information in the common strut
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

// read the PeerIndo.cfg file and find the info that matches the ID and fill in the selfInfo struct
void PeerProcess::readPeerInfo() {
    std::ifstream peerInfoFile("PeerInfo.cfg");
    std::string line;
    int id;

    while (std::getline(peerInfoFile, line)) {
        std::istringstream stream(line);
        stream >> id;
        // if the id matches then fill in the self info
        if (id == ID) {
            selfInfo.peerId = id;
            stream >> selfInfo.hostName;
            stream >> selfInfo.port;
            stream >> selfInfo.has;
        }
        // else just add it as another peer
        else{
            PeerInfo peer;
            peer.peerId = id;
            stream >> peer.hostName;
            stream >> peer.port;
            stream >> peer.has;
            allPeers.push_back(peer);
        }
        peerInfoFile.close();
    }
}

// create the bitfield with the proper size
void PeerProcess::bitfieldInit() {
    bitfield = BitfieldManager(getNumPieces(), selfInfo.has);
}

// get the number of pieces from the common struct pieces
size_t PeerProcess::getNumPieces() const {
    return (common.fileSize + common.pieceSize - 1) / common.pieceSize;
}

void PeerProcess::startListen() {
    // start thread
    std::thread listenerThread([this]() {

        // initialize the winsock
        static WSADATA wsaData;
        int wsaerr = WSAStartup(MAKEWORD(2, 0), &wsaData);
        if (wsaerr)
            exit(1);

        // initialize the server socket
        SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSocket == INVALID_SOCKET) {
            std::cerr << "Peer " << ID << " ERROR: Could not create socket" << std::endl;
            WSACleanup(); // have to clean up winsock
            return;
        }

        // allow reuse of the port
        BOOL opt = TRUE;
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char *) &opt, sizeof(opt));

        sockaddr_in service{};
        service.sin_family = AF_INET;
        service.sin_port = htons(selfInfo.port);
        service.sin_addr.s_addr = INADDR_ANY;

        // try to bind on peer port
        if (bind(serverSocket, (SOCKADDR *) &service, sizeof(service)) == SOCKET_ERROR) {
            std::cerr << "Peer " << ID << " ERROR: bind() failed on port" << selfInfo.port << std::endl;
            closesocket(serverSocket);
            WSACleanup();
            return;
        }

        // initiate listening
        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Peer " << ID << " ERROR: listen() failed" << std::endl;
            closesocket(serverSocket);
            WSACleanup();
            return;
        }

        // socket is successfully listening for other peers
        std::cout << "Peer " << ID << "now listening from port " << selfInfo.port << std::endl;

        // listening loop
        while (true) {
            sockaddr_in clientInfo{};
            int clientInfoSize = sizeof(clientInfo);

            // try to accept incoming message from other peer
            SOCKET clientSocket = accept(serverSocket, (SOCKADDR*)&clientInfo, &clientInfoSize);

            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "Peer " << ID << " ERROR: accept() failed" << std::endl;
                continue;
            }

            // get the IP
            char clientIP[32];
            inet_ntop(AF_INET, &(clientInfo.sin_addr), clientIP, sizeof(clientIP));

            // go handle the connection
            std::thread(&PeerProcess::handleIncomingConnection, this, clientSocket, std::string(clientIP)).detach();
        }

        closesocket(serverSocket);
        WSACleanup();
    });

    listenerThread.detach();
}

void PeerProcess::handleIncomingConnection(SOCKET clientSocket, std::string clientIP) const{
    // connection accepted
    std::cout << "Peer " << ID << " accepted connection from " << clientIP << std::endl;

    // confirm handshake
    unsigned char handshake[32];
    int received = recv(clientSocket, (char*)handshake, 32, MSG_WAITALL);
    if (received != 32) {
        std::cerr << "Peer " << ID << " ERROR: Invalid handshake received" << std::endl;
        closesocket(clientSocket);
        return;
    }

    // validate header
    const char expectedHeader[19] = "P2PFILESHARINGPROJ";

    if (memcmp(handshake, expectedHeader, 18) != 0) {
        std::cerr << "Peer " << ID << " ERROR: Invalid header" << std::endl;
        closesocket(clientSocket);
        return;
    }

    // get the other peer's ID
    int otherPeerId;
    memcpy(&otherPeerId, handshake + 28, 4);
    otherPeerId = ntohl(otherPeerId);
    std::cout << "Peer " << ID << " received valid handshake from peer "<< otherPeerId << std::endl;

    // send handshake back
    // messageSender

}

void PeerProcess::connectToEarlierPeers() {

}

void PeerProcess::performHandshakes() {

}

void PeerProcess::startMessageLoop() {

}

bool PeerProcess::allPeersHaveFile() {
    return false;
}
