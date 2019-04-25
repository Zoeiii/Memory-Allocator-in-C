/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h> //to set ENOMEM when error
#include "debug.h"
#include "sfmm.h"


sf_prologue *my_prologue = NULL;
sf_epilogue *my_epilogue = NULL;
sf_block *main_freelist = NULL;

//footer mismatch,
//Either the block size is wrong, or the footer was updated in the wrong place.

//set a new footer;
void set_footer(void *addr, sf_header *new_block_header){
    sf_header *block_footer = addr; //set the footer to specifit address
    *block_footer = *new_block_header;
}
int find_block_size(sf_header *header){
    return (long unsigned)header->block_size & BLOCK_SIZE_MASK;
}
void *footer_addr(sf_header *header){
    sf_block *footer;
    int block_size =find_block_size(header);
    footer = (void*)header +block_size-8;

    return footer;
}

int prev_allocated(sf_header *header){
    return ((header->block_size) & PREV_BLOCK_ALLOCATED)>>1;
}

int allocated(void *addr){
    sf_header *header = addr;
    return (header->block_size) & THIS_BLOCK_ALLOCATED;
}

void *pop_block(sf_block *currentFree){
    //pop it
    currentFree->body.links.prev->body.links.next = currentFree->body.links.next;
    currentFree->body.links.next->body.links.prev = currentFree->body.links.prev;
    return currentFree;
}

void push_block(sf_block *block){
    //push back to head
    //push to where prev or next
    sf_block *currentFree = sf_free_list_head.body.links.next->body.links.prev;

    currentFree->body.links.next->body.links.prev = block;
    currentFree->body.links.next->body.links.prev->body.links.prev
                                                    = currentFree;
    currentFree->body.links.next->body.links.prev->body.links.next
                                    = currentFree->body.links.next;
    currentFree->body.links.next = block;
}

void *prev_block(sf_block *currentBlock){//only call this when the block beofre ep is free
    sf_block *block;
    sf_header *footer;
    int block_size=0;
    footer = (void*)currentBlock - 8;
    block_size = (long unsigned)footer->block_size & BLOCK_SIZE_MASK;
    block = (void*)footer - block_size +8; //that footer is 8 byte big
    return block;
}
void *find_next_block(sf_block *currentBlock){
    sf_block *next_block;
    next_block = (void*)currentBlock + find_block_size(&currentBlock->header);

    return next_block;
}

int calculate_blocksize(int block_size){
    block_size = block_size + 8; //size+header size
    if(block_size%16 != 0){
        block_size = ((block_size/16) +1) *16;
    }
    if(block_size < 32){ //extra space for next and prev when freed
        block_size = 32;
    }
    return block_size;
}
void set_next_block_pa_to_zero(sf_block * block){

    if((void*)block == (void*)my_epilogue){ //if next is net ep
        block->header.block_size = (long unsigned)block->header.block_size & ~PREV_BLOCK_ALLOCATED;
    }
    else{
        block->header.block_size = (long unsigned)block->header.block_size & ~PREV_BLOCK_ALLOCATED;
        set_footer(footer_addr(&block->header), &block->header);
    }
}

void *coalesce(sf_block *currentBlock){

    int block_size=0, set_next_flag =0, push_flag=1,
    currentBlock_size = (long unsigned)currentBlock->header.block_size & BLOCK_SIZE_MASK;
    sf_block *my_prev_block, *next_block = find_next_block(currentBlock);
    //when prev is free
    while(prev_allocated(&currentBlock->header) == 0){
        push_flag=0;
        my_prev_block = prev_block(currentBlock);
        if(prev_allocated(&my_prev_block->header)==1)
            my_prev_block->header.block_size = (my_prev_block->header.block_size +currentBlock_size)|PREV_BLOCK_ALLOCATED;
        else
            my_prev_block->header.block_size = my_prev_block->header.block_size +currentBlock_size;
        set_footer(footer_addr(&my_prev_block->header), &my_prev_block->header);
        set_next_block_pa_to_zero(next_block);
        currentBlock = (void*)my_prev_block;
    }
    //if next is a free block and not epil, coalesce
    next_block = find_next_block(currentBlock);
    while(allocated(next_block)==0 && (void*)next_block != (void*)my_epilogue){
       if((void*)next_block != (void*)my_epilogue){ //next is not ep, pop it from free list
            next_block = pop_block(next_block);
            set_next_flag = 1;
        }
        block_size = find_block_size(&next_block->header);
        currentBlock_size = find_block_size(&currentBlock->header);
        if(prev_allocated(&currentBlock->header)==1)
            currentBlock->header.block_size = (currentBlock_size + block_size)|PREV_BLOCK_ALLOCATED;
        //that we might update the currentBlock if the one before is free, so the size need update
        else
            currentBlock->header.block_size = currentBlock_size + block_size;
        set_footer(footer_addr(&currentBlock->header), &currentBlock->header);
        if(set_next_flag!=1){
            next_block = find_next_block(currentBlock);
            set_next_block_pa_to_zero(next_block);
        }
        next_block = find_next_block(currentBlock);
    }
    if(prev_allocated(&currentBlock->header) == 1 && push_flag ==1 && allocated(next_block)==1){
        push_block(currentBlock);
        set_next_block_pa_to_zero(next_block);
    }
    return currentBlock;

}

void *requested_grow(){
    sf_block *last_block;
    int free_block_size=0;
    sf_epilogue epilogue = *my_epilogue;

    if(sf_mem_grow()){
        if(prev_allocated(&my_epilogue->header)==0){ //prev is free, coalesce
            last_block = (void*)my_epilogue;
            free_block_size = (sf_mem_end()-8)-(void*)last_block;
            last_block->header.block_size = free_block_size;
            set_footer(sf_mem_end()-16, &last_block->header);
            my_epilogue = sf_mem_end()-8; //modify the addr of epilogue
            my_epilogue->header = epilogue.header;
            last_block = coalesce(last_block);
        }
        else{ //prev allocated, add new block
            last_block = (void*)my_epilogue;
            free_block_size = (sf_mem_end()-8)-(void*)last_block;

            last_block->header.block_size = free_block_size | PREV_BLOCK_ALLOCATED;
            set_footer(sf_mem_end()-16, &last_block->header);
            my_epilogue = sf_mem_end()-8; //modify the addr of epilogue
            my_epilogue->header = epilogue.header;
            my_epilogue->header.block_size = my_epilogue->header.block_size & ~PREV_BLOCK_ALLOCATED;
            push_block(last_block);
         }

        return last_block;
    }
    else
        return NULL;
}
int mem_init(){
    int free_block_size;
    sf_free_list_head.body.links.next = sf_mem_start()+sizeof(sf_prologue);
    sf_free_list_head.body.links.prev = sf_free_list_head.body.links.next;
            //set up epi and prologue
    my_prologue = sf_mem_start();
    my_prologue->block.header.block_size = 33;
    my_prologue->footer.block_size = 33;
    my_epilogue = sf_mem_end()-8;
    my_epilogue->header.block_size = my_epilogue->header.block_size|THIS_BLOCK_ALLOCATED;

    free_block_size = (sf_mem_end()-8)-(sf_mem_start()+sizeof(sf_prologue));
    sf_free_list_head.body.links.next->header.block_size = free_block_size | PREV_BLOCK_ALLOCATED;
    //sf_free_list_head.body.links.next->header.requested_size = size;
    set_footer(sf_mem_end()-16, &(sf_free_list_head.body.links.next->header));

    //link the main_free list to head, and set the first block
    sf_free_list_head.body.links.next->body.links.next = &sf_free_list_head;
    sf_free_list_head.body.links.next->body.links.prev = &sf_free_list_head;
    return free_block_size;
}

int valid_ptr(sf_block *block){
    if(block == NULL)
        return 0;
    //if header_bit =0, error
    int header_bit = (long unsigned)(block->header.block_size) & BLOCK_SIZE_MASK ;
    if(header_bit==0){
    //    printf("allocated bit = 0\n");
        return 0;
    }
    //check if pp is a valid ptr, if not, error
    //The header of the block is before the end of the prologue
    //or after the beginning of the epilogue
    void *prologue_end_addr = (void*)my_prologue+32+8;
    //printf("prologue_end_addr: %p\n", prologue_end_addr);
    if((void*)block < prologue_end_addr && (void*)block > (void*)my_prologue){
        //printf("wrong addr\n");
        return 0;
    }
//    printf("end of prologue %p, end of epilogue %p\n", prologue_end_addr, (void *)my_epilogue);
    int block_size = block->header.block_size & BLOCK_SIZE_MASK;
    if(block_size < 32 || block_size%16 != 0){
        // printf("The block_size field is not a multiple of 16\n");
        // printf("or is less than the minimum block size of 32 bytes.\n");
        return 0;
    }

    if(block->header.requested_size+8 >block_size){
    //   printf("requested_size + header_size > block_size\n");
        return 0;
    }
    //If the prev_alloc field is 0, indicating that the previous block is free,
    //then the alloc fields of the previous block header and footer should also be 0.
    if(prev_allocated(&block->header)==0 &&  allocated(prev_block(block))!=0 ){
    //   printf("prev block a = %d\n", allocated(prev_block(block)));
        return 0;
    }
    return 1;
}
void *split(sf_block * split_block, int after_split_size, int size){
    sf_block * allocated_block, *next_block;//, *p_block;
    int block_size = calculate_blocksize(size);
    void* footer;

    allocated_block = split_block;
    allocated_block->header.block_size = block_size | PREV_BLOCK_ALLOCATED;
    allocated_block->header.block_size = allocated_block->header.block_size | THIS_BLOCK_ALLOCATED;
    allocated_block->header.requested_size = size;

    split_block = footer_addr(&allocated_block->header)+8;
    split_block->header.block_size = after_split_size | PREV_BLOCK_ALLOCATED;
    footer = footer_addr(&split_block->header);
    set_footer(footer, &(split_block->header));
        //set next block's pa to 1
    next_block = find_next_block(split_block);
    next_block->header.block_size = next_block->header.block_size & ~PREV_BLOCK_ALLOCATED;
    if((void*)next_block != (void*)my_epilogue)
        set_footer(footer_addr(&next_block->header), &next_block->header);
    //push back to head
    push_block(split_block);

    return (void*)allocated_block;
}
void *sf_malloc(size_t size) {
    sf_block *allocated_block,*poped_block, *currentFree;
    int free_block_size, blockFlag=0;

    if(size == 0)
        return NULL;
    if(sf_mem_start()==sf_mem_end()){//not init
        //increase the memory for allocating block
        if(sf_mem_grow()){
            //initlized the free block
            free_block_size = mem_init();
        }
        else{
            sf_errno = ENOMEM;
            return NULL;
        }
    }

    int block_size = calculate_blocksize(size);
    //check quick list if have the exact size
    //quick lists are indexed by size/16
    int ql_index = (block_size-32) / 16;
    sf_block *currentBlock;
    //ql is LIFO
    if((ql_index < NUM_QUICK_LISTS) && (sf_quick_lists[ql_index].first != NULL)){
        int length = sf_quick_lists[ql_index].length;
        currentBlock = sf_quick_lists[ql_index].first;
        if(length!=0){
           sf_quick_lists[ql_index].first = sf_quick_lists[ql_index].first->body.links.next;
        }
        sf_quick_lists[ql_index].length = length-1;
        currentBlock->header.requested_size= size;
        set_footer(footer_addr(&currentBlock->header), &currentBlock->header);
        return (void*)currentBlock+8;
    }
    else{ //go to main free list
        currentFree = sf_free_list_head.body.links.next;
        free_block_size = currentFree->header.block_size & BLOCK_SIZE_MASK;
        //pop out the node then split them, and add back to mfreelist
        do{
            if(free_block_size > block_size){
                blockFlag = 1; //set flag, found the block
                poped_block = pop_block(currentFree);
                break;
            }
            else{//next
                currentFree = currentFree->body.links.next;
            }
        }while(&sf_free_list_head != currentFree);

        if(blockFlag==0){ //if no block with big enough size, call sf_grow
            //currentFree show be the one before epil
            currentFree = prev_block((void*)my_epilogue);
            free_block_size = (unsigned long)currentFree->header.block_size & BLOCK_SIZE_MASK;
            //keep checking if the size after grow is bigger enough
            while(free_block_size<=block_size){
                currentFree = requested_grow();
                if(currentFree==NULL) { //error no more memory to grow
                    sf_errno = ENOMEM;
                    return NULL;
                }
                free_block_size = (unsigned long)currentFree->header.block_size & BLOCK_SIZE_MASK;
            }
        }
        poped_block =pop_block(currentFree);
        //the size of the current free block
        if(prev_allocated(&currentFree->header)==1)
                free_block_size = currentFree->header.block_size -2;
        else
            free_block_size = currentFree->header.block_size;

        //check size after split, if >32 split the node, else return
        int after_split_size = free_block_size - block_size;
        if(after_split_size > 32){ //split
            allocated_block = split(poped_block, after_split_size, size);
            return (void*)allocated_block+8; //return addr of payload

        }
        else{ //allocated the poped_block without split it
            poped_block->header.block_size = poped_block->header.block_size | THIS_BLOCK_ALLOCATED;
            poped_block->header.requested_size = size;
            return (void*)poped_block+8;
        }
    }
    return NULL;
}
void flush(int ql_index){
    int length = sf_quick_lists[ql_index].length;
    sf_block * currentBlock;
    while(sf_quick_lists[ql_index].length>0){
        currentBlock =sf_quick_lists[ql_index].first;
        if(length!=0){
            sf_quick_lists[ql_index].first = sf_quick_lists[ql_index].first->body.links.next;
        }
        sf_quick_lists[ql_index].length = sf_quick_lists[ql_index].length-1;
        currentBlock->header.block_size = currentBlock->header.block_size & ~THIS_BLOCK_ALLOCATED;
        set_footer(footer_addr(&currentBlock->header), &currentBlock->header);
        coalesce(currentBlock);
    }
}
void insert_quick_list(sf_block *block){
    int ql_index = (find_block_size(&block->header)-32) / 16,
        length = sf_quick_lists[ql_index].length;
        //if not full add to the end of the quick list
        if(length< QUICK_LIST_MAX){
            block->body.links.next = sf_quick_lists[ql_index].first;
            sf_quick_lists[ql_index].first = block;

            sf_quick_lists[ql_index].length = sf_quick_lists[ql_index].length + 1;
        }
        else{//if quick list is full, flush all to main, then add to empty ql
            //printf("call flush?\n");
            flush(ql_index);
            block->body.links.next = sf_quick_lists[ql_index].first;
            sf_quick_lists[ql_index].first = block;

            sf_quick_lists[ql_index].length = sf_quick_lists[ql_index].length + 1;
        }
}

void sf_free(void *pp) {
    pp = pp - 8; //addr of the heade
    if(valid_ptr(pp)==0)
        abort();
    sf_block *block = (sf_block*)pp;

    //check quick list, if have the exact block size
    int ql_index = (find_block_size(&block->header)-32) / 16;
    //printf("block size is: %d ql_index is: %d\n",find_block_size(&block->header), ql_index);
    if(ql_index < NUM_QUICK_LISTS-1){

        insert_quick_list(block);
    }
    else{
        //if no exact block add to free list, cloalese
        block->header.block_size = block->header.block_size & ~THIS_BLOCK_ALLOCATED;
        block->header.requested_size = 0;
        set_footer(footer_addr(&block->header), &block->header);
        coalesce(block);
    }
    return;
}

void *sf_realloc(void *pp, size_t rsize) {
    if((int)rsize==0){
        sf_free(pp);
        return pp;
    }
    if(valid_ptr(pp-8)==0)
        abort();
    sf_block *block = (sf_block*)(pp-8);
    int block_size = find_block_size(&block->header);
    //printf("block_size %d, rsize: %d\n", block_size, (int)rsize);
    //realloc to a bigger block
    if((int)rsize > block_size){
        void* addr = sf_malloc(rsize);
        sf_free(pp);
        memcpy(addr, pp, rsize);
        return addr;
    }
    else if((int)rsize == block_size){ //requested for the same size
        return pp;
    }
    else{ //request for a smaller size
    //printf("smaller\n");
        int after_split_size = block_size- calculate_blocksize(rsize);
        if(after_split_size < 32){//result in a spliner, dont split
        //    printf("smaller case 1\n");
            //addject the rs of the original block, return it
            block->header.requested_size = rsize;
            set_footer(footer_addr(&block->header), &block->header);
            return pp;
        }
        else{//split
        //    printf("smaller case 2\n");
            sf_block *poped_block = pop_block(block);
            block = split(poped_block, after_split_size, rsize);
            coalesce(find_next_block(block));
            return pp;
        }

    }

    return NULL;
}

