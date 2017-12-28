/*
 * arch/i386/mach-generic/topology.c - Populate driverfs with topology information
 *
 * Written by: Matthew Dobson, IBM Corporation
 * Original Code: Paul Dorwin, IBM Corporation, Patrick Mochel, OSDL
 *
 * Copyright (C) 2002, IBM Corp.
 *
 * All rights reserved.          
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Send feedback to <colpatch@us.ibm.com>
 */
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/nodemask.h>
#include <asm/cpu.h>

struct i386_cpu cpu_devices[NR_CPUS];

#ifdef CONFIG_NUMA
#include <linux/mmzone.h>
#include <asm/node.h>

struct i386_node node_devices[MAX_NUMNODES];

static int __init topology_init(void)
{
	int i;

	for (i = 0; i < MAX_NUMNODES; i++) {
		if (node_online(i))
			arch_register_node(i);
	}
	for (i = 0; i < NR_CPUS; i++)
		if (cpu_possible(i)) arch_register_cpu(i);
	return 0;
}

#else /* !CONFIG_NUMA */

static int __init topology_init(void)
{
	int i;

	for (i = 0; i < NR_CPUS; i++)
		if (cpu_possible(i)) arch_register_cpu(i);
	return 0;
}

#endif /* CONFIG_NUMA */

subsys_initcall(topology_init);
