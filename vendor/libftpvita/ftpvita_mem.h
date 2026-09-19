/*
 * Kernel-backed allocation for libftpvita.
 *
 * Each allocation is its own SceSysmem memory block, so there is no
 * user-space heap to fragment or corrupt and nothing to leak once the
 * block is freed.
 */

#ifndef FTPVITA_MEM_H
#define FTPVITA_MEM_H

#include <stddef.h>

void *ftpvita_mem_alloc(size_t size);
void ftpvita_mem_free(void *ptr);

#endif
