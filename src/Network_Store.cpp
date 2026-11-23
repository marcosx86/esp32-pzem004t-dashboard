#include "Network_Store.h"

#include <algorithm>
#include <cctype>

namespace {

bool equalsIgnoreCase(const std::string &a, const std::string &b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

bool startsWithIgnoreCase(const std::string &text, const char* prefix) {
    size_t i = 0;
    while (prefix[i] != '\0') {
        if (i >= text.size()) {
            return false;
        }
        if (std::tolower(static_cast<unsigned char>(text[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
        ++i;
    }
    return true;
}

} // namespace

std::string serializeWifiCredentials(const std::vector<StoredWifiCredential> &networks) {
    std::string output;
    for (size_t i = 0; i < networks.size(); ++i) {
        if (i > 0) {
            output += '\n';
        }
        output += networks[i].ssid;
        output += '\n';
        output += networks[i].password;
    }
    return output;
}

std::vector<StoredWifiCredential> parseWifiCredentials(const std::string &content) {
    std::vector<StoredWifiCredential> networks;
    if (content.empty()) {
        return networks;
    }

    std::string current_ssid;
    int line_index = 0;
    size_t start = 0;

    while (start <= content.size()) {
        size_t end = content.find('\n', start);
        bool at_end = (end == std::string::npos);
        if (at_end) {
            end = content.size();
        }

        std::string line = content.substr(start, end - start);
        if (line_index % 2 == 0) {
            current_ssid = line;
        } else {
            if (!current_ssid.empty() || !line.empty()) {
                networks.push_back(StoredWifiCredential{current_ssid, line});
            }
        }

        ++line_index;
        if (at_end) {
            break;
        }
        start = end + 1;
    }

    return networks;
}

bool addOrUpdateWifiCredential(std::vector<StoredWifiCredential> &networks,
                               const std::string &ssid,
                               const std::string &password,
                               int index) {
    if (ssid.empty()) {
        return false;
    }

    if (index >= 0 && index < static_cast<int>(networks.size())) {
        networks.erase(networks.begin() + index);
    }

    for (auto it = networks.begin(); it != networks.end();) {
        if (equalsIgnoreCase(it->ssid, ssid)) {
            it = networks.erase(it);
        } else {
            ++it;
        }
    }

    networks.insert(networks.begin(), StoredWifiCredential{ssid, password});
    return true;
}

bool deleteWifiCredential(std::vector<StoredWifiCredential> &networks, int index) {
    if (index < 0 || index >= static_cast<int>(networks.size())) {
        return false;
    }
    networks.erase(networks.begin() + index);
    return true;
}

std::vector<ScannedWifiInfo> normalizeScanResults(const std::vector<ScannedWifiInfo> &raw) {
    std::vector<ScannedWifiInfo> out;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i].ssid.empty()) {
            continue;
        }
        bool merged = false;
        for (size_t j = 0; j < out.size(); ++j) {
            if (out[j].ssid == raw[i].ssid) {
                if (raw[i].rssi > out[j].rssi) {
                    out[j].rssi = raw[i].rssi;
                    out[j].encrypted = raw[i].encrypted;
                }
                merged = true;
                break;
            }
        }
        if (!merged) {
            out.push_back(raw[i]);
        }
    }

    std::sort(out.begin(), out.end(), [](const ScannedWifiInfo &a, const ScannedWifiInfo &b) {
        if (a.rssi != b.rssi) {
            return a.rssi > b.rssi;
        }
        return a.ssid < b.ssid;
    });
    return out;
}

WifiScanUiState classifyWifiScanStatus(int scan_status) {
    if (scan_status == -1) {
        return WifiScanUiState::RUNNING;
    }
    if (scan_status < -1) {
        return WifiScanUiState::FAILED;
    }
    return WifiScanUiState::READY;
}

std::string buildMetricsUrl(const std::string &ip, const std::string &path) {
    std::string host = ip;
    while (!host.empty() && host.back() == '/') {
        host.pop_back();
    }

    std::string norm_path = path.empty() ? std::string("/metrics") : path;
    if (norm_path.front() != '/') {
        norm_path.insert(norm_path.begin(), '/');
    }

    if (startsWithIgnoreCase(host, "http://") || startsWithIgnoreCase(host, "https://")) {
        return host + norm_path;
    }
    return std::string("http://") + host + norm_path;
}
