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

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size)|(alloc))

#define GET(p) (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp))-DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE( ((char *)(bp) - WSIZE) ))
#define PREV_BLKP(bp) ((char *)(bp) + GET_SIZE( ((char *)(bp) - DSIZE) ))

//
static char *heap_listp;  // 처음에 쓸 큰 가용블록 힙을 만들어줌.
//

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


static void *find_fit(size_t asize){ // first fit 검색을 수행
    void *bp;
    for (bp= heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){ // init에서 쓴 heap_listp를 쓴다. 처음 출발하고 그 다음이 regular block 첫번째 헤더 뒤 위치네.
        // for문이 계속 돌면 epilogue header까기 간다. epilogue header는 0이니까 종료가 된다.
        if (!GET_ALLOC(HDRP(bp)) && (asize<=GET_SIZE(HDRP(bp)))){ // 이 블록이 가용하고(not 할당) 내가 갖고있는 asize를 담을 수 있으면
            return bp; // 내가 넣을 수 있는 블록만 찾는거니까, 그게 나오면 바로 리턴.
        }
    }
    return NULL;  // 종료 되면 null 리턴. no fit 상태.
}
static void place(void *bp, size_t asize){ // 들어갈 위치를 포인터로 받는다.(find fit에서 찾는 bp) 크기는 asize로 받음.
    // 요청한 블록을 가용 블록의 시작 부분에 배치, 나머지 부분의 크기가 최소 블록크기와 같거나 큰 경우에만 분할하는 함수.
    size_t csize = GET_SIZE(HDRP(bp)); // 현재 있는 블록의 사이즈.
    if ( (csize-asize) >= (2*DSIZE)){ // 현재 블록 사이즈안에서 asize를 넣어도 2*DSIZE(헤더와 푸터를 감안한 최소 사이즈)만큼 남냐? 남으면 다른 data를 넣을 수 있으니까.
        PUT(HDRP(bp), PACK(asize,1)); // 헤더위치에 asize만큼 넣고 1(alloc)로 상태변환. 원래 헤더 사이즈에서 지금 넣으려고 하는 사이즈(asize)로 갱신.(자르는 효과)
        PUT(FTRP(bp), PACK(asize,1)); //푸터 위치도 변경.
        bp = NEXT_BLKP(bp); // regular block만큼 하나 이동해서 bp 위치 갱신.
        PUT(HDRP(bp), PACK(csize-asize,0)); // 나머지 블록은(csize-asize) 다 가용하다(0)하다라는걸 다음 헤더에 표시.
        PUT(FTRP(bp), PACK(csize-asize,0)); // 푸터에도 표시.
    }
    else{
        PUT(HDRP(bp), PACK(csize,1)); // 위의 조건이 아니면 asize만 csize에 들어갈 수 있으니까 내가 다 먹는다.
        PUT(FTRP(bp), PACK(csize,1));
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
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
	// return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }

    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE*((size+(DSIZE)+(DSIZE-1))/DSIZE);

    if((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
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

void *mm_realloc(void *bp, size_t size){
    if(size <= 0){ 
        mm_free(bp);
        return 0;
    }
    if(bp == NULL){
        return mm_malloc(size); 
    }
    void *newp = mm_malloc(size); 
    if(newp == NULL){
        return 0;
    }
    size_t oldsize = GET_SIZE(HDRP(bp));
    if(size < oldsize){
    	oldsize = size; 
	}
    memcpy(newp, bp, oldsize); 
    mm_free(bp);
    return newp;
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