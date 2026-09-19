#include "ftpvita_mem.h"

#include <psp2/kernel/sysmem.h>

/* SCE_KERNEL_MEMBLOCK_TYPE_USER_RW blocks must be a multiple of 4 KiB. */
#define FTPVITA_MEM_ALIGN 4096u

void *ftpvita_mem_alloc(size_t size)
{
	SceUID block;
	void *base = NULL;
	size_t rounded;

	if (size == 0)
		return NULL;

	rounded = (size + FTPVITA_MEM_ALIGN - 1) & ~(size_t)(FTPVITA_MEM_ALIGN - 1);
	if (rounded < size)
		return NULL;

	block = sceKernelAllocMemBlock("FTPVita_mem",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, (SceSize)rounded, NULL);
	if (block < 0)
		return NULL;

	if (sceKernelGetMemBlockBase(block, &base) < 0 || base == NULL) {
		sceKernelFreeMemBlock(block);
		return NULL;
	}

	return base;
}

void ftpvita_mem_free(void *ptr)
{
	SceUID block;

	if (ptr == NULL)
		return;

	block = sceKernelFindMemBlockByAddr(ptr, 0);
	if (block >= 0)
		sceKernelFreeMemBlock(block);
}
