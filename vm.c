#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define BITS_DESLOCAMENTO 8
#define MASCARA_DESLOCAMENTO 0xFF

static int extrair_pagina(int endereco) {
    return endereco >> BITS_DESLOCAMENTO;
}

static int extrair_deslocamento(int endereco) {
    return endereco & MASCARA_DESLOCAMENTO;
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
    int exemplos[] = {16916, 62493, 0, 65535};
    for (int i = 0; i < 4; i++) {
        printf("endereco %d -> pagina %d deslocamento %d\n",
               exemplos[i], extrair_pagina(exemplos[i]), extrair_deslocamento(exemplos[i]));
    }
    return 0;
}