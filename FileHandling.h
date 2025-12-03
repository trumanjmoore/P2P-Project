#pragma once
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <cstdint>

class FileHandling {
public:

    FileHandling ();

    FileHandling(std::filesystem::path workDir,
               int peerId,
               std::string fileName,
               uint64_t fileSize,
               uint32_t pieceSize,
               bool startsWithCompleteFile);

    bool init();

    bool hasCompleteFile() const;       
    std::filesystem::path peerDir() const; 

    // Piece API (0-based index)
    uint32_t pieceLength(uint32_t index) const;            
    bool writePiece(uint32_t index, const uint8_t* buf, size_t len);
    std::optional<std::vector<uint8_t>> readPiece(uint32_t index) const;

    bool finalize();

    // Paths (useful for logging)
    std::filesystem::path finalPath() const {
         return finalPath_; 
    }
    std::filesystem::path partPath()  const { 
        return partPath_;  
    }

private:
    std::filesystem::path workDir_, peerDir_, finalPath_, partPath_;
    std::string fileName_;
    uint64_t fileSize_{};
    uint32_t pieceSize_{};
    bool seeder_{};

    uint64_t offset(uint32_t idx) const { 
        return uint64_t(idx) * pieceSize_; 
    }
};