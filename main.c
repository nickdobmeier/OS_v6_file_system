// Nicholas Dobmeier
// njd170130

#include <stdio.h>              // necessary header files
#include <stdlib.h> // lseek()
#include <string.h>
#include <unistd.h> // read()
#include <sys/types.h> // read()
#include <sys/uio.h> // read()
#include <stdbool.h>
#include <fcntl.h>  // open()


    // Superblock Structure
#pragma pack(1)
typedef struct {
    unsigned short isize;       // 2 bytes - Blocks for i-list
    unsigned int fsize;         // 4 bytes - Number of blocks
    unsigned short nfree;       // 2 bytes - Pointer of free block array
    unsigned short ninode;      // 2 bytes - Pointer of free inodes array
    unsigned int free[151];     // Array to track free blocks
    unsigned short inode[201];  // Array to store free inodes
    char flock;
    char ilock;
    unsigned short fmod;
    unsigned short time[2];     // To store epoch
} superblock_type;
// 2+4+2+2 + (4*151) + (2*201) +1+1+2+(2+2) = 1024 Bytes (size of each block - this is the super block)


    // I-Node Structure
#pragma pack(1)
typedef struct {
    unsigned short flags;       // Flag of a file
    unsigned short nlinks;      // Number of links to a file
    unsigned short uid;         // User ID of owner
    unsigned short gid;         // Group ID of owner
    unsigned int size;          // 4 bytes - Size of the file
    unsigned int addr[11];      // Block numbers of the file location. ADDR[10] is used for double indirect block
    unsigned short actime[2];   // Last Access time
    unsigned short modtime[2];  // Last modified time
} inode_type;


// Indirect block Structure
#pragma pack(1)
typedef struct {
    unsigned int address[256];    // each address is 4 Bytes, 256 addresses * 4 Bytes each = 1024 Byte indirect block size
} indirectblock_type;



// global variables
superblock_type superBlock;
inode_type inode;
#define SIZE 90
char** strTokens;               // global so that can properly FREE() data in free up dynamically alloc memory without passing through multiple functions
int numTokens = 0;


// prototypes
bool getStringFromUser(char filename[]);
void delimitFilePath(char* fileToFind);
bool isDirectory(unsigned short flagShort);
bool isInodeAlloc(unsigned short flagShort);
bool isSmallFile(unsigned short flagShort);
void truncatePast14Chars();
void accessSmallFileContents(int* fileDescriptor, inode_type inode, int* fdWrite); // pass file descripter by reference since will be modifying it in the function
                                                                            // wait I do NOT think FD is actually modified ^^^^ ***
void cleanUp(int* fd);
void toLseek(int* fd, int offset, char* msg);
int toRead(int* fd, void* targetBuff, int nBytes, char* msg);       // void pointer to commodate types: superBlock, inode, indirectblock, char []
void accessLargeFileContents(int* fileDescriptor, inode_type inode, int* fdWrite);

// should be 112 i-nodes (16 i-nodes per block with 7 i-node bocks)

int main(int argc, char** argv)
{
    printf("Enter name of V6 file system:  ");
    char v6filename [SIZE];                                     // create buffer to hold v6 filesystem name input
    if(getStringFromUser(v6filename) == false){                 // read and store user input
        return 1;
    }
    printf("Enter name of file inside the V6 disk:  ");
    char fileToFind [SIZE];                                     // create buffer to hold v6 filesystem name input
    if(getStringFromUser(fileToFind) == false){                 // read and store user input
        return 1;
    }
    
    
    int fileDescriptor = open(v6filename, O_RDONLY);            // open file so that it is read only
    if(fileDescriptor < 0){
        printf("\tERROR opening file\n\t file descriptor value:  %d\n", fileDescriptor);
        return 1;
    }
    
    toLseek(&fileDescriptor, 1024, "seeking SUPER BLOCK");       // skip boot loader, ready to read super block. offset contains the new offset from BEGINING of file
    toRead(&fileDescriptor, &superBlock, sizeof(superBlock), "reading SUPER BLOCK");    // Read in Super block
    
    delimitFilePath(fileToFind);                                    // split fileToFind into array of strings stored in global strTokens (removes '\n' too)
    truncatePast14Chars();                                          // truncate any characters AFTER the 14th char in the fileNames given

    
// USE strTokens[][] *****************
    unsigned short inodeNum = 1;                        // start with ROOT directory -> inode number 1 (inodes numbers start at 1, even tho indexes are zero based)
    for(int depth = 0; depth < numTokens+1; depth++)    // < numTokens + 1 so that can come around one more time and access file
    {
        toLseek(&fileDescriptor, ((1024*2)+((inodeNum-1)*sizeof(inode))), "seeking i-node");    // skip to 2nd block where first i-nodes are located     (inodeNum-1) because inode numbers start at 1, but their indexes start at 0
        toRead(&fileDescriptor, &inode, sizeof(inode), "reading i-node");                       // READ first i-node (root directory)
    
        if(isInodeAlloc(inode.flags) == false){
            printf("\tReached an un-allocated inode. Flags: (%u). TERMINATING\n", inode.flags);
            break;}
        
        //printf("\t%s size:  %u\n", (isDirectory(inode.flags) == true) ? "directory" : "FILE", inode.size); //*******
        
        if(isDirectory(inode.flags) == false)           // verify i-node corresponds to a directory to continue. ALL dirctories are SMALL
        {
            int fdWrite = open("myoutputfile.txt", O_WRONLY | O_CREAT, 420);    // S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH  ->  110100100    ->     420
            if(fdWrite < 0){
                printf("\tERROR opening file\n\t file descriptor value:  %d\n", fdWrite);
                break;
            }
            
            if(isSmallFile(inode.flags) == true)
            {
                if(inode.size > (11 * 1024) ){      // max SMALL file size:  11 * 1024  =  11,264 Bytes
                    printf("\tFlag indicates file is SMALL, but the file's SIZE is larger than a small file can store. TERMINATING\n");
                }else{
                    accessSmallFileContents(&fileDescriptor, inode, &fdWrite);
                }
            }
            else{
                // (10*256*(long)1024) + (1*256*256*256*(long)1024)  =  17,182,490,624  bytes  (~17.182 GB)
                /* do NOT need to check file size, because this large file size structure can hold a file up to
                    17,182,490,624 Bytes, yet the SIZE field of the inode is only an 'unsigned int' which only supports max value of 4,294,967,295
                 */
                accessLargeFileContents(&fileDescriptor, inode, &fdWrite);
            }
            close(fdWrite);
            break;
        }
        if(isSmallFile(inode.flags) == false){
            printf("\tTHIS directory is a LARGE directory. Terminating.\n");
            break;                  // if run into large directory, exit program. Will NOT run into large directories though
        }

            // ASSUMES ALL DIRECTORIES ARE SMALL FILES
        bool wasFileFound = false;
        unsigned int dataDirBytesLeft = inode.size;
        
            // LOOP through each element in ADDR array until find the file looking for - each i-node has 11 elements in this structure
        for(int addrIndex = 0; addrIndex < 11; addrIndex++)
        {
            unsigned int dataNodeIndex = inode.addr[addrIndex];     // each address (ADDR or inside indirect blocks) is 4 Bytes - INTEGER
    
                // move to first DATA block of this i-node (note: block numbers start at 0  ie. bootloader = block 0)
            toLseek(&fileDescriptor, (1024*dataNodeIndex), "seeking a DIRECTORY i-node DATA block"); // skip to the DATA block which is pointed to by addr[addrIndex]

            
            int bytesToRead = (dataDirBytesLeft > 1024)  ?  (1024) : (dataDirBytesLeft);
                // read the data block that we just shifted to
            char dataBlockBuffer[bytesToRead+1];    // buffer is exact size of data in block that is to be read + 1 for null terminator
            toRead(&fileDescriptor, &dataBlockBuffer, bytesToRead, "reading a DIRECTORY i-node DATA block");
            dataBlockBuffer[bytesToRead] = 0;
            
                // directory data block layout: first 2 Bytes of each SLICE is the i-node number.
            for(int i=0; i<bytesToRead; i+=16)
            {
                char fileNameBuff[15];              // why size 15? 14 characters + 1 null terminator. filename comparing to on v6disk CAN be up to 14 chars
                for(int k=0; k<14; k++){
                    fileNameBuff[k] = dataBlockBuffer[i+k+2];       // if file name is LESS than 14 chars, it will be NULL terminated inside the directory!
                }
                fileNameBuff[14] = 0;
                
                if(strcmp(strTokens[depth], fileNameBuff) == 0)     // check if this slice in directory MATCHES the user input at same depth
                {
                    wasFileFound = true;
                
                    inodeNum = ( (int)dataBlockBuffer[i] << 0 ) | ( (int)dataBlockBuffer[i+1] << 8 );
                    break;
                }
            }
            
            if(wasFileFound == true){           // if file was found, do NOT need to search the rest of the ADDR array
                break;
            }
            dataDirBytesLeft -= bytesToRead;
            if(dataDirBytesLeft == 0)           // once exact file size amount has been read in, no where left to search
            {
                break;
            }
            
        }
        if(wasFileFound == false){              // if file NOT found current directory, we can't go any DEEPER into the file structure
            printf("\tSORRY, file does not exist in this directory\n");
            break;
        }
        
    }
// ***********************************
    cleanUp(&fileDescriptor);
    return 0;
}



bool getStringFromUser(char filename[])
{
    fgets(filename , SIZE , stdin);                             // 90 specifices max length (including '\n' so really max length is 89)
    int length = (int)strlen(filename);                             // last character of input string is always a NEWLINE character
                                                                // check if user entered any characters (there will ALWAYS atleast be the newline hence >1 )
    if( (length > 1) && (length <= SIZE) )                      // also check that length is inbounds
    {
        filename[length-1]=0;                                   // delete '\n', replace with null terminator
        return true;
    }else{
        printf("\tNo filename was given or filename is too long\n\tENDING PROGRAM NOW\n");
        return false;
    }
}


void delimitFilePath(char* fileToFind)
{
    int num_Tokens_Allowed = 20;
    char delimiterChars[2] = {47, 10};                          //   '/'   &  '\n' delimeter chars
    
    strTokens = malloc(num_Tokens_Allowed * sizeof(char*));     // max of 19 tokens allowed per command (+1 for NULL)
    
    char* token = strtok(fileToFind, delimiterChars);           // grab first token (or NULL)
    
    while (token != NULL)
    {
        if(numTokens >= num_Tokens_Allowed-1 )                  // need room for a NULL index at the end, so need to have atleast one spot allocated for that
        {
            num_Tokens_Allowed *= 2;                            // for realloc(), double number of allowed tokens
            strTokens = realloc(strTokens, num_Tokens_Allowed * sizeof(char*));
        }
        strTokens[numTokens] = malloc((SIZE+1) * sizeof(char));     // create space for each new token found
        strcpy(strTokens[numTokens], token);                        // copy token into newly created space
        
        numTokens++;
        token = strtok(NULL, delimiterChars);                       // grab next token
    }                                                               // In subsequent calls, the function expects a null pointer and uses the position right after
    // the end of the last token as the new starting location for scanning.
    strTokens[numTokens] = NULL;                                    // last index NULL so that exec() knows where last argument in strTokens is
}



bool isDirectory(unsigned short flagShort)
{
    flagShort = flagShort << 1;         // ISOLATE "bc" field - such that every OTHER bit field is 0 for each bit
    flagShort = flagShort >> 14;
    if(flagShort == 2){
        //printf("* bc:  %u\n", flagShort);
        return true;
    }
    return false;
}

bool isInodeAlloc(unsigned short flagShort)
{
    flagShort = flagShort >> 15;        // ISOLATE "a" field - such that every OTHER bit field is 0 for each bit
    if(flagShort == 1){
        //printf("* a:  %u\n", flagShort);
        return true;
    }
    return false;
}

bool isSmallFile(unsigned short flagShort)
{
    flagShort = flagShort << 3;         // ISOLATE "d" field - such that every OTHER bit field is 0 for each bit
    flagShort = flagShort >> 15;
    if(flagShort == 0){
        //printf("* d:  %u\n", flagShort);
        return true;
    }
    return false;
}

void truncatePast14Chars()
{
    for(int i=0; i<numTokens; i++)          // for each file in the chain
    {
        int k = 0;
        while (strTokens[i][k] != 0) {      // set ALL chars past the 14th char equal to  '\0'  until we reach where the String originally ended
            if(k >= 14){
                strTokens[i][k] = 0;
            }
            k++;
        }
    }
}


void accessSmallFileContents(int* fileDescriptor, inode_type inode, int* fdWrite)
{
    unsigned int dataBytesLeft = inode.size;
    
        // LOOP through each element in ADDR array until find the file looking for - each i-node has 11 elements in this structure
    for(int addrIndex = 0; addrIndex < 11; addrIndex++)
    {
        unsigned int dataNodeIndex = inode.addr[addrIndex];                                 // grab each data block pointed to by ADDR index
        toLseek(fileDescriptor, (1024*dataNodeIndex), "seeking FILE data block");           // skip to the DATA block which is pointed to by addr[addrIndex]

        
        int bytesToRead = (dataBytesLeft > 1024)  ?  (1024) : (dataBytesLeft);
        
        
        char dataBlockBuffer[bytesToRead+1];     // read the data block that we just shifted to
        int numBytesRead = toRead(fileDescriptor, &dataBlockBuffer, bytesToRead, "reading a FILE i-node DATA block");
        dataBlockBuffer[bytesToRead] = 0;      // in-case entire 1024 of Data block has contents, make sure it is NULL terminated for printf() below.
    
        
        //printf("%s", dataBlockBuffer);  //*****   printf output looks "messed up" on last data block of the file because upon checking the final data block (from ADDR) it uses the same address for "dataBlockuffer" as before, so the first N bytes read into the buffer are correct, but the remaing 1024-N bytes is data from the PREVIOUS block (but is still outputed) thus looking messed up
        //write(1, &dataBlockBuffer, bytesToRead); //*****
        write(*fdWrite, &dataBlockBuffer, bytesToRead);
    
        dataBytesLeft -= bytesToRead;
        if(numBytesRead != 1024 || dataBytesLeft == 0)       // if full 1024 Bytes were not read in, then this data-block is where file data ENDS
        {
            if( !(dataBytesLeft == 0 && numBytesRead == bytesToRead) ){
                printf("[EOF]\nDISCREPANCY\n");
                printf("dataBytesLeft == 0            ->  %s\n", (dataBytesLeft == 0) ? "true" : "FALSE");
                printf("numBytesRead == bytesToRead   ->  %s\n", (numBytesRead == bytesToRead) ? "true" : "FALSE");
                printf("numBytesRead != 1024          ->  %s\n", (numBytesRead != 1024) ? "true" : "FALSE");
            }
            break;
        }
        
    }
}


void cleanUp(int* fd)
{
    for (int i = 0; i < numTokens; i++){                    // free up each space in 2nd dimension
        free(strTokens[i]);
    }
    free(strTokens);                                        // free up space in first dimension
    close(*fd);                                             // close file
}


void toLseek(int* fd, int offset, char* msg)
{
    int val = lseek(*fd, offset, SEEK_SET);         // SEEK_SET == 0
    if(val < 0){
        printf("\tERROR SEEKING: %s\n", msg);
        cleanUp(fd);
        exit(1);
    }
}


int toRead(int* fd, void* targetBuff, int nBytes, char* msg)
{
    int numBytesRead = read(*fd, targetBuff, nBytes);
    if(numBytesRead < 0){
        printf("\tERROR READING: %s\n", msg);
        cleanUp(fd);
        exit(1);
    }
    return numBytesRead;
}



void accessLargeFileContents(int* fileDescriptor, inode_type inode, int* fdWrite)
{
    unsigned int dataBytesLeft = inode.size;    // LOOP through each element in ADDR array until find the file looking for - each i-node has 11 elements in this structure
    
    for(int addrIndex = 0; addrIndex < 11; addrIndex++)
    {
        unsigned int indirectNodeIndex = inode.addr[addrIndex];
        toLseek(fileDescriptor, (1024*indirectNodeIndex), "seeking FILE indirect block");       // skip to the indirect block which is pointed to by addr[addrIndex]
        
        indirectblock_type indirectBlock;           // read the indirect block that we just shifted to, STRUCTURED so exactly 1024 Bytes
        toRead(fileDescriptor, &indirectBlock, 1024, "reading a FILE i-node indirect block");
        
        bool didReachEOF = false;
        int numAddressesInBlock = 1024/4;           // EACH BLOCK ADDRESS is 4 BYTES (integer)
        
        if(addrIndex != 10)                         // first 10 ADDR indexes can store:  10 * 256 * 1024  =  2,621,440 Bytes  ( ~2.6 MB )
        {
            for(int i=0; i<numAddressesInBlock; i++)  // loop through ALL 256 data block addresses in each SINGLE indirect block
            {
                unsigned int dataNodeIndex = indirectBlock.address[i];
                toLseek(fileDescriptor, (1024*dataNodeIndex), "seeking FILE DATA block");   // skip to the DATA block which is pointed to by indirect block
                
                int bytesToRead = (dataBytesLeft > 1024)  ?  (1024) : (dataBytesLeft);
                
                char dataBlockBuffer[bytesToRead+1];
                int numBytesRead = toRead(fileDescriptor, &dataBlockBuffer, bytesToRead, "reading a FILE indirect block's DATA block");
                dataBlockBuffer[bytesToRead] = 0;      // this is fine since ONLY writing 1024 characters to the file... which does NOT include that null terminator at the end there
                
                write(*fdWrite, &dataBlockBuffer, bytesToRead); // every other data block is FULL of null-terminators....
                
                dataBytesLeft -= bytesToRead;
                if(numBytesRead != 1024 || dataBytesLeft == 0)       // if full 1024 Bytes were not read in, then this data-block is where file data ENDS
                {
                    if(!(dataBytesLeft == 0 && numBytesRead == bytesToRead)){  // do NOT need (numBytesRead != 1024) because its possible the last BLOCK is totally full
                        printf("[EOF]\nDISCREPANCY\n");
                        printf("dataBytesLeft == 0            ->  %s\n", (dataBytesLeft == 0) ? "true" : "FALSE");
                        printf("numBytesRead == bytesToRead   ->  %s\n", (numBytesRead == bytesToRead) ? "true" : "FALSE");
                        printf("numBytesRead != 1024          ->  %s\n", (numBytesRead != 1024) ? "true" : "FALSE");
                    }
                    didReachEOF = true;
                    break;      // break out of ADDR loop && the looping through current indirect block
                }
                
                
            }
            
        }else{      // deal with TRIPLE indirection once reach ADDR[10]     // last index
            
            for(int i=0; i<numAddressesInBlock; i++)    //  1 * 256 * 256 * 256 * 1024  =  17,179,869,184  ( ~17.179 GB )
            {
                unsigned int indirect2NodeIndex = indirectBlock.address[i];
                toLseek(fileDescriptor, (1024*indirect2NodeIndex), "seeking a FILE double indirect-block");
                indirectblock_type indirectBlockDOUBLE;           // read the indirect block that we just shifted to, STRUCTURED so exactly 1024 Bytes
                toRead(fileDescriptor, &indirectBlockDOUBLE, 1024, "reading a FILE double indirect-block");
                
                for(int k=0; k<numAddressesInBlock; k++)
                {
                    unsigned int indirect3NodeIndex = indirectBlockDOUBLE.address[k];
                    toLseek(fileDescriptor, (1024*indirect3NodeIndex), "seeking a FILE TRIPLE indirect-block");
                    indirectblock_type indirectBlockTRIPLE;           // read the indirect block that we just shifted to, STRUCTURED so exactly 1024 Bytes
                    toRead(fileDescriptor, &indirectBlockTRIPLE, 1024, "reading a FILE TRIPLE indirect-block");
                    
                
                    for(int h=0; h<numAddressesInBlock; h++)
                    {
                        unsigned int dataNodeIndex = indirectBlockTRIPLE.address[h];
                        toLseek(fileDescriptor, (1024*dataNodeIndex), "seeking FILE triple-indirect-block's DATA block");   // skip to the data block which is pointed to by indirect block
                        
                        int bytesToRead = (dataBytesLeft > 1024)  ?  (1024) : (dataBytesLeft);
                        
                        char dataBlockBuffer[bytesToRead+1];
                        int numBytesRead = toRead(fileDescriptor, &dataBlockBuffer, bytesToRead, "reading a FILE triple-indirect-block's DATA block");
                        dataBlockBuffer[bytesToRead] = 0;
                        
                        
                        write(*fdWrite, &dataBlockBuffer, bytesToRead);
                        
                        dataBytesLeft -= bytesToRead;
                        if(numBytesRead != 1024 || dataBytesLeft == 0)       // if full 1024 Bytes were not read in, then this data-block is where file data ENDS
                        {
                            if( !(dataBytesLeft == 0 && numBytesRead == bytesToRead) ){
                                printf("\nDISCREPANCY\n");
                                printf("dataBytesLeft == 0            ->  %s\n", (dataBytesLeft == 0) ? "true" : "FALSE");
                                printf("numBytesRead == bytesToRead   ->  %s\n", (numBytesRead == bytesToRead) ? "true" : "FALSE");
                                printf("numBytesRead != 1024          ->  %s\n", (numBytesRead != 1024) ? "true" : "FALSE");
                            }
                            didReachEOF = true;
                            break;      // break out of ALL indirection loops
                        }
                    }
                    
                    if(didReachEOF == true){
                        break;
                    }
                }
                
                if(didReachEOF == true){
                    break;
                }
            }
        }
        
    
        if(didReachEOF == true){
            break;
        }
        
    }
}



/*
 The following files exist:
 
        /dir2/file1.txt  and the size is  2KB
 
        LARGE:
            /dir3/subdir1/subdir2/subdir3/file4.txt   ( 1.34 MB )
 
            /dir1/subdir1/subdir2/file3.txt

 
 
 The following two files DON'T exist:
 
        /dir5/file1.cc
 
        /dir1/sample.c
*/
