#include <algorithm>
#include <unordered_set>
#include "PeerProcess.h"

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
    fileHandlinitInit();
    loggerInit();

    // start peer processes
    startListen();
    connectToEarlierPeers();

    // choose new neighbors
    findPreferredNeighbor();
    startOptimisticUnchoke();
}
// read the Common.cfg file and place the information in the common strut
void PeerProcess::readCommon() {
    std::ifstream commonFile("Common.cfg");
    if (!commonFile.is_open()) {
        std::cerr << "Failed to open Common.cfg. CWD: " << std::filesystem::current_path() << std::endl;
    }
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
    }
    peerInfoFile.close();
}

// create the bitfield with the proper size
void PeerProcess::bitfieldInit() {
    bitfield = BitfieldManager(getNumPieces(), selfInfo.has);
}

// get the number of pieces from the common struct pieces
size_t PeerProcess::getNumPieces() const {
    return (common.fileSize + common.pieceSize - 1) / common.pieceSize;
}

void PeerProcess::fileHandlinitInit() {
    using std::filesystem::exists;
    fileHandler = FileHandling(std::filesystem::path("."), ID, common.fileName, common.fileSize, common.pieceSize, selfInfo.has == 1);
    fileHandler.init();

    if(selfInfo.has){
        fileHandler.finalize();
    }
}

void PeerProcess::loggerInit() {
    logger.init(ID);
}

// start listening for connections from other peers
void PeerProcess::startListen() {
    // start thread
    std::thread listenerThread([this]() {

        // initialize the winsock
        try {
            static WSADATA wsaData;
            int wsaerr = WSAStartup(MAKEWORD(2, 0), &wsaData);
            if (wsaerr)
                exit(1);
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in listener thread: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown exception in listener thread" << std::endl;
        }

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
        std::cout << "Peer " << ID << " now listening from port " << selfInfo.port << std::endl;

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
        std::cerr << "Peer " << ID << " ERROR: Invalid handshake received of size " << received << std::endl;
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
        MessageSender sender(ID, clientSocket);
        sender.sendHandshake();
        logger.logConnectedFrom(otherPeerId);
    }
    else{
        logger.logMakeConnection(otherPeerId);
    }

    // after connecting and verifying handshake, send bitfield
    MessageSender bitfieldSender(ID, clientSocket);
    bitfieldSender.sendBitfield(bitfield.getBits());

    // null bitfield as placeholder till their bitfield is recieved, if its not then they have nothing anyway
    BitfieldManager nullBitfield(bitfield.getSize(), false);
    // choked and not interested initially
    PeerRelationship newPeer(clientSocket, nullBitfield, otherPeerId, true, true, false, false);
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
	std::cout << "Peer " << ID << " resolving " << peer.hostName << ":" << peer.port << std::endl;
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


        // send handshake message before handling connection
        // this is because this process is the one initiating the connection
        MessageSender sender(ID, sock);
        sender.sendHandshake();
        // handle connection with new peer
        std::thread(&PeerProcess::handleConnection, this, sock, false).detach();
    }
}

// keep messaging peers while the connection is open
void PeerProcess::connectionMessageLoop(SOCKET sock, int peerId){
    while (true) {
        // first 4 bytes: message length
        uint32_t netLen;
        int r = recv(sock, (char *) &netLen, sizeof(netLen), MSG_WAITALL);
        if (r <= 0) {
            std::cout << "Peer " << ID << " disconnected from peer " << peerId << std::endl;
            closesocket(sock);
            return;
        }

        uint32_t messageLen = ntohl(netLen);

        // next byte is message type
        unsigned char messageType;
        r = recv(sock, (char *) &messageType, 1, MSG_WAITALL);
        if (r <= 0) {
            std::cout << "Peer " << ID << " lost connection to peer " << peerId << std::endl;
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
                std::cout << "Peer " << ID << " received CHOKE from " << peerId << std::endl;
                handleChoke(peerId);
                break;

            // unchoke
            case 1:
                std::cout << "Peer " << ID << " received UNCHOKE from " << peerId << std::endl;
                handleUnchoke(peerId);
                break;

            // interested
            case 2:
                std::cout << "Peer " << ID << " received INTERESTED from " << peerId << std::endl;
                handleInterested(peerId);
                break;

            // not interested
            case 3:
                std::cout << "Peer " << ID << " received NOT INTERESTED from " << peerId << std::endl;
                handleNotInterested(peerId);
                break;

            // have
            case 4:
                std::cout << "Peer " << ID << " received HAVE from " << peerId << std::endl;
                handleHave(peerId, payload);
                break;

            // bitfield
            case 5:
                std::cout << "Peer " << ID << " received BITFIELD from " << peerId << std::endl;
                handleBitfield(peerId, payload);
                break;

            // request
            case 6:
                std::cout << "Peer " << ID << " received REQUEST from " << peerId << std::endl;
                handleRequest(peerId, payload);
                break;

            // piece
            case 7:
                std::cout << "Peer " << ID << " received PIECE from " << peerId << std::endl;
                handlePiece(peerId, payload);
                break;

            // other message
            default:
                std::cout << "Peer " << ID << " received UNKNOWN message type from" << peerId << std::endl;
                break;
        }
    }
}

void PeerProcess::initShutdown(int peerId){
    std::lock_guard<std::mutex> lock(peersMutex);
    SOCKET theirSocket = relationships.at(peerId).theirSocket;

    if (theirSocket == INVALID_SOCKET) return;

    // shutdown connection
    shutdown(theirSocket, SD_SEND);

    // wait for the connection to clear of any incoming data
    char buffer[256];
    while (recv(theirSocket, buffer, sizeof(buffer), 0) > 0);

    // close the socket
    relationships.at(peerId).theirSocket = INVALID_SOCKET;
}

int PeerProcess::getPieceToRequest(int peerId) {
    // keep a list of candidate pieces
    std::vector<int> candidates;
    for (int i = 0; i < bitfield.getSize(); i++) {
        // the other peer needs to have it and  // we need to not have it
        if (!relationships.at(peerId).theirBitfield.hasPiece(i) || bitfield.hasPiece(i))
            continue;

        // we cant have requested it before
        if (requests.count(i)) {
            continue;
        }

        // add it as a candidate
        candidates.push_back(i);
    }

    // if there's no candidates then we cant request anything from them
    if (candidates.empty())
        return -1;

    // random selection
    return candidates[rand()%candidates.size()];
}

void PeerProcess::handleChoke(int peerId){
    relationships.at(peerId).chokedMe = true;

    logger.logChokedBy(peerId);
}

void PeerProcess::handleUnchoke(int peerId){
    relationships.at(peerId).chokedMe = false;

    logger.logUnchokedBy(peerId);

    // get the next missing piece
    int piece = getPieceToRequest(peerId);

    // send a request for the missing piece
    if (piece >= 0) {
        MessageSender sender(peerId, relationships.at(peerId).theirSocket);
        sender.sendRequest( piece);
        requests[piece] = peerId;
        std::cout << "Peer " << ID << " requested piece " << piece << " from peer " << peerId << std::endl;
    }
}

void PeerProcess::handleInterested(int peerId){
    relationships.at(peerId).interestedInMe = true;

    logger.logReceivedInterested(peerId);
}

void PeerProcess::handleNotInterested(int peerId){
    relationships.at(peerId).interestedInMe = false;

    logger.logReceivedNotInterested(peerId);
}

void PeerProcess::handleHave(int peerId, const std::vector<unsigned char>& payload){
    // get the index
    std::string str(payload.begin(), payload.end());
    int index = (payload[0] << 24) | (payload[1] << 16) | (payload[2] << 8)  | payload[3];
    // update their bitfield with the new piece
    relationships.at(peerId).theirBitfield.setPiece(index);

    logger.logReceivedHave(peerId, index);

    // if we have the full file, and they have the full file, then we can terminate the connection
    if(bitfield.isComplete() && relationships.at(peerId).theirBitfield.isComplete()){
        initShutdown(peerId);
    }
    // check to see if we need the piece and check to see if we are not already interested
    else if(!bitfield.hasPiece(index) && !relationships.at(peerId).interestedInThem){
        relationships.at(peerId).interestedInThem = true;
        // send that we are interested
        MessageSender sender(peerId, relationships.at(peerId).theirSocket);
        sender.sendInterested();
    }
}

void PeerProcess::handleBitfield(int peerId, const std::vector<unsigned char>& payload){
    relationships.at(peerId).theirBitfield = BitfieldManager::toBits(payload, getNumPieces());

    // check to see if we should be interested i.e. if they have a piece that we do not
    bool interested = bitfield.compareBitfields(relationships.at(peerId).theirBitfield);
    if(interested && !relationships.at(peerId).interestedInThem){
        // send that we are interested
        MessageSender sender(peerId, relationships.at(peerId).theirSocket);
        sender.sendInterested();
    }
    relationships.at(peerId).interestedInThem = interested;
}

void PeerProcess::handleRequest(int peerId, const std::vector<unsigned char>& payload){
    // check to see if we are choking them
    if(!relationships.at(peerId).chokedThem){
        //get the index
        int index = (payload[0] << 24) | (payload[1] << 16) | (payload[2] << 8)  | payload[3];

        std::vector<char> data;
        
        auto pieceDataOpt = fileHandler.readPiece(index);

        if(pieceDataOpt.has_value()){
            
            for(uint8_t byte : *pieceDataOpt) {
                data.push_back(static_cast<char>(byte));
            }
        }

        MessageSender sender(peerId, relationships.at(peerId).theirSocket);
        sender.sendPiece(index, data);
    }
}

void PeerProcess::handlePiece(int peerId, const std::vector<unsigned char>& payload){
    int index = (payload[0] << 24) | (payload[1] << 16) | (payload[2] << 8) | payload[3];
    
    std::vector<unsigned char> pieceData(payload.begin() + 4, payload.end());

    fileHandler.writePiece(index, &pieceData[0], pieceData.size());
    bitfield.setPiece(index);
    requests.clear();

    {
        std::lock_guard<std::mutex> lock(peersMutex);
        relationships.at(peerId).bytesDownloaded += pieceData.size(); 
    }

    int receivedCount = 0;
    for(size_t i = 0; i < bitfield.getSize(); i++){
        if(bitfield.hasPiece(i)) receivedCount++;
    }
    std::cout << "Peer " << ID << " progress: " << receivedCount << "/" << bitfield.getSize() << " pieces." << std::endl;

    logger.logDownloadedPiece(peerId, index, receivedCount);

    for (auto& [id, pr] : relationships) {
        MessageSender sender(pr.theirID, pr.theirSocket);
        sender.sendHave(index);
    }

    if (bitfield.isComplete()) {
        std::cout << "Peer " << ID << " has downloaded the complete file!" << std::endl;

        if (fileHandler.finalize()) {
            std::cout << "File finalized successfully." << std::endl;
        } else {
            std::cerr << "Failed to finalize file." << std::endl;
        }
        logger.logCompletedDownload();

        // we check every other peer to see if anyone else has all the pieces, we can terminate the connection
        for (auto &[id, pr]: relationships) {
            if(pr.theirBitfield.isComplete()){
                initShutdown(id);
            }
        }
    }
    else{
        int nextPiece = getPieceToRequest(peerId);
        if (nextPiece >= 0 && !relationships.at(peerId).chokedMe) {
            MessageSender sender(peerId, relationships.at(peerId).theirSocket);
            sender.sendRequest(nextPiece);
            requests[nextPiece] = peerId;
        }
    }
}

// choosing preffered neighbors
void PeerProcess::findPreferredNeighbor() {
    preferredNeighborThread = std::thread([this]() {
        std::random_device rd;
        std::mt19937 rng(rd());

        const int k = common.numberOfPreferredNeighbors;
        const int interval = common.unchokingInterval;

        while (!schedulerStop.load()) {
            // wait for p seconds
            for (int i = 0; i < interval && !schedulerStop.load(); ++i)
                std::this_thread::sleep_for(std::chrono::seconds(1));
            if (schedulerStop.load()) break;

            std::vector<std::pair<int,double>> candidateRates;
            {
                std::lock_guard<std::mutex> lock(peersMutex);
                for (auto &peer : relationships) {
                    int pid = peer.first;
                    if (!peer.second.interestedInMe){
                        continue;
                    }
                    uint64_t delta = peer.second.bytesDownloaded - peer.second.lastDownloaded;
                    double rate = static_cast<double>(delta) / std::max(1, interval);
                    candidateRates.emplace_back(pid, rate);
                }
            }

            // if we are a seeder i.e have the whole file, randomly choose peers
            bool amSeeder = bitfield.isComplete();

            std::vector<int> selected; selected.reserve(k);

            if (amSeeder) {
                std::vector<int> ids;
                ids.reserve(candidateRates.size());
                for (auto &pr : candidateRates){
                    ids.push_back(pr.first);
                }
                if (!ids.empty()) {
                    std::shuffle(ids.begin(), ids.end(), rng);
                    for (size_t i = 0; i < ids.size() && (int)selected.size() < k; ++i) {
                        selected.push_back(ids[i]);
                    }
                }
            }
            else {
                // break ties randomly
                std::shuffle(candidateRates.begin(), candidateRates.end(), rng);
                std::stable_sort(candidateRates.begin(), candidateRates.end(),
                                 [](const auto &a, const auto &b){ return a.second > b.second; });
                for (size_t i = 0; i < candidateRates.size() && (int)selected.size() < k; ++i)
                    selected.push_back(candidateRates[i].first);
            }

            // make lookup table for optimistic candidates
            std::unordered_set<int> selectedSet(selected.begin(), selected.end());
            int currentOptimistic = optimisticUnchokedPeer.load();

            // decide if we should choke or unchoke
            {
                std::lock_guard<std::mutex> lock(peersMutex);
                std::vector<int> preferredNeighbors;

                for (auto &peer : relationships) {
                    int pid = peer.first;
                    bool shouldBeUnchoked = (selectedSet.count(pid));
                    if (shouldBeUnchoked) {
                        if (peer.second.chokedThem) {
                            SOCKET theirSocket = peer.second.theirSocket;
                            if (theirSocket != INVALID_SOCKET){
                                MessageSender sender(pid, theirSocket);
                                sender.sendUnchoke();
                            }
                            peer.second.chokedThem = false;
                        }
                        preferredNeighbors.push_back(pid);
                    }
                    else {
                        if (!peer.second.chokedThem) {
                            SOCKET theirSocket = peer.second.theirSocket;
                            if (theirSocket != INVALID_SOCKET){
                                MessageSender sender(pid, theirSocket);
                                sender.sendChoke();
                            }
                            peer.second.chokedThem = true;
                        }
                    }

                    // update snapshot for next interval
                    peer.second.lastDownloaded = peer.second.bytesDownloaded;
                }
                if(!preferredNeighbors.empty()){
                    logger.logChangePreferredNeighbors(preferredNeighbors);
                }
            }
        }
    });
}

// choosing who to optimisticly unchoke
void PeerProcess::startOptimisticUnchoke() {
    optimisticUnchokeThread = std::thread([this]() {
        std::random_device rd;
        std::mt19937 rng(rd());

        const int interval = common.optimisticUnchokingInterval;

        while (!schedulerStop.load()) {
            for (int i = 0; i < interval && !schedulerStop.load(); ++i)
                std::this_thread::sleep_for(std::chrono::seconds(1));
            if (schedulerStop.load()) break;

            // candidates must be choked by us and interested in us
            std::vector<int> candidates;
            {
                std::lock_guard<std::mutex> lock(peersMutex);
                for (auto &peer: relationships) {
                    int pid = peer.first;
                    if (peer.second.interestedInMe && peer.second.chokedThem) candidates.push_back(pid);
                }
            }

            if (candidates.empty()) {
                continue;
            }

            std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
            int chosen = candidates[dist(rng)];
            int prev = optimisticUnchokedPeer.exchange(chosen);

            {
                std::lock_guard<std::mutex> lock(peersMutex);
                // unchoke the optimistic peer only if they are choked
                if (relationships.at(chosen).chokedThem) {
                    SOCKET theirSocket = relationships.at(chosen).theirSocket;
                    if (theirSocket != INVALID_SOCKET) {
                        MessageSender sender(chosen, theirSocket);
                        sender.sendUnchoke();
                    }
                    relationships.at(chosen).chokedThem = false;

                    logger.logChangeOptimisticUnchoke(chosen);
                }

                // choke previous optimistic peer
                if (prev != -1 && prev != chosen) {
                    if (!relationships.at(prev).chokedThem) {
                        SOCKET theirSocket = relationships.at(prev).theirSocket;
                        if (theirSocket != INVALID_SOCKET) {
                            MessageSender sender(prev, theirSocket);
                            sender.sendChoke();
                        }
                        relationships.at(prev).chokedThem = true;
                    }
                }
            }
        }
    });
}

