// Nuvio Wii U - homebrew (.rpx) - esqueleto inicial
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <curl/curl.h>
#include <whb/sdcard.h>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include "tmdb.h"

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
