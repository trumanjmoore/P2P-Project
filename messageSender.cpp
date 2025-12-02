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

std::vector<char> MessageSender::buildMessage(uint8_t type, const std::vector<char>& payload = {})
{
    uint32_t length = htonl(1 + payload.size());
    std::vector<char> message(4 + 1 + payload.size());

    std::memcpy(message.data(), &length, 4);
    message[4] = type;
    if (!payload.empty())
    {
        std::memcpy(message.data() + 5, payload.data(), payload.size());
    }
    return message;
}


void MessageSender::sendChoke()
{
    sendRaw(buildMessage(0));
}

void MessageSender::sendUnchoke()
{
    sendRaw(buildMessage(1));
}

void MessageSender::sendInterested()
{
    sendRaw(buildMessage(2));
}

void MessageSender::sendNotInterested()
{
    sendRaw(buildMessage(3));
}

void MessageSender::sendHave(int pieceIndex)
{
    std::vector<char> payload = intToBytes(pieceIndex);
    sendRaw(buildMessage(4, payload));
}

void MessageSender::sendBitfield(const std::vector<bool> &bitfield)
{
    size_t numBytes = (bitfield.size() + 7) / 8;
    std::vector<char> payload(numBytes, 0);

    for (size_t i = 0; i < bitfield.size(); ++i)
    {
        if (bitfield[i])
        {
            payload[i / 8] |= (1 << (7 - (i % 8)));
        }
    }
    sendRaw(buildMessage(5, payload));
}

void MessageSender::sendRequest(int pieceIndex)
{
    std::vector<char> payload = intToBytes(pieceIndex);
    sendRaw(buildMessage(6, payload));
}

void MessageSender::sendPiece(int pieceIndex, const std::vector<char> &pieceData)
{
    std::vector<char> payload = intToBytes(pieceIndex);
    payload.insert(payload.end(), pieceData.begin(), pieceData.end());
    sendRaw(buildMessage(7, payload));
}