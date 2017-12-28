/**
 * @file nmi_timer_int.c
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Zwane Mwaikambo <zwane@linuxpower.ca>
 */

#include <linux/init.h>
#include <linux/smp.h>
#include <linux/irq.h>
#include <linux/oprofile.h>
#include <linux/rcupdate.h>


#include <asm/nmi.h>
#include <asm/apic.h>
#include <asm/ptrace.h>
 
static int nmi_timer_callback(struct pt_regs * regs, int cpu)
{
	unsigned long eip = instruction_pointer(regs);
 
	oprofile_add_sample(eip, !user_mode(regs), 0, cpu);
	return 1;
}

static int timer_start(void)
{
	disable_timer_nmi_watchdog();
	set_nmi_callback(nmi_timer_callback);
	return 0;
}


static void timer_stop(void)
{
	enable_timer_nmi_watchdog();
	unset_nmi_callback();
	synchronize_kernel();
}


static struct oprofile_operations nmi_timer_ops = {
	.start	= timer_start,
	.stop	= timer_stop,
	.cpu_type = "timer"
};

int __init nmi_timer_init(struct oprofile_operations ** ops)
{
	extern int nmi_active;

	if (nmi_active <= 0)
		return -ENODEV;

	*ops = &nmi_timer_ops;
	printk(KERN_INFO "oprofile: using NMI timer interrupt.\n");
	return 0;
}
