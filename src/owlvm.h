#ifndef OWLVM_H_
#define OWLVM_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Open and return the file handle with [filename].[extension]
 **/
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

#define TABLE_LOAD_FACTOR 0.7

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

/*
 * Initialize the table.
 **/
owl_table_ctx_t*
owl_table_make(void);

/*
 * Free the table.
 **/
void
owl_table_free(owl_table_ctx_t* tbl);

/*
 * Update [tbl] with [key] : [value]
 * Used to track "labels" and jump in source file.
 **/
int
owl_table_add(owl_table_ctx_t* tbl, const char* key, int value);

/*
 * Add a new reference data to the table.
 * Used to replace %define macros.
 **/
int
owl_table_addref(
    owl_table_ctx_t* tbl,
    const char*      key,
    const void*      data,
    int              len);

/*
 * Predicate on node found with [key]
 **/
int
owl_table_find(owl_table_ctx_t* tbl, const char* key);

/*
 * Find and return string in tbl, returns NULL if not found.
 **/
char*
owl_table_findref(owl_table_ctx_t* tbl, const char* key);

/*
 * Step through our %define and %include directives until fully expanded.
 **/
int
owl_comptime(char** src, int* src_len, owl_table_ctx_t* defines);

/* ## Memory / Stack ## */

#define MIN_STACK_SIZE (2 * 1024 * 1024) /* 2 MB */

#define MIN_MEMORY_SIZE (64 * 1024 * 1024) /* 64 MB */

#define NUM_REGISTERS 17

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

/*
 * Return Initialized memory with registers [NUM_REGISTERS] and space
 **/
owl_mem_t*
owl_memory_make(size_t size);

/*
 * Free [owl_mem_t*]
 **/
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
 * Pop the stack pointer setting [dest] with result.
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

/*
 * Return a [owl_program_t*] with [label_table] and [defines] initialized.
 **/
owl_program_t*
owl_program_make(void);

/*
 * Free args, values, instructions, and labels associated with p.
 * @NOTE[name=drew]{Free defines?}
 **/
void
owl_program_free(owl_program_t* p);

/* Tokens */

#define TOKEN_INCLUDE "%include"

#define TOKEN_DEFINE "%define"

/* define opcodes seperatley */
extern const char* owl_opcode_table[];
/* define registers speratley */
extern const char* owl_register_table[];

typedef struct owl_task
{
    int              registers[NUM_REGISTERS];  // Saves snapshot of registers.
    int              instruction_index;
    int*             stack_base;  // each task gets own stack region.
    int              stack_size;
    int              id;
    int              alive;       // 0 = done, 1 = runnable, 2 = waiting.
    int              waiting_on;  // address being waited on.
    struct owl_task* next;        // linked list.
} owl_task_t;

typedef struct owl_ctx
{
    owl_program_t* program;
    owl_mem_t*     memory;
    owl_task_t*    current_task;
    owl_task_t*    task_queue;
    int            next_task_id;
} owl_ctx_t;

typedef enum
{
    OWL_STEP_OK    = 0,
    OWL_STEP_YIELD = 1,
    OWL_STEP_DONE  = 2,
    OWL_STEP_ERR   = -1,
} owl_step_result_t;

/*
 * Initialize our memory [MIN_MEMORY_SIZE], program, and stack.
 **/
owl_ctx_t*
owl_vm_make(void);

/*
 * Free vm memory and program
 **/
void
owl_vm_free(owl_ctx_t* vm);

/*
 * Run comptime then lex, parse program.
 **/
int
owl_vm_evaluate(owl_ctx_t* vm, char* file_name);

/*
 * run [owl_step] on parsed program.
 **/
void
owl_vm_run(owl_ctx_t* vm);

/* Opcodes */
enum
{
    OP_NOP    = 0x0,
    OP_INT    = 0x1,
    OP_MOV    = 0x2,
    OP_PUSH   = 0x3,
    OP_POP    = 0x4,
    OP_PUSHF  = 0x5,
    OP_POPF   = 0x6,
    OP_INC    = 0x7,
    OP_DEC    = 0x8,
    OP_ADD    = 0x9,
    OP_SUB    = 0xA,
    OP_MUL    = 0xB,
    OP_DIV    = 0xC,
    OP_MOD    = 0xD,
    OP_REM    = 0xE,
    OP_NOT    = 0xF,
    OP_XOR    = 0x10,
    OP_OR     = 0x11,
    OP_AND    = 0x12,
    OP_SHL    = 0x13,
    OP_SHR    = 0x14,
    OP_CMP    = 0x15,
    OP_JMP    = 0x16,
    OP_CALL   = 0x17,
    OP_RET    = 0x18,
    OP_JE     = 0x19,
    OP_JNE    = 0x1A,
    OP_JG     = 0x1B,
    OP_JGE    = 0x1C,
    OP_JL     = 0x1D,
    OP_JLE    = 0x1E,
    OP_PRN    = 0x1F,
    OP_EXT    = 0x20,
    OP_YIELD  = 0x21,
    OP_SPAWN  = 0x22,
    OP_SEND   = 0x23,
    OP_RECV   = 0x24,
    OP_JOIN   = 0x25,
    OP_WAIT   = 0x26,
    OP_SIGNAL = 0x27,
};

#define TASK_DONE     0
#define TASK_RUNNABLE 1
#define TASK_WAITING  2
#define TASK_SENDING  3 /* blocked */

/* ## Task-Concurrency ## */

owl_task_t*
owl_task_make(owl_ctx_t* vm, int start_index);

void
owl_task_free(owl_task_t* t);

void
owl_task_enqueue(owl_ctx_t* vm, owl_task_t* t);

/*
 * Advance to the next RUNNABLE task, cycling the queue.
 * Returns NULL if nothing is runnable (deadlock or all done).
 */
owl_task_t*
owl_task_next(owl_ctx_t* vm); /* round-robin, skips waiting */

owl_task_t*
owl_task_find(owl_ctx_t* vm, int id);

/*
 * Snapshot live registers + instruction index into current_task.
 */
void
owl_task_save(owl_ctx_t* vm, int instruction_index);

/*
 * Restore current_task's registers into the live memory context.
 */
void
owl_task_restore(owl_ctx_t* vm);

/*
 * Preform one step of the virtual machine.
 **/
static inline owl_step_result_t
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
        case OP_EXT:
            return OWL_STEP_DONE;
        case OP_YIELD:
            return OWL_STEP_YIELD;
        case OP_SPAWN: {
            /*
             * args[0] = label address (instruction index) to start at.
             * The new task id is returned in EAX (register 0) of the parent.
             */
            owl_task_t* child = owl_task_make(vm, *args[0]);
            if (!child)
                return OWL_STEP_ERR;
            owl_task_enqueue(vm, child);
            vm->memory->registers[0].i32 = child->id;
            return OWL_STEP_OK;
        }
        case OP_SEND: {
            /*
             * send task_id, value
             * args[0] = target task id, args[1] = value to deliver.
             * Sender blocks (TASK_SENDING) until receiver calls recv.
             */
            owl_task_t* target = owl_task_find(vm, *args[0]);
            if (!target || target->alive == TASK_DONE) {
                return OWL_STEP_ERR;
            }
            /* I have mail from */
            target->waiting_on = vm->current_task->id;
            /* if recv is already blocked, wake it and hand off. */
            if (target->alive == TASK_WAITING) {
                /* deliver to EAX */
                target->registers[0]    = *args[1];
                target->alive           = TASK_RUNNABLE;
                vm->current_task->alive = TASK_RUNNABLE;
            } else {
                /* Recv not ready yet; park value and block */
                vm->current_task->alive = TASK_SENDING;
                /* park in EAX */
                vm->current_task->registers[0] = *args[1];
            }
            return OWL_STEP_YIELD;
        }
        case OP_RECV: {
            /*
             * recv  (no args — result always lands in EAX)
             * Block until a sender delivers a value to this task.
             */
            owl_task_t* sender =
                owl_task_find(vm, vm->current_task->waiting_on);
            if (sender && sender->alive == TASK_SENDING) {
                /* Value is already parked in sender's EAX */
                vm->memory->registers[0].i32 = sender->registers[0];
                sender->alive                = TASK_RUNNABLE;
                vm->current_task->waiting_on = -1;
            } else {
                /* no sender yet - block and wait*/
                vm->current_task->alive = TASK_WAITING;
                return OWL_STEP_YIELD;
            }
            return OWL_STEP_OK;
        }
        case OP_JOIN: {
            /*
             * join task_id
             * Block until target task is TASK_DONE.
             */
            owl_task_t* target = owl_task_find(vm, *args[0]);
            if (!target || target->alive == TASK_DONE) {
                return OWL_STEP_OK; /* already finished, fall through */
            }
            vm->current_task->alive      = TASK_WAITING;
            vm->current_task->waiting_on = *args[0];
            return OWL_STEP_YIELD;
        }
        case OP_WAIT: {
            /* wait addr -- suspend until signal addr */
            vm->current_task->alive      = TASK_WAITING;
            vm->current_task->waiting_on = *args[0];
            return OWL_STEP_YIELD;
        }
        case OP_SIGNAL: {
            /*
             * signal addr — wake one task waiting on addr.
             * Deliberately wakes only one to avoid thundering herd.
             */
            owl_task_t* t = vm->task_queue;
            while (t) {
                if (t->alive == TASK_WAITING && t->waiting_on == *args[0]) {
                    t->alive      = TASK_RUNNABLE;
                    t->waiting_on = -1;
                    break;
                }
                t = t->next;
            }
            return OWL_STEP_OK;
        }
    }
    return OWL_STEP_OK;
}
/* ## Lexer ## */

#define MAX_ARGS   2
#define MAX_TOKENS 4

typedef struct owl_lexer_ctx
{
    char**  source_lines;
    char*** tokens;
} owl_lexer_ctx_t;

/*
 * Initialize the lexer.
 **/
owl_lexer_ctx_t*
lexer_make(void);

/*
 * Free the lexer.
 **/
void
owl_lexer_free(owl_lexer_ctx_t* l);

/*
 * Read into the lexer
 **/
void
owl_lex(owl_lexer_ctx_t* lexer, char* src, owl_table_ctx_t* defines);

/* ## Parser ## */

/*
 * parse labels and add them to table.
 * special label "start" gets set to vm->program->start.
 **/
int
owl_parse_labels(owl_ctx_t* vm, const char*** tokens);

/*
 * Parse instructions and args.
 * The final instruction will be set to [-0x1] to indicate finished.
 * -1 Indicates error
 **/
int
owl_parse_program(owl_ctx_t* vm, const char*** tokens);

/*
 * append values to vm.
 **/
int*
owl_add_value(owl_ctx_t* vm, const int val);

/*
 * Parse string into integer value.
 **/
int
owl_parse_value(const char* string);

#endif
