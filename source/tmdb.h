#pragma once
#include <string>
#include <vector>

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
