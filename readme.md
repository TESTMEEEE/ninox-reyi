
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

Execution is cooperative and single-threaded. The program starts as **task 0**;
`spawn` assigns increasing ids (1, 2, ...) to new tasks. Only one task runs at a
time. The scheduler switches tasks when the current task yields, blocks, or
finishes, picking the next runnable task in round-robin order. All registers
(including `esp` and `ebp`) are saved and restored per task; main memory,
`FLAGS`, and the remainder register are shared. Each task is assigned a 512 KB
stack region in the upper part of memory (see limitations below).

If every task is done or blocked, the VM stops. Finishing a task — via `ext`, an
error, or running off the end of the instruction list — wakes tasks blocked in
`join` on it. `ext` does not pass its code to joiners.

`yield`
Voluntarily give up the turn to the next runnable task.

`spawn label`
Start a new task at `label` and write its id into `eax`. The child inherits the
caller's registers at the moment of the spawn but gets its own stack pointers;
the parent's `eax` is then overwritten with the child id.

`send task_id, value`
Send `value` to `task_id` and block until that task calls `recv`. If the target
is already blocked in `recv`, `eax` is set immediately; otherwise the value
stays with the sender until the target's next `recv`. Sending to an unknown or
finished task terminates the sender.

`recv`
No args. Block until a sender targets this task. The value lands in `eax` and
the sender is unblocked. Only one pending sender is tracked, so a second `send`
to the same task replaces the first.

`join task_id`
Block until `task_id` finishes. An unknown or already finished task returns
immediately. No exit status is returned.

`wait addr`
Block until `signal addr` is executed. `addr` is an integer tag, not a memory
address; signals issued while nobody is waiting are lost.

`signal addr`
Wake one task waiting on the same `addr`. Other waiters remain blocked.

### Current limitations

These are behaviors of the current implementation, not intended semantics:

- `yield` resumes at the same instruction and yields again, so a task never
  advances past it. `wait` likewise re-executes after being signaled and blocks
  again. Use `send`/`recv` for synchronization for now.
- A `send` whose receiver never calls `recv` leaves the sender blocked forever;
  the same happens if the receiver finishes first.
- Per-task stack pointers are computed outside the allocated memory and stored
  as 32-bit ints, so `push`/`pop` inside a spawned task is unreliable (it can
  segfault).






