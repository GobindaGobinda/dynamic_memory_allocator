/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */


/*

	Developed by: Gobinda Senchury
	October 13 2017

	This program is a dynamic memory allocator, with the implementation of
	function  free as sf_free, malloc as sf_malloc and realloc as sf_realloc.
	The pointer of the retun values of this file is off by 8. To address the
	header and footer alignment.

*/



#include "sfmm.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>


#define DSIZE 8  //  double word
#define OVERHEAD 16
#define MINBLOCK 32

#define NOTALLOCATED 0
#define ALLOCATED 1
#define PADDING 1
#define NOPADDING 0


#define NEXT_BLOCK_POINTER(bp) ((char*)bp + DSIZE);
#define PREV_BLOCK_POINTER(bp) ((char*)bp - DSIZE);


 int sbrkCalled = 0;

void *find_first_fit(size_t allocatedSize);

void* findInList(int i, size_t allocatedSize);


void  place(void *allocatedAreaPointer, size_t allocatedSize, size_t size);


void setFooter(sf_free_header* tempHeader, size_t allocatedSize, int padding, size_t requestedSize, int allocated );

void setHeader(sf_free_header* tempHeader, size_t allocatedSize, int padding, int allocated );


void addtoSeglist(sf_free_header* freeHeader, size_t csize);
void addNodeToList(sf_free_header* freeHeader, int headOftheList);


void removeFromSeglist(sf_free_header* freeHeader, size_t csize);
void removeNodeFromList(sf_free_header* freeHeader, int headOftheList);


void extendPage();

void* coalesce(sf_free_header* freeHeader);
void* coalescePrevious(sf_free_header* freeHeader);


int checkFooterValueMakeSense(sf_free_header* tempHeader);
int checkHeaderFooterConsistency(sf_free_header* tempHeader);
int isValueBelowHeapStartAboveHeapEnd(sf_free_header* tempHeader);


sf_footer *getFooter(sf_free_header* tempHeader);
void *getNextHeader(sf_free_header* tempHeader);


void pointerValidation(void* ptr);



/**
 * You should store the heads of your free lists in these variables.
 * Doing so will make it accessible via the extern statement in sfmm.h
 * which will allow you to pass the address to sf_snapshot in a different file.
 */
free_list seg_free_list[4] = {
    {NULL, LIST_1_MIN, LIST_1_MAX},
    {NULL, LIST_2_MIN, LIST_2_MAX},
    {NULL, LIST_3_MIN, LIST_3_MAX},
    {NULL, LIST_4_MIN, LIST_4_MAX}
};


int sf_errno = 0;

void *sf_malloc(size_t size) {

   size_t allocatedSize = size;
   char *allocatedBlockPointer;

    // validating if the given input size is valid or not
   if( size == PAGE_SZ * 4){
   		sf_errno = ENOMEM;
   		return NULL;
   }

    if(size <= 0 || (size > PAGE_SZ * 4)){

       sf_errno = EINVAL ;
       return NULL;
    }



    // adjust the  size to include the overhead and aligh requirements
    if (size <= (2* DSIZE))
    {
    	allocatedSize = 2*DSIZE + OVERHEAD ;
    }
    else{




    	while(1){
    		if ((allocatedSize % (2*DSIZE)) == 0)
    		{
    			break;
    		}

    		allocatedSize++;
    	}

    	allocatedSize = allocatedSize +OVERHEAD;


    };

    // search the lists of free list for the free list and place if found

    if((allocatedBlockPointer = find_first_fit(allocatedSize)) != NULL){

   		place(allocatedBlockPointer, allocatedSize, size);

    	return ((char*)allocatedBlockPointer + 8);
    }





    while(sf_errno != ENOMEM){

     	/* if there is no fit then we sbrk and then increase the size of heap */
		extendPage();

    	 // search the lists of free list for the free list and place if found

    	if((allocatedBlockPointer = find_first_fit(allocatedSize)) != NULL){

   			place(allocatedBlockPointer, allocatedSize, size);
    		return ((char*)allocatedBlockPointer + 8);
    	}


    }


	return NULL;
}



void *sf_realloc(void* oldPointer, size_t newSize) {

	size_t oldSize;
	size_t allocatedSize = newSize ;
	char* newPointer = NULL;


	pointerValidation(oldPointer);


	// this is to take care of the offset while using from the main
	oldPointer = (char*)oldPointer-8;


	if(newSize == 0){

		sf_free((sf_free_header*)((char*)oldPointer + 8));

		return NULL;
	}


	oldSize = (((sf_free_header*) oldPointer)->header).block_size << 4;


	// adjust the  size to include the overhead and aligh requirements
    if (newSize <= 2* DSIZE)
    {
    	allocatedSize = 2*DSIZE + OVERHEAD ;
    }
    else{

    	while(1){
    		if (((int)allocatedSize % (2*DSIZE)) == 0)
    		{
    			break;
    		}

    		allocatedSize++;
    	}

    	allocatedSize = allocatedSize +OVERHEAD;
    };

	// if the new size is less than the old size
	if ( allocatedSize <= oldSize)
	{
			// trncate the back of the thing

		int padding = 0;
		int remainingSize = (int)oldSize-(int)allocatedSize;


		if(!(remainingSize < MINBLOCK)){


			/* next memory row  adding the allocated size and pointing next location of footer of allocated block*/
			char* nextAddr  = (char*)oldPointer + allocatedSize;


			if (newSize != allocatedSize)
			{
				padding = 1;
			}


			// the new actual block
			setHeader((sf_free_header*)oldPointer,allocatedSize,padding,ALLOCATED);
			setFooter((sf_free_header*)oldPointer,allocatedSize,padding,newSize, ALLOCATED);


			addtoSeglist((sf_free_header*)nextAddr,remainingSize);

			// the New free block


			setHeader((sf_free_header*)nextAddr,remainingSize,NOPADDING,NOTALLOCATED);
			setFooter((sf_free_header*)nextAddr,remainingSize,NOPADDING,remainingSize, NOTALLOCATED);

			coalesce((sf_free_header* )nextAddr);

		}

		return ((char*)oldPointer + 8);
	}
	else{


		newPointer = sf_malloc(newSize);
		if(newPointer == NULL) return NULL;

	}


	if(newPointer  != NULL){

		memcpy(((char*)newPointer), ((char*)oldPointer + 8), (getFooter((sf_free_header*)((char*)oldPointer + 8)))->requested_size);


		sf_free((char*)oldPointer+8);


	}


	return ((char*)newPointer);
}



void sf_free(void *ptr) {


	pointerValidation(ptr);

	//to make ajustment of the  header
	ptr = (char*)ptr - 8;

	sf_free_header* tempHeader = (sf_free_header*)(ptr);


	(tempHeader->header).allocated = NOTALLOCATED;
	(tempHeader->header).padded = NOPADDING;

	getFooter(tempHeader)->allocated = NOTALLOCATED;
	getFooter(tempHeader)->padded = NOPADDING;

	addtoSeglist(tempHeader,((tempHeader->header).block_size << 4));

	coalesce(tempHeader);


	return;
}


/* calls the sbrk and gets 1 page more data for the heap */


void extendPage(){


	if (sbrkCalled < 4)
		{

			/* increase the page by 1 */
			sf_sbrk();

			sbrkCalled++;


			char* heapEnd  = get_heap_end() - 8;
			char* heapStart = heapEnd  + 8 - PAGE_SZ;


    		addtoSeglist((sf_free_header*)heapStart, PAGE_SZ);


 			//set header and footer
    		setHeader((sf_free_header*)heapStart, PAGE_SZ, NOPADDING, NOTALLOCATED);
			setFooter((sf_free_header*)heapEnd, PAGE_SZ, NOPADDING,PAGE_SZ, NOTALLOCATED);

			coalescePrevious((sf_free_header*)heapStart);


		}
	else{

			sf_errno = ENOMEM;

		}

}


/*
	this method places the appropriate payload to the apporopriate location and also
	creates new free  blocks to prevent the fragmentation.
	If we splice the free block that yeilds a block that is greater than the min Blck size
	we keep it as a free block in the respective segeregated list.
*/

void  place(void *allocatedBlockPointer, size_t allocatedSize, size_t size){

	sf_free_header* tempHeader =  (sf_free_header*)allocatedBlockPointer;


	int padding = 0;
	if(((int)allocatedSize - OVERHEAD ) != size ){
		padding = 1;
	}

	/* original free block size*/
	size_t csize = ((tempHeader->header).block_size) << 4;

	/*Remaining free block size*/
	size_t remainingSize = csize - allocatedSize;



	/* next memory row  adding the allocated size and pointing next location of footer of allocated block*/
	char* nextAddr  = ((char*)tempHeader) + allocatedSize;


	removeFromSeglist(tempHeader, csize);


	if (remainingSize >= MINBLOCK)
	{

		// set the footer and the header of the payload block in the respective address

		setHeader(tempHeader, allocatedSize, padding, ALLOCATED);
		setFooter(tempHeader, allocatedSize, padding, size, ALLOCATED);

		// add the free list to the seglist
		addtoSeglist((sf_free_header* )nextAddr, remainingSize);


		// now remake the free list
		setHeader((sf_free_header* )nextAddr,remainingSize, NOPADDING, NOTALLOCATED);
		setFooter((sf_free_header* )nextAddr, remainingSize, NOPADDING, remainingSize, NOTALLOCATED);

		coalesce((sf_free_header* )nextAddr);


	}else{

		// set the footer and the header of the payload block in the respective address

		setHeader(tempHeader, csize, padding, ALLOCATED);
		setFooter(tempHeader, csize, padding, allocatedSize, ALLOCATED);

	}


}



void* coalescePrevious(sf_free_header* freeHeader){


	int totalBlockSize = (freeHeader->header.block_size << 4);


	sf_footer* prevFooter;

	sf_free_header* prevFreeAddrHeader = freeHeader;



	// this checking if the prevBlock was allocated as free or not
	if(freeHeader != get_heap_start()){

		prevFooter = (sf_footer* )PREV_BLOCK_POINTER(freeHeader);

		if ((prevFooter-> allocated) == NOTALLOCATED)
		{
			// next memory row  adding the allocated size and pointing next location of footer of allocated block
			prevFreeAddrHeader  = (sf_free_header*) ((char*)prevFooter)+ DSIZE - (prevFooter->block_size << 4) ;

			totalBlockSize += (prevFooter->block_size << 4);

			removeFromSeglist((sf_free_header*) prevFreeAddrHeader, prevFooter->block_size << 4);

		}

	}


	removeFromSeglist(freeHeader,(freeHeader->header).block_size << 4);

	addtoSeglist(prevFreeAddrHeader, totalBlockSize);

	// now remake the free list
	setHeader(prevFreeAddrHeader,totalBlockSize, NOPADDING, NOTALLOCATED);
	setFooter(prevFreeAddrHeader, totalBlockSize, NOPADDING, totalBlockSize, NOTALLOCATED);

	return NULL;

}



void* coalesce(sf_free_header* freeHeader){


	int totalBlockSize = (freeHeader->header.block_size << 4);

	// nextFreeHeader is actually pointing to a footer of current freeList right now
	sf_free_header* nextFreeAddrHeader ;

	if( (nextFreeAddrHeader =(sf_free_header*) getNextHeader(freeHeader)) != NULL ){

		if( (nextFreeAddrHeader->header).allocated == NOTALLOCATED && ((nextFreeAddrHeader->header).block_size != 0) ){

			totalBlockSize += ((nextFreeAddrHeader->header).block_size << 4);

			removeFromSeglist(nextFreeAddrHeader, (nextFreeAddrHeader->header).block_size << 4);

		}

	}


	removeFromSeglist(freeHeader,(freeHeader->header).block_size << 4);

	addtoSeglist(freeHeader, totalBlockSize);

	// now remake the free list
	setHeader(freeHeader,totalBlockSize, NOPADDING, NOTALLOCATED);
	setFooter(freeHeader, totalBlockSize, NOPADDING, totalBlockSize, NOTALLOCATED);

	return NULL;

}


sf_footer *getFooter(sf_free_header* tempHeader){


	    sf_footer* footer = (sf_footer*)((char *) tempHeader + (tempHeader->header.block_size << 4)-8);

	    return footer;
}


void *getNextHeader(sf_free_header* tempHeader){

	if(((char *) tempHeader + (tempHeader->header.block_size << 4) -1)  == get_heap_end()){
		return NULL;
	}

	return ((char *) tempHeader + (tempHeader->header.block_size << 4));

}



// this method checks all the free list and the return the pointer to a free block
// that is able the hold the requested size

void *find_first_fit(size_t allocatedSize){

	char* returnAddress = NULL;


	if(allocatedSize <= LIST_1_MAX && (seg_free_list[0].head != NULL)){

		returnAddress = findInList(0, allocatedSize);

		return returnAddress;

	}


	if(allocatedSize <= LIST_2_MAX && (seg_free_list[1].head != NULL)){

		returnAddress = findInList(1, allocatedSize);
		return returnAddress;

	}

	if(allocatedSize <= LIST_3_MAX && (seg_free_list[2].head != NULL)){


		returnAddress = findInList(2, allocatedSize);
		return returnAddress;


	}

	if(allocatedSize <= PAGE_SZ && (seg_free_list[3].head != NULL)){


		returnAddress = findInList(3, allocatedSize);
		return returnAddress;

	}


	return returnAddress;

}



//this fucntion goes over the list of free blocks of the category of
//single level and returns the appropriate value.


void* findInList(int i, size_t allocatedSize){


	sf_free_header  *tempHeader = NULL;

	    	//printf("%s\n","this is working until here." );

	if(seg_free_list[i].head != NULL){

		tempHeader =  seg_free_list[i].head;


			// checks if the head in the free has sufficient place for the requested size
			if (((tempHeader->header.block_size) << 4) >= allocatedSize)
			{
				return tempHeader;
			}

		// if needed some higher spaces in a different category we return the another best fit
		while(tempHeader -> next != NULL){

			if (((tempHeader->header.block_size) << 4) >= allocatedSize)
			{
				return tempHeader;
			}

			tempHeader  = tempHeader-> next;

		}

	}


	return tempHeader;

}



/* this method finds which for which level in the segregated lists should
	remove the free packet from.
	calls helper function removeNodeFromList

*/


void removeFromSeglist(sf_free_header* freeHeader, size_t csize){


	if(csize <= LIST_1_MAX && (seg_free_list[0].head != NULL)){

		removeNodeFromList(freeHeader, 0);


	}

	else if(csize <= LIST_2_MAX && (seg_free_list[1].head != NULL)){

		removeNodeFromList(freeHeader, 1);

	}

	else if(csize <= LIST_3_MAX && (seg_free_list[2].head != NULL)){


		removeNodeFromList(freeHeader, 2);


	}

	else if(csize<= PAGE_SZ && (seg_free_list[3].head != NULL)){


		removeNodeFromList(freeHeader, 3);

	}


}


/*
	this method is helper method for the  remove from the seglist.
	It works with the linked list that is stored in the seg_free_list, arrays of free lists

*/

void removeNodeFromList(sf_free_header* freeHeader, int headOftheList){


	// if the node is the head of the list then make the list NULL
	if(freeHeader == seg_free_list[headOftheList].head ){
		seg_free_list[headOftheList].head = NULL;

	}else{

		// if it the first
		freeHeader->prev->next = freeHeader->next;

	}

}



/* this method finds in which level in the segregated lists should
	add the free packet frtoom.
	calls helper function addNodeToList

	*/

void addtoSeglist(sf_free_header* freeHeader, size_t csize){



	if(csize <= LIST_1_MAX ){

		addNodeToList(freeHeader, 0);


	}

	else if(csize <= LIST_2_MAX){

		addNodeToList(freeHeader, 1);

	}

	else if(csize <= LIST_3_MAX){


		addNodeToList(freeHeader, 2);


	}

	else if(csize<= PAGE_SZ){


		addNodeToList(freeHeader, 3);

	}


}


/*
	this method is helper method for the  add to the seglist.
	It works with the linked list that is stored in the seg_free_list, arrays of free lists

*/
void addNodeToList(sf_free_header* freeHeader, int headOftheList){

// if the node is the head of the list then make the list NULL
	if(seg_free_list[headOftheList].head == NULL){
		seg_free_list[headOftheList].head = freeHeader;

	}else{

		// put the node in the first place and then push the attach the other to it
		freeHeader->next = seg_free_list[headOftheList].head;
		seg_free_list[headOftheList].head = freeHeader;
		freeHeader->next->prev = freeHeader;

	}

}


void setHeader(sf_free_header* tempHeader, size_t allocatedSize, int padding, int allocated ){

 	tempHeader->header.allocated = allocated;
    tempHeader->header.padded = padding;
    tempHeader-> header.two_zeroes = 0 ;
    tempHeader->header.block_size = allocatedSize >> 4 ;
    tempHeader->header.unused = 0;


}

void setFooter(sf_free_header* tempHeader, size_t allocatedSize, int padding, size_t requestedSize, int allocated ){


    sf_footer* footer = (sf_footer*)((char *) tempHeader + (tempHeader->header.block_size << 4)-8);

    footer -> allocated = allocated;
    footer -> padded = padding;
    footer -> two_zeroes = 0 ;
    footer -> block_size =  allocatedSize >> 4 ;
    footer -> requested_size = requestedSize;


}


/*
	all the functions below this are header, footer, valid pointer  VALIDATION METHODS

*/



void pointerValidation(void* ptr){

	ptr = (char*)ptr - 8;


	if (ptr == NULL)
	{
		abort();
	}

	sf_free_header* tempHeader = (sf_free_header*)(ptr);


	if( (tempHeader -> header).allocated == NOTALLOCATED){

		abort();
	}



	if (checkHeaderFooterConsistency(tempHeader) == 0)
	{
		abort();
	}


	if (isValueBelowHeapStartAboveHeapEnd( tempHeader) == 0)
	{
		abort();
	}

	if (checkFooterValueMakeSense(tempHeader) == 0)
	{

		abort();
	}

}


int checkHeaderFooterConsistency(sf_free_header* tempHeader){

	int consistent = 1;

	//sf_blockprint(tempHeader);

	sf_header header = tempHeader->header;
	sf_footer* footer = (sf_footer*)((char *) tempHeader + (tempHeader->header.block_size << 4)-8);


	if(header.allocated != footer -> allocated){
		consistent =0;
	}

	if(header.padded != footer -> padded){
		consistent =0;
	}

	if(header.block_size != footer -> block_size){
		consistent =0;
	}

	return consistent;


}



int checkFooterValueMakeSense(sf_free_header* tempHeader){

	int consistent = 1;


	//sf_blockprint(tempHeader);


	sf_footer* footer = getFooter(tempHeader);

	if((((footer->block_size) << 4)  != (footer->requested_size + 16) )){

		if((footer->padded != 1)){

		 consistent = 0;
		};

	}
	else{



		printf("%i\n", (int)footer->padded);

		if((footer->padded != 0)){

		 consistent = 0;
		}

	}


	return consistent;

}


int isValueBelowHeapStartAboveHeapEnd(sf_free_header* tempHeader){


	int consistent = 1;

	sf_footer* footer = (sf_footer*)((char *) tempHeader + (tempHeader->header.block_size << 4)-8);


	if(((char*)footer+8) > (char*)get_heap_end()){
		consistent = 0;
	}


	return consistent;

}