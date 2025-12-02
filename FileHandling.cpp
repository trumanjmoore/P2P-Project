#include "FileHandling.h"
#include <fstream>
#include <iostream>  // For std::cerr
#include <thread>    // For std::this_thread::sleep_for
#include <chrono>    // For std::chrono::milliseconds

using std::filesystem::exists;
using std::filesystem::create_directories;
using std::filesystem::rename;

FileHandling::FileHandling() = default;

FileHandling::FileHandling(std::filesystem::path workDir, int peerId, std::string fileName,
                            uint64_t fileSize, uint32_t pieceSize, bool startsWithCompleteFile)
    : workDir_(std::move(workDir)),
        fileName_(std::move(fileName)),
        fileSize_(fileSize),
        pieceSize_(pieceSize),
        seeder_(startsWithCompleteFile) {
    peerDir_   = workDir_ / ("peer_" + std::to_string(peerId));
    finalPath_ = peerDir_ / fileName_;
    partPath_  = peerDir_ / (fileName_ + ".part");
}

bool FileHandling::init() {
    try { 
        create_directories(peerDir_); } catch (...) { return false; 
    }
    if (seeder_) return hasCompleteFile();

    if (hasCompleteFile()) return true;

    std::fstream f(partPath_, std::ios::in | std::ios::out | std::ios::binary);
    if (!f){
        std::ofstream c(partPath_, std::ios::binary | std::ios::trunc);
        if (!c) return false;
        if (fileSize_ > 0) {
        c.seekp(static_cast<std::streamoff>(fileSize_ - 1));
        char z = 0; c.write(&z, 1);
        }
    }
    return true;
}

bool FileHandling::hasCompleteFile() const{
    return exists(finalPath_);
}

std::filesystem::path FileHandling::peerDir() const{
    return peerDir_;
}

uint32_t FileHandling::pieceLength(uint32_t index) const{
    const uint64_t start = offset(index);
    if (start >= fileSize_) return 0;
    const uint64_t end = std::min(start + uint64_t(pieceSize_), fileSize_);
    return static_cast<uint32_t>(end - start);
}

bool FileHandling::writePiece(uint32_t index, const uint8_t* buf, size_t len) {
    if (hasCompleteFile()) return false;

    const uint32_t need = pieceLength(index);
    if (need == 0 || len != need) return false;

    std::fstream io(partPath_, std::ios::in | std::ios::out | std::ios::binary);
    if (!io) return false;
    io.seekp(static_cast<std::streamoff>(offset(index)));
    io.write(reinterpret_cast<const char*>(buf), need);
    return static_cast<bool>(io);
}

std::optional<std::vector<uint8_t>> FileHandling::readPiece(uint32_t index) const {
    const uint32_t len = pieceLength(index);
    if (len == 0) return std::nullopt;

    const auto& src = hasCompleteFile() ? finalPath_ : partPath_;
    std::ifstream in(src, std::ios::binary);
    if (!in) return std::nullopt;

    std::vector<uint8_t> out(len);
    in.seekg(static_cast<std::streamoff>(offset(index)));
    in.read(reinterpret_cast<char*>(out.data()), len);
    if (!in) return std::nullopt;
    return out;
    }

bool FileHandling::finalize() {
    if (hasCompleteFile()) return true;

    // a lot of this might be unecessary
    // another fix is what actually fixed the finalizing issue
    // but i don't want to deal with reverting and maybe messing things up
    for (int i = 0; i < 5; ++i) {
        try {
            std::filesystem::rename(partPath_, finalPath_);
            return true;
        } 
        catch (const std::filesystem::filesystem_error& e) {
            // if this is the last attempt, log the failure
            if (i == 4) {
                std::cerr << "Finalize failed: " << e.what() << std::endl;
                std::cerr << "Source: " << partPath_ << "\nDest: " << finalPath_ << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50 * (i + 1)));
        }
        catch (...) {
            std::cerr << "Unknown error occurred during finalize." << std::endl;
            return false;
        }
    }
    return false;
}
