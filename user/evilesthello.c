// evilest hello world -- kernel pointer passed to kernel
// kernel should catch this with the user_mem_check assertions
// kernel should destroy user environment in response

#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	// try to print the kernel entry point as a string!  mua ha ha!
	// this time by dereferencing the entry point! mua ha ha ha!
	char *entry = (char *) 0xf010000c;
	char first = *entry;
	sys_cputs(&first, 1);
}