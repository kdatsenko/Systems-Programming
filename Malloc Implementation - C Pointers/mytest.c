#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "smalloc.h"


#define SIZE 80

/* Test for smalloc and sfree. 
 * Test covers the following scenarios for memory allocation: 
 * - allocation where required size of memory exactly fits free block  
 * - allocation where the first available free block is larger than the required size
 * resulting in a split. 
 * - multiple allocation that results in consumption of all available memory and 
 * further allocation results in error.
 * - freeing blocks of memory that are not beside eachother.
 * - freeing blocks that have a shared edge which results in merge of multiple blocks.
 * - Reuse of freed merged blocks. 
 * - Releasing all allocated memory. 
 */
int main(void) {

    mem_init(SIZE);
    
    char *ptrs[12];
    int i;

    /* Call smalloc 6 times. */
    /* Reference in the comments to individual blocks starts at 1 */

    int num_bytes = 10;
    for(i = 0; i < 6; i++) {
        ptrs[i] = smalloc(num_bytes);
        write_to_mem(num_bytes, ptrs[i], i);
    }

    /* Should diplay 6 blocks of reserved memory, 10 bytes each in allocated_list*/
    printf("List of allocated blocks:\n");
    print_allocated();
    printf("List of free blocks:\n");
    print_free();
    printf("Contents of allocated memory:\n");
    print_mem();
    
    /* Frees the 2nd and 3rd blocks allocated.*/
    printf("freeing %p result = %d\n", ptrs[1], sfree(ptrs[1]));
    printf("freeing %p result = %d\n", ptrs[2], sfree(ptrs[2]));

    /* 2nd and 3rd blocks should be removed from allocated_list */
    printf("List of allocated blocks:\n");
    print_allocated();
    /* freelist now should have ONE NEW free block: 
    the result of freeing the 2nd and 3rd block and merging them 
    to produce a free block of size '20 bytes' */
    printf("List of free blocks:\n");
    print_free();
    printf("Contents of allocated memory:\n");
    print_mem();

    /* Frees the 5th block */
    printf("freeing %p result = %d\n", ptrs[4], sfree(ptrs[4]));
    printf("List of allocated blocks:\n");
    print_allocated();
    /* ONE new free block (5th - 10 bytes) located in sequence AFTER 2nd-3rd blocks (20 bytes), 
    and BEFORE unused block left over (size = 80 - 60 = 20 bytes)*/
    printf("List of free blocks:\n");
    print_free();
    printf("Contents of allocated memory:\n");
    print_mem();

    /* Frees the 4th block */
    printf("freeing %p result = %d\n", ptrs[3], sfree(ptrs[3]));
    printf("List of allocated blocks:\n");
    print_allocated();
    /* 4th block should merge with 2nd-3rd and 5th to produce ONE 40 byte block. */
    printf("List of free blocks:\n");
    print_free();
    printf("Contents of allocated memory:\n");
    print_mem();

    /* Allocate 7th block with size 35 bytes*/

    ptrs[6] = smalloc(35); 
    printf("allocating %p size = %d\n", ptrs[6], 35);
    write_to_mem(35, ptrs[6], 6);
    /* For 7th block, 40 byte block of memory freed previously should be reused.
    Since 40 available bytes > 35 required bytes, 40 block will be SPLIT.
    The result will be 35 byte block in allocated_list, and 5 byte remainder in freelist.*/
    printf("List of allocated blocks:\n");
    print_allocated();
    printf("List of free blocks:\n");
    print_free();
    printf("Contents of allocated memory:\n");
    print_mem();

    /* Allocate 8th block with size 20 bytes*/

    ptrs[7] = smalloc(20); 
    printf("allocating %p size = %d\n", ptrs[7], 20);
    write_to_mem(20, ptrs[7], 7);
    printf("List of allocated blocks:\n");
    print_allocated();
    /* freelist should contain only 5 byte block. */
    printf("List of free blocks:\n");
    print_free();
    printf("Contents of allocated memory:\n");
    print_mem();

    /* Allocate 9th block with size 5 bytes*/

    ptrs[8] = smalloc(5);
    printf("allocating %p size = %d\n", ptrs[8], 5);
    write_to_mem(5, ptrs[8], 8);
    printf("List of allocated blocks:\n");
    print_allocated();
    /* freelist should be empty. */
    printf("List of free blocks:\n");
    print_free();
    printf("Contents of allocated memory:\n");
    print_mem();

    /* Memory now full*/

    /* Allocate 10th block with size 30 bytes*/
    ptrs[9] = smalloc(30);
    printf("allocating for ptrs[9] size = %d. Expected: NULL. Result: %p\n", 30, ptrs[9]);
    /* Allocate 11th block with size 1 byte*/
    ptrs[10] = smalloc(1);
    printf("allocating for ptrs[10] size = %d. Expected: NULL. Result: %p\n", 1, ptrs[10]);
    /* Unchanged */
    printf("List of allocated blocks:\n");
    print_allocated();
    printf("List of free blocks:\n");
    print_free();
    printf("Contents of allocated memory:\n");
    print_mem();

    /* Free all. Expected: no crash */
    for (i = 0; i < 11; i++){
        printf("freeing ptrs[%d] %p, result = %d\n", i, ptrs[i], sfree(ptrs[i]));
    }

    /* Restart: 1 free block of 80 total SIZE bytes*/
    printf("List of allocated blocks:\n");
    print_allocated();
    printf("List of free blocks:\n");
    print_free();
    printf("Contents of allocated memory:\n");
    print_mem();

    /* Clean all */
    printf("Cleaning memory\n");
    mem_clean();

    /* Each print-out should be blank.*/
    printf("List of allocated blocks:\n");
    print_allocated();
    printf("List of free blocks:\n");
    print_free();
    printf("Contents of allocated memory:\n");
    print_mem();
    return 0;
}