# Gerenciador de Memória Virtual

Implementação 6 de Infraestrutura de Software (CESAR School).

- Aluno: Arthur Rodrigues de Andrade
- Turma: 3B
- Login: ara

## Descrição

Simulador de um gerenciador de memória virtual, baseado no exercício "Designing a Virtual Memory Manager" do livro *Operating System Concepts* (Silberschatz et al., 10ª edição). O programa lê endereços lógicos de 16 bits, traduz cada um para um endereço físico usando TLB e tabela de páginas, e grava o resultado em `correct.txt`.

Parâmetros da simulação:
- Espaço lógico de 65536 bytes: 256 páginas de 256 bytes.
- Memória física de 128 frames de 256 bytes.
- TLB de 16 entradas.
- Memória secundária simulada pelo arquivo `BACKING_STORE.bin`.

## Requisitos

- Linux, Unix ou macOS
- `gcc` e `make`
- Biblioteca `pthread`

## Compilação

```
make
```

Gera o executável `vm`. Para apagar o executável:

```
make clean
```

## Execução

```
./vm <arquivo_enderecos> <algoritmo_pagina> <algoritmo_tlb>
```

Os dois algoritmos aceitam `fifo` ou `lru`. Exemplo, com LRU para páginas e FIFO para a TLB:

```
./vm addresses.txt lru fifo
```

O arquivo `BACKING_STORE.bin` precisa estar na mesma pasta de onde o programa é executado.

## Arquivo de entrada

Um endereço lógico por linha, número inteiro entre 0 e 65535. Espaços e quebra de linha no final da linha são aceitos.

## Saída

O programa grava `correct.txt`, com uma linha por endereço:

```
Virtual address: 16916 TLB: 0 Physical address: 20 Value: 0
```

- `TLB` é o índice da entrada da TLB usada: onde a página foi encontrada (acerto) ou onde foi inserida (falha).
- `Value` é o byte lido no endereço físico, interpretado com sinal.

No final do arquivo há um resumo com o total de endereços traduzidos, page faults, taxa de page faults, acertos na TLB e taxa de acertos.

## Decisões de implementação

- **Preenchimento dos frames:** do 0 ao 127, em ordem. Quando a memória enche, o algoritmo de substituição escolhe o frame a ser reaproveitado.
- **Substituição de páginas (FIFO):** sai a página que está há mais tempo na memória. Como os frames são preenchidos em ordem, a vítima é o frame apontado por um contador circular.
- **Substituição de páginas (LRU):** cada frame guarda o instante do último acesso, e sai o de menor valor. O instante é atualizado em todo acesso, seja acerto ou falta.
- **TLB (FIFO):** a próxima entrada a ser substituída anda em círculo pelas 16 entradas.
- **TLB (LRU):** usa uma entrada vazia, se houver, e senão a menos recentemente usada. Cada acerto atualiza o instante de uso.
- **Busca na TLB com threads:** cada uma das 16 entradas é verificada por uma thread própria. Cada thread só lê a TLB e escreve no próprio resultado, então não há condição de corrida. O programa espera todas com `pthread_join` antes de decidir. Se a criação de uma thread falhar, a entrada é verificada diretamente.
- **Consistência da TLB:** quando uma página perde o frame, a entrada correspondente da TLB é invalidada.

## Tratamento de erros

O programa não falha: mostra uma mensagem em `stderr` e encerra com código 1 quando:

- o número de argumentos é diferente de 3;
- o algoritmo de página ou de TLB não é `fifo` nem `lru`;
- o arquivo de endereços não existe ou não pode ser aberto;
- o arquivo está vazio ou alguma linha não é um inteiro entre 0 e 65535;
- `BACKING_STORE.bin` não pode ser aberto ou lido;
- `correct.txt` não pode ser criado.

## Resultados

Com o `addresses.txt` fornecido (1000 endereços):

| Páginas | TLB  | Page faults | TLB hits |
|---------|------|-------------|----------|
| lru     | fifo | 539         | 54       |
| lru     | lru  | 539         | 55       |
| fifo    | fifo | 538         | 54       |
| fifo    | lru  | 538         | 55       |

A combinação `lru fifo` gera um `correct.txt` idêntico ao arquivo de referência do professor.

## Arquivos

- `vm.c`: código-fonte
- `Makefile`: compilação e limpeza
- `evidencias.log`: registro do terminal durante o desenvolvimento
