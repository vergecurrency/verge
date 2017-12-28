/*
 * Written by: Patricia Gaughen <gone@us.ibm.com>, IBM Corporation
 * August 2002: added remote node KVA remap - Martin J. Bligh 
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
 */

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/mmzone.h>
#include <linux/highmem.h>
#include <linux/initrd.h>
#include <linux/nodemask.h>
#include <asm/e820.h>
#include <asm/setup.h>
#include <asm/mmzone.h>
#include <bios_ebda.h>

struct pglist_data *node_data[MAX_NUMNODES];
bootmem_data_t node0_bdata;

/*
 * numa interface - we expect the numa architecture specfic code to have
 *                  populated the following initialisation.
 *
 * 1) numnodes         - the total number of nodes configured in the system
 * 2) physnode_map     - the mapping between a pfn and owning node
 * 3) node_start_pfn   - the starting page frame number for a node
 * 3) node_end_pfn     - the ending page fram number for a node
 */

/*
 * physnode_map keeps track of the physical memory layout of a generic
 * numa node on a 256Mb break (each element of the array will
 * represent 256Mb of memory and will be marked by the node id.  so,
 * if the first gig is on node 0, and the second gig is on node 1
 * physnode_map will contain:
 *
 *     physnode_map[0-3] = 0;
 *     physnode_map[4-7] = 1;
 *     physnode_map[8- ] = -1;
 */
s8 physnode_map[MAX_ELEMENTS] = { [0 ... (MAX_ELEMENTS - 1)] = -1};

unsigned long node_start_pfn[MAX_NUMNODES];
unsigned long node_end_pfn[MAX_NUMNODES];

extern unsigned long find_max_low_pfn(void);
extern void find_max_pfn(void);
extern void one_highpage_init(struct page *, int, int);

extern struct e820map e820;
extern unsigned long init_pg_tables_end;
extern unsigned long highend_pfn, highstart_pfn;
extern unsigned long max_low_pfn;
extern unsigned long totalram_pages;
extern unsigned long totalhigh_pages;

#define LARGE_PAGE_BYTES (PTRS_PER_PTE * PAGE_SIZE)

unsigned long node_remap_start_pfn[MAX_NUMNODES];
unsigned long node_remap_size[MAX_NUMNODES];
unsigned long node_remap_offset[MAX_NUMNODES];
void *node_remap_start_vaddr[MAX_NUMNODES];
void set_pmd_pfn(unsigned long vaddr, unsigned long pfn, pgprot_t flags);

/*
 * FLAT - support for basic PC memory model with discontig enabled, essentially
 *        a single node with all available processors in it with a flat
 *        memory map.
 */
int __init get_memcfg_numa_flat(void)
{
	printk("NUMA - single node, flat memory mode\n");

	/* Run the memory configuration and find the top of memory. */
	find_max_pfn();
	node_start_pfn[0]  = 0;
	node_end_pfn[0]	  = max_pfn;

        /* Indicate there is one node available. */
	node_set_online(0);
	numnodes = 1;
	return 1;
}

/*
 * Find the highest page frame number we have available for the node
 */
static void __init find_max_pfn_node(int nid)
{
	if (node_end_pfn[nid] > max_pfn)
		node_end_pfn[nid] = max_pfn;
	/*
	 * if a user has given mem=XXXX, then we need to make sure 
	 * that the node _starts_ before that, too, not just ends
	 */
	if (node_start_pfn[nid] > max_pfn)
		node_start_pfn[nid] = max_pfn;
	if (node_start_pfn[nid] > node_end_pfn[nid])
		BUG();
}

/* 
 * Allocate memory for the pg_data_t for this node via a crude pre-bootmem
 * method.  For node zero take this from the bottom of memory, for
 * subsequent nodes place them at node_remap_start_vaddr which contains
 * node local data in physically node local memory.  See setup_memory()
 * for details.
 */
static void __init allocate_pgdat(int nid)
{
	if (nid)
		NODE_DATA(nid) = (pg_data_t *)node_remap_start_vaddr[nid];
	else {
		NODE_DATA(nid) = (pg_data_t *)(__va(min_low_pfn << PAGE_SHIFT));
		min_low_pfn += PFN_UP(sizeof(pg_data_t));
		memset(NODE_DATA(nid), 0, sizeof(pg_data_t));
	}
}

/*
 * Register fully available low RAM pages with the bootmem allocator.
 */
static void __init register_bootmem_low_pages(unsigned long system_max_low_pfn)
{
	int i;

	for (i = 0; i < e820.nr_map; i++) {
		unsigned long curr_pfn, last_pfn, size;
		/*
		 * Reserve usable low memory
		 */
		if (e820.map[i].type != E820_RAM)
			continue;
		/*
		 * We are rounding up the start address of usable memory:
		 */
		curr_pfn = PFN_UP(e820.map[i].addr);
		if (curr_pfn >= system_max_low_pfn)
			continue;
		/*
		 * ... and at the end of the usable range downwards:
		 */
		last_pfn = PFN_DOWN(e820.map[i].addr + e820.map[i].size);

		if (last_pfn > system_max_low_pfn)
			last_pfn = system_max_low_pfn;

		/*
		 * .. finally, did all the rounding and playing
		 * around just make the area go away?
		 */
		if (last_pfn <= curr_pfn)
			continue;

		size = last_pfn - curr_pfn;
		free_bootmem_node(NODE_DATA(0), PFN_PHYS(curr_pfn), PFN_PHYS(size));
	}
}

void __init remap_numa_kva(void)
{
	void *vaddr;
	unsigned long pfn;
	int node;

	for (node = 1; node < numnodes; ++node) {
		for (pfn=0; pfn < node_remap_size[node]; pfn += PTRS_PER_PTE) {
			vaddr = node_remap_start_vaddr[node]+(pfn<<PAGE_SHIFT);
			set_pmd_pfn((ulong) vaddr, 
				node_remap_start_pfn[node] + pfn, 
				PAGE_KERNEL_LARGE);
		}
	}
}

static unsigned long calculate_numa_remap_pages(void)
{
	int nid;
	unsigned long size, reserve_pages = 0;

	for (nid = 1; nid < numnodes; nid++) {
		/* calculate the size of the mem_map needed in bytes */
		size = (node_end_pfn[nid] - node_start_pfn[nid] + 1) 
			* sizeof(struct page) + sizeof(pg_data_t);
		/* convert size to large (pmd size) pages, rounding up */
		size = (size + LARGE_PAGE_BYTES - 1) / LARGE_PAGE_BYTES;
		/* now the roundup is correct, convert to PAGE_SIZE pages */
		size = size * PTRS_PER_PTE;
		printk("Reserving %ld pages of KVA for lmem_map of node %d\n",
				size, nid);
		node_remap_size[nid] = size;
		reserve_pages += size;
		node_remap_offset[nid] = reserve_pages;
		printk("Shrinking node %d from %ld pages to %ld pages\n",
			nid, node_end_pfn[nid], node_end_pfn[nid] - size);
		node_end_pfn[nid] -= size;
		node_remap_start_pfn[nid] = node_end_pfn[nid];
	}
	printk("Reserving total of %ld pages for numa KVA remap\n",
			reserve_pages);
	return reserve_pages;
}

/*
 * workaround for Dell systems that neglect to reserve EBDA
 */
static void __init reserve_ebda_region_node(void)
{
	unsigned int addr;
	addr = get_bios_ebda();
	if (addr)
		reserve_bootmem_node(NODE_DATA(0), addr, PAGE_SIZE);
}

unsigned long __init setup_memory(void)
{
	int nid;
	unsigned long bootmap_size, system_start_pfn, system_max_low_pfn;
	unsigned long reserve_pages, pfn;

	/*
	 * When mapping a NUMA machine we allocate the node_mem_map arrays
	 * from node local memory.  They are then mapped directly into KVA
	 * between zone normal and vmalloc space.  Calculate the size of
	 * this space and use it to adjust the boundry between ZONE_NORMAL
	 * and ZONE_HIGHMEM.
	 */
	get_memcfg_numa();

	/* Fill in the physnode_map */
	for (nid = 0; nid < numnodes; nid++) {
		printk("Node: %d, start_pfn: %ld, end_pfn: %ld\n",
				nid, node_start_pfn[nid], node_end_pfn[nid]);
		printk("  Setting physnode_map array to node %d for pfns:\n  ",
				nid);
		for (pfn = node_start_pfn[nid]; pfn < node_end_pfn[nid];
	       				pfn += PAGES_PER_ELEMENT) {
			physnode_map[pfn / PAGES_PER_ELEMENT] = nid;
			printk("%ld ", pfn);
		}
		printk("\n");
	}

	reserve_pages = calculate_numa_remap_pages();

	/* partially used pages are not usable - thus round upwards */
	system_start_pfn = min_low_pfn = PFN_UP(init_pg_tables_end);

	find_max_pfn();
	system_max_low_pfn = max_low_pfn = find_max_low_pfn() - reserve_pages;
	printk("reserve_pages = %ld find_max_low_pfn() ~ %ld\n",
			reserve_pages, max_low_pfn + reserve_pages);
	printk("max_pfn = %ld\n", max_pfn);
#ifdef CONFIG_HIGHMEM
	highstart_pfn = highend_pfn = max_pfn;
	if (max_pfn > system_max_low_pfn)
		highstart_pfn = system_max_low_pfn;
	printk(KERN_NOTICE "%ldMB HIGHMEM available.\n",
	       pages_to_mb(highend_pfn - highstart_pfn));
#endif
	printk(KERN_NOTICE "%ldMB LOWMEM available.\n",
			pages_to_mb(system_max_low_pfn));
	printk("min_low_pfn = %ld, max_low_pfn = %ld, highstart_pfn = %ld\n", 
			min_low_pfn, max_low_pfn, highstart_pfn);

	printk("Low memory ends at vaddr %08lx\n",
			(ulong) pfn_to_kaddr(max_low_pfn));
	for (nid = 0; nid < numnodes; nid++) {
		node_remap_start_vaddr[nid] = pfn_to_kaddr(
			(highstart_pfn + reserve_pages) - node_remap_offset[nid]);
		allocate_pgdat(nid);
		printk ("node %d will remap to vaddr %08lx - %08lx\n", nid,
			(ulong) node_remap_start_vaddr[nid],
			(ulong) pfn_to_kaddr(highstart_pfn + reserve_pages
			    - node_remap_offset[nid] + node_remap_size[nid]));
	}
	printk("High memory starts at vaddr %08lx\n",
			(ulong) pfn_to_kaddr(highstart_pfn));
	vmalloc_earlyreserve = reserve_pages * PAGE_SIZE;
	for (nid = 0; nid < numnodes; nid++)
		find_max_pfn_node(nid);

	NODE_DATA(0)->bdata = &node0_bdata;

	/*
	 * Initialize the boot-time allocator (with low memory only):
	 */
	bootmap_size = init_bootmem_node(NODE_DATA(0), min_low_pfn, 0, system_max_low_pfn);

	register_bootmem_low_pages(system_max_low_pfn);

	/*
	 * Reserve the bootmem bitmap itself as well. We do this in two
	 * steps (first step was init_bootmem()) because this catches
	 * the (very unlikely) case of us accidentally initializing the
	 * bootmem allocator with an invalid RAM area.
	 */
	reserve_bootmem_node(NODE_DATA(0), HIGH_MEMORY, (PFN_PHYS(min_low_pfn) +
		 bootmap_size + PAGE_SIZE-1) - (HIGH_MEMORY));

	/*
	 * reserve physical page 0 - it's a special BIOS page on many boxes,
	 * enabling clean reboots, SMP operation, laptop functions.
	 */
	reserve_bootmem_node(NODE_DATA(0), 0, PAGE_SIZE);

	/*
	 * But first pinch a few for the stack/trampoline stuff
	 * FIXME: Don't need the extra page at 4K, but need to fix
	 * trampoline before removing it. (see the GDT stuff)
	 */
	reserve_bootmem_node(NODE_DATA(0), PAGE_SIZE, PAGE_SIZE);

	/* reserve EBDA region, it's a 4K region */
	reserve_ebda_region_node();

#ifdef CONFIG_ACPI_SLEEP
	/*
	 * Reserve low memory region for sleep support.
	 */
	acpi_reserve_bootmem();
#endif

	/*
	 * Find and reserve possible boot-time SMP configuration:
	 */
	find_smp_config();

#ifdef CONFIG_BLK_DEV_INITRD
	if (LOADER_TYPE && INITRD_START) {
		if (INITRD_START + INITRD_SIZE <= (system_max_low_pfn << PAGE_SHIFT)) {
			reserve_bootmem_node(NODE_DATA(0), INITRD_START, INITRD_SIZE);
			initrd_start =
				INITRD_START ? INITRD_START + PAGE_OFFSET : 0;
			initrd_end = initrd_start+INITRD_SIZE;
		}
		else {
			printk(KERN_ERR "initrd extends beyond end of memory "
			    "(0x%08lx > 0x%08lx)\ndisabling initrd\n",
			    INITRD_START + INITRD_SIZE,
			    system_max_low_pfn << PAGE_SHIFT);
			initrd_start = 0;
		}
	}
#endif
	return system_max_low_pfn;
}

void __init zone_sizes_init(void)
{
	int nid;

	/*
	 * Insert nodes into pgdat_list backward so they appear in order.
	 * Clobber node 0's links and NULL out pgdat_list before starting.
	 */
	pgdat_list = NULL;
	for (nid = numnodes - 1; nid >= 0; nid--) {       
		if (nid)
			memset(NODE_DATA(nid), 0, sizeof(pg_data_t));
		NODE_DATA(nid)->pgdat_next = pgdat_list;
		pgdat_list = NODE_DATA(nid);
	}

	for (nid = 0; nid < numnodes; nid++) {
		unsigned long zones_size[MAX_NR_ZONES] = {0, 0, 0};
		unsigned long *zholes_size;
		unsigned int max_dma;

		unsigned long low = max_low_pfn;
		unsigned long start = node_start_pfn[nid];
		unsigned long high = node_end_pfn[nid];

		max_dma = virt_to_phys((char *)MAX_DMA_ADDRESS) >> PAGE_SHIFT;

		if (start > low) {
#ifdef CONFIG_HIGHMEM
			BUG_ON(start > high);
			zones_size[ZONE_HIGHMEM] = high - start;
#endif
		} else {
			if (low < max_dma)
				zones_size[ZONE_DMA] = low;
			else {
				BUG_ON(max_dma > low);
				BUG_ON(low > high);
				zones_size[ZONE_DMA] = max_dma;
				zones_size[ZONE_NORMAL] = low - max_dma;
#ifdef CONFIG_HIGHMEM
				zones_size[ZONE_HIGHMEM] = high - low;
#endif
			}
		}
		zholes_size = get_zholes_size(nid);
		/*
		 * We let the lmem_map for node 0 be allocated from the
		 * normal bootmem allocator, but other nodes come from the
		 * remapped KVA area - mbligh
		 */
		if (!nid)
			free_area_init_node(nid, NODE_DATA(nid),
					zones_size, start, zholes_size);
		else {
			unsigned long lmem_map;
			lmem_map = (unsigned long)node_remap_start_vaddr[nid];
			lmem_map += sizeof(pg_data_t) + PAGE_SIZE - 1;
			lmem_map &= PAGE_MASK;
			NODE_DATA(nid)->node_mem_map = (struct page *)lmem_map;
			free_area_init_node(nid, NODE_DATA(nid), zones_size,
				start, zholes_size);
		}
	}
	return;
}

void __init set_highmem_pages_init(int bad_ppro) 
{
#ifdef CONFIG_HIGHMEM
	struct zone *zone;

	for_each_zone(zone) {
		unsigned long node_pfn, node_high_size, zone_start_pfn;
		struct page * zone_mem_map;
		
		if (!is_highmem(zone))
			continue;

		printk("Initializing %s for node %d\n", zone->name,
			zone->zone_pgdat->node_id);

		node_high_size = zone->spanned_pages;
		zone_mem_map = zone->zone_mem_map;
		zone_start_pfn = zone->zone_start_pfn;

		for (node_pfn = 0; node_pfn < node_high_size; node_pfn++) {
			one_highpage_init((struct page *)(zone_mem_map + node_pfn),
					  zone_start_pfn + node_pfn, bad_ppro);
		}
	}
	totalram_pages += totalhigh_pages;
#endif
}

void __init set_max_mapnr_init(void)
{
#ifdef CONFIG_HIGHMEM
	struct zone *high0 = &NODE_DATA(0)->node_zones[ZONE_HIGHMEM];
	if (high0->spanned_pages > 0)
	      	highmem_start_page = high0->zone_mem_map;
	else
		highmem_start_page = pfn_to_page(max_low_pfn - 1) + 1;
	num_physpages = highend_pfn;
#else
	num_physpages = max_low_pfn;
#endif
}
