# Nuvio Wii U

Cliente de catálogo open source para **Wii U** (homebrew `.rpx`), feito com devkitPro, wut e SDL2.
Mostra filmes e séries em uma grade navegável com o controle do console.

> **Status: fase 1.** Já tem grade de títulos, notas e troca entre Filmes/Séries.
> Ainda **não** tem pôsteres, tela de detalhes nem player de vídeo.

Projeto independente, inspirado no [Nuvio](https://github.com/NuvioMedia). Não é afiliado a ele.

## Como funciona

- **Metadados:** vêm do [TMDB](https://www.themoviedb.org/) (título, nota, sinopse).
- **Catálogos extras:** o app também lê catálogos de um addon compatível com o protocolo Stremio, **cuja URL você mesmo informa**.
- **Fontes de vídeo:** o app **não embute nenhuma**. O player (próxima fase) vai usar o seu servidor
  (Jellyfin, Plex) ou arquivos que você tem direito de assistir.

## Baixar o app pronto (.rpx)

O `.rpx` é gerado automaticamente pelo GitHub Actions:

1. Abra a aba **Actions** deste repositório.
2. Clique no build mais recente com o check verde.
3. Em **Artifacts**, baixe `nuvio-wiiu` e extraia o `nuvio.rpx`.

## Instalar no Wii U

Requer um Wii U com homebrew (ambiente **Aroma**).

1. No cartão SD, crie a pasta `wiiu/apps/nuvio/`.
2. Coloque dentro dela:
   - `nuvio.rpx`
   - `font.ttf`: qualquer fonte TrueType (por exemplo DejaVu Sans ou Noto Sans)
   - `config.txt`: veja abaixo
   - `cacert.pem`: baixe em https://curl.se/ca/cacert.pem (necessário para validar o HTTPS)
3. Abra o **Nuvio** pelo menu do Aroma.

### config.txt

Uma opção por linha, sem espaços ao redor do `=`:

```
tmdb_key=SUA_CHAVE_DO_TMDB
addon_url=https://url-do-seu-addon
```

- `tmdb_key`: chave gratuita em themoviedb.org (Configurações → API).
- `addon_url`: opcional. Se estiver preenchida, tem prioridade sobre o TMDB.

## Controles

| Botão | Ação |
|---|---|
| D-Pad | Mover a seleção |
| L / R | Alternar entre Filmes e Séries |
| + | Sair |

Funciona com o GamePad e com o Pro Controller.

## Compilar

O caminho mais simples é pelo GitHub Actions (veja acima): cada commit gera um build.

Para compilar localmente você precisa do [devkitPro](https://devkitpro.org/) com os pacotes:

```
dkp-pacman -S wut wiiu-sdl2 wiiu-sdl2_ttf wiiu-curl ppc-zlib ppc-freetype
```

E do arquivo `json.hpp` ([nlohmann/json](https://github.com/nlohmann/json/releases/latest)) em `source/json.hpp`. Depois:

```
export DEVKITPRO=/opt/devkitpro
make
```

O resultado é o arquivo `nuvio.rpx`.

## Estrutura

```
.github/workflows/build.yml   build automático do .rpx
source/main.cpp               código do app (interface + cliente TMDB/addon)
Makefile                      regras de compilação
```

## Roadmap

- [x] Grade de títulos e navegação por controle
- [x] Catálogo do TMDB e de addons
- [ ] Pôsteres (SDL2_image)
- [ ] Tela de detalhes
- [ ] Player de vídeo (ffmpeg, H.264 em 480p/720p) via Jellyfin/Plex
- [ ] Séries e episódios, progresso salvo, legendas

## Limitações do Wii U

CPU PowerPC de 1,2 GHz e pouca memória para homebrew. Na prática, só dá para decodificar
vídeo por software em H.264 de baixa resolução e bitrate. 1080p, HEVC e AV1 não são viáveis.

## Aviso legal

Este projeto é uma ferramenta de exibição de catálogo e reprodução. Os autores não hospedam,
distribuem nem indicam conteúdo protegido por direitos autorais. Cada usuário é responsável
pelas fontes que configura e deve usar apenas conteúdo que tenha direito de assistir.
Este projeto usa a API do TMDB, mas não é endossado nem certificado por ele.

## Licença

GPL-3.0, veja o arquivo [LICENSE](LICENSE).
