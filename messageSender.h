#pragma once

#include <unistd.h>
#include <iostream>
#include <vector>
#include <cerrno>
#include <cstring>

class MessageSender
{
    private:
    int peerID;
    int socket;

    void sendRaw(const std::vector<char>& data);
    std::vector<char> intToBytes(int value);

    public:
    MessageSender(int peerID, int socket);
    std::vector<char> buildMessage(uint8_t type, const std::vector<char>& payload = {});

    void sendHandshake();

    void sendChoke();
    void sendUnchoke();
    void sendInterested();
    void sendNotInterested();
    void sendHave(int pieceIndex);
    void sendBitfield(const std::vector<bool>& bitfield);
    void sendRequest(int pieceIndex);
    void sendPiece(int pieceIndex, const std::vector<char>& pieceData);
};