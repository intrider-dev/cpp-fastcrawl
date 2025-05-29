#include "HttpClient.hpp"
#include <curl/curl.h>
#include <vector>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>

// Конструктор и деструктор: инициализация/очистка libcurl (глобально)
HttpClient::HttpClient() {
    curl_global_init(CURL_GLOBAL_ALL);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
}

// Callback для записи данных тела ответа
size_t HttpClient::writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    HttpResponse *resp = static_cast<HttpResponse*>(userdata);
    // Дописываем полученные данные в буфер тела ответа
    resp->body.append(ptr, total);
    return total;
}

// Callback для записи заголовков ответа
size_t HttpClient::headerCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    HttpResponse *resp = static_cast<HttpResponse*>(userdata);
    std::string headerLine(ptr, total);
    // Проверяем начало новой HTTP-ответа (статусная строка).
    if (headerLine.rfind("HTTP/", 0) == 0) {
        // Если это промежуточный статус 100 Continue – пропускаем его
        if (headerLine.find("100 Continue") != std::string::npos) {
            return total;
        }
        // Начало нового ответа (например, после редиректа) – очищаем предыдущие заголовки и тело
        resp->headers.clear();
        resp->body.clear();
    }
    // Если строка заголовка пуста (CRLF) – конец заголовков, не сохраняем ее
    if (headerLine == "\r\n") {
        return total;
    }
    // Сохраняем строку заголовка
    resp->headers += headerLine;
    return total;
}

// Метод для одновременной загрузки нескольких URL
// Возвращает HttpResponse для каждого URL, независимо от кода ответа или ошибок
std::vector<HttpResponse> HttpClient::fetchMulti(const std::vector<std::string>& urls) {
    std::vector<HttpResponse> responses;
    responses.resize(urls.size());
    // Инициализируем CURL multi-стек
    CURLM *multi_handle = curl_multi_init();
    if (!multi_handle) {
        std::cerr << "Не удалось инициализировать curl_multi" << std::endl;
        return responses;
    }

    // Создаем и настраиваем CURL easy handle для каждого URL
    std::vector<CURL*> easy_handles(urls.size(), nullptr);
    for (size_t i = 0; i < urls.size(); ++i) {
        responses[i].url = urls[i];
        CURL *easy = curl_easy_init();
        if (!easy) {
            std::cerr << "Не удалось инициализировать curl_easy для URL: "
                      << urls[i] << std::endl;
            continue;
        }
        easy_handles[i] = easy;

        // Получаем указатель на соответствующую структуру ответа
        HttpResponse *resp = &responses[i];

        // Настраиваем URL запроса
        curl_easy_setopt(easy, CURLOPT_URL, urls[i].c_str());
        // Callback-и для тела и заголовков
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, HttpClient::writeCallback);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, resp);
        curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, HttpClient::headerCallback);
        curl_easy_setopt(easy, CURLOPT_HEADERDATA, resp);
        // Разрешаем автоматическое перенаправление по Location
        curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(easy, CURLOPT_MAXREDIRS, 10L);
        // Отключаем проверку SSL-сертификата (как указано в исходном коде)
        curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 0L);
        // Отключаем использование сигналов (для таймаутов) – повышение стабильности при массовых запросах
        curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);

        // Добавляем easy-handle в multi-handle
        CURLMcode mc = curl_multi_add_handle(multi_handle, easy);
        if (mc != CURLM_OK) {
            std::cerr << "Ошибка curl_multi_add_handle: "
                      << curl_multi_strerror(mc) << std::endl;
        }

        // Сохраняем указатель на структуру ответа в easy-handle (для идентификации при завершении)
        curl_easy_setopt(easy, CURLOPT_PRIVATE, resp);
    }

    // Запускаем мульти-запросы
    int still_running = 0;
    CURLMcode mc = curl_multi_perform(multi_handle, &still_running);
    // Цикл обработки до завершения всех запросов
    while (still_running) {
        // Ожидаем активности (или таймаута) на мульти-хендле
        mc = curl_multi_wait(multi_handle, nullptr, 0, 1000, nullptr);
        if (mc != CURLM_OK) {
            std::cerr << "Ошибка curl_multi_wait: "
                      << curl_multi_strerror(mc) << std::endl;
            break;
        }
        // Выполняем очередной проход мульти-запросов
        mc = curl_multi_perform(multi_handle, &still_running);
        if (mc != CURLM_OK) {
            std::cerr << "Ошибка curl_multi_perform: "
                      << curl_multi_strerror(mc) << std::endl;
            break;
        }
    }

    // Считываем результаты завершённых запросов
    int msgs_left = 0;
    CURLMsg *msg = nullptr;
    while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            CURL *easy = msg->easy_handle;
            HttpResponse *resp = nullptr;
            curl_easy_getinfo(easy, CURLINFO_PRIVATE, &resp);
            // Получаем HTTP-код ответа
            long http_code = 0;
            curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &http_code);
            resp->code = http_code;
            // Логируем результат (код CURL и HTTP, длина данных)
            char *eff_url = nullptr;
            curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &eff_url);
            if (msg->data.result != CURLE_OK) {
                std::cerr << "URL: " << (eff_url ? eff_url : "")
                          << " завершился ошибкой: " << curl_easy_strerror(msg->data.result)
                          << " (CURL код " << msg->data.result << "), HTTP код: "
                          << http_code << ", получено байт: " << resp->body.size()
                          << std::endl;
            } else {
                std::cerr << "URL: " << (eff_url ? eff_url : "")
                          << " успешно получен, HTTP код: " << http_code
                          << ", размер: " << resp->body.size() << " байт" << std::endl;
            }
            // Удаляем обработанный easy-handle из мульти и очищаем его
            curl_multi_remove_handle(multi_handle, easy);
            curl_easy_cleanup(easy);
            // Обнуляем указатель в массиве (на случай дальнейшей очистки)
            size_t index = resp - &responses[0];
            easy_handles[index] = nullptr;
        }
    }

    // Очистка оставшихся дескрипторов (на случай преждевременного выхода из цикла)
    for (CURL *easy : easy_handles) {
        if (easy) {
            curl_multi_remove_handle(multi_handle, easy);
            curl_easy_cleanup(easy);
        }
    }
    curl_multi_cleanup(multi_handle);

    // Возвращаем массив результатов (HttpResponse) для всех URL,
    // включая ошибки и коды != 200 (тело и заголовки сохранены)
    return responses;
}
