
# Ninox-reyi

Simple Virtual machine that follows Intel x86 assembly syntax.

## Instruction Syntax
Destination first, Source Second.

`mov eax, ONE`

Separates comments, instructions.
`;` 

## Labels. 
Must start the beginning of a line or on their own line.
`start:`

NASM like preprocessor macros

## Preprocessor 

### Define. `%define identifier value`
All instances of `identifier` replaced with `value`.
`%define ONE 1`
`%define LIMIT 100`

### Include. `%include filename`
Pastes the contents of `filename` into the source code before interpretation.
`%include some_module.vm`

## Values

Values can be specified in decimal, hexadecimal, or binary.
Unless prefixed by `0x` `0b` or suffixed by `|h` or `|b`,
the is assumed to be in decimal.

Example: 32

decimal: `32`.
hexadecimal: `0x20` `20|h`.
binary: `0b100000` `100000|b`.

## Registers

Total registers = 17.

General Purpose =
, eax
, ebx
, ecx
, edx
, esi
, edi
, r08 - r15

Special Registers =
, esp - Stack pointer, points to the top of the stack.
, ebp - Base pointer, pointer to the base of the stack.
, eip - Instruction pointer, only modified by jump instructions, never directly.
, flags - 

## Memory

Addressed in units of four bytes.

256th word in the address space.
`[256]`
`[100|h]`
`[0x100]`
`[100000000|b]`

No positive address within the address space is off limits.

# Instruction List

## Memory
`mov arg0, arg1`
Moves value from arg1 to arg0. arg0 <- arg1.

## Stack
`push arg`
Pushes arg onto the stack.

`pop arg`
Pops a value from the stack, storing it in `arg`

`pushf`
Pushes `FLAGS` register onto the stack.

`popf arg`
Pops the flag register to arg.

## Calling

`call address`
Push the current address to the stack and jump to the subroutine specified.

`ret`
Pop the previous address from the stack to the instruction pointer to return control to the caller.

## Arithmetic

`inc arg`
Increments arg.

`dec arg`
Decrements arg.

`add arg0, arg1`
Adds arg1 to arg0, storing the result in arg0.

`sub arg0, arg1`
Subtracts arg1 from arg0, storing the result in arg0

`mul arg0, arg1`
Multiplies arg1 and arg0, storing the result in arg0

`div arg0, arg1`
Divides arg0 by arg1, storing the quotient in arg0

`mod arg0, arg1`
Same as the '%' (modulus) operator in C. 
Calculates arg0 mod arg1 and stores the result in the remainder register.

`rem arg`
Retrieves the value stored in the remainder register, storing it in arg

`shl arg0, arg1`
Shift arg0 left by arg1 places

`shr arg0, arg1`
Shifts arg0 right by arg1 places

## Logical 

`not arg`
Calculates the binary NOT of arg, storing it in arg

`xor arg0, arg1`
Calculates the binary XOR of arg0 and arg1, storing the result in arg0

`or arg0, arg1`
Calculates the binary OR of arg0 and arg1, storing the result in arg0

`and arg0, arg1`
Calculates the binary AND of arg0 and arg1, storing the result in arg0

`cmp arg0, arg1`
Compares arg0 and arg1, storing the result in the FLAGS register

## Jumps

`jmp address`
Jumps to an address or label

`je address`
Jump if equal

`jne address`
Jump if not equal

`jg address`
Jump if greater

`jge address`
Jump if equal or greater

`jl address`
Jump if lesser

`jle address`
Jump if lesser or equal

## IO

`prn arg`
Print an integer.

`ext code`
Exit with a code.

## Concurrency

Each task gets a slice of the main memory as stack space.
Upper positions of the memory space get divided into regions.

`yield`
Yield Stack.

`spawn label`
Clones all register states, 
Points `esp` and `ebp` to the top of the stacks region.

`send task_id, value`

`recv`
No args, result always lands in `eax` 

`join task_id`
Block until target task is `TASK_DONE`

`wait address`
Suspend until signal address.

`signal addr`
Wake one task waiting on address.






