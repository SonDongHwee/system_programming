// CU151KO0TIF57XE6PH215S44JM3YWTX8
// Seglist version.
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

// ALIGNMENT
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12) // hyperparam : 12 is optimal

#define SEGSIZE 40 // Design Decision hyperparam : cannot exceed 90

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define GET_SIZE(p) (GET(p) & ~0X7)
#define GET_ALLOC(p) (GET(p) & 0X1)

#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

#define SET(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))
#define NOT_NULL(p) ((p) != NULL)

#define NEXT_PTR(ptr) ((char *)(ptr))
#define PREV_PTR(ptr) ((char *)(ptr) + WSIZE)

#define PRED_BKLP(ptr) (*(char **)(ptr))
#define SUCC_BLKP(ptr) (*(char **)(ptr + WSIZE))

char *heap_listp = 0;
void *free_roots[SEGSIZE];

static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void *place(void *bp, size_t asize);
static void *coalesce(void *bp);
static void pushFreeList(void *ptr, size_t size);
static void popFreeList(void *ptr);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{

    for (int i = 0; i < SEGSIZE; i++)
    {
        free_roots[i] = NULL;
    }

    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // Prologue
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1)); // Epilogue
    heap_listp = heap_listp + (2 * WSIZE);

    if (extend_heap(4) == NULL)
        return -1;
    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((bp = mem_sbrk(size)) == (void *)-1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    pushFreeList(bp, size);

    return coalesce(bp);
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; // adjusted size
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;

    asize = (size <= DSIZE) ? 2 * DSIZE : DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL)
    {
        bp = place(bp, asize);
        return bp;
    }
    // cannot fit
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;

    bp = place(bp, asize);
    return bp;
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    pushFreeList(bp, size);
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) // case1
    {
        // pushFreeList(bp, size);
        return bp;
    }
    else if (prev_alloc && !next_alloc) // case2
    {
        popFreeList(bp);
        popFreeList(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) // case3
    {
        popFreeList(bp);
        popFreeList(PREV_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else // case4
    {
        popFreeList(bp);
        popFreeList(PREV_BLKP(bp));
        popFreeList(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    pushFreeList(bp, size);
    return bp;
}

static void *find_fit(size_t asize)
{
    char *bp;
    int ind = 0;
    size_t ssize = asize;

    while (ind < SEGSIZE)
    {
        if ((ssize <= 1) || ((ind == SEGSIZE - 1) && (NOT_NULL(free_roots[ind]))))
        {
            bp = free_roots[ind];
            while (NOT_NULL(bp) && (asize > GET_SIZE(HDRP(bp))))
                bp = PRED_BKLP(bp);
            if NOT_NULL (bp)
                return bp;
        }
        ssize = ssize / 2;
        ind++;
    }

    return NULL;
}

static void *place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    popFreeList(bp);

    if (csize - asize <= 2 * DSIZE) // min block size
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
    else if (asize >= 120) // hyperparams
    {
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 1));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 1));
        pushFreeList(bp, csize - asize);
        return NEXT_BLKP(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(csize - asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(csize - asize, 0));
        pushFreeList(NEXT_BLKP(bp), csize - asize);
    }
    return bp;
}

static void pushFreeList(void *ptr, size_t size)
{
    int ind = 0;
    void *cur_ptr = ptr;
    void *opt_ptr = NULL;

    while ((ind < SEGSIZE - 1) && (size > 1))
    {
        size = size / 2;
        ind++;
    }
    cur_ptr = free_roots[ind];
    while ((NOT_NULL(cur_ptr)) && (size > GET_SIZE(HDRP(cur_ptr))))
    {
        opt_ptr = cur_ptr;
        cur_ptr = PRED_BKLP(cur_ptr);
    }
    if (NOT_NULL(cur_ptr))
    {
        SET(NEXT_PTR(ptr), cur_ptr);
        SET(PREV_PTR(cur_ptr), ptr);
        if (NOT_NULL(opt_ptr))
        {
            SET(PREV_PTR(ptr), opt_ptr);
            SET(NEXT_PTR(opt_ptr), ptr);
        }
        else
        {
            SET(PREV_PTR(ptr), NULL);
            free_roots[ind] = ptr;
        }
    }
    else
    {
        SET(NEXT_PTR(ptr), NULL);
        if (NOT_NULL(opt_ptr))
        {
            SET(PREV_PTR(ptr), opt_ptr);
            SET(NEXT_PTR(opt_ptr), ptr);
        }
        else
        {
            SET(PREV_PTR(ptr), NULL);
            free_roots[ind] = ptr;
        }
    }
}

static void popFreeList(void *ptr)
{
    int ind = 0;
    size_t size = GET_SIZE(HDRP(ptr));

    while ((ind < SEGSIZE - 1) && (size > 1))
    {
        size = size / 2;
        ind++;
    }
    if (NOT_NULL(PRED_BKLP(ptr)))
    {
        if (NOT_NULL(SUCC_BLKP(ptr)))
        {
            SET(PREV_PTR(PRED_BKLP(ptr)), SUCC_BLKP(ptr));
            SET(NEXT_PTR(SUCC_BLKP(ptr)), PRED_BKLP(ptr));
        }
        else
        {
            SET(PREV_PTR(PRED_BKLP(ptr)), NULL);
            free_roots[ind] = PRED_BKLP(ptr);
        }
    }
    else
    {
        if (NOT_NULL(SUCC_BLKP(ptr)))
            SET(NEXT_PTR(SUCC_BLKP(ptr)), NULL);
        else
            free_roots[ind] = NULL;
    }
}