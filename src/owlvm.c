#include "owlvm.h"

FILE*
owl_fopen(const char* filename, const char* extension, const char* mode)
{
    FILE*  f           = NULL;
    size_t fname_chars = strlen(filename) + strlen(extension) + 1;
    char*  fname       = calloc(fname_chars, sizeof(char));
    strcpy(fname, filename);

    for (int i = 0; i < 2 && !f; i++) {
        if (i > 0) {
            strcat(fname, extension);
        }
        f = fopen(fname, mode);
    }
    free(fname);
    return f;
}

int
owl_fcopy(char* dst, size_t size, FILE* src)
{
    long   pos = ftell(src);
    size_t i   = 0;

    for (i = 0; i < size && !feof(src); i++) {
        dst[i] = fgetc(src);
    }
    dst[i - 1] = 0;
    fseek(src, pos, SEEK_SET);
    return 0;
}

int
owl_fsize(FILE* f)
{
    int  length = 0;
    long pos    = ftell(f);
    for (length = 0; !feof(f); length++) {
        fgetc(f);
    }
    fseek(f, pos, SEEK_SET);
    return length;
}

owl_table_ctx_t*
owl_table_make(void)
{
    owl_table_ctx_t* tbl = (owl_table_ctx_t*)calloc(1, sizeof(owl_table_ctx_t));
    tbl->size            = TABLE_SIZE;
    tbl->nodes =
        (owl_table_node_t**)calloc(tbl->size, sizeof(owl_table_node_t*));
    tbl->num_nodes = 0;
    return tbl;
}

void
owl_table_free(owl_table_ctx_t* tbl)
{
    owl_table_node_t* node;
    owl_table_node_t* next;
    for (int i = 0; i < tbl->size; i++) {
        node = tbl->nodes[i];
        while (node) {
            next = node->next;
            if (node->data)
                free(node->data);
            free(node->key);
            free(node);
            node = next;
        }
    }
    free(tbl->nodes);
    free(tbl);
}

static inline unsigned int
table_hash(const char* k, const unsigned int size)
{
    unsigned int hash = 1;
    char*        c;
    for (c = (char*)k; *c; c++)
        hash += (hash << *c) - *c;
    return hash % size;
}

static void
table_rehash(owl_table_ctx_t* original, unsigned int size)
{
    int               i;
    owl_table_node_t* node;
    owl_table_node_t* next;
    owl_table_ctx_t*  new;
    new        = (owl_table_ctx_t*)calloc(1, sizeof(owl_table_ctx_t));
    new->nodes = (owl_table_node_t**)calloc(size, sizeof(owl_table_node_t*));
    new->size  = size;
    new->num_nodes = 0;

    for (i = 0; i < original->size; i++) {
        node = original->nodes[i];
        while (node) {
            next = node->next;
            if (node->data) {
                owl_table_addref(
                    new, node->key, node->data, strlen(node->data) + 1);
                free(node->data);
            } else {
                owl_table_add(new, node->key, node->value);
            }
            free(node->key);
            free(node);
            node = next;
        }
    }
    free(original->nodes);
    /* Transpose new hash table onto the old one */
    memcpy(original, new, sizeof(owl_table_ctx_t));
    free(new);
}

static owl_table_node_t*
table_add_core(owl_table_ctx_t* tbl, const char* k)
{
    if ((float)++tbl->num_nodes / tbl->size > TABLE_LOAD_FACTOR) {
        table_rehash(tbl, tbl->num_nodes * 2);
    }
    int               hash = table_hash(k, tbl->size);
    owl_table_node_t* node = tbl->nodes[hash];
    owl_table_node_t* prev = NULL;

    if (node) {
        while (node->next) {
            node = node->next;
        }
        prev = node;
    }
    node      = calloc(1, sizeof(owl_table_node_t));
    node->key = (char*)calloc((strlen(k) + 1), sizeof(char));
    strcpy(node->key, k);

    if (prev) {
        prev->next = node;
    } else {
        tbl->nodes[hash] = node;
    }
    node->next = NULL;
    return node;
}

int
owl_table_add(owl_table_ctx_t* tbl, const char* key, int value)
{
    owl_table_node_t* node = table_add_core(tbl, key);
    if (!node) {
        return -1;
    }
    node->value = value;
    return 0;
}

int
owl_table_addref(
    owl_table_ctx_t* tbl,
    const char*      key,
    const void*      data,
    int              len)
{
    owl_table_node_t* node = table_add_core(tbl, key);
    if (!node) {
        return -1;
    }
    node->data = calloc(len, sizeof(char));
    memcpy(node->data, data, len);
    return 0;
}

static owl_table_node_t*
table_find_core(owl_table_ctx_t* tbl, const char* key)
{
    int               hash = table_hash(key, tbl->size);
    owl_table_node_t* node = tbl->nodes[hash];
    while (node) {
        if (!strcmp(node->key, key))
            return node;
        node = node->next;
    }
    return NULL;
}

int
owl_table_find(owl_table_ctx_t* tbl, const char* key)
{
    owl_table_node_t* node = table_find_core(tbl, key);
    if (!node)
        return -1;
    return node->value;
}

char*
owl_table_findref(owl_table_ctx_t* tbl, const char* key)
{
    owl_table_node_t* node = table_find_core(tbl, key);
    if (!node)
        return NULL;
    return node->data;
}

int
owl_comptime(char** src, int* src_len, owl_table_ctx_t* defines);
