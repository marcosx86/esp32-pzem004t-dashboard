#ifndef NETWORK_STORE_H
#define NETWORK_STORE_H

#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief Plain-text Wi-Fi credential pair for hardware-independent list logic.
 */
struct StoredWifiCredential {
    std::string ssid;
    std::string password;
};

/**
 * @brief Hardware-independent discovered Wi-Fi AP entry.
 */
struct ScannedWifiInfo {
    std::string ssid;
    int32_t rssi;
    bool encrypted;
};

enum class WifiScanUiState {
    RUNNING = 0,
    FAILED = 1,
    READY = 2
};

/**
 * @brief Serializes credentials as newline-separated "ssid\npassword" pairs.
 */
std::string serializeWifiCredentials(const std::vector<StoredWifiCredential> &networks);

/**
 * @brief Parses serialized "ssid\npassword" pairs, preserving open networks with empty passwords.
 */
std::vector<StoredWifiCredential> parseWifiCredentials(const std::string &content);

/**
 * @brief Inserts or updates a credential at index 0 (boot priority) and removes case-insensitive duplicate SSIDs.
 * @return false if @p ssid is empty.
 */
bool addOrUpdateWifiCredential(std::vector<StoredWifiCredential> &networks,
                               const std::string &ssid,
                               const std::string &password,
                               int index = -1);

/**
 * @brief Removes the credential at @p index if within bounds.
 */
bool deleteWifiCredential(std::vector<StoredWifiCredential> &networks, int index);

/**
 * @brief Drops empty SSIDs, keeps strongest RSSI per duplicate SSID, and sorts descending by RSSI.
 */
std::vector<ScannedWifiInfo> normalizeScanResults(const std::vector<ScannedWifiInfo> &raw);

/**
 * @brief Classifies NetworkManager::getWifiScanStatus() code into RUNNING (-1), FAILED (<-1), or READY (>=0).
 */
WifiScanUiState classifyWifiScanStatus(int scan_status);

/**
 * @brief Builds an HTTP URL from host/IP and endpoint path without duplicate scheme or slashes.
 */
std::string buildMetricsUrl(const std::string &ip, const std::string &path);

#endif /* NETWORK_STORE_H */
