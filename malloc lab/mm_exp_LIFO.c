/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

#define ALIGNMENT 8

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4 // word size
#define DSIZE 8 // double word size
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

/*Read and Write word */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/*Compute address of header and footer*/
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/*Compute previous and next block addr*/
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

/* Doubly linked Explicit free list */
#define SUCC_BLKP(bp) (*(char **)(bp + WSIZE))
#define PRED_BLKP(bp) (*(char **)(bp))

#define SET_SUCC(bp, ptr) (SUCC_BLKP(bp) = (ptr))
#define SET_PRED(bp, ptr) (PRED_BLKP(bp) = (ptr))

static char *heap_list = 0;
static char *free_root = 0;

/* Function prototypes */
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/* Function prototypes for maintaining free list*/
static void pushFreeList(void *bp);
static void popFreeList(void *bp);

int mm_init(void)
{
    /* Create the initial empty heap. */
    if ((heap_list = mem_sbrk(8 * WSIZE)) == NULL)
        return -1;

    PUT(heap_list, 0);
    PUT(heap_list + (1 * WSIZE), PACK(DSIZE, 1)); // prolofue blk
    PUT(heap_list + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_list + (3 * WSIZE), PACK(0, 1)); // epilogue blk
    free_root = heap_list + 2 * WSIZE;

    if (extend_heap(WSIZE) == NULL)
    {
        return -1;
    }
    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    // Alignment
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    // min block size
    if (size < 16)
    {
        size = 16;
    }

    if ((int)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));         // free block header
    PUT(FTRP(bp), PACK(size, 0));         // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // epilogue

    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;

    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
    { // case1
        pushFreeList(bp);
        return bp;
    }
    else if (prev_alloc && !next_alloc) // case2
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        popFreeList(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) // case3
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        popFreeList(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && !next_alloc) // case4
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        popFreeList(PREV_BLKP(bp));
        popFreeList(NEXT_BLKP(bp));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    pushFreeList(bp);
    return bp;
}

void *mm_malloc(size_t size)
{
    size_t asize; // adjusted size
    size_t extendsize;
    void *bp;

    if (size == 0)
        return (NULL);

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return (bp);
    }

    // cannot find_fit
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return (NULL);
    place(bp, asize);
    return (bp);
}

static void *find_fit(size_t asize)
{
    void *bp;
    static int last_malloced_size = 0;
    static int repeat_counter = 0;
    if (last_malloced_size == (int)asize)
    {
        if (repeat_counter > 60)
        {
            int extendsize = MAX(asize, 4 * WSIZE);
            bp = extend_heap(extendsize / 4);
            return bp;
        }
        else
            repeat_counter++;
    }
    else
        repeat_counter = 0;

    for (bp = free_root; GET_ALLOC(HDRP(bp)) == 0; bp = SUCC_BLKP(bp))
    {
        if (asize <= (size_t)GET_SIZE(HDRP(bp)))
        {
            last_malloced_size = asize;
            return bp;
        }
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= 4 * WSIZE)
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        popFreeList(bp);
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        coalesce(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        popFreeList(bp);
    }
}

static void pushFreeList(void *bp)
{
    SET_SUCC(bp, free_root);
    SET_PRED(free_root, bp);
    SET_PRED(bp, NULL);
    free_root = bp;
}

static void popFreeList(void *bp)
{

    if (PRED_BLKP(bp))
        SET_SUCC(PRED_BLKP(bp), SUCC_BLKP(bp));
    else
        free_root = SUCC_BLKP(bp);
    SET_PRED(SUCC_BLKP(bp), PRED_BLKP(bp));
}

void mm_free(void *bp)
{
    size_t size;
    if (bp == NULL)
        return;
    size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

void *mm_realloc(void *bp, size_t size)
{
    if ((int)size < 0)
        return NULL;
    else if ((int)size == 0)
    {
        mm_free(bp);
        return NULL;
    }
    else if (size > 0)
    {
        size_t oldsize = GET_SIZE(HDRP(bp));
        size_t newsize = size + (2 * WSIZE); // 2 words for header and footer
        /*if newsize가 oldsize보다 작거나 같으면 그냥 그대로 써도 됨. just return bp */
        if (newsize <= oldsize)
        {
            return bp;
        }
        // oldsize 보다 new size가 크면 바꿔야 함.*/
        else
        {
            size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
            size_t csize;

            if (!next_alloc && ((csize = oldsize + GET_SIZE(HDRP(NEXT_BLKP(bp))))) >= newsize)
            {
                popFreeList(NEXT_BLKP(bp));
                PUT(HDRP(bp), PACK(csize, 1));
                PUT(FTRP(bp), PACK(csize, 1));
                return bp;
            }

            else
            {
                void *new_ptr = mm_malloc(newsize);
                place(new_ptr, newsize);
                memcpy(new_ptr, bp, newsize);
                mm_free(bp);
                return new_ptr;
            }
        }
    }
    else
        return NULL;
}
