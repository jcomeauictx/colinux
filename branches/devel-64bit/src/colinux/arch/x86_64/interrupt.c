/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */

#include <colinux/arch/interrupt.h>

static inline void call_intr(void *func)
{
		/* FIXME: W64: Check, is this code working? Check with gdb! */
	asm("    call 1f"                            "\n"
	    "1:  pop %%rax"                          "\n"
	    "    add $2f-1b,%%rax"                   "\n"
	    "    pushf"              /* flags */     "\n"
	    /*"    push %%cs"*/          /* cs */        "\n"
	    /* FIXME: W64: Why can't push CS directly here ? */
	    "    mov %%cs,%%rbx"   "\n"
	    "    push %%rbx"         /* cs */        "\n"
	    "    push %%rax"         /* eip (2:) */  "\n"
	    "    jmp *%0"            /* jmp func */  "\n"
	    "2:  sti"                                "\n"
	    : : "r"(func): "rax", "rbx", "sp");
}

void co_monitor_arch_real_hardware_interrupt(co_monitor_t *cmon)
{
	/* 16byte gate */
	struct gate_struct64 {
		uint16_t offset_low;
		uint16_t segment;
		uint16_t ist_zero0_type_dpl_p;
		uint16_t offset_middle;
		uint32_t offset_high;
		uint32_t zero1;
	} __attribute__((packed)) *host;
	void *func;

	host = (typeof(host))(cmon->passage_page->host_state.idt.table);
	host = &host[co_passage_page->params[0]];
	func = (void *)(host->offset_low | ((uint64_t)host->offset_middle << 16) | ((uint64_t)host->offset_high << 32));
	
	call_intr(func);
}

void co_monitor_arch_enable_interrupts(void)
{
	asm("sti\n");
}
