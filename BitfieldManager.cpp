#include "BitfieldManager.h"

BitfieldManager::BitfieldManager() {}

BitfieldManager::BitfieldManager(size_t numPieces, bool has){
    size = numPieces;
    bits.insert(bits.end(), numPieces, has);
}

// set a bit
void BitfieldManager::setPiece(size_t index) {
    bits[index] = true;
}

// clear a bit
void BitfieldManager::clearPiece(size_t index) {
    bits[index] = false;
}

// return a bit
bool BitfieldManager::hasPiece(size_t index) const {
    return bits[index];
}

// check if all bits are 1: has the full file
bool BitfieldManager::isComplete() const {
    for (int i =0; i < size; i++) {
        if (!bits[i])
            return false;
    }
    return true;
}


// convert the bitfield into an array of bytes
std::vector<uint8_t> BitfieldManager::toBytes() const {
    std::vector<uint8_t> bytes((size + 7) / 8, 0);
    for (size_t i = 0; i < size; ++i) {
        if (bits[i]) {
            bytes[i / 8] |= (1 << (7 - (i % 8))); // big-endian
        }
    }
    return bytes;
}

// convert an array of bytes to bitfield
BitfieldManager BitfieldManager::toBits(const std::vector<uint8_t>& bytes, size_t numPieces) {
    BitfieldManager bitfield(numPieces, false);
    for (size_t i = 0; i < numPieces; i++) {
        size_t byteIndex = i / 8;
        size_t bitIndex = 7 - (i % 8);
        bitfield.bits[i] = (bytes[byteIndex] >> bitIndex) & 1;

	if (byteIndex < bytes.size()) {
            bitfield.bits[i] = (bytes[byteIndex] >> bitIndex) & 1;
        } else {
            bitfield.bits[i] = false; // extra bits are just 0
        }
    }
    return bitfield;
}

bool BitfieldManager::compareBitfields(const BitfieldManager& theirs) {
    for(int i = 0; i < size; i++){
        if(theirs.hasPiece(i) && !hasPiece(i)){
            return true;
        }
    }
    return false;
}
