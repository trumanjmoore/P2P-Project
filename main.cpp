#include "PeerProcess.h"
#include "FileHandling.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "peerProcess <peerID>" << std::endl;
        return 1;
    }

    int myPeerId = std::stoi(argv[2]);
    PeerProcess mainPeer(myPeerId);
    mainPeer.start();
}
