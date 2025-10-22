#include "Config.h"
#include <fstream>
#include <sstream>
#include <algorithm>

static std::string trim(std::string s) {
    auto wsfront = std::find_if_not(s.begin(), s.end(), ::isspace);
    auto wsback  = std::find_if_not(s.rbegin(), s.rend(), ::isspace).base();
    if (wsback <= wsfront) return {};
    return std::string(wsfront, wsback);
}

bool Config::loadCommon(const std::filesystem::path& path, CommonCfg& out) {
    std::ifstream in(path);
    if (!in) return false;
    std::string k, v;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream iss(line);
        if (!(iss >> k >> v)) continue;
        if (k == "NumberOfPreferredNeighbors") out.NumberOfPreferredNeighbors = std::stoi(v);
        else if (k == "UnchokingInterval") out.UnchokingInterval = std::stoi(v);
        else if (k == "OptimisticUnchokingInterval") out.OptimisticUnchokingInterval = std::stoi(v);
        else if (k == "FileName") out.FileName = v;
        else if (k == "FileSize") out.FileSize = static_cast<uint64_t>(std::stoll(v));
        else if (k == "PieceSize") out.PieceSize = static_cast<uint32_t>(std::stoul(v));
    }
    return !out.FileName.empty() && out.FileSize > 0 && out.PieceSize > 0;
}

bool Config::loadPeerInfo(const std::filesystem::path& path, PeerInfo& out) {
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        PeerRow r{};
        int has = 0;
        if (!(iss >> r.peerId >> r.host >> r.port >> has)) continue;
        r.hasFile = (has != 0);
        out.peers.push_back(std::move(r));
    }
    return !out.peers.empty();
}

std::optional<PeerRow> PeerInfo::find(int peerId) const {
    for (const auto& r : peers) if (r.peerId == peerId) return r;
    return std::nullopt;
}
