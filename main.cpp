#include "PeerProcess.h"
#include "FileHandling.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "PeerProcess <peerID>" << std::endl;
        return 1;
    }

    int myPeerId = std::stoi(argv[1]);
    PeerProcess mainPeer(myPeerId);
}
