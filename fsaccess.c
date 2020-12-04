/****************
* Compilation :-$ gcc fsaccessPart2.c -ofs
* Run using :- $ ./fs
******************/


#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>


//Splitting file size prior to storing in inode using bitwise operations
#define LAST(k,n) ((k) & ((1<<(n))-1))
#define MID(k,m,n) LAST((k)>>(m),((n)-(m)))

#define FREE_SIZE 152
#define I_SIZE 200
#define ADDR_SIZE 11
#define INPUT_SIZE 256
#define BLOCK_SIZE 1024

//Superblock structure
typedef struct  {
    unsigned short isize;
    unsigned short fsize;
    unsigned short nfree;
    unsigned short free[FREE_SIZE];
    unsigned short ninode;
    unsigned short inode[I_SIZE];
    char flock;
    char ilock;
    char fmode;
    unsigned short time[2];
} superblock_type;

//Inode structure
typedef struct {
    unsigned short flags;
    unsigned short nlinks;
    unsigned short uid;
    unsigned short gid;
    unsigned int size0;
    unsigned int size1;
    unsigned int addr[ADDR_SIZE];
    unsigned short actime[2];
    unsigned short modtime[2];
} inode_type;

//directory structure
typedef struct {
    unsigned short inode;
    char file_name[14];
} dir_type;

//free blocks structure
typedef struct  {
    unsigned short nfree;
    unsigned short free[FREE_SIZE];
} free_blocks_struct;

int cwdInodeNumber;        // stores current working directory's inode number in this variable
char cwdPath[INPUT_SIZE];  // stores current working directory's complete path


//Functions used
int preInitialization(); //checking if filesystem exists already. If doesn't exist, initializes filesystem calling initfs
int initfs(int numBlocks, int numInodes, FILE* fileSystem); //initialize file system - initialize superblock, free inode list, root directory
int cpin(const char* fromFilename, const char* toFilename, FILE* fileSystem);  //copy content from an external file to a file in the V6 file system
int cpout(const char* fromFilename, const char* toFilename, FILE* fileSystem); //copying a file in the V6 file system to an external file

inode_type initializeInode(unsigned int fileSize); //initializing an inode
inode_type readInode(int to_file_inode_num, FILE* fileSystem); //reading an inode in file system
int writeInode(int inodeNum, inode_type inode, FILE* fileSystem); //writing inode to file system
unsigned int getInodeFilesize(int to_file_inode_num, FILE* fileSystem); //retrieve the size of a file that a inode points to
int getInodeByFilename(const char* to_filename,FILE* fileSystem); //checking to see if a file exists in root directory by filename and returns inode of file

void addBlockToInode(int blockOrderNum, int blockNum, int to_file_inode_num, FILE* fileSystem); //adding data block to inode addr array and writing inode to file system
void add_block_to_free_list(int blockNum, FILE* fileSystem); //adding free blocks to free array in superblock
int getFreeBlock(FILE* fileSystem); //Retrieving a free block from free array in superblock
unsigned short getBlockOfFile(int fileInodeNum, int block_number_order, FILE* fileSystem); //reading inode addr array and returning a file block number
int addFileToDirec(const char* toFilename, FILE* fileSystem); //adding a file entry to root directory
void copyIntArray(unsigned short *fromArray, unsigned short *toArray, int bufferLength); //function to copy from one array to another

int cd(char* dirName, FILE* fileSystem); //change current working directory to the directory passed into the function
int new_mkdir(const char* filename, FILE* file_system); //creates a directory using the name passed in
int Remove(const char* filename,FILE* file_system); //removes a file from the v6 file system
void removeFileFromDirectory(int file_node_num, FILE*  file_system); //removing a directory entry from current working directory
void printcwd(); // prints current working directory


int inode_size=64;
FILE* fileSystem = NULL;
int fileDescriptor ;

int main()
{
    char *splitter;
    char input[INPUT_SIZE];
    int status;


    //V6 file system menu
    printf("===================================================\n");
    printf("\n ~WELCOME TO UNIX V6 FILE SYSTEM~ \n");
    printf("\n PLEASE ENTER ONE OF THE FOLLOWING COMMANDS \n");
    printf("\n initfs filename numberOfBlocks numberOfInodes - To initialize the file system ex: initfs v6filesystem 8000 300 \n");
    printf("\n cpin externalFile V6File - To copy contents from an external file to a new file in the V6 file system ex: cpin test.txt fstest.txt \n");
    printf("\n cpout V6File externalFile - To copy contents from a file in the V6 file system to an external file ex: cpout fstest.txt test2.txt \n");
	printf("\n mkdir directoryName - To create a directory ex: mkdir /user \n");
	printf("\n cd directoryName - To change directory ex: cd /user \n");
	printf("\n rm FileName - To remove a file ex: rm fstest.txt \n");
	printf("\n q - To exit file system \n");
    printf("===================================================\n");

	strcpy(cwdPath, "/");
    while(1){
        printf("V6FileSystem:%s>>",cwdPath);

        scanf(" %[^\n]s", input);
		char userinputcopy[INPUT_SIZE] = " ";
		strcpy(userinputcopy, input);
		
        splitter = strtok(input," ");

        if(strcmp(splitter, "initfs") == 0){

            preInitialization();
            splitter = NULL;
        }else if (strcmp(splitter, "q") == 0){

            printf("Exiting V6 file system\n");
            fclose(fileSystem);
            exit(0);

        }else if (strcmp(splitter,"cpin")==0){
            char *  ptr    = strtok (NULL, " ");
            const char *fromFilename = ptr;
            ptr = strtok (NULL, " ");
            const char *toFilename = ptr;
            if (strlen(toFilename) > 14){
                printf("Destination file name  %s is too long:%i, maximum allowed length is 14", toFilename, strlen(toFilename));
                status=1;
            }
            else
                status = cpin(fromFilename, toFilename, fileSystem);
            if (status ==0)
                printf("\nFile copied successfully!\n");
            else
                printf("\nError! File copy failed\n");

        }

        else if (strcmp(splitter,"cpout")==0){
            char *  p    = strtok (NULL, " ");
            const char *fromFilename = p;
            p = strtok (NULL, " ");
            const char *toFilename = p;
            status = cpout(fromFilename, toFilename, fileSystem);
            if (status ==0)
                printf("\nFile %s successfully copied\n", fromFilename);
            else
                printf("\nFile copy failed\n");
        }
		
		else if (strcmp(splitter, "mkdir")==0){
			char copyPath1[INPUT_SIZE] = " ";
			strcpy(copyPath1, cwdPath); //save initial working directory file path
			int copycwdinodebefore1 = cwdInodeNumber; //save initial inode number of current working directory
			char *dirToCreate;
			int j;
			
			
			char *filepath = strstr(userinputcopy,"/");  //get pointer to first / in user input; extract cwd path starting from first /
			if(filepath==NULL){
				printf("Error! specify paths with slash\n");
				continue;
			}
			int lastposition = strrchr(filepath, '/') - filepath;  //get position of last / in file path as int
			if(lastposition==0){
					//create new directory in root
					char *directoryname = strtok(filepath, "/");
					status = new_mkdir(directoryname, fileSystem);
				if (status ==0)
					printf("\nDirectory %s successfully created\n",directoryname);
				else
					printf("\nDirectory creation failed\n");
				continue;
			}
			
			char *targetDir1;
			char filepathbuffer[INPUT_SIZE] = " ";
			
			strncpy(filepathbuffer, filepath, lastposition); //copy characters upto last / in file path to a new buffer
			
			targetDir1 = strtok(filepathbuffer, "/"); //extract each directory in file path separated by /, call cd
			
			while(targetDir1 != NULL){
				j = cd(targetDir1, fileSystem);
				targetDir1 = strtok(NULL, "/");	
			}
			dirToCreate = strrchr(filepath,'/')+1; //get last directory in file path
			
			char *pathUserInput = strstr(userinputcopy," ")+1;  //extract only file path from user input
			int pos = strrchr(pathUserInput, '/') - pathUserInput;  //get position of last / in user inputted file path
			char buf[INPUT_SIZE] = " ";
			strncpy(buf, pathUserInput, pos); //copy characters upto last / in user inputted file path to a new buffer
			
			if((strcmp(cwdPath,buf)!=0)){  
				if((j==1) && (strcmp(buf,"..")!=0) && (strcmp(buf,".")!=0)){
					printf("Error! Incorrect file path\n");
					memset(cwdPath, 0, sizeof (cwdPath));
					strcpy(cwdPath, copyPath1);
					cwdInodeNumber = copycwdinodebefore1;
					continue;
				}
			}
			
				const char *directory = dirToCreate;
				status = new_mkdir(directory, fileSystem);
				if (status ==0)
					printf("\nDirectory %s successfully created\n",directory);
				else
					printf("\nDirectory creation failed\n");
			
		}
		else if (strcmp(splitter, "cd")==0){
			char copyPath[INPUT_SIZE] = " ";
			strcpy(copyPath, cwdPath); //save initial file path
			int copycwdinodebefore = cwdInodeNumber;  //save current working directory inode
			int i;
			char *targetDir = strtok(NULL, "/");  //extract each directory from user inputted file path, call cd
			
			while(targetDir != NULL){
				i = cd(targetDir, fileSystem);
				targetDir = strtok(NULL, "/");	
			}
			
					
			char *userinput1= strstr(userinputcopy," ")+1;   //if user inputted file path and cwd path match, don't do anything
			if(strcmp(userinput1,cwdPath)==0){
				continue;
			}
					
			
			if((i==1 || (strcmp(cwdPath,userinput1)!=0)) && (strcmp(userinput1,"..")!=0) && (strcmp(userinput1,".")!=0)){
				printf("Error! Directory does not exist\n");
				memset(cwdPath, 0, sizeof (cwdPath));  //if wrong path, reset cwd path and inode
				strcpy(cwdPath, copyPath);
				cwdInodeNumber = copycwdinodebefore;				
			}
			
			
            
		}
		else if(strcmp(splitter, "rm")==0){
			char *  rm    = strtok (NULL, " ");
            const char *rmFilename = rm;
            status = Remove(rmFilename, fileSystem);
            if (status ==0)
                printf("\nFile %s successfully removed\n");
            else
                printf("\nFile removal failed\n");
			
		}
		
		else if (strcmp(splitter, "pwd")==0){
			printcwd();
		}


        else
            printf("Error! Not a valid command, please try again. Available commands are: initfs, cpin, cpout, cd, mkdir, rm, q\n");

    }
}


//checking if filesystem exists already. If doesn't exist, initializes filesystem calling initfs
int preInitialization(){

    char *n1, *n2;
    unsigned int numBlocks = 0, numInodes = 0;
    char *filepath;

    filepath = strtok(NULL, " ");
    n1 = strtok(NULL, " ");
    n2 = strtok(NULL, " ");


    if(access(filepath, F_OK) != -1) {

        if((fileDescriptor = open(filepath, O_RDWR, 0600)) == -1){

            printf("\n filesystem already exists but open() failed with error [%s]\n", strerror(errno));
            return 1;
        }
        printf("filesystem already exists and the same will be used.\n");
        fileSystem = fopen(filepath, "r+");

    } else {

        if (!n1 || !n2) {
            printf(" All arguments(path, number of inodes and total number of blocks) have not been entered\n");
            return 1; }
        else {
            numBlocks = atoi(n1);
            numInodes = atoi(n2);
            fileSystem = fopen(filepath, "w+");

            if((initfs(numBlocks, numInodes, fileSystem )) == 0){
                printf("The file system is initialized\n");
            } else {
                printf("Error initializing file system. Exiting... \n");
                return 1;
            }
        }
    }
}



//initialize file system - initialize superblock, free inode list, root directory
int initfs(int numBlocks, int numInodes, FILE* fileSystem)
{
    char buffer[BLOCK_SIZE];
    superblock_type superblock;
    dir_type directoryEntry;
    memset(buffer, 0, BLOCK_SIZE);
    printf("\nInitialized fileSystem with %i blocks and %i inodes\n", numBlocks, numInodes);
    int i;
    rewind(fileSystem);
    //writing zeros to all blocks to initialize v6 file system
    for (i=0; i < numBlocks; i++){
        fwrite(buffer, 1, BLOCK_SIZE, fileSystem);
    }


    //Initializing superblock
    superblock.isize=0;
    superblock.fsize=0;
    superblock.nfree=1;
    superblock.free[0] = 0;
    fseek(fileSystem, BLOCK_SIZE, SEEK_SET);
    fwrite(&superblock, BLOCK_SIZE, 1, fileSystem);
    //each block has 16 inodes (1024/64)
    int numberInodeBlocks = numInodes / 16;
    //root directory block placed after blocks containing inodes
    int startingFreeBlock= 2 + numberInodeBlocks + 1;
    int nextFreeBlock;

    // free blocks initialized
    for (nextFreeBlock=startingFreeBlock; nextFreeBlock < numBlocks; nextFreeBlock++ ){
        add_block_to_free_list(nextFreeBlock, fileSystem);
    }


    //Initializing free inode list

    fseek(fileSystem, BLOCK_SIZE, SEEK_SET); //loading superblock from filesystem
    fread(&superblock, sizeof(superblock), 1, fileSystem);  //reading superblock
    superblock.ninode=199;
    int nextFreeInode=1; //Root Directory has 1st inode
    for (i=0;i<=199;i++){
        superblock.inode[i]=nextFreeInode;
        nextFreeInode++;
    }
    //Go to beginning of the second block
    fseek(fileSystem, BLOCK_SIZE, SEEK_SET);
    fwrite(&superblock, BLOCK_SIZE, 1, fileSystem);


    inode_type rootInode;
    rootInode.flags=0;       //Initialize
    rootInode.flags |=1 << 15; //Set first bit to 1 (inode is allocated)
    rootInode.flags |=1 << 14; // Set 2nd & 3rd bits to 10 (inode is a directory)
    rootInode.flags |=0 << 13;
    rootInode.nlinks=0;
    rootInode.uid=0;
    rootInode.gid=0;
    rootInode.size0=0;
    rootInode.size1= 16 * 2;
    rootInode.addr[0]=0;
    rootInode.addr[1]=0;
    rootInode.addr[2]=0;
    rootInode.addr[3]=0;
    rootInode.addr[4]=0;
    rootInode.addr[5]=0;
    rootInode.addr[6]=0;
    rootInode.addr[7]=0;
    rootInode.addr[8]=0;
    rootInode.addr[9]=0;
    rootInode.addr[10]=0;
    rootInode.actime[0]=0;
    rootInode.actime[1]=0;
    rootInode.modtime[0]=0;
    rootInode.modtime[1]=0;


    // initializing root directory with "." and ".." and setting inode to 1
    directoryEntry.inode = 1;
    strcpy(directoryEntry.file_name, ".");
    // keep block for file_directory
    int dir_file_block = startingFreeBlock - 1;
    //go to beginning of first available data block
    fseek(fileSystem, dir_file_block * BLOCK_SIZE, SEEK_SET);
    fwrite(&directoryEntry, 16, 1, fileSystem);
    strcpy(directoryEntry.file_name, "..");
    fwrite(&directoryEntry, 16, 1, fileSystem);

    rootInode.addr[0]=dir_file_block;
    writeInode(1, rootInode, fileSystem);
	
	// set current working directory to root
    cwdInodeNumber = 1;
    strcpy(cwdPath, "/");


    return 0;
}

//adding free blocks to free array in superblock
void add_block_to_free_list(int blockNum, FILE* fileSystem){
    superblock_type superBlock;
    free_blocks_struct copy_to_block;
    fseek(fileSystem, BLOCK_SIZE, SEEK_SET);
    fread(&superBlock, sizeof(superBlock), 1, fileSystem);
    if (superBlock.nfree == FREE_SIZE){ //free array is full. Copy nfree & free array content to the new block
        copy_to_block.nfree= FREE_SIZE;
        copyIntArray(superBlock.free, copy_to_block.free, FREE_SIZE);
        fseek(fileSystem, blockNum * BLOCK_SIZE, SEEK_SET);
        fwrite(&copy_to_block, sizeof(copy_to_block), 1, fileSystem);
        superBlock.nfree = 1;
        superBlock.free[0] = blockNum; //set new block to position 0 in free array
    }
    else {
        superBlock.free[superBlock.nfree] = blockNum;
        superBlock.nfree++;
    }
    fseek(fileSystem, BLOCK_SIZE, SEEK_SET);
    fwrite(&superBlock, sizeof(superBlock), 1, fileSystem);
}

//function to copy from one array to another
void copyIntArray(unsigned short *fromArray, unsigned short *toArray, int bufferLength){
    int i;
    for (i=0; i < bufferLength; i++){
        toArray[i] = fromArray[i];
    }
}

//copy content from an external file to a file in the V6 file system
int cpin(const char* fromFilename, const char* toFilename, FILE* fileSystem){
    inode_type toFileInode;
    int to_file_inode_num, status_ind, num_bytes_read, freeBlockNum, fileInodeNum;
    unsigned long fileSize;
    FILE* fromFileFd;
    unsigned char readBuffer[BLOCK_SIZE];

    fileInodeNum = getInodeByFilename(toFilename, fileSystem);
    if (fileInodeNum != -1){
        printf("\nError! File %s already exists. Please choose a different file name", toFilename);
        return -1;
    }

    if(access(fromFilename, F_OK ) != -1 ) {
        // source file exists
        printf("\nSource file %s exists. Opening...\n", fromFilename);
        fromFileFd = fopen(fromFilename, "rb");
        fseek(fromFileFd, 0L, SEEK_END);
        fileSize = ftell(fromFileFd);
        if (fileSize == 0){
            printf("\nError! Source file %s does not exist. Enter correct file name\n", fromFilename);
            return -1;
        }
        else{
            printf("\nSize of source file is %i\n", fileSize);
            rewind(fileSystem);
        }
    }
    else {
        // source file doesn't exist
        printf("\nSource file %s does not exist. Enter correct file name\n", fromFilename);
        return -1;
    }

    //Adding new file to root directory
    to_file_inode_num = addFileToDirec(toFilename, fileSystem);
    //Initialize & load new file inode
    toFileInode = initializeInode(fileSize);
    status_ind = writeInode(to_file_inode_num, toFileInode, fileSystem);

    //copying content from source file to destination file
    int numBlocksRead=1;
    int totalNumBlocks=0;
    fseek(fromFileFd, 0L, SEEK_SET);
    int block_order=0;
    while(numBlocksRead == 1){
        numBlocksRead = fread(&readBuffer, BLOCK_SIZE, 1, fromFileFd);
        totalNumBlocks+= numBlocksRead;
        freeBlockNum = getFreeBlock(fileSystem);

        if (freeBlockNum == -1){
            printf("\nError! Ran out of free blocks. Total blocks read so far:%i", totalNumBlocks);
            return -1;
        }

        addBlockToInode(block_order, freeBlockNum, to_file_inode_num, fileSystem);

        // Writing block by block into destination file
        fseek(fileSystem, freeBlockNum * BLOCK_SIZE, SEEK_SET);
        fwrite(&readBuffer, sizeof(readBuffer), 1, fileSystem);
        block_order++;

    }
    return 0;
}


//adding data block to inode addr array and writing inode to file system
void addBlockToInode(int blockOrderNum, int blockNum, int to_file_inode_num, FILE* fileSystem){
    inode_type fileInode;
    unsigned short block_num_tow = blockNum;
    fileInode = readInode(to_file_inode_num, fileSystem);
    fileInode.addr[blockOrderNum] = block_num_tow;
    writeInode(to_file_inode_num, fileInode, fileSystem);
}

//Retrieving a free block from free array in superblock
int getFreeBlock(FILE* fileSystem){
    superblock_type superBlock;
    free_blocks_struct copyFromBlock;
    int freeBlock;
    //reading superBlock
    fseek(fileSystem, BLOCK_SIZE, SEEK_SET);
    fread(&superBlock, sizeof(superBlock), 1, fileSystem);
    superBlock.nfree--;
    freeBlock = superBlock.free[superBlock.nfree];
    //checking if there are no free block left
    if (freeBlock == 0){
        printf("(\nError! No free blocks left");
        fseek(fileSystem, BLOCK_SIZE, SEEK_SET);
        fwrite(&superBlock, sizeof(superBlock), 1, fileSystem);
        fflush(fileSystem);
        return -1;
    }

    // Check if need to retrieve free blocks from the list at free[0]
    if (superBlock.nfree == 0) {
        fseek(fileSystem, BLOCK_SIZE * superBlock.free[superBlock.nfree], SEEK_SET); //go to free[0]
        fread(&copyFromBlock, sizeof(copyFromBlock), 1, fileSystem);
        superBlock.nfree = copyFromBlock.nfree;
        copyIntArray(copyFromBlock.free, superBlock.free, FREE_SIZE);
        superBlock.nfree--;
        freeBlock = superBlock.free[superBlock.nfree];
    }

    fseek(fileSystem, BLOCK_SIZE, SEEK_SET);
    fwrite(&superBlock, sizeof(superBlock), 1, fileSystem);  //write updated superBlock to file system
    fflush(fileSystem);
    return freeBlock;
}


//adding a file entry to root directory  ****changed to adding file to current directory****
int addFileToDirec(const char* toFilename, FILE* fileSystem){

    inode_type directoryInode, freeNode;
    dir_type directoryEntry;
    int to_file_inode_num, flag;
    int found = 0;
    to_file_inode_num=1;
    //searching for an unallocated inode
    while(found==0){
        to_file_inode_num++;
        freeNode = readInode(to_file_inode_num, fileSystem);
        flag = MID(freeNode.flags, 15, 16);
        if (flag == 0){
            found = 1;
        }
    }
    //reading root directory
    directoryInode = readInode(cwdInodeNumber, fileSystem);  //******changed to reading current directory*****

    // Move to the end of root directory file
    fseek(fileSystem, (BLOCK_SIZE * directoryInode.addr[0] + directoryInode.size1), SEEK_SET);
    // Add record to root directory file
    directoryEntry.inode = to_file_inode_num;
    strcpy(directoryEntry.file_name, toFilename);
    fwrite(&directoryEntry, 16, 1, fileSystem);


    //Update root directory inode. Increase size by one file record
    directoryInode.size1+=16;
    writeInode(cwdInodeNumber, directoryInode, fileSystem);
    //returning new inode found which is now the inode of new entry in root
    return to_file_inode_num;

}

//initializing an inode
inode_type initializeInode(unsigned int fileSize){
    inode_type toFileInode;
    unsigned short bit0_15;
    unsigned char bit16_23;
    unsigned short bit24;

    bit0_15 = LAST(fileSize, 16);
    bit16_23 = MID(fileSize, 16, 24);
    bit24 = MID(fileSize, 24, 25);

    toFileInode.flags=0;
    toFileInode.flags |=1 << 15; //Setting 1st bit to 1(inode is allocated)
    toFileInode.flags |=0 << 14; // Setting 2nd & 3rd bits to 00 (inode is a plain file)
    toFileInode.flags |=0 << 13;
    if (bit24 == 1){
        toFileInode.flags |=1 << 0;
    }
    else{
        toFileInode.flags |=0 << 0;
    }
    if (fileSize <= 10 * BLOCK_SIZE){
        toFileInode.flags |=0 << 12; //Seting 4th bit to 0 (small file)
    }
    else{
        toFileInode.flags |=1 << 12; //Set 4th bit to 1 (large file)
    }
    toFileInode.nlinks=0;
    toFileInode.uid=0;
    toFileInode.gid=0;
    toFileInode.size0=bit16_23;
    toFileInode.size1=bit0_15;
    toFileInode.addr[0]=0;
    toFileInode.addr[1]=0;
    toFileInode.addr[2]=0;
    toFileInode.addr[3]=0;
    toFileInode.addr[4]=0;
    toFileInode.addr[5]=0;
    toFileInode.addr[6]=0;
    toFileInode.addr[7]=0;
    toFileInode.addr[8]=0;
    toFileInode.addr[9]=0;
    toFileInode.addr[10]=0;
    toFileInode.actime[0]=0;
    toFileInode.actime[1]=0;
    toFileInode.modtime[0]=0;
    toFileInode.modtime[1]=0;
    return toFileInode;
}


//checking to see if a file exists in root directory by filename and returns inode of file
int getInodeByFilename(const char* filename,FILE* fileSystem){
    inode_type directoryInode;
    dir_type directoryEntry;
    //read the root directory
    directoryInode = readInode(cwdInodeNumber, fileSystem);  //****changed to read current working directory inode instead of root inode*****

    // Move to the end of root directory file
    fseek(fileSystem, (BLOCK_SIZE * directoryInode.addr[0]), SEEK_SET);
    int records= (BLOCK_SIZE - 2) / sizeof(directoryEntry);
    int i;
    //loop through the root directory to check if filename is present. Return inode of file if found
    for(i=0;i<records; i++){
        fread(&directoryEntry, sizeof(directoryEntry), 1, fileSystem);
        if (strcmp(filename, directoryEntry.file_name) == 0)
            return directoryEntry.inode;
    }
    return -1;
}



//copying a file in the V6 file system to an external file
int cpout(const char* fromFilename, const char* toFilename, FILE* fileSystem){
    int fileInodeNum;
    fileInodeNum = getInodeByFilename(fromFilename, fileSystem); //checks if the fromfile exists in the v6filesystem, if not found give error. 
    if (fileInodeNum == -1){
        printf("\nError! File %s is not found in current directory", fromFilename);
        return -1;
    }
    FILE* writeToFile;
    unsigned char buffer[BLOCK_SIZE];
    int nextBlockNumber, block_number_order, numberOfBlocks,fileSize;
    writeToFile = fopen(toFilename, "w");  //open or creates writetofile (file in external file system)
    block_number_order=0;
    fileSize = getInodeFilesize(fileInodeNum, fileSystem); //passes inode of fromfile to the function. Gets the size of file in v6filesystem
    printf("\nFile size %i", fileSize);
    int number_of_bytes_last_block = fileSize % BLOCK_SIZE;
    unsigned char lastBuffer[number_of_bytes_last_block];
    if (number_of_bytes_last_block ==0){
        numberOfBlocks = fileSize / BLOCK_SIZE;   //calculates number of blocks in fromfile
    }
    else
        numberOfBlocks = fileSize / BLOCK_SIZE + 1;


    while(block_number_order < numberOfBlocks){  //loops through the number of blocks and retrieves each one

        nextBlockNumber = getBlockOfFile(fileInodeNum, block_number_order, fileSystem);  //gets the block number from inode addr array

        fseek(fileSystem, nextBlockNumber * BLOCK_SIZE, SEEK_SET);  //navigates to the block in the v6filesystem, reads contents, write to new file
        if ((block_number_order <(numberOfBlocks - 1)) || (number_of_bytes_last_block == 0)){
            fread(buffer, sizeof(buffer), 1, fileSystem);
            fwrite(buffer, sizeof(buffer), 1, writeToFile);
        }
        else {
            fread(lastBuffer, sizeof(lastBuffer), 1, fileSystem);
            fwrite(lastBuffer, sizeof(lastBuffer), 1, writeToFile);
        }

        block_number_order++;

    }

    fclose(writeToFile);
    return 0;
}

//reading an inode in file system
inode_type readInode(int to_file_inode_num, FILE* fileSystem){
    inode_type toFileInode;
    fseek(fileSystem, (BLOCK_SIZE * 2 + inode_size * (to_file_inode_num - 1)), SEEK_SET); //move to the beginning of given inode number in the file system
    fread(&toFileInode, inode_size, 1, fileSystem);
    return toFileInode;
}

//writing inode to file system
int writeInode(int inodeNum, inode_type inode, FILE* fileSystem){
    fseek(fileSystem, (BLOCK_SIZE * 2 + inode_size * (inodeNum - 1)), SEEK_SET); //move to the beginning of given inode number
    fwrite(&inode, inode_size, 1, fileSystem);  //writing the inode passed as parameter
    return 0;
}


//reading inode addr array and returning a file block number
unsigned short getBlockOfFile(int fileInodeNum, int block_number_order, FILE* fileSystem){
    inode_type fileInode;
    fileInode = readInode(fileInodeNum, fileSystem);
    return (fileInode.addr[block_number_order]);
}


//retrieve the size of a file that a inode points to
unsigned int getInodeFilesize(int to_file_inode_num, FILE* fileSystem){
    inode_type toFileInode;
    unsigned int fileSize;
    unsigned short bit0_15;
    unsigned char bit16_23;
    unsigned short bit24;

    toFileInode = readInode(to_file_inode_num, fileSystem);

    bit24 = LAST(toFileInode.flags, 1);
    bit16_23 = toFileInode.size0;
    bit0_15 = toFileInode.size1;

    fileSize = (bit24 << 24) | ( bit16_23 << 16) | bit0_15;
    return fileSize;
}





//creates a directory using the name passed in
int new_mkdir(const char* filename, FILE* file_system){
	inode_type directory_inode, free_node;
	dir_type directory_entry;
	int to_file_inode_num, to_file_first_block, flag, status_ind, file_node_num;
	
	file_node_num = getInodeByFilename(filename,file_system);  //reads the inode of passed in directory
	if (file_node_num !=-1){				//checking whether directory already exists
		printf("\nDirectory %s already exists. Please choose a different name",filename);
		return -1;
	}
	
	//find a free inode
	int found = 0;
	to_file_inode_num=1;
	while(found==0){
		to_file_inode_num++;
		free_node = readInode(to_file_inode_num,file_system);  //reads all the inodes and finds an unallocated one
		flag = MID(free_node.flags,15,16);  
		if (flag == 0){
			found = 1;
		}
	}
	
	//add new inode to current working directory	
	
	directory_inode = readInode(cwdInodeNumber,file_system);  //read current working directory inode
	
	// Move to the end of current working directory inode
	fseek(file_system,(BLOCK_SIZE*directory_inode.addr[0]+directory_inode.size1),SEEK_SET);
	// create a new directory entry and add it to current working directory inode
	directory_entry.inode = to_file_inode_num; 
	strcpy(directory_entry.file_name, filename);
	fwrite(&directory_entry,16,1,file_system);
	
	
	//Update cw Directory file inode to increment size by one record
	directory_inode.size1+=16;
	writeInode(cwdInodeNumber,directory_inode,file_system);
	
	//getting free block and adding 2 entries (. and ..)
	int freeBlockNum = getFreeBlock(file_system);
	dir_type directory;   
    
	
	//write directory entries to newly retrieved block
	
    directory.inode = to_file_inode_num;
    strcpy(directory.file_name, ".");
   
    fseek(fileSystem, freeBlockNum * BLOCK_SIZE, SEEK_SET);
    fwrite(&directory, 16, 1, file_system);
	
	directory.inode = cwdInodeNumber;
    strcpy(directory.file_name, "..");
    fwrite(&directory, 16, 1, file_system);
	
	//Initialize inode of new directory 
	free_node.flags=0;       
	free_node.flags |=1 <<15; //Setting 1st bit to 1 : inode is allocated
	free_node.flags |=1 <<14; // Set bits 2-3 to 10 : inode is a directory
	free_node.flags |=0 <<13;
	free_node.size1 = 2 * sizeof(dir_type);  //size is 16x2 with 2 entries (. and ..)
    free_node.addr[0] = freeBlockNum;
	
	status_ind = writeInode(to_file_inode_num, free_node, file_system);
	
	return 0;
		
}




//change current working directory to the directory passed into the function
int cd(char* dirName, FILE* fileSystem) {                                             

	char *cwdinput= strstr(cwdPath,"/")+1;
	if(strcmp(cwdinput,dirName)==0){  //if passed in directory same as current directory don't do anything
		return 0;
	}
	
    inode_type curInode = readInode(cwdInodeNumber, fileSystem);  // read the current working directory inode 
    int blockNumber = curInode.addr[0];                              // get 1st block of current working directory inode
    dir_type directory[100];
	fseek(fileSystem, BLOCK_SIZE * blockNumber, SEEK_SET);
	fread(&directory, 16, (curInode.size1 / sizeof(dir_type)), fileSystem); //read contents of the 1st block 
	
	int i;
    for (i = 0; i < curInode.size1 / sizeof(dir_type); i++) {  // loop through contents of the 1st block of cwd inode
		
        if (strcmp(dirName, directory[i].file_name) == 0) {   //checking if passed in directory exists in the block
            inode_type dir = readInode(directory[i].inode,fileSystem);   //read inode of passed in directory
			int flagalloc = MID(dir.flags, 15, 16); //checking if inode is allocated
			int flagdirec1 = MID(dir.flags, 13, 14);  //checking if inode is for a directory
			int flagdirec2 = MID(dir.flags, 14, 15);  //checking if inode is for a directory
			//printf("printing flagalloc value %d", flagalloc);
			//printf("printing flagdirec1 value %d", flagdirec1);
			//printf("printing flagdirec2 value %d", flagdirec2);
            if (flagalloc == 1 && flagdirec1 == 0 && flagdirec2 == 1) {
				
                if (strcmp(dirName, ".") == 0) {  // If . passed in simply stay in current directory
                    return 0;
                } else if (strcmp(dirName, "..") == 0) {   //if .. passed in move back one directory
                    if (cwdInodeNumber == directory[i].inode) {  //reached current working directory
                        continue;
                    }
                    if (directory[i].inode == 1) {   //reached root directory
                        cwdInodeNumber = directory[i].inode;
                        strcpy(cwdPath, "/");
                        continue;
                    }
                    cwdInodeNumber = directory[i].inode;   //update the current working directory inode with inode of passed in directory
                    int lastSlashPosition = strrchr(cwdPath, '/') - cwdPath;  //get last / position of cwd path
					//printf("printing lastSlashPosition %d", lastSlashPosition);
                    char temp[INPUT_SIZE] = " ";
					memset(temp, 0, sizeof (temp));  //clearing buffer
					//printf("printing path %s",cwdPath);
                    strncpy(temp, cwdPath, lastSlashPosition);  // moving back one directory ex: copying /user from /user/ml and setting it as new path
                    strcpy(cwdPath, temp);
					
                } else {
                    if (cwdInodeNumber != 1) {
                        strcat(cwdPath, "/");   //adding / before concatenating directory name to cwd path
                    }
                    cwdInodeNumber = directory[i].inode;
                    strcat(cwdPath, dirName);
					
                }
            } else {
                printf("Entered path is not a directory\n");
            }
            return 0;
        }
    }
	if(strcmp(dirName, "..") != 0){
		//printf("Entered directory does not exist\n");
		return 1;
	}
	
	
}



//removes a file from the v6 file system
int Remove(const char* filename,FILE* file_system){
			
			int inodeNum, file_size, blockNumber; 
			int next_block_number; 
			inode_type file_inode;
			unsigned char bit14; 
			inodeNum = getInodeByFilename(filename,file_system);  //get inode number of file we want to delete
			if (inodeNum ==-1){		//checking if file we want to delete exists
				printf("\nFile %s not found",filename);
				return -1;
			}
			file_inode = readInode(inodeNum, file_system);  //read inode of file
			bit14 = MID(file_inode.flags,14,15);
			if (bit14 == 0){ //checking whether file is plain file and not a directory
					file_size = getInodeFilesize(inodeNum, file_system);
					blockNumber = file_size/BLOCK_SIZE;  //calculating number of blocks stored in inode using file size
					if (file_size%BLOCK_SIZE !=0)
							blockNumber++;  
					
					blockNumber--; //blocks in addr array start from 0
					while(blockNumber>0){  //loop through the blocks and add each to free block list
							next_block_number = file_inode.addr[blockNumber];
							
						add_block_to_free_list(next_block_number, file_system);
						blockNumber--;
					}
			}
			file_inode.flags=0; //unallocate the inode and initialize inode flags
			writeInode(inodeNum, file_inode, file_system);  //write inode back to file system
			removeFileFromDirectory(inodeNum, file_system);  //to remove directory entry from cwd
			fflush(file_system);
			return 0;
}


//removing a directory entry from current working directory
void removeFileFromDirectory(int inodeNum, FILE*  file_system){
	inode_type directoryInode;
	dir_type directoryEntry;
	
	directoryInode = readInode(cwdInodeNumber,file_system);  //read the current working directory inode
	
	fseek(file_system,(BLOCK_SIZE*directoryInode.addr[0]+sizeof(directoryEntry)*2),SEEK_SET); //Move to third record (after . and ..) in inode
	int entries=(BLOCK_SIZE-2)/sizeof(directoryEntry);  //calculate number of entries in inode
	
	int i;
	for(i=0;i<entries; i++){   //loop through the entries and read each one
		fread(&directoryEntry,sizeof(directoryEntry),1,file_system);
		
		if (directoryEntry.inode == inodeNum){  //remove the entry that matches inode number passed in
			fseek(file_system, (-1)*sizeof(directoryEntry), SEEK_CUR); //Go to the beginning of entry stored in current working directory
			directoryEntry.inode = 0;   //emtpyting inode number
			memset(directoryEntry.file_name,0,sizeof(directoryEntry.file_name));  //emptying file name
			fwrite(&directoryEntry,sizeof(directoryEntry),1,file_system);      
			return;
			}
	}
	
	return;
}



// prints current working directory
void printcwd() {  
    printf("%s\n", cwdPath);
}





