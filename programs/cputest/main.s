#include <cpuregs.inc>
#include <mem.inc>
#include <irq.inc>

// reset vector

.global ResetEntry

#define TimerCounter (MEM_SEC2_BASE + 0)

.org 0
	jmp ResetEntry

.org IRQ_HANDLE_OFF(IRQn_TIMER)
	// inline
	ld r1, [TimerCounter]
	add r1, r1, 1
	st.l r0, [TimerCounter]
	st.l r14, [TASK_ACTIVE]

// Main program start
.org 0x100
ResetEntry:
	// Setup timer (TODO)
	st.l r0, [TimerCounter]
	// Enable timer interrupt
	mov r1, IRQ_EN_MASK(IRQn_TIMER)
	st.l r1, [IRQ_EN]
	// Loop forever while interrupts
	jmp pc
