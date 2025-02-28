/* Requirements

A. Sistema de Arquivos

- Sistema de arquivos dentro de um arquivo de 1 GB
- Diretório raiz com arquivos: implementar sem árvore de diretórios

B. Comandos (Criar, excluir, listar e armazenar)
1. criar(nome, tamanho)
    - Criar arquivo 'nome' com inteiros 32-bits aleatórios
    - Pode ser armazenado como binário ou string (binário parece mais eficiente). Obrigatório ser legível
    - Gerar 'tam' números

2. apagar(nome)
    - Apaga arquivo com nome 'nome'

3. listar
    - Mostrar todos os arquivos com seus tamanhos, espaço total consumido e espaço livre do sistema de arquivos

4. ordenar(nome): IMPLEMENTAR POR ULTIMO
    - Ordenar os inteiros no arquivo 'nome'. Método de ordenação é livre (usar algo já implementado)
    - Problema é quando o arquivo passar de 2 MB.

5. ler(nome, inicio, fim)
    - Exibir sublista de um arquivo com nome passado como argumento. O intervalo da lista vai de 'inicio' a 'fim'

6. concatenar(nome1, nome2)
    - Concatena arquivos nome1 e nome2, cria um novo arquivo (ou sobrescreve nome1), e deleta os originais


Now, structuring the code. The main components would be:

- The virtual disk file (1GB) with structures for superblock, inodes, data blocks, and swap area.

- Functions to read/write inodes, data blocks, manage free space.

- Commands handling each operation (criar, apagar, etc.).

- Sorting with the huge page and paging to the swap area.

Possible data structures:

Superblock:

- Magic number to identify the file system.

- Total blocks.

- Free blocks count.

- Inode count.

- Root directory inode.

Inode structure:

- File name (fixed size, say 255 chars).

- File size in bytes.

- Block pointers (direct, maybe some indirect for larger files).

- Timestamps.

Each data block could be 4KB, similar to traditional file systems.

For the 1GB file layout:

- First block: superblock.

- Inode table: fixed number of inodes, maybe 1024.

- Data blocks: the rest of the space after superblock and inodes.

- Swap area: possibly a fixed section at the end of the file for paging during sorting.

So the 1GB file is divided into:

- Superblock (first block)

- Inode table

- Data blocks (for files)

- Swap area (fixed size, maybe 256MB)

Now, the code structure.

First, the virtual disk file:

- The code will create a 1GB file, initialized with the superblock, inode table, data blocks, and swap area.

- Need functions to read and write blocks from the virtual disk.

The in-memory data structures:

- A list of inodes, each representing a file.

- Free block management, perhaps using a bitmap.

But how to represent the inodes and free blocks in the virtual disk.

For simplicity, the superblock could contain:

- Magic number.

- Total blocks in the file system.

- Number of free blocks.

- Block size (e.g., 4096 bytes).

- Inode count.

- Inode table block number.

- Data block start.

- Swap area start.

The inode table is a contiguous block of inodes. Each inode has:

- File name (fixed length, say 255 bytes).

- File size.

- Block pointers (direct and indirect).

- Time stamps.

So the virtual disk layout:

Block 0: superblock.

Blocks 1 to N: inode table.

Blocks N+1 to M: data blocks (managed by the file system's free list).

Blocks M+1 to P: swap area.

Now, coding in C.

First, define the structures:

Superblock:

struct superblock {

uint32_t magic;

uint32_t block_size;

uint32_t fs_blocks; // number of blocks in the file system area (excluding swap)

uint32_t swap_blocks; // number of blocks in swap area

uint32_t inode_table_block; // starting block of inode table

uint32_t data_blocks_start; // starting block of data area

uint32_t swap_start; // starting block of swap area

uint32_t free_block_count;

uint32_t first_free_block; // block number of first free block in the data area

// ... other fields as needed

};

Inode:

struct inode {

char name[256];

uint32_t size; // in bytes

uint32_t first_block;

time_t created;

time_t modified;

// ... other fields

};

Each data block header (for linked list):

struct data_block {

uint32_t next_block; // 0 if last block

char data[block_size - sizeof(uint32_t)];

};

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <Windows.h>

#define DISK_SIZE (1ULL * 1024 * 1024 * 1024) // 1 GB
#define META_DATA_SIZE (1 * 1024 * 1024) // 1MB reservado para metadados
#define LARGE_PAGE_SIZE (2 * 1024 * 1024) // 2 MB
#define MAX_FILENAME_LENGTH 255
#define MAX_FILES 1000
#define BLOCK_SIZE 4096       // Tamanho de um bloco (4 KB)
#define NUM_BLOCKS (DISK_SIZE / BLOCK_SIZE) // Número total de blocos

typedef struct {
    char nome[MAX_FILENAME_LENGTH];
    size_t tamanho;
    size_t posicao;
} Arquivo;

typedef struct {
    unsigned char bitmap[NUM_BLOCKS / 8]; // 1 bit por bloco (array de bytes)
    Arquivo arquivos[MAX_FILES];
    size_t quantidade_arquivos;
    size_t espaco_livre;
} SistemaDeArquivos;

// typedef struct {
//     Arquivo arquivos[MAX_FILES];
//     size_t quantidade_arquivos;
//     size_t espaco_livre;
// } SistemaDeArquivos;

SistemaDeArquivos sa;
FILE* disco_virtual;
void* huge_page = NULL;

// Inicialização
void iniciar_sistema_arquivos() {
    printf("Iniciando sistema de arquivos\n");
    disco_virtual = fopen("disco_virtual.bin", "r+b");
    if (!disco_virtual) {
        disco_virtual = fopen("disco_virtual.bin", "w+b");
        fseek(disco_virtual, DISK_SIZE - META_DATA_SIZE - 1, SEEK_SET); // Define tamanho do arquivo
        fputc('\0', disco_virtual); // Escreve um byte nulo no final
        fclose(disco_virtual); // Fecha o arquivo
        disco_virtual = fopen("disco_virtual.bin", "r+b"); // Reabre o arquivo

        sa.quantidade_arquivos = 0;
        sa.espaco_livre = DISK_SIZE - META_DATA_SIZE;
    }

    else {
        // Carregar estado salvo
        fseek(disco_virtual, DISK_SIZE - META_DATA_SIZE - 1, SEEK_SET);
        fread(&sa, sizeof(SistemaDeArquivos), 1, disco_virtual);
    }
    printf("Sistema de arquivos inicializado\n");
}

// Helpers
int bloco_esta_livre(size_t bloco) {
    size_t byte_index = bloco / 8; // Índice do byte
    size_t bit_index = bloco % 8;  // Posição do bit dentro do byte
    return !(sa.bitmap[byte_index] & (1 << bit_index)); // Retorna 1 se livre, 0 se ocupado
}

void marcar_bloco_ocupado(size_t bloco) {
    size_t byte_index = bloco / 8;
    size_t bit_index = bloco % 8;
    sa.bitmap[byte_index] |= (1 << bit_index); // Define o bit como 1 (ocupado)
}

void marcar_bloco_livre(size_t bloco) {
    size_t byte_index = bloco / 8;
    size_t bit_index = bloco % 8;
    sa.bitmap[byte_index] &= ~(1 << bit_index); // Define o bit como 0 (livre)
}

size_t encontrar_bloco_livre(size_t tamanho) {
    size_t blocos_necessarios = tamanho / BLOCK_SIZE + (tamanho % BLOCK_SIZE != 0);
    size_t contador = 0;
    size_t inicio = -1;

    for (size_t i = 0; i < NUM_BLOCKS; i++) {
        if (bloco_esta_livre(i)) {
            if (contador == 0) inicio = i; // Primeiro bloco encontrado
            contador++;

            if (contador == blocos_necessarios) { // Achou espaço suficiente
                for (size_t j = inicio; j < inicio + blocos_necessarios; j++) {
                    marcar_bloco_ocupado(j); // Reservar os blocos
                }
                return inicio * BLOCK_SIZE; // Retorna a posição no disco
            }
        }
        else {
            contador = 0;
            inicio = -1;
        }
    }

    return -1; // Nenhum espaço suficiente encontrado
}

void salvar_estado() {
    fseek(disco_virtual, DISK_SIZE - META_DATA_SIZE - 1, SEEK_SET);
    fwrite(&sa, sizeof(SistemaDeArquivos), 1, disco_virtual);
    fflush(disco_virtual);
    _commit(_fileno(disco_virtual)); // Garante que os dados são persistidos no disco
}

void EnableLargePagePrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tp;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        if (LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &tp.Privileges[0].Luid)) {
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

            AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL);
            if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
                printf("Warning: Large Page privilege could not be enabled.\n");
            }
        }
        CloseHandle(hToken);
    }
}

void* allocateLargePage() {
    SIZE_T size = LARGE_PAGE_SIZE; // 2MB (Large Page Size)
    void* pMemory = NULL;

    // Try to enable Large Page support
    EnableLargePagePrivilege();

    // Try to allocate Large Pages first
    pMemory = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);

    if (!pMemory) {
        DWORD error = GetLastError();
        printf("Large Page allocation failed (Error %d). Falling back to normal pages.\n", error);

        // Allocate normal pages
        pMemory = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

        if (pMemory) {
            // Try to lock pages in RAM to prevent paging
            if (!VirtualLock(pMemory, size)) {
                printf("Warning: Failed to lock memory (Error %d). Paging may occur.\n", GetLastError());
            }
            else {
                printf("Memory locked successfully. OS will not page it out.\n");
            }
        }
    }

    if (pMemory) {
        printf("Memory allocated at: %p\n\n", pMemory);
        return pMemory;
    }
    else {
        printf("Memory allocation failed. Error: %d\n", GetLastError());
        // Fallback to malloc if VirtualAlloc fails
        return malloc(LARGE_PAGE_SIZE);
    }
}

// Liberar a memória da Large Page
void freeLargePage(void* pMemory) {
    if (pMemory) {
        // Verificar se a memória foi alocada via VirtualAlloc ou malloc
        // Uma heurística simples: endereços alocados por VirtualAlloc são tipicamente
        // alinhados a múltiplos de página (4KB)
        if ((uintptr_t)pMemory % 4096 == 0) {
            VirtualFree(pMemory, 0, MEM_RELEASE);
        }
        else {
            free(pMemory);
        }
    }
}

// Find

Arquivo* find(const char* nome) {
    Arquivo* arquivo = NULL;
    for (int i = 0; i < sa.quantidade_arquivos; i++) {
        if (strcmp(sa.arquivos[i].nome, nome) == 0) {
            arquivo = &sa.arquivos[i];
            return arquivo;
            break;
        }

    }
    return NULL;
}

// Criar
void criar(const char* nome, int tamanho) {

    // Marca o tempo de início
    clock_t start_time = clock();

    if (find(nome) != NULL) {
        printf("Erro: Arquivo '%s' já existe.\n", nome);
        return;
    }

    size_t file_size = tamanho * sizeof(int);
    if (file_size > sa.espaco_livre) {
        printf("Erro: Sem espaço suficiente\n");
        return;
    }

    // Encontrar espaço livre (exemplo: first-fit)
    size_t posicao = encontrar_bloco_livre(file_size);
    if (posicao == -1) {
        printf("Erro: Não há espaço suficiente no disco.\n");
        return;
    }

    Arquivo* arquivo = &sa.arquivos[sa.quantidade_arquivos++];
    strncpy(arquivo->nome, nome, MAX_FILENAME_LENGTH);
    arquivo->tamanho = file_size;
    arquivo->posicao = posicao;
    sa.espaco_livre -= file_size;

    // Criar e armazenar números aleatórios no arquivo
    int* numbers = malloc(file_size); // Permitido usar
    if (!numbers) {
        printf("Erro: Falha ao alocar memória para os números\n");
        return;
    }

    for (int i = 0; i < tamanho; i++) {
        numbers[i] = rand() % 1000000; // Números aleatórios entre 0 e 999999
    }

    // Posicionar ponteiro do arquivo na posição correta e escrever os dados
    fseek(disco_virtual, arquivo->posicao, SEEK_SET);
    fwrite(numbers, sizeof(int), tamanho, disco_virtual);
    fflush(disco_virtual);  // Garante que os dados são gravados imediatamente

    free(numbers); // Liberar memória após a gravação

    salvar_estado();

    // Marca o tempo de fim e calcula a duração
    clock_t end_time = clock();
    double duration = (double)(end_time - start_time) / CLOCKS_PER_SEC * 1000.0;

    printf("Arquivo '%s' criado com sucesso em %.2f ms\n", nome, duration);
}

// Apagar
void apagar(const char* nome) {
    int indice = -1;

    // Procurar pelo arquivo na lista
    for (int i = 0; i < sa.quantidade_arquivos; i++) {
        if (strcmp(sa.arquivos[i].nome, nome) == 0) {
            indice = i;
            break;
        }
    }

    // Se não encontrou, retorna erro
    if (indice == -1) {
        printf("Erro: Arquivo '%s' não encontrado\n", nome);
        return;
    }

    Arquivo* arquivo = &sa.arquivos[indice];

    // Limpar o espaço do arquivo no disco
    // Liberar os blocos no bitmap
    size_t bloco_inicial = arquivo->posicao / BLOCK_SIZE;
    size_t num_blocos = arquivo->tamanho / BLOCK_SIZE;

    for (size_t i = 0; i < num_blocos; i++) {
        sa.bitmap[bloco_inicial + i] = 0;  // Marca o bloco como livre
    }

    // Atualizar espaço livre
    sa.espaco_livre += arquivo->tamanho;

    // Remover o arquivo da lista de arquivos
    for (int j = indice; j < sa.quantidade_arquivos - 1; j++) {
        sa.arquivos[j] = sa.arquivos[j + 1];
    }

    sa.quantidade_arquivos--;

    salvar_estado();

    printf("Arquivo '%s' excluído com sucesso\n", nome);
}

// Concatenar
void concatenar(const char* nome1, const char* nome2) {
    Arquivo* arquivo1 = find(nome1);
    Arquivo* arquivo2 = find(nome2);

    if (arquivo1 == NULL || arquivo2 == NULL) {
        printf("Erro: Um dos arquivos não foi encontrado\n");
        return;
    }

    size_t novo_tamanho = arquivo1->tamanho + arquivo2->tamanho;
    if (novo_tamanho > sa.espaco_livre + arquivo1->tamanho + arquivo2->tamanho) {
        printf("Erro: Não há espaço suficiente em disco\n");
        return;
    }

    // Criar buffer para armazenar conteúdo de arquivo2
    char* buffer = malloc(arquivo2->tamanho);
    if (!buffer) {
        printf("Erro: Falha ao alocar memória\n");
        return;
    }

    // Ler conteúdo de arquivo2
    fseek(disco_virtual, arquivo2->posicao, SEEK_SET);
    fread(buffer, 1, arquivo2->tamanho, disco_virtual);

    // Escrever conteúdo de arquivo2 no final de arquivo1
    fseek(disco_virtual, arquivo1->posicao + arquivo1->tamanho, SEEK_SET);
    fwrite(buffer, 1, arquivo2->tamanho, disco_virtual);
    fflush(disco_virtual);  // Garantir que os dados foram gravados

    sa.espaco_livre += arquivo1->tamanho;
    arquivo1->tamanho = novo_tamanho;

    free(buffer);

    printf("Arquivos '%s' e '%s' foram concatenados com sucesso\n", nome1, nome2);

    apagar(nome2);

    sa.espaco_livre -= novo_tamanho;

    salvar_estado();
}

// Listar
void listar() {
    //printf("Arquivos:\n");
    printf("Listagem de arquivos:\n");
    printf("%-32s %-15s\n", "Nome", "Tamanho (bytes)");
    printf("--------------------------------------------------------------\n");
    for (int i = 0; i < sa.quantidade_arquivos; i++) {
        printf(" % -32s % -15zu\n", sa.arquivos[i].nome, sa.arquivos[i].tamanho);
    }
    if (sa.quantidade_arquivos == 0) {
        printf("Nenhum arquivo encontrado.\n");
    }
    printf("--------------------------------------------------------------\n");
    printf("Total de arquivos: %d\n", sa.quantidade_arquivos);
    printf("Espaço total: %zu bytes\n", DISK_SIZE);
    printf("Espaço disponível: %zu bytes\n", sa.espaco_livre);
}

// Ler
void ler(const char* nome, int inicio, int fim) {
    Arquivo* arquivo = NULL;
    for (int i = 0; i < sa.quantidade_arquivos; i++) {
        if (strcmp(sa.arquivos[i].nome, nome) == 0) {
            arquivo = &sa.arquivos[i];
            break;
        }
    }

    if (arquivo == NULL) {
        printf("Error: Arquivo '%s' não encontrado\n", nome);
        return;
    }

    int num_count = arquivo->tamanho / sizeof(int);

    if (inicio < 0 || fim >= num_count || inicio > fim) {
        printf("Error: Invalid range\n");
        return;
    }

    int* buffer = malloc(arquivo->tamanho);
    if (!buffer) {
        printf("Erro: Falha ao alocar memória\n");
        return;
    }

    fseek(disco_virtual, arquivo->posicao, SEEK_SET);
    fread(buffer, sizeof(int), num_count, disco_virtual);

    printf("Números %d a %d no arquivo '%s':\n", inicio, fim, nome);
    for (int i = inicio; i <= fim; i++) {
        printf("%d ", buffer[i]);
    }
    printf("\n");

    // Liberar memória
    free(buffer);
}


int comparar(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}

// Função auxiliar para criar o arquivo pagefile
size_t criar_pagefile(size_t tamanho_necessario) {
    // Verificar se pagefile já existe
    for (int i = 0; i < sa.quantidade_arquivos; i++) {
        if (strcmp(sa.arquivos[i].nome, "pagefile") == 0) {
            // Apaga o pagefile existente
            apagar("pagefile");
            break;
        }
    }
    if(find("pagefile") != NULL) apagar("pagefile");

    // Criar novo pagefile
    size_t nums_inteiros = tamanho_necessario / sizeof(int);
    criar("pagefile", nums_inteiros);

    // Encontrar o arquivo pagefile criado
    Arquivo* pagefile = NULL;
    for (int i = 0; i < sa.quantidade_arquivos; i++) {
        if (strcmp(sa.arquivos[i].nome, "pagefile") == 0) {
            pagefile = &sa.arquivos[i];
            break;
        }
    }

    if (!pagefile) {
        printf("Erro: Falha ao criar pagefile\n");
        return -1;
    }

    return pagefile->posicao;
}

// Função para mesclar dois segmentos ordenados
void merge_runs_improved(void* huge_buffer, size_t arquivo_pos, size_t run1_start, size_t run1_end,
    size_t run2_start, size_t run2_end, size_t pagefile_pos) {
    // Verificação de limites
    if (run1_end < run1_start || run2_end < run2_start) {
        printf("Erro: Limites de runs inválidos\n");
        return;
    }

    // Calcular tamanhos dos runs
    size_t run1_size = run1_end - run1_start + 1;
    size_t run2_size = run2_end - run2_start + 1;
    size_t merged_size = run1_size + run2_size;

    //printf("Mesclando runs - Run1: %zu elementos (%zu-%zu), Run2: %zu elementos (%zu-%zu)\n",
    //    run1_size, run1_start, run1_end, run2_size, run2_start, run2_end);

    // Dividir os 2MB da Large Page:
    // - 40% (0.8MB) para buffer1
    // - 40% (0.8MB) para buffer2
    // - 20% (0.4MB) para out_buffer
    size_t buffer_size = LARGE_PAGE_SIZE * 0.4 / sizeof(int);      // ~0.8MB em inteiros
    size_t out_buffer_size = LARGE_PAGE_SIZE * 0.2 / sizeof(int);  // ~0.4MB em inteiros

    int* buffer1 = (int*)huge_buffer;                              // Início da Large Page
    int* buffer2 = buffer1 + buffer_size;                          // Após buffer1
    int* out_buffer = buffer2 + buffer_size;                       // Após buffer2

    // Verificar se a divisão cabe nos 2MB
    if ((buffer_size + buffer_size + out_buffer_size) * sizeof(int) > LARGE_PAGE_SIZE) {
        printf("Erro: Divisão dos buffers excede Large Page\n");
        return;
    }

    // Posições atuais em cada run
    size_t pos1 = run1_start;
    size_t pos2 = run2_start;
    size_t output_count = 0;
    size_t output_pos = 0;

    // Buffers de leitura para cada run
    size_t buf1_size = 0;  // Elementos válidos no buffer1
    size_t buf2_size = 0;  // Elementos válidos no buffer2
    size_t buf1_pos = 0;   // Posição atual no buffer1
    size_t buf2_pos = 0;   // Posição atual no buffer2

    // Carregar dados iniciais nos buffers
    if (pos1 <= run1_end) {
        size_t read_size = (run1_end - pos1 + 1) < buffer_size ? (run1_end - pos1 + 1) : buffer_size;
        fseek(disco_virtual, arquivo_pos + pos1 * sizeof(int), SEEK_SET);
        buf1_size = fread(buffer1, sizeof(int), read_size, disco_virtual);
        pos1 += buf1_size;
    }

    if (pos2 <= run2_end) {
        size_t read_size = (run2_end - pos2 + 1) < buffer_size ? (run2_end - pos2 + 1) : buffer_size;
        fseek(disco_virtual, arquivo_pos + pos2 * sizeof(int), SEEK_SET);
        buf2_size = fread(buffer2, sizeof(int), read_size, disco_virtual);
        pos2 += buf2_size;
    }

    // Enquanto houver dados a processar
    while (buf1_pos < buf1_size || buf2_pos < buf2_size) {
        // Decidir qual elemento pegar
        if (buf1_pos < buf1_size && (buf2_pos >= buf2_size || buffer1[buf1_pos] <= buffer2[buf2_pos])) {
            out_buffer[output_count++] = buffer1[buf1_pos++];
        }
        else {
            out_buffer[output_count++] = buffer2[buf2_pos++];
        }

        // Se o buffer de saída estiver cheio, escrever no pagefile
        if (output_count == out_buffer_size) {
            fseek(disco_virtual, pagefile_pos + output_pos * sizeof(int), SEEK_SET);
            fwrite(out_buffer, sizeof(int), output_count, disco_virtual);
            output_pos += output_count;
            output_count = 0;
        }

        // Recarregar buffer1 se necessário
        if (buf1_pos == buf1_size && pos1 <= run1_end) {
            size_t read_size = (run1_end - pos1 + 1) < buffer_size ? (run1_end - pos1 + 1) : buffer_size;
            fseek(disco_virtual, arquivo_pos + pos1 * sizeof(int), SEEK_SET);
            buf1_size = fread(buffer1, sizeof(int), read_size, disco_virtual);
            pos1 += buf1_size;
            buf1_pos = 0;
        }

        // Recarregar buffer2 se necessário
        if (buf2_pos == buf2_size && pos2 <= run2_end) {
            size_t read_size = (run2_end - pos2 + 1) < buffer_size ? (run2_end - pos2 + 1) : buffer_size;
            fseek(disco_virtual, arquivo_pos + pos2 * sizeof(int), SEEK_SET);
            buf2_size = fread(buffer2, sizeof(int), read_size, disco_virtual);
            pos2 += buf2_size;
            buf2_pos = 0;
        }
    }

    // Escrever qualquer dado restante no pagefile
    if (output_count > 0) {
        fseek(disco_virtual, pagefile_pos + output_pos * sizeof(int), SEEK_SET);
        fwrite(out_buffer, sizeof(int), output_count, disco_virtual);
        output_pos += output_count;
    }

    // Copiar dados mesclados do pagefile de volta para o arquivo original
    size_t total_written = 0;
    size_t read_pos = 0;

    while (total_written < merged_size) {
        size_t to_read = (merged_size - total_written) < out_buffer_size ?
            (merged_size - total_written) : out_buffer_size;
        fseek(disco_virtual, pagefile_pos + read_pos * sizeof(int), SEEK_SET);
        size_t read = fread(out_buffer, sizeof(int), to_read, disco_virtual);

        if (read == 0) break;

        fseek(disco_virtual, arquivo_pos + (run1_start + total_written) * sizeof(int), SEEK_SET);
        fwrite(out_buffer, sizeof(int), read, disco_virtual);

        total_written += read;
        read_pos += read;
    }

    fflush(disco_virtual);

    //printf("Mesclagem concluída: %zu elementos mesclados\n", total_written);
}

// Ordenar: implementar por último
void ordenar(const char* nome) {
    clock_t start_time = clock();

    Arquivo* arquivo = NULL;
    for (int i = 0; i < sa.quantidade_arquivos; i++) {
        if (strcmp(sa.arquivos[i].nome, nome) == 0) {
            arquivo = &sa.arquivos[i];
            break;
        }
    }

    if (!arquivo) {
        printf("Erro: Arquivo '%s' não encontrado.\n", nome);
        return;
    }

    size_t num_ints = arquivo->tamanho / sizeof(int);
    printf("Ordenando arquivo '%s' com %zu inteiros (%zu bytes)\n", nome, num_ints, arquivo->tamanho);

    void* huge_buffer = allocateLargePage();
    if (!huge_buffer) {
        printf("Erro: Falha ao alocar memória para ordenação\n");
        return;
    }

    int* buffer = (int*)huge_buffer;
    size_t max_ints_in_memory = LARGE_PAGE_SIZE / sizeof(int);

    if (num_ints <= max_ints_in_memory) {
        printf("Arquivo cabe na memória. Usando ordenação direta...\n");
        fseek(disco_virtual, arquivo->posicao, SEEK_SET);
        fread(buffer, sizeof(int), num_ints, disco_virtual);
        qsort(buffer, num_ints, sizeof(int), comparar);
        fseek(disco_virtual, arquivo->posicao, SEEK_SET);
        fwrite(buffer, sizeof(int), num_ints, disco_virtual);
        fflush(disco_virtual);
    }
    else {
        printf("Arquivo excede 2MB, usando ordenação externa com paginação...\n");

        size_t pagefile_pos = criar_pagefile(arquivo->tamanho * 2);
        if (pagefile_pos == -1) {
            freeLargePage(huge_buffer);
            return;
        }

        size_t num_segments = (num_ints + max_ints_in_memory - 1) / max_ints_in_memory;
        printf("Dividindo em %zu segmentos...\n", num_segments);

        for (size_t seg = 0; seg < num_segments; seg++) {
            size_t start_idx = seg * max_ints_in_memory;
            size_t end_idx = start_idx + max_ints_in_memory;
            if (end_idx > num_ints) end_idx = num_ints;
            size_t segment_size = end_idx - start_idx;

            //printf("Ordenando segmento %zu/%zu (%zu elementos)...\n", seg + 1, num_segments, segment_size);
            fseek(disco_virtual, arquivo->posicao + start_idx * sizeof(int), SEEK_SET);
            fread(buffer, sizeof(int), segment_size, disco_virtual);
            qsort(buffer, segment_size, sizeof(int), comparar);
            fseek(disco_virtual, arquivo->posicao + start_idx * sizeof(int), SEEK_SET);
            fwrite(buffer, sizeof(int), segment_size, disco_virtual);
            fflush(disco_virtual);
        }

        size_t run_size = max_ints_in_memory;
        while (run_size < num_ints) {
            //printf("Mesclando runs de tamanho %zu...\n", run_size);

            for (size_t i = 0; i < num_ints; i += 2 * run_size) {
                size_t run1_start = i;
                size_t run1_end = i + run_size - 1;
                if (run1_end >= num_ints) run1_end = num_ints - 1;

                if (run1_end + 1 < num_ints) {
                    size_t run2_start = run1_end + 1;
                    size_t run2_end = run2_start + run_size - 1;
                    if (run2_end >= num_ints) run2_end = num_ints - 1;

                    merge_runs_improved(huge_buffer, arquivo->posicao, run1_start, run1_end,
                        run2_start, run2_end, pagefile_pos);
                }
            }
            run_size *= 2;
        }

        apagar("pagefile");
    }

    freeLargePage(huge_buffer);

    clock_t end_time = clock();
    double duration = (double)(end_time - start_time) / CLOCKS_PER_SEC * 1000.0;

    salvar_estado();

    printf("Arquivo '%s' ordenado em %.2f ms.\n", nome, duration);
}


int main() {

    iniciar_sistema_arquivos();
    allocateLargePage();

    char command[20];
    char arg1[MAX_FILENAME_LENGTH], arg2[MAX_FILENAME_LENGTH];
    int arg3, arg4;

    printf("Mini Sistema de Arquivos\n");
    printf("Comandos disponíveis:\n");
    printf("  criar nome tam\n");
    printf("  apagar nome\n");
    printf("  listar\n");
    printf("  ordenar nome\n");
    printf("  ler nome inicio fim\n");
    printf("  concatenar nome1 nome2\n");
    printf("  ajuda\n");
    printf("  sair\n");

    while (1) {
        printf("> ");
        scanf("%s", command);

        if (strcmp(command, "criar") == 0) {
            scanf("%s %d", arg1, &arg3);
            criar(arg1, arg3);
        }
        else if (strcmp(command, "apagar") == 0) {
            scanf("%s", arg1);
            apagar(arg1);
        }
        else if (strcmp(command, "listar") == 0) {
            listar();
        }
        else if (strcmp(command, "ordenar") == 0) {
            scanf("%s", arg1);
            ordenar(arg1);
        }
        else if (strcmp(command, "ler") == 0) {
            scanf("%s %d %d", arg1, &arg3, &arg4);
            ler(arg1, arg3, arg4);
        }
        else if (strcmp(command, "concatenar") == 0) {
            scanf("%s %s", arg1, arg2);
            concatenar(arg1, arg2);
        }
        else if (strcmp(command, "ajuda") == 0) {
            printf("Mini Sistema de Arquivos\n");
            printf("Comandos disponíveis:\n");
            printf("  criar nome tam\n");
            printf("  apagar nome\n");
            printf("  listar\n");
            printf("  ordenar nome\n");
            printf("  ler nome inicio fim\n");
            printf("  concatenar nome1 nome2\n");
            printf("  ajuda\n");
            printf("  sair\n");
        }
        else if (strcmp(command, "sair") == 0) {
            salvar_estado();
            break;
        }
        else {
            printf("Comando desconhecido\n");
        }
    }

    return 0;
}