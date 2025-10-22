#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <optional>

struct CommonCfg {
  int NumberOfPreferredNeighbors = 2;
  int UnchokingInterval = 5;
  int OptimisticUnchokingInterval = 15;
  std::string FileName;
  uint64_t FileSize = 0;
  uint32_t PieceSize = 0;
};

struct PeerRow {
  int peerId;
  std::string host;
  int port;
  bool hasFile; 
};

struct PeerInfo {
  std::vector<PeerRow> peers;
  std::optional<PeerRow> find(int peerId) const;
};

namespace Config {
    bool loadCommon(const std::filesystem::path& path, CommonCfg& out);
    bool loadPeerInfo(const std::filesystem::path& path, PeerInfo& out);
}
