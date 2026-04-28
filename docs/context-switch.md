# Cortex-M4 Context Switch

## How the CPU saves state on exception entry

When any exception fires (SysTick, PendSV, etc.), the Cortex-M4 hardware
**automatically** pushes 8 registers onto the currently active stack (PSP if
in Thread mode, MSP if in Handler mode) before jumping to the handler:

```
High address  ┌─────────┐  ← PSP before exception
              │  xPSR   │  processor state (Thumb bit, condition flags)
              │   PC    │  address to return to after exception
              │   LR    │  link register
              │   R12   │
              │   R3    │
              │   R2    │
              │   R1    │
              │   R0    │
Low address   └─────────┘  ← PSP after exception (SP now points here)
```

On exception return (`BX LR` with EXC_RETURN value `0xFFFFFFFD`), the CPU
pops these 8 registers back and resumes execution at the saved PC.

## What the hardware does NOT save

Registers R4–R11 are callee-saved by the C ABI — the compiler assumes any
function it calls will preserve them. The hardware leaves them untouched
across exception entry/exit. This means **PendSV must save and restore
R4–R11 manually** by pushing/popping them around the switch.

## Full stack layout during a context switch

A suspended task's PSP stack looks like this (low address at bottom):

```
              ┌─────────┐  ← PSP (stack pointer points here after full save)
              │   R4    │  ─┐
              │   R5    │   │
              │   R6    │   │  software frame — PendSV pushes/pops these
              │   R7    │   │
              │   R8    │   │
              │   R9    │   │
              │   R10   │   │
              │   R11   │  ─┘
              │   R0    │  ─┐
              │   R1    │   │
              │   R2    │   │
              │   R3    │   │  hardware frame — CPU pushes/pops these
              │   R12   │   │                   automatically
              │   LR    │   │
              │   PC    │   │  ← CPU jumps here on exception return
              │  xPSR   │  ─┘
High address  └─────────┘  ← PSP before exception entry
```

## The fake frame problem

PendSV switches to a task by loading its saved PSP and executing `BX LR`
with `EXC_RETURN = 0xFFFFFFFD`. The CPU then pops the hardware frame and
jumps to PC. It does not know or care whether that frame was put there by a
real interrupt or constructed manually.

A brand-new task has never been interrupted, so its stack is empty — there
is nothing to pop. `init_tasks_stack()` solves this by planting a complete
fake frame (both hardware and software layers) on each task's stack before
the scheduler starts:

```
hardware frame:  xPSR = 0x01000000  (Thumb bit set — required)
                 PC   = task_func   (CPU jumps here on first "return")
                 LR   = 0xFFFFFFFD  (EXC_RETURN: Thread mode, use PSP)
                 R12, R3, R2, R1, R0 = 0

software frame:  R4–R11 = 0
```

After `init_tasks_stack()`, every task looks identical to one that was
previously interrupted mid-execution. PendSV treats them all the same from
the very first context switch.

## PendSV flow (once implemented)

```
PendSV_Handler:
  PUSH {LR}              ; save EXC_RETURN value

  MRS  R0, PSP           ; get current task's stack pointer
  STMDB R0!, {R4-R11}    ; push software frame onto current task's stack
  ; save R0 (new PSP) into tasks[current].stack_pointer

  ; call update_next_task() → advances current_task_index

  ; load tasks[next].stack_pointer into R0
  LDMIA R0!, {R4-R11}    ; pop software frame from next task's stack
  MSR  PSP, R0           ; set PSP to next task's stack pointer

  POP  {LR}              ; restore EXC_RETURN
  BX   LR                ; exception return → CPU pops hardware frame,
                         ; jumps to next task's PC
```

## EXC_RETURN = 0xFFFFFFFD

This magic value tells the CPU what mode to return to:

| Bit field | Value | Meaning                        |
|-----------|-------|--------------------------------|
| [3]       | 1     | Return to Thread mode          |
| [2]       | 1     | Restore from PSP               |
| [1:0]     | 01    | Return to Thumb state          |

Any other `EXC_RETURN` value would return to Handler mode or use MSP,
which would corrupt the scheduler's stack.
