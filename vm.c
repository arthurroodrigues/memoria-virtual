#include <stdio.h>
#include <string.h>

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

int main(int argc, char *argv[]) {
    if (!validar_argumentos(argc, argv)) {
        return 1;
    }
    printf("Argumentos validos.\n");
    return 0;
}