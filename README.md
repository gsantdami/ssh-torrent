# SSH Torrent - Buscador de Torrents via CLI

Um buscador de torrents rápido e leve escrito em C, com suporte a interface gráfica (TUI) via ncurses e busca em linha de comando.

## Requisitos

O usuário deve possuir as seguintes dependências instaladas para o projeto funcionar.

- **gcc** - Compilador C
- **make** - Sistema de build
- **libcurl** - Para requisições HTTP
- **libcjson** - Para parsing JSON
- **ncurses** - Para interface gráfica (terminal interativo)
- **transmission-cli** (opcional) - Para download automático

### Instalação de Dependências

Caso não possua alguma das dependências listadas acima, rode:

#### Ubuntu/Debian:
```bash
sudo apt-get install -y build-essential libcurl4-openssl-dev libcjson-dev libncurses-dev transmission-cli
```

#### Fedora/RHEL:
```bash
sudo dnf install -y gcc make libcurl-devel cjson-devel ncurses-devel transmission
```

#### macOS:
```bash
brew install curl cjson ncurses transmission
```

## Instalação

Clone o repositório e na pasta do mesmo utilize os seguintes comandos:

### Compilação Local
```bash
make
```

### Instalação no Sistema
```bash
sudo make install
```

O binário será instalado em `/usr/local/bin/torrent-search`. O usuário poderá, portanto, utilizar as funcionalidades em escopo global.

### Desinstalação
```bash
sudo make uninstall
```

## Uso
Quando executado em um terminal interativo, a interface gráfica ncurses é ativada:
```bash
./torrent-search linux
```

**Navegação:**
- ⬆ / ⬇ - Navegar entre resultados
- Enter - Download do arquivo escolhido na pasta Downloads.
- q - Sair


### Exemplos
```bash
# Buscar por "python"
./torrent-search python

# Buscar com múltiplas palavras
./torrent-search python machine learning

# Buscar e salvar resultados
./torrent-search "python" > torrents.txt
```

## Estrutura

```
ssh-torrent/
├── src/                      # Código-fonte
│   ├── main.c               # Programa principal
│   └── tui.c                # Interface de usuário (ncurses)
├── include/                 # Headers/Bibliotecas
│   ├── fetch.h              # Cliente HTTP com curl
│   ├── url.h                # Construtor de URLs
│   ├── download.h           # Integração com transmission
│   └── tui.h                # Interface TUI
├── build/                   # Artefatos de build
├── Makefile                 # Sistema de build
├── compile_commands.json    # Configuração para LSP
└── README.md                # Este arquivo
```


## Estatísticas para nerds

### Testes de Vazamento de Memória
```bash
make check
```

### Performance

- **Tempo de busca**: ~500ms para a API do PirateBay
- **Memória**: ~2-3 MB durante a execução
- **Tamanho do binário**: ~50 KB


## 🤝 Contribuindo

Contribuições são bem-vindas! Por favor:

1. Fork o projeto
2. Crie uma branch para sua feature (`git checkout -b feature/AmazingFeature`)
3. Commit suas mudanças (`git commit -m 'Add some AmazingFeature'`)
4. Push para a branch (`git push origin feature/AmazingFeature`)
5. Abra um Pull Request

## Suporte

Para relatar bugs ou sugerir melhorias, abra uma issue no repositório.

## Próximos passos

O projeto ainda não está completamente pronto e não possuo expectatativas de quando consiguirei terminá-lo. Entretanto, pretendo implementar a opção para abrir arquivo no qbittorrent, e copiar magnet link. Além disso, pesquisarei mais APIs para adicionar para uma base de dados de pesquisa maior.
