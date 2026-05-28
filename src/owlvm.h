#ifndef OWLVM_H_
#define OWLVM_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE*
owl_fopen(const char* filename, const char* extension, const char* mode);

/* Copy the contents from [src] handle to [dst] */
int
owl_fcopy(char* dst, size_t size, FILE* src);

/* Get the length of a file in bytes (including EOF) */
int
owl_fsize(FILE* f);

/* ## Hash Table ## */

#define KEY_LENGTH 64

#define TABLE_SIZE 4096

typedef struct owl_table_node
{
    char*                  key;
    int                    value;
    void*                  data;
    struct owl_table_node* next;
} owl_table_node_t;

typedef struct owl_table_ctx
{
    unsigned int            num_nodes;
    unsigned int            size;
    struct owl_table_node** nodes;
} owl_table_ctx_t;

owl_table_ctx_t*
owl_table_make(void);

void
owl_table_free(owl_table_ctx_t* tbl);

int
owl_table_add(owl_table_ctx_t* tbl, const char* key, int value);

int
owl_table_addref(
    owl_table_ctx_t* tbl,
    const char*      key,
    const void*      data,
    int              len);

int
owl_table_find(owl_table_ctx_t* tbl, const char* key);

char*
owl_table_findref(owl_table_ctx_t* tbl, const char* key);

int
owl_comptime(char** src, int* src_len, owl_table_ctx_t* defines);

/* ## Memory / Stack ## */

#define MIN_STACK_SIZE (2 * 1024 * 1024) /* 2 MB */

#define MIN_MEMORY_SIZE (64 * 1024 * 1024) /* 64 MB */

union owl_reg_u
{
    int32_t  i32;
    int32_t* i32_ptr;
    union
    {
        int16_t h;
        int16_t l;
    } i16;
};

typedef struct owl_mem
{
    int              FLAGS;
    int              remainder;
    void*            memory_space;
    int              memory_space_size;
    union owl_reg_u* registers;
} owl_mem_t;

owl_mem_t*
owl_memory_make(size_t size);

void
owl_mem_free(owl_mem_t* memory);

/*
 * Set base pointer(0x7) and stack pointer(0x6)
 **/
static inline void
owl_stack_make(owl_mem_t* memory, size_t size)
{
    memory->registers[0x7].i32_ptr =
        ((int32_t*)memory->memory_space) + (size / sizeof(int32_t));
    memory->registers[0x6].i32_ptr = memory->registers[0x7].i32_ptr;
}

/*
 * Decrease stack pointer (0x6)
 * Push the item
 **/
static inline void
owl_stack_push(owl_mem_t* memory, int* item)
{
    memory->registers[0x6].i32_ptr -= 1;
    *memory->registers[0x6].i32_ptr = *item;
}

/*
 * Pop the stack pointer.
 **/
static inline void
owl_stack_pop(owl_mem_t* memory, int* dest)
{
    *dest = *memory->registers[0x6].i32_ptr;
    memory->registers[0x6].i32_ptr += 1;
}

typedef struct owl_program
{
    int              start;
    int              num_instructions;
    int*             instructions;
    int***           args;
    int**            values;
    int              num_values;
    owl_table_ctx_t* defines;
    owl_table_ctx_t* label_table;
} owl_program_t;

owl_program_t*
owl_program_make(void);

void
owl_program_free(owl_program_t* p);

/* Tokens */

#define TOKEN_INCLUDE "%include"

#define TOKEN_DEFINE "%define"

extern const char* owl_opcode_table[];

extern const char* owl_register_table[];

typedef struct owl_ctx
{
    owl_program_t* program;
    owl_mem_t*     memory;
} owl_ctx_t;

owl_ctx_t*
owl_vm_make(void);

void
owl_vm_free(owl_ctx_t* vm);

int
owl_vm_evaluate(owl_ctx_t* vm, char* file_name);

void
owl_vm_run(owl_ctx_t* vm);

/* Opcodes */
enum
{
    OP_NOP   = 0x0,
    OP_INT   = 0x1,
    OP_MOV   = 0x2,
    OP_PUSH  = 0x3,
    OP_POP   = 0x4,
    OP_PUSHF = 0x5,
    OP_POPF  = 0x6,
    OP_INC   = 0x7,
    OP_DEC   = 0x8,
    OP_ADD   = 0x9,
    OP_SUB   = 0xA,
    OP_MUL   = 0xB,
    OP_DIV   = 0xC,
    OP_MOD   = 0xD,
    OP_REM   = 0xE,
    OP_NOT   = 0xF,
    OP_XOR   = 0x10,
    OP_OR    = 0x11,
    OP_AND   = 0x12,
    OP_SHL   = 0x13,
    OP_SHR   = 0x14,
    OP_CMP   = 0x15,
    OP_JMP   = 0x16,
    OP_CALL  = 0x17,
    OP_RET   = 0x18,
    OP_JE    = 0x19,
    OP_JNE   = 0x1A,
    OP_JG    = 0x1B,
    OP_JGE   = 0x1C,
    OP_JL    = 0x1D,
    OP_JLE   = 0x1E,
    OP_PRN   = 0x1F,
};

/*
 * VM STEP
 **/
static inline void
owl_step(owl_ctx_t* vm, int* instruction_index)
{
    int** args = vm->program->args[*instruction_index];
    switch (vm->program->instructions[*instruction_index]) {
        case OP_NOP:
            break;
        case OP_INT: /* unimplemented */
            break;
        case OP_MOV:
            *args[0] = *args[1];
            break;
        case OP_PUSH:
            owl_stack_push(vm->memory, args[0]);
            break;
        case OP_POP:
            owl_stack_pop(vm->memory, args[0]);
            break;
        case OP_PUSHF:
            owl_stack_push(vm->memory, &vm->memory->FLAGS);
            break;
        case OP_POPF:
            owl_stack_pop(vm->memory, args[0]);
            break;
        case OP_INC:
            ++(*args[0]);
            break;
        case OP_DEC:
            --(*args[0]);
            break;
        case OP_ADD:
            *args[0] += *args[1];
            break;
        case OP_SUB:
            *args[0] -= *args[1];
            break;
        case OP_MUL:
            *args[0] *= *args[1];
            break;
        case OP_DIV:
            *args[0] /= *args[1];
            break;
        case OP_MOD:
            vm->memory->remainder = *args[0] % *args[1];
            break;
        case OP_REM:
            *args[0] = vm->memory->remainder;
            break;
        case OP_NOT:
            *args[0] = ~(*args[0]);
            break;
        case OP_XOR:
            *args[0] ^= *args[1];
            break;
        case OP_OR:
            *args[0] |= *args[1];
            break;
        case OP_AND:
            *args[0] &= *args[1];
            break;
        case OP_SHL:
            *args[0] <<= *args[1];
            break;
        case OP_SHR:
            *args[0] >>= *args[1];
            break;
        case OP_CMP:
            vm->memory->FLAGS =
                ((*args[0] == *args[1]) | (*args[0] > *args[1]) << 1);
            break;
        case OP_CALL:
            owl_stack_push(vm->memory, instruction_index);
        case OP_JMP:
            *instruction_index = *args[0] - 1;
            break;
        case OP_RET:
            owl_stack_pop(vm->memory, instruction_index);
            break;
        case OP_JE:
            *instruction_index =
                (vm->memory->FLAGS & 0x1) ? *args[0] - 1 : *instruction_index;
            break;
        case OP_JNE:
            *instruction_index = (!(vm->memory->FLAGS & 0x1))
                                     ? *args[0] - 1
                                     : *instruction_index;
            break;
        case OP_JG:
            *instruction_index =
                (vm->memory->FLAGS & 0x2) ? *args[0] - 1 : *instruction_index;
            break;
        case OP_JGE:
            *instruction_index =
                (vm->memory->FLAGS & 0x3) ? *args[0] - 1 : *instruction_index;
            break;
        case OP_JL:
            *instruction_index = (!(vm->memory->FLAGS & 0x3))
                                     ? *args[0] - 1
                                     : *instruction_index;
            break;
        case OP_JLE:
            *instruction_index = (!(vm->memory->FLAGS & 0x2))
                                     ? *args[0] - 1
                                     : *instruction_index;
            break;
        case OP_PRN:
            printf("%i\n", *args[0]);
    }
}

#endif
