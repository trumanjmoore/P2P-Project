#include "BitfieldManager.h"

BitfieldManager::BitfieldManager() {}

BitfieldManager::BitfieldManager(size_t numPieces){
    size = numPieces;
    bits.insert(bits.end(), numPieces, false);
}

// Set a specific bit to 1
void BitfieldManager::setPiece(size_t index) {
    bits[index] = true;
}

// Clear a specific bit to 0
void BitfieldManager::clearPiece(size_t index) {
    bits[index] = false;
}

// Return a specific bit
bool BitfieldManager::hasPiece(size_t index) const {
    return bits[index];
}

// Set all bits to 1, for when peer starts with full file
void BitfieldManager::setAllPieces() {
    std::fill(bits.begin(), bits.end(), true);
}

// Set all bits to 0, for when peer starts with an empty file
void BitfieldManager::clearAllPieces() {
    std::fill(bits.begin(), bits.end(), false);
}

// Check if all bits are 1, has the full file
bool BitfieldManager::isComplete() const {
    for (int i =0; i < size; i++) {
        if (bits[i])
            return false;
    }
    return true;
}


// Convert the bitfield into array of bytes
std::vector<uint8_t> BitfieldManager::toBytes() const {
    std::vector<uint8_t> bytes((size + 7) / 8, 0);
    for (size_t i = 0; i < size; ++i) {
        if (bits[i]) {
            bytes[i / 8] |= (1 << (7 - (i % 8))); // big-endian
        }
    }
    return bytes;
}

// Convert bytes from other peers to bitfield
BitfieldManager BitfieldManager::toBits(const std::vector<uint8_t>& bytes, size_t numPieces) {
    BitfieldManager bitfield(numPieces);
    for (size_t i = 0; i < numPieces; i++) {
        size_t byteIndex = i / 8;
        size_t bitIndex = 7 - (i % 8);
        bitfield.bits[i] = (bytes[byteIndex] >> bitIndex) & 1;
    }
    return bitfield;
}
