// Nuvio Wii U - arquivo unico
#include <string>
#include <vector>
#include <curl/curl.h>
#include <sys/stat.h>
#include "json.hpp"   // nlohmann/json (single header) em source/json.hpp



struct Item {
    std::string id;       // id do TMDB ou id do addon (ex: tt1234567)
    std::string title;
    std::string overview;
    std::string posterUrl;
    double rating = 0.0;
};

// Catalogo "popular" do TMDB (metadados). tv=false -> filmes, tv=true -> series.
std::vector<Item> tmdbPopular(const std::string &apiKey, bool tv, std::string &err);

// Catalogo de um addon compativel com o protocolo Stremio (URL informada PELO USUARIO).
// Le {base}/manifest.json e depois {base}/catalog/{movie|series}/{id}.json
std::vector<Item> addonCatalog(const std::string &baseUrl, bool tv, std::string &err);

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

// ===== interface =====
// Nuvio Wii U - homebrew (.rpx) - esqueleto inicial
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <curl/curl.h>
#include <whb/sdcard.h>
#include <fstream>
#include <map>
#include <string>
#include <vector>

static const int W = 1280, H = 720;
static const char *APP_DIR  = "fs:/vol/external01/wiiu/apps/nuvio/";
static const int COLS = 5, CARD_W = 220, CARD_H = 250, GAP = 24, TOP = 110;

static SDL_Renderer *ren;
static TTF_Font *fontBig, *fontSm;
static std::map<std::string, SDL_Texture *> cache;

static void text(TTF_Font *f, const std::string &s, int x, int y, SDL_Color col, int maxW = 0) {
    if (s.empty()) return;
    std::string key = std::to_string((uintptr_t)f) + "|" + std::to_string(maxW) + "|" + s;
    SDL_Texture *&t = cache[key];
    if (!t) {
        SDL_Surface *sf = maxW ? TTF_RenderUTF8_Blended_Wrapped(f, s.c_str(), col, maxW)
                               : TTF_RenderUTF8_Blended(f, s.c_str(), col);
        if (!sf) return;
        t = SDL_CreateTextureFromSurface(ren, sf);
        SDL_FreeSurface(sf);
    }
    int w, h;
    SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
    SDL_Rect r{x, y, w, h};
    SDL_RenderCopy(ren, t, nullptr, &r);
    if (cache.size() > 400) {
        for (auto &kv : cache) SDL_DestroyTexture(kv.second);
        cache.clear();
    }
}

static std::string readCfg(const std::string &name) {
    std::ifstream f(std::string(APP_DIR) + "config.txt");
    std::string line, prefix = name + "=";
    while (std::getline(f, line))
        if (line.rfind(prefix, 0) == 0) {
            std::string k = line.substr(prefix.size());
            while (!k.empty() && (k.back() == '\r' || k.back() == ' ')) k.pop_back();
            return k;
        }
    return "";
}

static void splash(const std::string &msg) {
    SDL_SetRenderDrawColor(ren, 15, 15, 22, 255);
    SDL_RenderClear(ren);
    text(fontBig, msg, 60, 320, {255, 255, 255, 255}, W - 120);
    SDL_RenderPresent(ren);
}

int main(int, char **) {
    WHBMountSdCard();
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
    TTF_Init();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    SDL_Window *win = SDL_CreateWindow("Nuvio", 0, 0, W, H, 0);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    std::string fp = std::string(APP_DIR) + "font.ttf";
    fontBig = TTF_OpenFont(fp.c_str(), 30);
    fontSm  = TTF_OpenFont(fp.c_str(), 20);
    if (!fontBig || !fontSm) {
        // Sem fonte nao ha como mostrar texto; encerra limpo.
        SDL_Quit();
        return 1;
    }

    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i)) SDL_GameControllerOpen(i);

    std::string key = readCfg("tmdb_key"), addon = readCfg("addon_url"), err;
    bool tv = false;
    std::vector<Item> items;
    int sel = 0, scrollRow = 0;
    bool needLoad = true;

    bool running = true;
    while (running) {
        if (needLoad) {
            splash("Carregando...");
            err.clear();
            if (!addon.empty())    items = addonCatalog(addon, tv, err);
            else if (!key.empty()) items = tmdbPopular(key, tv, err);
            else { err = "Crie config.txt com tmdb_key=... ou addon_url=..."; items.clear(); }
            sel = 0; scrollRow = 0; needLoad = false;
        }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_CONTROLLERDEVICEADDED) SDL_GameControllerOpen(e.cdevice.which);
            if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                int n = (int)items.size();
                switch (e.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: if (sel + 1 < n) sel++; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  if (sel > 0) sel--; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  if (sel + COLS < n) sel += COLS; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:    if (sel - COLS >= 0) sel -= COLS; break;
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: tv = !tv; needLoad = true; break;
                    case SDL_CONTROLLER_BUTTON_START: running = false; break;
                    default: break;  // A: abrir detalhes/player (proximo passo)
                }
                int row = sel / COLS;
                if (row < scrollRow) scrollRow = row;
                if (row > scrollRow + 1) scrollRow = row - 1;
            }
        }

        SDL_SetRenderDrawColor(ren, 15, 15, 22, 255);
        SDL_RenderClear(ren);
        text(fontBig, "NUVIO", 40, 24, {229, 9, 20, 255});
        text(fontSm, tv ? "Filmes      [ Series ]" : "[ Filmes ]      Series", 200, 34, {200, 200, 200, 255});
        text(fontSm, "L/R: alternar   D-Pad: mover   +: sair", 40, 680, {130, 130, 140, 255});

        if (!err.empty()) text(fontSm, "Erro: " + err, 40, 120, {255, 120, 120, 255}, W - 80);

        for (int i = 0; i < (int)items.size(); i++) {
            int row = i / COLS - scrollRow, col = i % COLS;
            if (row < 0 || row > 1) continue;
            int x = 40 + col * (CARD_W + GAP), y = TOP + row * (CARD_H + GAP);
            SDL_Rect r{x, y, CARD_W, CARD_H};
            SDL_SetRenderDrawColor(ren, 35, 38, 55, 255);
            SDL_RenderFillRect(ren, &r);
            text(fontSm, items[i].title, x + 10, y + 10, {255, 255, 255, 255}, CARD_W - 20);
            char buf[32];
            snprintf(buf, sizeof buf, "* %.1f", items[i].rating);
            text(fontSm, buf, x + 10, y + CARD_H - 32, {255, 200, 60, 255});
            if (i == sel) {
                SDL_SetRenderDrawColor(ren, 229, 9, 20, 255);
                for (int b = 0; b < 4; b++) {
                    SDL_Rect o{x - b, y - b, CARD_W + 2 * b, CARD_H + 2 * b};
                    SDL_RenderDrawRect(ren, &o);
                }
            }
        }
        SDL_RenderPresent(ren);
    }

    for (auto &kv : cache) SDL_DestroyTexture(kv.second);
    curl_global_cleanup();
    TTF_Quit();
    SDL_Quit();
    WHBUnmountSdCard();
    return 0;
}
