/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */
#ifndef SFMM_H
#define SFMM_H
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/*

                                 Format of an allocated memory block
    +-----------------------------------------------------------------------------------------+
    |                                       64-bits wide                                      |
    +-----------------------------------------------------------------------------------------+

    +--------------------------------------------+------------------------+---------+---------+ <- header
    |              requested_size                |      block_size        |prv alloc|  alloc  |
    |                                            | (2 LSB's implicitly 0) |  (0/1)  |   (1)   |
    |                 32 bits                    |        32 bits         |  1 bit  |  1 bit  |
    +--------------------------------------------+------------------------+---------+---------+ <- (aligned)
    |                                                                                         |
    |                                   Payload and Padding                                   |
    |                                     (N Memory Rows)                                     |
    |                                                                                         |
    |                                                                                         |
    +-----------------------------------------------------------------------------------------+

*/

/*
                                     Format of a free memory block
    +--------------------------------------------+------------------------+---------+---------+ <- header
    |                 unused                     |      block_size        |prv alloc|  alloc  |
    |                                            | (2 LSB's implicitly 0) |  (0/1)  |   (1)   |
    |                 32 bits                    |        32 bits         |  1 bit  |  1 bit  |
    +--------------------------------------------+------------------------+---------+---------+ <- (aligned)
    |                                                                                         |
    |                                Pointer to next free block                               |
    |                                                                                         |
    +-----------------------------------------------------------------------------------------+
    |                                                                                         |
    |                               Pointer to previous free block                            |
    |                                                                                         |
    +-----------------------------------------------------------------------------------------+
    |                                                                                         | 
    |                                         Unused                                          | 
    |                                     (N Memory Rows)                                     |
    |                                                                                         |
    |                                                                                         |
    +--------------------------------------------+------------------------+---------+---------+ <- footer
    |                 unused                     |      block_size        |prv alloc|  alloc  |
    |                                            | (2 LSB's implicitly 0) |  (0/1)  |   (0)   |
    |                 32 bits                    |        32 bits         |  1 bit  |  1 bit  |
    +--------------------------------------------+------------------------+---------+---------+

    NOTE: Footer contents must always be identical to header contents, including "unused" fields.

*/

#define THIS_BLOCK_ALLOCATED  0x1
#define PREV_BLOCK_ALLOCATED  0x2
#define BLOCK_SIZE_MASK 0xfffffffc

/* Structure of a block header or footer. */
typedef struct sf_header {
    unsigned block_size     : 32;
    unsigned requested_size : 32;
} sf_header;

/* Structure of a block. */
typedef struct sf_block {
    sf_header header;
    union {
	/* A free block contains links to other blocks in a free list. */
	struct {
	    struct sf_block *next;
	    struct sf_block *prev;
	} links;
	/* An allocated block contains a payload (aligned), starting here. */
	char payload[0];
    } body;
} sf_block;

/*
 * The heap is designed to keep the payload area of each block aligned to a double row (16-byte)
 * boundary.  The header of a block precedes the payload area, and is only single-row (8-byte)
 * aligned.  The heap starts with a "prologue" that consists of padding (to achieve the desired
 * alignment) and an allocated block with just a header and a footer and a minimum-size payload
 * area (which is unused).  The heap ends with an "epilogue" that consists only of an allocated
 * footer.  The prologue and epilogue are never freed, and they serve as sentinels that eliminate
 * edge cases in coalescing that would otherwise have to be treated.
 */

/*
                                         Format of the heap
    +-----------------------------------------------------------------------------------------+
    |                                       64-bits wide                                      |
    +-----------------------------------------------------------------------------------------+

                                                                                                   heap start
    +-----------------------------------------------------------------------------------------+ <- (aligned)
    |                                                                                         |
    |                                         unused                                          | padding
    |                                         64 bits                                         |
    +--------------------------------------------+------------------------+---------+---------+
    |                 unused                     |    block_size (= 32)   |prv alloc|  alloc  | prologue
    |                                            | (2 LSB's implicitly 0) |   (0)   |   (1)   | header
    |                 32 bits                    |        32 bits         |  1 bit  |  1 bit  |
    +--------------------------------------------+------------------------+---------+---------+ <- (aligned)
    |                                                                                         |
    |                                         unused                                          | padding
    |                                         64 bits                                         |
    +--------------------------------------------+--------------------------------------------+
    |                                                                                         |
    |                                         unused                                          | padding
    |                                         64 bits                                         |
    +--------------------------------------------+------------------------+---------+---------+ <- (aligned)
    |                 unused                     |    block_size (= 32)   |prv alloc|  alloc  |
    |                                            | (2 LSB's implicitly 0) |   (0)   |   (1)   | prologue
    |                 32 bits                    |        32 bits         |  1 bit  |  1 bit  | footer
    +-----------------------------------------------------------------------------------------+
    |                                                                                         |
    |                                                                                         |
    |                                                                                         |
    |                                                                                         |
    |                                 Allocated and free blocks                               |
    |                                                                                         |
    |                                                                                         |
    |                                                                                         |
    +---------------------------------------------------------------------+---------+---------+
    |                 unused                     |    block_size (= 0)    |prv alloc|  alloc  |
    |                                            | (2 LSB's implicitly 0) |  (0/1)  |   (1)   | epilogue
    |                 32 bits                    |        32 bits         |  1 bit  |  1 bit  |
    +---------------------------------------------------------------------+---------+---------+ <- heap end
                                                                                                   (aligned)
    NOTE: Prologue footer contents must always be identical to prologue header contents,
          including "unused" fields.

*/

typedef struct {
    uint64_t unused1;
    struct sf_block block;
    struct sf_header footer;
} sf_prologue;

typedef struct {
    struct sf_header header;
} sf_epilogue;

/*
 * Free blocks are maintained in a single free list that contains blocks of all sizes.
 * The freelist has the format of a circular, doubly linked lists that has a "dummy" block
 * header node between the beginning and the end of the list.  In an empty list, the next and
 * free pointers of the header node point back to itself.  In a list with something in it,
 * the next pointer of the header points to the first node in the list and the previous pointer
 * of the header points to the last thing in the list.  The header itself is never removed from
 * the list and it contains no data.  The reason for doing things this way is to avoid edge cases
 * in insertion and deletion of nodes from the list.
 */
struct sf_block sf_free_list_head;

/* sf_errno: will be set on error */
int sf_errno;

/*
 * "Quick lists":  Used to hold recently freed blocks of small sizes, so that they can be
 * used to satisfy allocations without searching lists or splitting blocks.
 * Blocks on a quick list are marked as allocated, so they are not available for coalescing.
 * The number of blocks in any individual quick list is limited to QUICK_LIST_MAX.
 * If adding a block to a quick list would cause it to exceed QUICK_LIST_MAX, then the
 * list is flushed, returning the existing blocks in the list to the main pool, and then
 * the block being freed is added to the now-empty list, leaving that list containing one block.
 *
 * The quick lists are indexed by size/16, starting from the minimum block size of 32.
 * They are maintained as singly linked lists, using a LIFO discipline.
 */

#define NUM_QUICK_LISTS 10  /* Number of quick lists. */
#define QUICK_LIST_MAX   5  /* Maximum number of blocks permitted on a single quick list. */

struct {
    int length;             // Number of blocks currently in the list.
    struct sf_block *first; // Pointer to first block in the list.
} sf_quick_lists[NUM_QUICK_LISTS];

/*
 * This is your implementation of sf_malloc. It acquires uninitialized memory that
 * is aligned and padded properly for the underlying system.
 *
 * @param size The number of bytes requested to be allocated.
 *
 * @return If size is 0, then NULL is returned without setting sf_errno.
 * If size is nonzero, then if the allocation is successful a pointer to a valid region of
 * memory of the requested size is returned.  If the allocation is not successful, then
 * NULL is returned and sf_errno is set to ENOMEM.
 */
void *sf_malloc(size_t size);

/*
 * Resizes the memory pointed to by ptr to size bytes.
 *
 * @param ptr Address of the memory region to resize.
 * @param size The minimum size to resize the memory to.
 *
 * @return If successful, the pointer to a valid region of memory is
 * returned, else NULL is returned and sf_errno is set appropriately.
 *
 *   If sf_realloc is called with an invalid pointer sf_errno should be set to EINVAL.
 *   If there is no memory available sf_realloc should set sf_errno to ENOMEM.
 *
 * If sf_realloc is called with a valid pointer and a size of 0 it should free
 * the allocated block and return NULL without setting sf_errno.
 */
void *sf_realloc(void *ptr, size_t size);

/*
 * Marks a dynamically allocated region as no longer in use.
 * Adds the newly freed block to the free list.
 *
 * @param ptr Address of memory returned by the function sf_malloc.
 *
 * If ptr is invalid, the function calls abort() to exit the program.
 */
void sf_free(void *ptr);

/* sfutil.c: Helper functions already created for this assignment. */

/*
 * Any program using the sfmm library must call this function ONCE
 * before issuing any allocation requests. This function DOES NOT
 * allocate any space to your allocator.
 */
void sf_mem_init();

/*
 * Any program using the sfmm library must call this function ONCE
 * after all allocation requests are complete. If implemented cleanly,
 * your program should have no memory leaks in valgrind after this function
 * is called.
 */
void sf_mem_fini();

/*
 * This function increases the size of your heap by adding one page of
 * memory to the end.
 *
 * @return On success, this function returns a pointer to the start of the
 * additional page, which is the same as the value that would have been returned
 * by sf_mem_end() before the size increase.  On error, NULL is returned
 * and sf_errno is set to ENOMEM.
 */
void *sf_mem_grow();

/* The size of a page of memory returned by sf_mem_grow(). */
#define PAGE_SZ 4096

/*
 * @return The starting address of the heap for your allocator.
 */
void *sf_mem_start();

/*
 * @return The ending address of the heap for your allocator.
 */
void *sf_mem_end();

/*
 * Display the contents of the heap in a human-readable form.
 */
void sf_show_header(sf_header *hp);
void sf_show_block(sf_block *bp);
void sf_show_blocks();
void sf_show_free_lists();
void sf_show_quick_lists();
void sf_show_heap();

#endif
