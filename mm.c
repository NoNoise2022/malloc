/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/*Basic constants and macros*/
#define WSIZE 4             /*Word and header/footer size (bytes)*/
#define DSIZE 8             /*Double word size (bytes)*/
#define CHUNKSIZE (1<<12)   /*Extend heap by this amount (bytes)*/

#define MAX(x, y) ((x) > (y)? (x) : (y))          /*큰 값이 앞으로 와서 짝을 지어라?*/

/* Pack a size and allocated bit into a word */     /*뭔 지 모르겠음*/
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p*/
#define GET(p)          (*(unsigned int *)(p))
#define PUT(p, val)     (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p*/
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)     (GET(p) & 0x1)    

/* Given block ptr bp, compute address of its header and footer*/
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Giveb block ptr bp, compute address of next and previous blocks*/
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp)-DSIZE)))
static char *heap_listp;
static char *recently_allocated;

static void *coalesced(void *bp)
{
    //
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc)
    {
        return bp;
    }
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));

        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else
    {
        //
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));        
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;

}
static void *extend_heap(size_t words)
{
    //
    char *bp;
    size_t size;
    //
    size = (words%2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

    return coalesced(bp);
}

//


static void *find_fit(size_t asize){
    void *bp;
    if (recently_allocated == NULL) {
        recently_allocated = heap_listp;
    }
    
    for (bp = recently_allocated; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
         if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            recently_allocated = bp;
            return bp;
            }
    }
    
    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)){  
        /* 분할 후에 이 블록의 나머지가 최소 블록 크기(16 bytes)와 같거나 크다면 */
        /* asize 할당 후 남은 공간 분할 후 빈 공간으로 둠 */
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else
    {
        /*그냥 할당*/
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE,1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE,1));    
    PUT(heap_listp + (3*WSIZE), PACK(0,1));

    heap_listp += (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE)==NULL)
        return -1;
    return 0;
}

//

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;       /* Adjusted block size*/
    size_t extendsize; /* Amount to extend heap if no fit */    /*공간이 모자라면 추가적으로 확장할 공간 크기*/
    char *bp;

    /* Ignore spurious requests : 안되는 요청 거절*/
    if (size == 0)
    {
        return NULL;
    }

    /* Adjust block size to include overhead and alignment reqs. */
    
    if (size <= DSIZE) asize = 2*DSIZE;
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);    /*size > DSIZE*/
    // asize => padding을 포함해 조정된 payload의 크기

    /* Search the free list for a fit */
    if((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memry and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
    {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 
void mm_free(void *ptr)
{
}
*/

// 블록을 반환하고 경계 태그 연결 사용 -> 상수 시간 안에 인접한 가용 블록들과 통합하는 함수
void mm_free(void *bp){ 
    // 어느 시점에 있는 bp를 인자로 받는다.
    size_t size = GET_SIZE(HDRP(bp)); // 얼만큼 free를 해야 하는지.
    PUT(HDRP(bp),PACK(size,0)); // header, footer 들을 free 시킨다. 안에 들어있는걸 지우는게 아니라 가용상태로 만들어버린다.
    PUT(FTRP(bp), PACK(size,0)); 
    coalesced(bp);
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
    if (newptr == NULL) {
        return NULL;
    }
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
    {
        copySize = size;
    }
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/*
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
*/