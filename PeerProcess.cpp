#include "PeerProcess.h"

// TODO: finish the handling stuff
// TODO: Implement the choosing neighbors stuff

// initiate with the peer id
PeerProcess::PeerProcess(int peerId) {
    ID = peerId;
}

// start the peerProcess
void PeerProcess::start() {
    // initializers
    readCommon();
    readPeerInfo();
    bitfieldInit();

    // start peer processes
    startListen();
    connectToEarlierPeers();
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

// start listening for connections from other peers
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
        while(true) {
            sockaddr_in clientInfo{};
            int clientInfoSize = sizeof(clientInfo);

            // try to accept incoming message from other peer
            SOCKET clientSocket = accept(serverSocket, (SOCKADDR*)&clientInfo, &clientInfoSize);

            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "Peer " << ID << " ERROR: accept() failed" << std::endl;
                continue;
            }

            // go handle the connection
            std::thread(&PeerProcess::handleConnection, this, clientSocket, true).detach();
        }

        closesocket(serverSocket);
        WSACleanup();
    });

    listenerThread.detach();
}

// handle the connection process, validate handshake
void PeerProcess::handleConnection(SOCKET clientSocket, bool receiver=true){
    // confirm handshake size
    unsigned char handshake[32];
    int received = recv(clientSocket, (char*)handshake, 32, MSG_WAITALL);
    if (received != 32) {
        std::cerr << "Peer " << ID << " ERROR: Invalid handshake received" << std::endl;
        closesocket(clientSocket);
        return;
    }

    // validate header
    const char expectedHeader[19] = "P2PFILESHARINGPROJ"; // expected handshake header
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

    // if didnt send first handshake, send handshake second
    if(receiver) {
        MessageSender sender(otherPeerId, clientSocket);
        sender.sendHandshake();
    }

    // after connecting and verifying handshake, send bitfield
    MessageSender sender(otherPeerId, clientSocket);
    sender.sendBitfield(bitfield.getBits());

    // null bitfield as placeholder till their bitfield is recieved, if its not then they have nothing anyway
    BitfieldManager nullBitfield(bitfield.getSize(), false);
    PeerRelationship newPeer(clientSocket, nullBitfield, false, false, false, false);
    // add them to the relationships list of connected peers
    relationships.emplace(otherPeerId, newPeer);

    // handle the rest of the message
    std::thread(&PeerProcess::connectionMessageLoop, this, clientSocket, otherPeerId).detach();


}

// start connected to peers with a smaller ID
void PeerProcess::connectToEarlierPeers() {
    // look through all peers but connect with earlier peers
    for (const auto& peer : allPeers) {
        if (peer.peerId >= ID)
            continue;
        std::cout << "Peer " << ID << " attempting connection to Peer "<< peer.peerId << std::endl;

        addrinfo hints{}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        // get host name
        std::string portStr = std::to_string(peer.port);
        if (getaddrinfo(peer.hostName.c_str(), portStr.c_str(), &hints, &result) != 0) {
            std::cerr << "Peer " << ID << " ERROR: getaddrinfo failed.\n";
            continue;
        }

        // open socket
        SOCKET sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (sock == INVALID_SOCKET) {
            std::cerr << "Peer " << ID << " ERROR: socket() failed" << std::endl;
            freeaddrinfo(result);
            continue;
        }

        // attempt to connect to peer
        if (connect(sock, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
            std::cerr << "Peer " << ID << " ERROR: connect() failed to peer " << peer.peerId << std::endl;
            closesocket(sock);
            freeaddrinfo(result);
            continue;
        }
        freeaddrinfo(result);
        std::cout << "Peer " << ID << " connected to Peer " << peer.peerId << std::endl;

        // handle connection with new peer
        std::thread(&PeerProcess::handleConnection, this, sock, false).detach();
    }
}

// keep messaging peers while the connection is open
void PeerProcess::connectionMessageLoop(SOCKET sock, int remotePeerId){
    while (true) {
        // first 4 bytes: message length
        uint32_t netLen;
        int r = recv(sock, (char *) &netLen, sizeof(netLen), MSG_WAITALL);
        if (r <= 0) {
            std::cout << "Peer " << ID << " disconnected from peer " << remotePeerId << std::endl;
            closesocket(sock);
            return;
        }

        // if message is just a keep-alive message
        uint32_t messageLen = ntohl(netLen);
        if (messageLen == 0) {
            std::cout << "Peer " << ID << " received KEEP-ALIVE from peer " << remotePeerId << std::endl;
            continue;
        }

        // next byte is message type
        unsigned char messageType;
        r = recv(sock, (char *) &messageType, 1, MSG_WAITALL);
        if (r <= 0) {
            std::cout << "Peer " << ID << " lost connection to peer " << remotePeerId << std::endl;
            closesocket(sock);
            return;
        }

        // next part is the actual message msglen bytes
        std::vector<unsigned char> payload;
        if (messageLen > 1) {
            payload.resize(messageLen - 1);
            r = recv(sock, (char *) payload.data(), messageLen - 1, MSG_WAITALL);
            if (r <= 0) {
                std::cout << "Peer " << ID << " connection closed while reading payload" << std::endl;
                closesocket(sock);
                return;
            }
        }

        switch (messageType) {
            // choke
            case 0:
                std::cout << "Peer " << ID << " received CHOKE from " << remotePeerId << std::endl;
                handleChoke(remotePeerId);
                break;

            // unchoke
            case 1:
                std::cout << "Peer " << ID << " received UNCHOKE from " << remotePeerId << std::endl;
                handleUnchoke(remotePeerId);
                break;

            // interested
            case 2:
                std::cout << "Peer " << ID << " received INTERESTED from " << remotePeerId << std::endl;
                handleInterested(remotePeerId);
                break;

            // not interested
            case 3:
                std::cout << "Peer " << ID << " received NOT INTERESTED from " << remotePeerId << std::endl;
                handleNotInterested(remotePeerId);
                break;

            // have
            case 4:
                std::cout << "Peer " << ID << " received HAVE from " << remotePeerId << std::endl;
                handleHave(remotePeerId, payload);
                break;

            // bitfield
            case 5:
                std::cout << "Peer " << ID << " received BITFIELD from " << remotePeerId << std::endl;
                handleBitfield(remotePeerId, payload);
                break;

            // request
            case 6:
                std::cout << "Peer " << ID << " received REQUEST from " << remotePeerId << std::endl;
                handleRequest(remotePeerId, payload);
                break;

            // piece
            case 7:
                std::cout << "Peer " << ID << " received PIECE from " << remotePeerId << std::endl;
                handlePiece(remotePeerId, payload);
                break;

            // other message
            default:
                std::cout << "Peer " << ID << " received UNKNOWN message type from" << remotePeerId << std::endl;
                break;
        }
    }
}

void PeerProcess::handleChoke(int peerId){
    relationships.at(peerId).chokedMe = true;
    // TODO: stop sending requests
}

void PeerProcess::handleUnchoke(int peerId){
    relationships.at(peerId).chokedMe = false;
    // TODO: can resume sending requests
}

void PeerProcess::handleInterested(int peerId){
    relationships.at(peerId).interestedInMe = true;
    // TODO: mark as interested
}

void PeerProcess::handleNotInterested(int peerId){
    relationships.at(peerId).interestedInMe = false;
    // TODO: mark as not interested
}

void PeerProcess::handleHave(int peerId, const std::vector<unsigned char>& payload){
    // TODO: update connected peer's bitfield
}

void PeerProcess::handleBitfield(int peerId, const std::vector<unsigned char>& payload){
    relationships.at(peerId).theirBitfield = BitfieldManager::toBits(payload, payload.size());
    // TODO: read the bitfield and see if they have pieces that I do not, if so then mark them as interested
}

void PeerProcess::handleRequest(int peerId, const std::vector<unsigned char>& payload){
    // TODO: send piece
}

void PeerProcess::handlePiece(int peerId, const std::vector<unsigned char>& payload){
    // TODO: update file
}

bool PeerProcess::allPeersHave() {
    // TODO: close connection?
}
