#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define BITS_DESLOCAMENTO 8
#define MASCARA_DESLOCAMENTO 0xFF
#define TAMANHO_PAGINA 256
#define ARQUIVO_ARMAZENAMENTO "BACKING_STORE.bin"
#define NUM_PAGINAS 256

static int extrair_pagina(int endereco) {
    return endereco >> BITS_DESLOCAMENTO;
}

static int extrair_deslocamento(int endereco) {
    return endereco & MASCARA_DESLOCAMENTO;
}

static int carregar_pagina(FILE *armazenamento, int pagina, signed char *destino) {
    if (fseek(armazenamento, (long)pagina * TAMANHO_PAGINA, SEEK_SET) != 0) return 0;
    if (fread(destino, 1, TAMANHO_PAGINA, armazenamento) != TAMANHO_PAGINA) return 0;
    return 1;
}

static int tabela_paginas[NUM_PAGINAS];
static signed char memoria_fisica[NUM_PAGINAS][TAMANHO_PAGINA];

static int traduzir_sem_tlb(FILE *armazenamento, int endereco, int *proximo_frame, int *faltas, signed char *valor) {
    int pagina = extrair_pagina(endereco);
    int deslocamento = extrair_deslocamento(endereco);
    if (tabela_paginas[pagina] == -1) {
        if (!carregar_pagina(armazenamento, pagina, memoria_fisica[*proximo_frame])) return -1;
        tabela_paginas[pagina] = *proximo_frame;
        (*proximo_frame)++;
        (*faltas)++;
    }
    int frame = tabela_paginas[pagina];
    *valor = memoria_fisica[frame][deslocamento];
    return frame * TAMANHO_PAGINA + deslocamento;
}

static int algoritmo_valido(const char *nome) {
    return strcmp(nome, "fifo") == 0 || strcmp(nome, "lru") == 0;
}

static int validar_argumentos(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Erro: uso correto: %s <arquivo_enderecos> <fifo|lru> <fifo|lru>\n", argv[0]);
        return 0;
    }
    if (!algoritmo_valido(argv[2])) {
        fprintf(stderr, "Erro: algoritmo de pagina '%s' invalido (use fifo ou lru)\n", argv[2]);
        return 0;
    }
    if (!algoritmo_valido(argv[3])) {
        fprintf(stderr, "Erro: algoritmo de TLB '%s' invalido (use fifo ou lru)\n", argv[3]);
        return 0;
    }
    return 1;
}

static int linha_valida(const char *linha) {
    char *fim;
    errno = 0;
    long valor = strtol(linha, &fim, 10);
    if (fim == linha || errno != 0) return 0;
    while (*fim == ' ' || *fim == '\t' || *fim == '\r' || *fim == '\n') fim++;
    if (*fim != '\0') return 0;
    return valor >= 0 && valor <= 65535;
}

static int validar_arquivo(const char *caminho) {
    FILE *arquivo = fopen(caminho, "r");
    if (arquivo == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir '%s': %s\n", caminho, strerror(errno));
        return 0;
    }
    char linha[100];
    int numero = 0;
    int total = 0;
    while (fgets(linha, sizeof(linha), arquivo) != NULL) {
        numero++;
        if (!linha_valida(linha)) {
            fprintf(stderr, "Erro: formato invalido na linha %d de '%s'\n", numero, caminho);
            fclose(arquivo);
            return 0;
        }
        total++;
    }
    fclose(arquivo);
    if (total == 0) {
        fprintf(stderr, "Erro: o arquivo '%s' esta vazio\n", caminho);
        return 0;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    if (!validar_argumentos(argc, argv)) {
        return 1;
    }
    if (!validar_arquivo(argv[1])) {
        return 1;
    }
    FILE *armazenamento = fopen(ARQUIVO_ARMAZENAMENTO, "rb");
    if (armazenamento == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir '%s': %s\n", ARQUIVO_ARMAZENAMENTO, strerror(errno));
        return 1;
    }
    FILE *enderecos = fopen(argv[1], "r");
    if (enderecos == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir '%s': %s\n", argv[1], strerror(errno));
        fclose(armazenamento);
        return 1;
    }
    for (int i = 0; i < NUM_PAGINAS; i++) {
        tabela_paginas[i] = -1;
    }
    int proximo_frame = 0;
    int faltas = 0;
    char linha[64];
    while (fgets(linha, sizeof(linha), enderecos) != NULL) {
        int endereco = atoi(linha);
        signed char valor;
        int fisico = traduzir_sem_tlb(armazenamento, endereco, &proximo_frame, &faltas, &valor);
        if (fisico < 0) {
            fprintf(stderr, "Erro: falha ao ler o armazenamento\n");
            fclose(enderecos);
            fclose(armazenamento);
            return 1;
        }
        printf("Virtual address: %d Physical address: %d Value: %d\n", endereco, fisico, valor);
    }
    fprintf(stderr, "Page faults: %d\n", faltas);
    fclose(enderecos);
    fclose(armazenamento);
    return 0;
}