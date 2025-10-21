#include "messageSender.h"

void MessageSender::sendRaw(const std::vector<char>& data)
{
    size_t totalSent = 0;
    size_t dataSize = data.size();

    while (totalSent < dataSize)
    {
        ssize_t bytesSent = write(socket, data.data() + totalSent, dataSize - totalSent);
        if (bytesSent == -1)
        {
            // some sort of issue while sending
            return;
        }
        // casting to just avoid warning
        // but i think this shouldn't need a cast(?)
        totalSent += static_cast<size_t>(bytesSent);
    }
}

// small helper to keep sendHandshake() c l e a n
std::vector<char> MessageSender::intToBytes(int value)
{
    // https://stackoverflow.com/questions/30386769/when-and-how-to-use-c-htonl-function
    // basically converts the peerID to big endian
    uint32_t networkOrder = htonl(value);
    std::vector<char> bytes(4);
    std::memcpy(bytes.data(), &networkOrder, 4);
    return bytes;
}

MessageSender::MessageSender(int peerID, int socket) : peerID(peerID), socket(socket) {}

void MessageSender::sendHandshake()
{
    std::vector<char> handshake(32);

    // handshake header
    const char* header = "P2PFILESHARINGPROJ";
    std::memcpy(handshake.data(), header, 18);

    // zero bits
    std::memset(handshake.data() + 18, 0, 10);

    // peer ID
    std::vector<char> peerIDBytes = intToBytes(peerID);
    std::memcpy(handshake.data() + 18 + 10, peerIDBytes.data(), 4);

    sendRaw(handshake);
}

void MessageSender::sendChoke()
{
}

void MessageSender::sendUnchoke()
{
}

void MessageSender::sendInterested()
{
}

void MessageSender::sendNotInterested()
{
}

void MessageSender::sendHave(int pieceIndex)
{
}

void MessageSender::sendBitfield(const std::vector<bool> &bitfield)
{
}

void MessageSender::sendRequest(int pieceIndex)
{
}

void MessageSender::sendPiece(int pieceIndex, const std::vector<char> &pieceData)
{
}