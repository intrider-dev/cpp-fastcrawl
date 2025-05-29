#pragma once
#include <curl/curl.h>
#include <mutex>

extern CURLSH* g_shareHandle;
extern std::once_flag g_shareCleanupOnce;
void initCurlShare();
