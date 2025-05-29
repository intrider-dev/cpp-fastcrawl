#include "CurlShare.hpp"
#include "Logger.hpp"

#include <mutex>

CURLSH* g_shareHandle = nullptr;
std::once_flag g_shareCleanupOnce;

void initCurlShare() {
    static std::once_flag shareFlag;
    std::call_once(shareFlag, [] {
        g_shareHandle = curl_share_init();
        if (!g_shareHandle) {
            Logger::warn("CurlShare: curl_share_init failed");
            return;
        }
        curl_share_setopt(g_shareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    });
}
