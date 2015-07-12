#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "smalloc.h"

void *mem;
struct block *freelist;
struct block *allocated_list;

int merge(struct block *preceeding);

void *smalloc(unsigned int nbytes) {
	struct block *current = freelist;
	//pointer to previous in case we find available block
	struct block *previous = NULL;

	//No more free memory left
	if ((freelist == NULL) | (nbytes == 0))
		return NULL;

	while (current->next != NULL && current->size < nbytes){
		previous = current;
		current = current->next;
	}
	/*If memory cannot be reserved (there is no block large enough to 
	hold size nbytes), returns null.*/
	if (current->size < nbytes){
    	return NULL;
	} 
	/* The case where we found a block of exactly the required size */
	else if (current->size == nbytes){
		//Remove block from freelist
		if (previous == NULL){ //We are at the head of the freelist
			freelist = current->next;
		} else{
			previous->next = current->next;
		}
		//Insert block into allocated_list
		current->next = allocated_list;
		allocated_list = current; //stack
		return current->addr; //want address of memory in mem_region, not the struct itself

	} else{ /* The case where we found a block that is larger than the required size*/
		//Split up the free block.
		//New block to maintain pointer to reserved memory
		struct block *reserved_block = malloc(sizeof(struct block));
		if (reserved_block == NULL){ //check for failure of malloc
    		fprintf(stderr, "Out of memory\n");
        	exit(1);
    	}
		reserved_block->addr = current->addr;
		reserved_block->size = nbytes;
		reserved_block->next = allocated_list;
		allocated_list = reserved_block;
		//Modify block remaining in freelist to have size and address corresponding
		//to memory left over.
		current->addr = ((char*)(current->addr)) + nbytes;
		current->size -= nbytes;
		return reserved_block->addr;
	}
}

/* Return memory allocated by smalloc to the list
 * of free blocks so that it might be reused later. 
 * Arguments:
 * - addr Address of the reserved block in mem_region
 * If sfree cannot find the block, reports error.
 * Returns -1 on error, and a 0 on success.
 */
int sfree(void *addr) {
	//need to merge free blocks whenever possible.
	struct block *freed = allocated_list;
	struct block *previous = NULL;
	if (allocated_list == NULL) //allocated_list is empty
		return -1;

	//remove reserved block from allocated_list
	while (freed->next != NULL && freed->addr != addr){
		previous = freed;
		freed = freed->next;
	}
	if (freed->addr != addr) //could not find allocated memory at addr.
		return -1;

	if (previous == NULL){ //We are at the head of the allocated_list
		allocated_list = freed->next;
	} else {
		previous->next = freed->next;
	}
	freed->next = NULL; //remove link to block in allocated_list

	//insert free block into freelist.
	//search for appropriate free spot in free list for insertion & possibly merging
	struct block *pre_block = NULL;
	struct block *post_block = freelist;

	if (freelist == NULL){ //freelist is empty
		freelist = freed;
		return 0;
	} 

	while (post_block->next != NULL && freed->addr > post_block->addr){
		pre_block = post_block;
		post_block = post_block->next;
	}

	if (freed->addr > post_block->addr){ //reached end of freelist
		post_block->next = freed;
		merge(post_block);
		return 0;
	} 
	else if (pre_block == NULL){ //beginning of freelist
		freed->next = post_block;
		freelist = freed;
		merge(freed);
		return 0;
	} 
	else if (pre_block->addr < freed->addr && freed->addr < post_block->addr) { // middle of freelist
		pre_block->next = freed;
		freed->next = post_block;
		if (merge(pre_block) == 0){ //pre_block was merged with freed
			merge(pre_block); //try to merge pre_block now with post_block
		} else {
			merge(freed); //since pre_block and freed could not be merged, 
						 //try merging freed with post_block.
		}
		return 0;
	} 
	else {
		return -1;
	}
    
}

/* Checks if two blocks of memory are located right beside eachother,
 * and if so, merges the two blocks into one for sake of efficiency
 * when allocating freed memory in the future.
 * Arguments:
 * - preceeding: a pointer to the block of memory we wish to merge 
 *         (if possible) with the next block in the freelist.
 * Returns 0 if the blocks can be merged, and -1 if they cannot.
 */
int merge(struct block *preceeding){
	struct block *following = preceeding->next;
	if (following == NULL)
		return -1;

	/*The address of memory located 'size' bytes from address of memory segment
	stored in preceeding block (the true following address point) */
	char *following_addr = ((char*)(preceeding->addr)) + preceeding->size;
	if (following_addr != ((char*)(following->addr))){ 
		//there exists a block of allocated memory at the following address
		return -1;
	} 
	//merge following block with preceeding block
	preceeding->size += following->size; 
	preceeding->next = following->next; 
	free(following); //don't need this block anymore
	return 0;
}


/* Initialize the memory space used by smalloc,
 * freelist, and allocated_list
 * Note:  mmap is a system call that has a wide variety of uses.  In our
 * case we are using it to allocate a large region of memory. 
 * - mmap returns a pointer to the allocated memory
 * Arguments:
 * - NULL: a suggestion for where to place the memory. We will let the 
 *         system decide where to place the memory.
 * - PROT_READ | PROT_WRITE: we will use the memory for both reading
 *         and writing.
 * - MAP_PRIVATE | MAP_ANON: the memory is just for this process, and 
 *         is not associated with a file.
 * - -1: because this memory is not associated with a file, the file 
 *         descriptor argument is set to -1
 * - 0: only used if the address space is associated with a file.
 */
void mem_init(int size) {
    mem = mmap(NULL, size,  PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if(mem == MAP_FAILED) {
         perror("mmap");
         exit(1);
    }
    //initialize data structures, use malloc
    allocated_list = NULL;
    //initializes first block of freelist
    freelist = malloc (sizeof(struct block));
    if (freelist == NULL){ //check for failure of malloc
    	fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    freelist->addr = mem;
    freelist->size = size;
    freelist->next = NULL;   
}

void mem_clean(){
	//free all the memory for blocks in allocated_list
	struct block *temp;
	while(allocated_list != NULL){
		temp = allocated_list->next;
		free(allocated_list);
		allocated_list = temp;
	}
	//free all the memory for blocks in free_list
	while(freelist != NULL){
		temp = freelist->next;
		free(freelist);
		freelist = temp;
	}
	//No need to worry about mem memory. "The mem region is 
	//automatically unmapped when the process is terminated."
}

