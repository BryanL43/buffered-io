/**************************************************************
* Class::  CSC-415-01 Summer 2024
* Name:: Bryan Lee
* Student ID:: 922649673
* GitHub-Name:: BryanL43
* Project:: Assignment 5 – Buffered I/O read
*
* File:: b_io.c
*
* Description:: This program handles the basic buffered
* file operations: open, read, and close.
* The buffered read function processes the file's content in blocks,
* returning the specified amount of data the caller requests.
*
**************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "b_io.h"
#include "fsLowSmall.h"

#define MAXFCBS 20	//The maximum number of files open at one time


// This structure is all the information needed to maintain an open file
// It contains a pointer to a fileInfo strucutre and any other information
// that you need to maintain your open file.
typedef struct b_fcb {
	fileInfo * fi;	//holds the low level systems file info

	// Add any other needed variables here to track the individual open file
	char* buffer;		//Buffer for holding excess data
	int bufferPos;		//Tracks current buffer index
    int filePos;		//Tracks the amount of bytes read from file
	int blockOffset;	//Block offset from the file location
} b_fcb;
	
//static array of file control blocks
b_fcb fcbArray[MAXFCBS];

// Indicates that the file control block array has not been initialized
int startup = 0;	

// Method to initialize our file system / file control blocks
// Anything else that needs one time initialization can go in this routine
void b_init () {
	if (startup)
		return;			//already initialized

	//init fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++) {
		fcbArray[i].fi = NULL; //indicates a free fcbArray
	}
		
	startup = 1;
}

//Method to get a free File Control Block FCB element
b_io_fd b_getFCB () {
	for (int i = 0; i < MAXFCBS; i++) {
		if (fcbArray[i].fi == NULL) {
			fcbArray[i].fi = (fileInfo *)-2; // used but not assigned
			return i;		//Not thread safe but okay for this project
		}
	}

	return (-1);  //all in use
}

// b_open is called by the "user application" to open a file.  This routine is 
// similar to the Linux open function.  	
// You will create your own file descriptor which is just an integer index into an
// array of file control blocks (fcbArray) that you maintain for each open file.  
// For this assignment the flags will be read only and can be ignored.

b_io_fd b_open (char * filename, int flags) {
	if (startup == 0) b_init();  //Initialize our system

	//*** TODO ***//  Write open function to return your file descriptor
	//				  You may want to allocate the buffer here as well
	//				  But make sure every file has its own buffer

	// This is where you are going to want to call GetFileInfo and b_getFCB
	fileInfo* fileInfo = GetFileInfo(filename);
	if (fileInfo == NULL) {
		return -1;
	}

	b_io_fd fd = b_getFCB();
	if (fd == -1) {
		return -1;
	}

	//Instantiate a buffer for file control block with B_CHUNK_SIZE
	fcbArray[fd].buffer = malloc(B_CHUNK_SIZE);
	if (fcbArray[fd].buffer == NULL) {
		fcbArray[fd].fi = NULL;
		perror("Failed to allocate buffer!");
		return -1;
	}

	//Populate the instance of file control block
	fcbArray[fd].fi = fileInfo;
	fcbArray[fd].bufferPos = 0;
	fcbArray[fd].filePos = 0;
	fcbArray[fd].blockOffset = 0;

	return fd;
}

int abcde = 0;

// b_read functions just like its Linux counterpart read.  The user passes in
// the file descriptor (index into fcbArray), a buffer where thay want you to 
// place the data, and a count of how many bytes they want from the file.
// The return value is the number of bytes you have copied into their buffer.
// The return value can never be greater then the requested count, but it can
// be less only when you have run out of bytes to read.  i.e. End of File	
int b_read (b_io_fd fd, char * buffer, int count) {
	//*** TODO ***//  
	// Write buffered read function tbufferPos = bytesToCopy;o return the data and # bytes read
	// You must use LBAread and you must buffer the data in B_CHUNK_SIZE byte chunks.
		
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS)) {
		return (-1); 					//invalid file descriptor
	}

	// and check that the specified FCB is actually in use	
	if (fcbArray[fd].fi == NULL) {		//File not open for this descriptor
		return -1;
	}	

	// Your Read code here - the only function you call to get data is LBAread.
	// Track which byte in the buffer you are at, and which block in the file

	//Ensure valid count request: must be greater than 0 bytes.
	if (count <= 0) {
		return count;
	}

	b_fcb* fcb = &fcbArray[fd];
    int bytesCopied = 0;

    //EOF cap: limit count to not exceed the file size
    if (fcb->filePos + count > fcb->fi->fileSize) {
        count = fcb->fi->fileSize - fcb->filePos;
    }

    //Check if there is remaining data in the file control block's
	//buffer to transfer to the caller's buffer
    if (fcb->bufferPos > 0) {
        //Copy the existing data from our buffer to the caller's buffer
        int bytesAvailable = B_CHUNK_SIZE - fcb->bufferPos;
        int bytesToCopy = (count < bytesAvailable) ? count : bytesAvailable;
        memcpy(buffer, fcb->buffer + fcb->bufferPos, bytesToCopy);

		//Update variables according to the number of bytes copied
		fcb->bufferPos += bytesToCopy;
        bytesCopied += bytesToCopy;
        count -= bytesToCopy;
    }

    //Check if the requested bytes is large enough to
	//read directly into the caller's buffer
    if (count >= B_CHUNK_SIZE) {
        //Read chunk of blocks at once directly to caller's buffer
        int blocksToRead = count / B_CHUNK_SIZE;

        fcb->blockOffset += 
			LBAread(buffer + bytesCopied, blocksToRead, fcb->fi->location + fcb->blockOffset);

		//Update variables according to the number of bytes copied
        int bytesRead = blocksToRead * B_CHUNK_SIZE;
        bytesCopied += bytesRead;
        count -= bytesRead;
    }

    //Check if the caller's requested bytes is fullfilled
	//after transferring available data and handling large reads
    if (count > 0) {
		//Read a single block of data to our file control block's buffer
        fcb->blockOffset += LBAread(fcb->buffer, 1, fcb->fi->location + fcb->blockOffset);

		//Transfer the necessary data to fullfill caller's requested bytes
		memcpy(buffer + bytesCopied, fcb->buffer, count);

		//Update variables according to the number of bytes copied
		fcb->bufferPos = count;
		bytesCopied += count;
    }

	fcb->filePos += bytesCopied;
    return bytesCopied;
}
	


// b_close frees and allocated memory and places the file control block back 
// into the unused pool of file control blocks.
int b_close (b_io_fd fd) {
	//*** TODO ***//  Release any resources

	if (fd < 0 || fd >= MAXFCBS || fcbArray[fd].fi == NULL) {
        return -1;
    }

	//Release resources
    fcbArray[fd].fi = NULL;

    free(fcbArray[fd].buffer);
    fcbArray[fd].buffer = NULL;

    fcbArray[fd].bufferPos = 0;
    fcbArray[fd].filePos = 0;
	fcbArray[fd].blockOffset = 0;

	return 0;
}
	
