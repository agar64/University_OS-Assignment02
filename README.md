# University_OS-Assignment02

This project implements a miniature file system stored inside a single 1 GB binary file. The command-line interface exposes operations to create, delete, list, read, concatenate, and sort files that live inside this virtual disk.

## Features
- **Virtual 1 GB disk** – `disco_virtual.bin` is created on first run with a reserved 1 MB metadata region and the remaining space available for file data.
- **Metadata and allocation** – The file system tracks up to 1,000 files with a bitmap allocator over 4 KB blocks plus per-file metadata (name, size, and byte offset in the disk image).
- **Persistent state** – Metadata and allocation state are flushed to the end of the disk image so the system survives process restarts.
- **File operations** – Commands let you create files of random integers, delete files, list the catalog, read ranges of values, concatenate two files, and sort file contents.
- **Large page aware sorting** – Sorting uses a 2 MB buffer allocated with Windows large pages when possible and falls back to external merge sort backed by a temporary `pagefile` for datasets larger than the in-memory buffer.

## Implementation overview

### Disk bootstrap and persistence
- On startup `iniciar_sistema_arquivos` opens or creates `disco_virtual.bin`, sizes it to 1 GB, initializes free space, and optionally reloads saved metadata from the reserved region near the end of the file.【F:OSTrab02-Main.c†L241-L314】
- Metadata lives in a `SistemaDeArquivos` struct containing the bitmap, the file table, the file count, and free-space bookkeeping. The struct is flushed with `_commit` to keep the on-disk catalog consistent between runs.【F:OSTrab02-Main.c†L224-L314】

### Space management
- The allocator uses a bitmap where each bit represents a 4 KB block. Helper functions mark bits as free/used and `encontrar_bloco_livre` performs a first-fit search for contiguous blocks large enough for the requested payload, returning the byte offset to write data.【F:OSTrab02-Main.c†L264-L307】

### Command implementations
- **criar** – Allocates space, stores the file entry, fills a buffer with random integers, writes them into the disk image, and updates metadata and free-space counters before reporting the elapsed time.【F:OSTrab02-Main.c†L403-L458】
- **apagar** – Looks up the file, zeros the relevant bitmap bits, adjusts free space, compacts the in-memory catalog, and persists the metadata.【F:OSTrab02-Main.c†L460-L501】
- **listar** – Prints a table of file names and sizes along with total and free space statistics drawn from the metadata struct.【F:OSTrab02-Main.c†L550-L566】
- **ler** – Validates the requested range, loads the file into memory, and prints the selected slice of integers by index.【F:OSTrab02-Main.c†L568-L607】
- **concatenar** – Reads the second file into memory, appends it to the first file inside the disk image, removes the second entry, and updates the tracked sizes and free space before persisting state.【F:OSTrab02-Main.c†L504-L548】

### Sorting strategy
- Sorting uses `ordenar`, which loads the target file, measures its integer count, and tries to fit the whole dataset inside a 2 MB buffer allocated via `VirtualAlloc` (with privilege escalation for large pages and a `VirtualLock` fallback when needed).【F:OSTrab02-Main.c†L316-L374】【F:OSTrab02-Main.c†L776-L814】
- If the file fits in memory, the program uses `qsort` and writes the sorted integers back in-place.【F:OSTrab02-Main.c†L802-L813】
- For larger files it performs an external merge sort: splitting the file into sorted runs sized to the buffer, storing intermediate merges in a temporary `pagefile` allocated through the same file-system API, and repeatedly merging runs until the file is sorted.【F:OSTrab02-Main.c†L814-L873】【F:OSTrab02-Main.c†L614-L774】

### Running the CLI
At startup the program prints the supported commands and enters a REPL-like loop that dispatches to each handler until `sair` is issued, persisting metadata on exit.【F:OSTrab02-Main.c†L876-L945】
