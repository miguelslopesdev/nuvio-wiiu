#include "tmdb.h"
#include <curl/curl.h>
#include <sys/stat.h>
#include "json.hpp"   // nlohmann/json (single header) em source/json.hpp

using nlohmann::json;

static const char *CA_PATH = "fs:/vol/external01/wiiu/apps/nuvio/cacert.pem";

static size_t writeCb(char *p, size_t s, size_t n, void *ud) {
    static_cast<std::string *>(ud)->append(p, s * n);
    return s * n;
}

static bool httpGet(const std::string &url, std::string &body, std::string &err) {
    CURL *c = curl_easy_init();
    if (!c) { err = "curl init falhou"; return false; }
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    struct stat st;
    if (stat(CA_PATH, &st) == 0) {
        curl_easy_setopt(c, CURLOPT_CAINFO, CA_PATH);
    } else {
        // TEMPORARIO: sem cacert.pem o HTTPS nao e verificado. Coloque o arquivo no SD.
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    CURLcode rc = curl_easy_perform(c);
    long code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(c);
    if (rc != CURLE_OK) { err = curl_easy_strerror(rc); return false; }
    if (code != 200)    { err = "HTTP " + std::to_string(code); return false; }
    return true;
}

std::vector<Item> tmdbPopular(const std::string &apiKey, bool tv, std::string &err) {
    std::vector<Item> out;
    std::string body;
    std::string url = std::string("https://api.themoviedb.org/3/") + (tv ? "tv" : "movie") +
                      "/popular?language=pt-BR&page=1&api_key=" + apiKey;
    if (!httpGet(url, body, err)) return out;
    try {
        auto j = json::parse(body);
        for (auto &r : j["results"]) {
            Item it;
            it.id = std::to_string(r.value("id", 0));
            it.title = r.value(tv ? "name" : "title", "?");
            it.overview = r.value("overview", "");
            it.rating = r.value("vote_average", 0.0);
            if (r.contains("poster_path") && r["poster_path"].is_string())
                it.posterUrl = "https://image.tmdb.org/t/p/w342" + r["poster_path"].get<std::string>();
            out.push_back(std::move(it));
        }
    } catch (const std::exception &e) { err = std::string("JSON: ") + e.what(); }
    return out;
}

std::vector<Item> addonCatalog(const std::string &baseUrlIn, bool tv, std::string &err) {
    std::vector<Item> out;
    std::string base = baseUrlIn;
    while (!base.empty() && base.back() == '/') base.pop_back();
    const std::string type = tv ? "series" : "movie";
    std::string body;
    try {
        if (!httpGet(base + "/manifest.json", body, err)) return out;
        auto m = json::parse(body);
        std::string catId;
        for (auto &c : m["catalogs"])
            if (c.value("type", "") == type) {
                // ignora catalogos que exigem parametro obrigatorio (ex: busca)
                bool needsExtra = false;
                if (c.contains("extra"))
                    for (auto &x : c["extra"]) if (x.value("isRequired", false)) needsExtra = true;
                if (!needsExtra) { catId = c.value("id", ""); break; }
            }
        if (catId.empty()) { err = "Addon sem catalogo de " + type; return out; }

        body.clear();
        if (!httpGet(base + "/catalog/" + type + "/" + catId + ".json", body, err)) return out;
        auto j = json::parse(body);
        for (auto &r : j["metas"]) {
            Item it;
            it.id = r.value("id", "");
            it.title = r.value("name", "?");
            it.overview = r.value("description", "");
            it.posterUrl = r.value("poster", "");
            if (r.contains("imdbRating") && r["imdbRating"].is_string())
                it.rating = atof(r["imdbRating"].get<std::string>().c_str());
            out.push_back(std::move(it));
        }
    } catch (const std::exception &e) { err = std::string("JSON: ") + e.what(); }
    return out;
}
