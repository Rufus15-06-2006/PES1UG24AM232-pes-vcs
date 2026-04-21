// tree.c — Tree object serialization and construction

#include "tree.h"
#include "index.h"
#include "pes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// ─── External prototypes ─────────────────────────────────────────────

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);
int index_load(Index *index);

// ─── PROVIDED ───────────────────────────────────────────────────────

uint32_t get_file_mode(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return 0100644;
}

// ─── TREE PARSE (Phase 2) ───────────────────────────────────────────

int tree_parse(const void *data, size_t len, Tree *tree_out) {
    tree_out->count = 0;

    const uint8_t *ptr = (const uint8_t *)data;
    const uint8_t *end = ptr + len;

    while (ptr < end && tree_out->count < MAX_TREE_ENTRIES) {
        TreeEntry *e = &tree_out->entries[tree_out->count++];

        // mode
        const uint8_t *space = memchr(ptr, ' ', end - ptr);
        if (!space) return -1;

        char mode_str[16] = {0};
        memcpy(mode_str, ptr, space - ptr);
        e->mode = strtol(mode_str, NULL, 8);

        ptr = space + 1;

        // name
        const uint8_t *null = memchr(ptr, '\0', end - ptr);
        if (!null) return -1;

        size_t name_len = null - ptr;
        memcpy(e->name, ptr, name_len);
        e->name[name_len] = '\0';

        ptr = null + 1;

        // hash
        if (ptr + HASH_SIZE > end) return -1;
        memcpy(e->hash.hash, ptr, HASH_SIZE);
        ptr += HASH_SIZE;
    }

    return 0;
}

// ─── SORT ───────────────────────────────────────────────────────────

static int cmp_entries(const void *a, const void *b) {
    return strcmp(((const TreeEntry *)a)->name,
                  ((const TreeEntry *)b)->name);
}

// ─── TREE SERIALIZE (Phase 2) ───────────────────────────────────────

int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
    size_t max_size = tree->count * 300;
    uint8_t *buf = malloc(max_size);
    if (!buf) return -1;

    Tree sorted = *tree;
    qsort(sorted.entries, sorted.count, sizeof(TreeEntry), cmp_entries);

    size_t offset = 0;

    for (int i = 0; i < sorted.count; i++) {
        const TreeEntry *e = &sorted.entries[i];

        int written = sprintf((char *)buf + offset, "%o %s", e->mode, e->name);
        offset += written + 1;

        memcpy(buf + offset, e->hash.hash, HASH_SIZE);
        offset += HASH_SIZE;
    }

    *data_out = buf;
    *len_out = offset;
    return 0;
}

#ifndef TEST_TREE_BUILD

// ─── TREE FROM INDEX (Phase 4) ──────────────────────────────────────

int tree_from_index(ObjectID *id_out) {
    Index index = {0};

    if (index_load(&index) != 0) return -1;

    Tree tree;
    tree.count = 0;

    for (int i = 0; i < index.count; i++) {
        if (tree.count >= MAX_TREE_ENTRIES) return -1;

        TreeEntry *te = &tree.entries[tree.count++];

        strncpy(te->name, index.entries[i].path, sizeof(te->name) - 1);
        te->name[sizeof(te->name) - 1] = '\0';

        te->mode = index.entries[i].mode;
        te->hash = index.entries[i].hash;
    }

    void *data;
    size_t len;

    if (tree_serialize(&tree, &data, &len) != 0) return -1;

    if (object_write(OBJ_TREE, data, len, id_out) != 0) {
        free(data);
        return -1;
    }

    free(data);
    return 0;
}

#endif
