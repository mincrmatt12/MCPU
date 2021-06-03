#pragma once

// Interrupt numbers

#define IRQn_TIMER 1

// Interrupt helpers


#define IRQ_EN_MASK(x)   (1 << x)
#define IRQ_DIS_MASK(x) ~(1 << x)
#define IRQ_HANDLE_OFF(x) (x * 4)

