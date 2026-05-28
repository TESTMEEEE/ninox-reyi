#include "owlvm.h"

#include <stddef.h>

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

/*
 * Simple Hash for string [k]
 **/
static inline unsigned int
table_hash(const char* k, const unsigned int size)
{
    unsigned int hash = 1;
    char*        c;
    for (c = (char*)k; *c; c++)
        hash += (hash << *c) - *c;
    return hash % size;
}

/*
 * Rehash the table when adding a new node.
 **/
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

/*
 * Add a key to the table, returning the new head node.
 **/
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

/*
 * Find and return the nodes.
 **/
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

/*
 * Process include directive "%include otherfile.vm"
 * 0 -> Ok nothing done.
 * -1 -> Err
 *  1 -> Success
 **/
static int
process_includes(char** src, int* src_len, owl_table_ctx_t* defines)
{
    char* directive_delimiter = strstr(*src, TOKEN_INCLUDE);
    if (!directive_delimiter)
        return 0;
    /* Process include up to the next line */
    char* begin = directive_delimiter;
    char* end   = strchr(begin, '\n');
    if (!end || !begin)
        return 0;
    /* allocate space for the include string */
    int   line_len = end - begin;
    char* temp_str = calloc(line_len + 1, sizeof(char));
    memcpy(temp_str, begin, line_len);
    /* Read filename up to space ' ' */
    char* filename = (strchr(temp_str, ' ') + 1);
    /* Try to open file */
    FILE* f = owl_fopen(filename, ".vm", "r");
    if (!f) {
        printf("Unable to open file \"%s\"\n", filename);
        free(temp_str);
        return -1;
    }
    /* now that we have the file in [f], drop filename */
    free(temp_str);
    /* Allocate space for file contents */
    size_t addition_length = owl_fsize(f);
    char*  addition_string = calloc(addition_length, sizeof(char));
    /* Copy over the file string to [addition_string] */
    owl_fcopy(addition_string, addition_length, f);
    fclose(f);
    /* Find original file block length and included one. */
    size_t first_block_length  = (begin - *src);
    size_t second_block_length = ((*src + *src_len) - end);
    /* Calculate size for the new source */
    size_t new_source_length =
        (first_block_length + addition_length + second_block_length);
    char* new_source = (char*)calloc(sizeof(char), new_source_length);
    /* copy the first block, addition, and end + 1 for '\0' to new_source */
    strncpy(new_source, (*src), first_block_length);
    strcat(new_source, addition_string);
    strcat(new_source, end + 1);
    free(*src);
    *src     = new_source;
    *src_len = strlen(*src);
    /* 1 for success */
    return 1;
}

/*
 * Read defines like "%define ONE 1"
 **/
static int
process_defines(char** src, int* src_len, owl_table_ctx_t* defines)
{
    char* directive_delimiter = strstr(*src, TOKEN_DEFINE);
    if (!directive_delimiter)
        return 0;
    char* begin = directive_delimiter;
    char* end   = strchr(begin, '\n');
    if (!end)
        return 0;
    int offset = strlen(TOKEN_DEFINE) + 1;
    if (begin + offset >= end) {
        printf("Define missing arguments.\n");
        return -1;
    }
    /* after parsing %define, get space for [k v] */
    int  length = (end - (begin + offset));
    char temp_str[length + 1];
    /* zero out temp_str */
    memset(temp_str, 0, length + 1);
    memcpy(temp_str, begin + offset, length);
    /* key_str at begin + offset */
    char* key_str = temp_str;
    /* find val_str after key ' ' */
    char* val_str = strchr(temp_str, ' ');
    if (val_str) {
        *val_str = 0;
        val_str += 1;
    }
    if (!key_str || !val_str) {
        printf("Define missing arguments.\n");
        return -1;
    }
    /* Add [key_str] and [value_str] to defines table */
    if (owl_table_find(defines, key_str) < 0) {
        owl_table_addref(defines, key_str, val_str, strlen(val_str) + 1);
    } else {
        printf("Multiple definitions for %s.\n", key_str);
        return -1;
    }
    /* Calculate the new source length */
    size_t new_source_length   = *src_len - (end - begin);
    size_t first_block_length  = begin - *src;
    size_t second_block_length = (*src + *src_len) - end;

    memmove(&(*src)[first_block_length], end, second_block_length);
    *src     = realloc(*src, sizeof(char) * new_source_length);
    *src_len = new_source_length;
    /* 1 for success */
    return 1;
}

/*
 * A Compile time step processes 1 expansion of includes and defines
 * If no defines or inclues ret = 0 causing our [owl_comptime] loop to break.
 **/
static int
comptime_step(char** src, int* src_len, owl_table_ctx_t* defines)
{
    int ret = 0;
    ret += process_includes(src, src_len, defines);
    ret += process_defines(src, src_len, defines);
    return ret;
}

int
owl_comptime(char** src, int* src_len, owl_table_ctx_t* defines)
{
    int ret = 1;
    while (ret > 0) {
        ret = comptime_step(src, src_len, defines);
    }
    return ret;
}

owl_mem_t*
owl_memory_make(size_t size)
{
    owl_mem_t* m         = (owl_mem_t*)calloc(1, sizeof(owl_mem_t));
    m->registers         = calloc(NUM_REGISTERS, sizeof(union owl_reg_u));
    m->memory_space_size = size;
    m->memory_space      = (int*)calloc(size, 1);
    return m;
}

void
owl_mem_free(owl_mem_t* memory)
{
    free(memory->memory_space);
    free(memory->registers);
    free(memory);
}

owl_program_t*
owl_program_make(void)
{
    owl_program_t* p = calloc(1, sizeof(owl_program_t));
    p->label_table   = owl_table_make();
    p->defines       = owl_table_make();
    return p;
}

void
owl_program_free(owl_program_t* p)
{
    owl_table_free(p->label_table);
    if (p->values) {
        for (int i = 0; i < p->num_values; i++) {
            free(p->values[i]);
        }
        free(p->values);
    }
    if (p->args) {
        for (int i = 0; i < p->num_values; i++) {
            free(p->values[i]);
        }
        free(p->values);
    }
    if (p->args) {
        for (int i = 0; p->args[i]; i++) {
            if (p->args[i]) {
                free(p->args[i]);
            }
        }
        free(p->args);
    }
    if (p->instructions) {
        free(p->instructions);
    }
    free(p);
}

owl_ctx_t*
owl_vm_make(void)
{
    owl_ctx_t* vm = (owl_ctx_t*)calloc(1, sizeof(owl_ctx_t));
    if (!vm) {
        return NULL;
    }
    vm->memory  = owl_memory_make(MIN_MEMORY_SIZE);
    vm->program = owl_program_make();
    if (!vm->memory || !vm->program) {
        owl_vm_free(vm);
        return NULL;
    }
    owl_stack_make(vm->memory, MIN_STACK_SIZE);
    return vm;
}

int
owl_vm_evaluate(owl_ctx_t* vm, char* file_name)
{
    FILE* f             = NULL;
    int   source_length = 0;
    if (file_name) {
        for (int i = 0; i < 2; i++) {
            if (!f) {
                f = owl_fopen(file_name, ".vm", "r");
            }
        }
    }
    if (!f) {
        printf("File was not found, or does not exist. Unable to interpret.\n");
        return 1;
    }
    source_length = owl_fsize(f);
    char* source  = calloc(source_length, sizeof(char));
    owl_fcopy(source, source_length, f);
    fclose(f);
    int err = owl_comptime(&source, &source_length, vm->program->defines);
    /* Preprocessor error */
    if (err < 0)
        return 1;
    owl_lexer_ctx_t* lexer_ctx = lexer_make();
    owl_lex(lexer_ctx, source, vm->program->defines);
    free(source);
    if (owl_parse_labels(vm, (const char***)lexer_ctx->tokens) != 0) {
        return 1;
    }
    if (owl_parse_program(vm, (const char***)lexer_ctx->tokens) != 0) {
        return 1;
    }
    owl_lexer_free(lexer_ctx);
    return 0;
}

void
owl_vm_run(owl_ctx_t* vm)
{
    int* instruction_idx = &vm->memory->registers[0x8].i32;
    *instruction_idx     = vm->program->start;
    for (; vm->program->instructions[*instruction_idx] != -0x1;
         ++(*instruction_idx)) {
        owl_step(vm, instruction_idx);
    }
}

void
owl_vm_free(owl_ctx_t* vm)
{
    if (vm && vm->memory) {
        owl_mem_free(vm->memory);
    }
    if (vm && vm->program) {
        owl_program_free(vm->program);
    }
    if (vm) {
        free(vm);
    }
}

owl_lexer_ctx_t*
lexer_make(void)
{
    return (owl_lexer_ctx_t*)calloc(1, sizeof(owl_lexer_ctx_t));
}

void
owl_lexer_free(owl_lexer_ctx_t* l)
{
    for (int i = 0; l->source_lines[i]; i++) {
        free(l->source_lines[i]);
    }
    free(l->source_lines);
    for (int i = 0; l->tokens[i]; i++) {
        for (int j = 0; j < MAX_TOKENS; j++) {
            free(l->tokens[i][j]);
        }
        free(l->tokens[i]);
    }
    free(l->tokens);
    free(l);
}

void
owl_lex(owl_lexer_ctx_t* lexer, char* src, owl_table_ctx_t* defines)
{
    int   i     = 0;
    int   j     = 0;
    char* tokp  = NULL;
    char* linep = strtok(src, "\n");
    /* split source into lines */
    for (i = 0; linep; i++) {
        lexer->source_lines =
            (char**)realloc(lexer->source_lines, sizeof(char*) * (i + 2));
        lexer->source_lines[i] = (char*)calloc(1, strlen(linep) + 1);
        strcpy(lexer->source_lines[i], linep);
        /* Comments '#' */
        char* comment_delimiter = strchr(lexer->source_lines[i], '#');
        if (comment_delimiter)
            *comment_delimiter = 0;
        linep = strtok(NULL, "\n");
    }
    lexer->source_lines[i] = NULL;
    /* Tokenize */
    for (i = 0; lexer->source_lines[i]; i++) {
        lexer->tokens =
            (char***)realloc(lexer->tokens, sizeof(char**) * (i + 2));
        lexer->tokens[i] = (char**)calloc(MAX_TOKENS, sizeof(char*));
        tokp             = strtok(lexer->source_lines[i], " \t,");
        for (j = 0; (tokp && j < MAX_TOKENS); j++) {
            char* token         = owl_table_findref(defines, tokp);
            token               = token ? token : tokp;
            lexer->tokens[i][j] = calloc(1, (strlen(token) + 1));
            strcpy(lexer->tokens[i][j], token);
            tokp = strtok(NULL, " \t,");
        }
    }
    lexer->tokens[i] = NULL;
    owl_table_free(defines);
}

const char* owl_opcode_table[] = {
    "nop", "int", "mov", "push", "pop", "pushf", "popf", "inc", "dec",
    "add", "sub", "mul", "div",  "mod", "rem",   "not",  "xor", "or",
    "and", "shl", "shr", "cmp",  "jmp", "call",  "ret",  "je",  "jne",
    "jg",  "jge", "jl",  "jle",  "prn", 0};

const char* owl_register_table[] = {
    "eax",
    "ebx",
    "ecx",
    "edx",
    "esi",
    "edi",
    "esp",
    "ebp",
    "eip",
    "r08",
    "r09",
    "r10",
    "r11",
    "r12",
    "r13",
    "r14",
    "r15",
    0};

static int*
token_to_register(const char* token, owl_mem_t* mem)
{
    for (int i = 0; owl_register_table[i]; i++) {
        if (strcmp(token, owl_register_table[i]) == 0) {
            return &mem->registers[i].i32;
        }
    }
    return NULL;
}

static int
instruction_to_opcode(const char* instruction)
{
    for (int i = 0; owl_opcode_table[i]; i++) {
        if (strcmp(instruction, owl_opcode_table[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static int**
owl_parse_args(
    owl_ctx_t*   vm,
    const char** instruction_tokens,
    int*         instruction_place)
{
    int** args = calloc(sizeof(int*), MAX_ARGS);
    for (int i = 0; i < MAX_ARGS; ++i) {
        if (!instruction_tokens[*instruction_place + 1 + i] ||
            !strlen(instruction_tokens[*instruction_place + 1 + i])) {
            continue;
        }
        char* newline =
            strchr(instruction_tokens[*instruction_place + 1 + i], '\n');
        if (newline)
            *newline = 0;
        /* Check to see if token specifies a register */
        int* regp = token_to_register(
            instruction_tokens[*instruction_place + 1 + i], vm->memory);
        if (regp) {
            args[i] = regp;
            continue;
        }
        /* Check to see whether the token specifies an address */
        if (instruction_tokens[*instruction_place + 1 + i][0] == '[') {
            char* end_symbol =
                strchr(instruction_tokens[*instruction_place + 1 + i], ']');
            if (end_symbol) {
                *end_symbol = 0;
                args[i]     = &((int*)vm->memory->memory_space)[owl_parse_value(
                    instruction_tokens[*instruction_place + 1 + i] + 1)];
                continue;
            }
        }
        /* Check if the argument is a label */
        int addr = owl_table_find(
            vm->program->label_table,
            instruction_tokens[*instruction_place + 1 + i]);
        if (addr != -1) {
            args[i] = owl_add_value(vm, addr);
            continue;
        }
        /* parse it as a value */
        args[i] = owl_add_value(
            vm,
            owl_parse_value(instruction_tokens[*instruction_place + 1 + i]));
    }
    return args;
}

/*
 * Frees the memory allocated by [owl_parse_args]
 **/
static void
owl_free_args(int** args)
{
    if (args) {
        for (int i = 0; args[i]; i++) {
            free(args[i]);
        }
    }
    free(args);
}

/*
 * Convert one instruction -> bytecode
 **/
static int
owl_parse_instruction(
    owl_ctx_t*   vm,
    const char** instruction_tokens,
    int*         instruction_place)
{
    for (int token_idx = 0; token_idx < MAX_TOKENS; token_idx++) {
        if (!instruction_tokens[token_idx])
            continue;
        int opcode = instruction_to_opcode(instruction_tokens[token_idx]);
        if (opcode == -1)
            continue;
        if (instruction_place)
            *instruction_place = token_idx;
        vm->program->num_instructions++;
        return opcode;
    }
    return -1;
}

int
owl_parse_labels(owl_ctx_t* vm, const char*** tokens)
{
    int            num_instructions = 0;
    owl_program_t* p                = vm->program;

    for (int i = 0; tokens[i]; i++) {
        int valid_instruction = 0;
        for (int token_idx = 0; token_idx < MAX_TOKENS; token_idx++) {
            if (!tokens[i][token_idx]) {
                continue;
            }
            if (instruction_to_opcode(tokens[i][token_idx]) != -1) {
                valid_instruction = 1;
            }
            char* label_delimiter = strchr(tokens[i][token_idx], ':');
            if (label_delimiter == NULL)
                continue;
            *label_delimiter = 0;
            if (strcmp(tokens[i][token_idx], "start") == 0) {
                p->start = num_instructions;
            }
            int label_addr =
                owl_table_find(p->label_table, tokens[i][token_idx]);
            if (label_addr != -1) {
                printf("Label '%s' defined twice\n", tokens[i][token_idx]);
                return 1;
            }
            owl_table_add(
                p->label_table, tokens[i][token_idx], num_instructions);
        }
        if (valid_instruction)
            num_instructions++;
    }
    return 0;
}

int
owl_parse_program(owl_ctx_t* vm, const char*** tokens)
{
    int line_idx;
    for (line_idx = 0; tokens[line_idx]; line_idx++) {
        int instruction_place = 0;
        int opcode =
            owl_parse_instruction(vm, tokens[line_idx], &instruction_place);
        if (opcode == -1)
            continue;
        int** args = owl_parse_args(vm, tokens[line_idx], &instruction_place);
        if (!args)
            continue;
        void* new_ptr = NULL;
        new_ptr       = realloc(
            vm->program->instructions,
            sizeof(int) * (vm->program->num_instructions + 1));
        if (new_ptr != NULL) {
            vm->program->instructions = new_ptr;
            vm->program->instructions[vm->program->num_instructions - 1] =
                opcode;
        } else {
            owl_free_args(args);
            return -1;
        }
        new_ptr = realloc(
            vm->program->args,
            sizeof(int**) * (vm->program->num_instructions + 1));
        if (new_ptr != NULL) {
            vm->program->args = (int***)new_ptr;
        } else {
            owl_free_args(args);
            return -1;
        }
        vm->program->args[vm->program->num_instructions - 1] = args;
    }
    vm->program->args[vm->program->num_instructions]         = NULL;
    vm->program->instructions[vm->program->num_instructions] = -0x1;
    return 0;
}

int*
owl_add_value(owl_ctx_t* vm, const int val)
{
    owl_program_t* p = vm->program;
    p->values        = realloc(p->values, sizeof(int*) * (p->num_values + 1));
    p->values[p->num_values]  = (int*)calloc(1, sizeof(int));
    *p->values[p->num_values] = val;
    return p->values[p->num_values++];
}

int
owl_parse_value(const char* string)
{
    char* delim = strchr(string, '|');
    int   base  = 0;
    if (delim) {
        char* ident = delim + 1;
        switch (*ident) {
            case 'h':
                base = 16;
                break;
            case 'b':
                base = 2;
                break;
            default:
                base = 0;
                break;
        }
    }
    return strtoul(string, NULL, base);
}
