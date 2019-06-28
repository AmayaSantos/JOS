// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW 0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	pte_t pte = uvpt[(uintptr_t)addr >> PGSHIFT];
	if(!(err & FEC_WR)) {
		panic("pgfault: faulting access was not a write");
	}
	if (!(pte & PTE_COW)){
		panic("pgfault: faulting access was not copy on write");
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	if ((r = sys_page_alloc(sys_getenvid(), PFTEMP, PTE_SYSCALL)) < 0) {
		panic("pgfault: sys_page_alloc failed for env_id=%d. Exit code %d (check error.h)", sys_getenvid(), r);
	}

	memmove(PFTEMP, ROUNDDOWN(addr,PGSIZE), PGSIZE);

	if ((r = sys_page_map(sys_getenvid(), PFTEMP, sys_getenvid(), ROUNDDOWN(addr,PGSIZE), PTE_SYSCALL)) < 0) {
		panic("pgfault: sys_page_map: failed for env_id=%d. Exit code %d (check error.h)", sys_getenvid(), r);
	}

	if ((r = sys_page_unmap(0, PFTEMP)) < 0) {
		panic("pgfault: sys_page_unmap failed for env_id=%d. Exit code %d (check error.h)", sys_getenvid(), r);
	}

}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn) {
	void *va = (void *) (pn * PGSIZE);
	int r;

	// LAB 4: Your code here.
	pte_t pte = uvpt[pn];
	int perm = pte & (PTE_COW | PTE_SYSCALL);

	// '''' pte_share
 	if (!(perm & PTE_SHARE) && ((perm & PTE_W) || (perm & PTE_COW))) {
		perm &= ~PTE_W;
		perm = perm | PTE_COW;
	}

	if ((r = sys_page_map(sys_getenvid(), va, envid, va, perm)) < 0) {
		panic("duppage: sys_page_map failed for env_id=%d. Exit code %d (check error.h)", sys_getenvid(), r);
	}

	if ((r = sys_page_map(sys_getenvid(), va, sys_getenvid(), va, perm)) < 0) {
		panic("duppage: sys_page_map failed for env_id=%d. Exit code %d (check error.h)", sys_getenvid(), r);
	}

	return 0;
}

static void
dup_or_share(envid_t dstenv, void *va, int perm)
{
	int r;

	// This is NOT what you should do in your fork.

	if ((perm & PTE_W) == PTE_W) {
		if ((r = sys_page_alloc(dstenv, va, perm)) < 0)
			panic("sys_page_alloc: %e", r);
		if ((r = sys_page_map(dstenv, va, 0, UTEMP, perm)) < 0)
			panic("sys_page_map: %e", r);
		memmove(UTEMP, va, PGSIZE);
		if ((r = sys_page_unmap(0, UTEMP)) < 0)
			panic("sys_page_unmap: %e", r);
	} else {
		if ((r = sys_page_map(0, va, dstenv, va, perm)) < 0)
			panic("sys_page_map: %e", r);
	}
}

envid_t
fork_v0(void)
{
	int r;
	envid_t envid;
	uint8_t *addr;

	envid = sys_exofork();
	if (envid < 0)
		panic("sys_exofork: %e", envid);
	if (envid == 0) {
		// We're the child.
		// The copied value of the global variable 'thisenv'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// We're the parent.
	// Eagerly copy our entire address space into the child.
	for (addr = 0; (int) addr < UTOP; addr += PGSIZE) {

		if ((PGOFF(uvpd[PDX(addr)]) & PTE_P) != PTE_P) {
			continue;
		}

		int perm = PGOFF(uvpt[PGNUM(addr)]);

		if ((perm & PTE_P) == PTE_P) {
			dup_or_share(envid, addr, perm & PTE_SYSCALL);
		}
	}

	// Also copy the stack we are currently running on.

	// Start the child environment running
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);

	return envid;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	int r;
	envid_t envid;
	uint8_t *addr;

	set_pgfault_handler(pgfault);
	envid = sys_exofork();
	if (envid < 0)
		panic("sys_exofork: %e", envid);
	if (envid == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}


	for (addr = 0; addr < (uint8_t*)(UXSTACKTOP - PGSIZE); addr += PGSIZE){
		if ((PGOFF(uvpd[PDX(addr)]) & PTE_P) != PTE_P) {
			continue;
		}
		int perm = PGOFF(uvpt[PGNUM(addr)]);
		if ((perm & PTE_P) == PTE_P) {
			duppage(envid, PGNUM(addr));
		}
	}

	if((r = sys_page_alloc(envid, (void*)(UXSTACKTOP - PGSIZE), PTE_SYSCALL)) < 0){
		panic("fork: sys_page_alloc failed for env_id=%d. Exit code %d (check error.h)", envid, r);
	}

	extern void _pgfault_upcall(void);

	if((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0){
		panic("fork: sys_env_set_pgfault_upcall failed for env_id=%d. Exit code %d (check error.h)", envid, r);
	}

	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);

	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
