/**
 * @file init.c
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <levon@movementarian.org>
 */

#include <linux/oprofile.h>
#include <linux/init.h>
#include <linux/errno.h>
 
/* We support CPUs that have performance counters like the Pentium Pro
 * with the NMI mode driver.
 */
 
extern int nmi_init(struct oprofile_operations ** ops);
extern int nmi_timer_init(struct oprofile_operations **ops);
extern void nmi_exit(void);

int __init oprofile_arch_init(struct oprofile_operations ** ops)
{
	int ret = -ENODEV;
#ifdef CONFIG_X86_LOCAL_APIC
	ret = nmi_init(ops);
#endif

#ifdef CONFIG_X86_IO_APIC
	if (ret < 0)
		ret = nmi_timer_init(ops);
#endif
	return ret;
}


void oprofile_arch_exit(void)
{
#ifdef CONFIG_X86_LOCAL_APIC
	nmi_exit();
#endif
}
