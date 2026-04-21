#include "index.h"
#include "pes.h"
#include "tree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

// fallback prototypes
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);
uint32_t get_file_mode(const char *path);

// ─── PROVIDED ───────────────────────────────────────────────────────────────

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            int remaining = index->count - i - 1;
            if (remaining > 0)
                memmove(&index->entries[i], &index->entries[i + 1],
                        remaining * sizeof(IndexEntry));
            index->count--;
            return index_save(index);
        }
    }
    fprintf(stderr, "error: '%s' is not in the index\n", path);
    return -1;
}

int index_status(const Index *index) {
    printf("Staged changes:\n");
    if (index->count == 0) printf("  (nothing to show)\n");
    for (int i = 0; i < index->count; i++) {
        printf("  staged:     %s\n", index->entries[i].path);
    }
    printf("\n");

    printf("Unstaged changes:\n");
    int unstaged = 0;
    for (int i = 0; i < index->count; i++) {
        struct stat st;
        if (stat(index->entries[i].path, &st) != 0) {
            printf("  deleted:    %s\n", index->entries[i].path);
            unstaged++;
        } else if (st.st_mtime != (time_t)index->entries[i].mtime_sec ||
                   st.st_size != (off_t)index->entries[i].size) {
            printf("  modified:   %s\n", index->entries[i].path);
            unstaged++;
        }
    }
    if (!unstaged) printf("  (nothing to show)\n");
    printf("\n");

    printf("Untracked files:\n");
    DIR *dir = opendir(".");
    int untracked = 0;
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
            if (!strcmp(ent->d_name, ".pes")) continue;

            int tracked = 0;
            for (int i = 0; i < index->count; i++) {
                if (!strcmp(index->entries[i].path, ent->d_name)) {
                    tracked = 1;
                    break;
                }
            }

            if (!tracked) {
                struct stat st;
                if (stat(ent->d_name, &st) == 0 && S_ISREG(st.st_mode)) {
                    printf("  untracked:  %s\n", ent->d_name);
                    untracked++;
                }
            }
        }
        closedir(dir);
    }
    if (!untracked) printf("  (nothing to show)\n");
    printf("\n");

    return 0;
}

// ─── FIXED IMPLEMENTATION ───────────────────────────────────────────────────

int index_load(Index *index) {
    index->count = 0;

    FILE *fp = fopen(INDEX_FILE, "r");
    if (!fp) return 0;

    while (index->count < MAX_INDEX_ENTRIES) {
        IndexEntry *e = &index->entries[index->count];
        char hash_hex[HASH_HEX_SIZE + 1];

        if (fscanf(fp, "%o %64s %ld %u %[^\n]\n",
                   &e->mode,
                   hash_hex,
                   &e->mtime_sec,
                   &e->size,
                   e->path) != 5)
            break;

        if (hex_to_hash(hash_hex, &e->hash) != 0) {
            fclose(fp);
            return -1;
        }

        index->count++;
    }

    fclose(fp);
    return 0;
}

int index_save(const Index *index) {
    char tmp_file[] = INDEX_FILE ".tmp";

    FILE *fp = fopen(tmp_file, "w");
    if (!fp) return -1;

    for (int i = 0; i < index->count; i++) {
        const IndexEntry *e = &index->entries[i];

        if (!e->path[0]) continue;   // safety

        char hash_hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&e->hash, hash_hex);

        fprintf(fp, "%o %s %ld %u %s\n",
                e->mode,
                hash_hex,
                e->mtime_sec,
                e->size,
                e->path);
    }

    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);

    rename(tmp_file, INDEX_FILE);
    return 0;
}

int index_add(Index *index, const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;

    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    size_t size = (size_t)st.st_size;
    char *data = NULL;

    if (size > 0) {
        data = malloc(size);
        if (!data) {
            fclose(fp);
            return -1;
        }

        if (fread(data, 1, size, fp) != size) {
            free(data);
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);

    ObjectID id;
    const void *write_data = (size > 0) ? data : "";

    if (object_write(OBJ_BLOB, write_data, size, &id) != 0) {
        free(data);
        return -1;
    }

    free(data);

    IndexEntry *e = index_find(index, path);
    if (!e) {
        if (index->count >= MAX_INDEX_ENTRIES) return -1;
        e = &index->entries[index->count++];
    }

    e->mode = get_file_mode(path);
    e->hash = id;
    e->mtime_sec = st.st_mtime;
    e->size = (uint32_t)st.st_size;

    strncpy(e->path, path, sizeof(e->path) - 1);
    e->path[sizeof(e->path) - 1] = '\0';

    return index_save(index);
}
