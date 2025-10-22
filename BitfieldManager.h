#include <vector>
#include <cstdint>
#include <string>
#pragma once

class BitfieldManager {
private:
    std::vector<bool> bits; // 1 = has, 0 = doesn't have
    size_t size;

public:
    BitfieldManager();
    explicit BitfieldManager(size_t numPieces);

    void setPiece(size_t index);
    void clearPiece(size_t index);
    bool hasPiece(size_t index) const;

    void setAllPieces();
    void clearAllPieces();
    bool isComplete() const;

    std::vector<uint8_t> toBytes() const;
    static BitfieldManager toBits(const std::vector<uint8_t>& data, size_t numPieces);
};
