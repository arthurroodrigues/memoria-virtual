#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>    

#define BITS_DESLOCAMENTO 8
#define MASCARA_DESLOCAMENTO 0xFF
#define TAMANHO_PAGINA 256
#define ARQUIVO_ARMAZENAMENTO "BACKING_STORE.bin"
#define NUM_PAGINAS 256
#define NUM_FRAMES 128
#define TAMANHO_TLB 16
#define ARQUIVO_SAIDA "correct.txt"

typedef struct {
    int indice;
    int pagina;
    int achou;
} ArgumentoBusca;

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
static int pagina_do_frame[NUM_FRAMES];
static long ultimo_uso[NUM_FRAMES];
static long relogio = 0;
static signed char memoria_fisica[NUM_FRAMES][TAMANHO_PAGINA];

static int tlb_pagina[TAMANHO_TLB];
static int tlb_frame[TAMANHO_TLB];
static int proxima_entrada_tlb = 0;
static long uso_tlb[TAMANHO_TLB];
static long relogio_tlb = 0;

static void *verificar_entrada_tlb(void *argumento) {
    ArgumentoBusca *busca = argumento;
    busca->achou = (tlb_pagina[busca->indice] == busca->pagina);
    return NULL;
}

static int buscar_tlb_threads(int pagina, int *frame) {
    pthread_t threads[TAMANHO_TLB];
    ArgumentoBusca argumentos[TAMANHO_TLB];
    int criadas = 0;
    for (int i = 0; i < TAMANHO_TLB; i++) {
        argumentos[i].indice = i;
        argumentos[i].pagina = pagina;
        argumentos[i].achou = 0;
        if (pthread_create(&threads[i], NULL, verificar_entrada_tlb, &argumentos[i]) != 0) break;
        criadas++;
    }
    for (int i = criadas; i < TAMANHO_TLB; i++) {
        argumentos[i].achou = (tlb_pagina[i] == pagina);
    }
    int encontrado = -1;
    for (int i = 0; i < TAMANHO_TLB; i++) {
        if (i < criadas) pthread_join(threads[i], NULL);
        if (argumentos[i].achou && encontrado == -1) encontrado = i;
    }
    if (encontrado >= 0) *frame = tlb_frame[encontrado];
    return encontrado;
}

static int inserir_tlb_fifo(int pagina, int frame) {
    int indice = proxima_entrada_tlb;
    tlb_pagina[indice] = pagina;
    tlb_frame[indice] = frame;
    proxima_entrada_tlb = (proxima_entrada_tlb + 1) % TAMANHO_TLB;
    return indice;
}

static void atualizar_uso_tlb(int indice) {
    relogio_tlb++;
    uso_tlb[indice] = relogio_tlb;
}

static int inserir_tlb_lru(int pagina, int frame) {
    int indice = 0;
    for (int i = 0; i < TAMANHO_TLB; i++) {
        if (tlb_pagina[i] == -1) {
            indice = i;
            break;
        }
        if (uso_tlb[i] < uso_tlb[indice]) indice = i;
    }
    tlb_pagina[indice] = pagina;
    tlb_frame[indice] = frame;
    atualizar_uso_tlb(indice);
    return indice;
}

static void invalidar_tlb(int pagina) {
    for (int i = 0; i < TAMANHO_TLB; i++) {
        if (tlb_pagina[i] == pagina) tlb_pagina[i] = -1;
    }
}

static int escolher_frame_fifo(int *proximo_frame, int *vitima_fifo) {
    if (*proximo_frame < NUM_FRAMES) {
        int frame = *proximo_frame;
        (*proximo_frame)++;
        return frame;
    }
    int frame = *vitima_fifo;
    *vitima_fifo = (*vitima_fifo + 1) % NUM_FRAMES;
    tabela_paginas[pagina_do_frame[frame]] = -1;
    invalidar_tlb(pagina_do_frame[frame]);
    return frame;
}

static int escolher_frame_lru(int *proximo_frame) {
    if (*proximo_frame < NUM_FRAMES) {
        int frame = *proximo_frame;
        (*proximo_frame)++;
        return frame;
    }
    int vitima = 0;
    for (int f = 1; f < NUM_FRAMES; f++) {
        if (ultimo_uso[f] < ultimo_uso[vitima]) vitima = f;
    }
    tabela_paginas[pagina_do_frame[vitima]] = -1;
    invalidar_tlb(pagina_do_frame[vitima]);
    return vitima;
}

static int traduzir_endereco(FILE *armazenamento, int endereco, int usa_lru, int usa_lru_tlb, int *proximo_frame, int *vitima_fifo, int *faltas, int *acertos_tlb, int *indice_tlb, signed char *valor) {
    int pagina = extrair_pagina(endereco);
    int deslocamento = extrair_deslocamento(endereco);
    int frame;
    int indice = buscar_tlb_threads(pagina, &frame);
    if (indice >= 0) {
        (*acertos_tlb)++;
        *indice_tlb = indice;
        if (usa_lru_tlb) atualizar_uso_tlb(indice);
    } else {
        if (tabela_paginas[pagina] == -1) {
            int novo;
            if (usa_lru) {
                novo = escolher_frame_lru(proximo_frame);
            } else {
                novo = escolher_frame_fifo(proximo_frame, vitima_fifo);
            }
            if (!carregar_pagina(armazenamento, pagina, memoria_fisica[novo])) return -1;
            tabela_paginas[pagina] = novo;
            pagina_do_frame[novo] = pagina;
            (*faltas)++;
        }
        frame = tabela_paginas[pagina];
        if (usa_lru_tlb) {
            *indice_tlb = inserir_tlb_lru(pagina, frame);
        } else {
            *indice_tlb = inserir_tlb_fifo(pagina, frame);
        }
    }
    relogio++;
    ultimo_uso[frame] = relogio;
    *valor = memoria_fisica[frame][deslocamento];
    return frame * TAMANHO_PAGINA + deslocamento;
}

static void gravar_resumo(FILE *saida, int total, int faltas, int acertos_tlb) {
    fprintf(saida, "Number of Translated Addresses = %d\n", total);
    fprintf(saida, "Page Faults = %d\n", faltas);
    fprintf(saida, "Page Fault Rate = %.3f\n", (double)faltas / total);
    fprintf(saida, "TLB Hits = %d\n", acertos_tlb);
    fprintf(saida, "TLB Hit Rate = %.3f\n", (double)acertos_tlb / total);
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
    FILE *saida = fopen(ARQUIVO_SAIDA, "w");
    if (saida == NULL) {
        fprintf(stderr, "Erro: nao foi possivel criar '%s': %s\n", ARQUIVO_SAIDA, strerror(errno));
        fclose(enderecos);
        fclose(armazenamento);
        return 1;
    }
    for (int i = 0; i < NUM_PAGINAS; i++) {
        tabela_paginas[i] = -1;
    }
    for (int i = 0; i < TAMANHO_TLB; i++) {
        tlb_pagina[i] = -1;
    }
    int proximo_frame = 0;
    int vitima_fifo = 0;
    int faltas = 0;
    int acertos_tlb = 0;
    int total = 0;
    int usa_lru = strcmp(argv[2], "lru") == 0;
    int usa_lru_tlb = strcmp(argv[3], "lru") == 0;
    char linha[64];
    while (fgets(linha, sizeof(linha), enderecos) != NULL) {
        int endereco = atoi(linha);
        signed char valor;
        int indice_tlb = 0;
        int fisico = traduzir_endereco(armazenamento, endereco, usa_lru, usa_lru_tlb, &proximo_frame, &vitima_fifo, &faltas, &acertos_tlb, &indice_tlb, &valor);
        if (fisico < 0) {
            fprintf(stderr, "Erro: falha ao ler o armazenamento\n");
            fclose(saida);
            fclose(enderecos);
            fclose(armazenamento);
            return 1;
        }
        fprintf(saida, "Virtual address: %d TLB: %d Physical address: %d Value: %d\n", endereco, indice_tlb, fisico, valor);
        total++;
    }
    gravar_resumo(saida, total, faltas, acertos_tlb);
    fclose(saida);
    fclose(enderecos);
    fclose(armazenamento);
    return 0;
}