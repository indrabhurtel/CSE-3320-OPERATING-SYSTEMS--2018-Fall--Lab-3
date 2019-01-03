/*

	Name:	Indra Bhurtel
	ID:	1001542825

*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>

#define WHITESPACE " \t\n"
#define MAX_COMMAND_SIZE 266

#define MAX_NUM_ARGUMENTS 6


struct __attribute__((__packed__)) DirectoryEntry {
  char DIR_Name[11];
  uint8_t DIR_Attr;
  uint8_t Unused1[8];
  uint16_t DIR_FirstClusterHigh;
  uint8_t Unused2[4];
  uint16_t DIR_FirstClusterLow;
  uint32_t DIR_FileSize;
};


struct FAT_32 {
  char BS_OEMNAME[8];
  short BPB_BytsPerSec;
  unsigned BPB_SecPerClus;
  short BPB_RsvdSecCnt;
  unsigned BPB_NumFATS;
  short BPB_RootEntCnt;
  char BS_VolLab[11];
  int BPB_FATSz32;
  int BPB_RootClus;

  int RootDirSectors;
  int FirstDataSector;
  int FirstSectorofCluster;


  int root_offset;
  int TotalFATSize;
  int bytesPerCluster;
};


int LBAToOffset(int sector, struct FAT_32 *fat);
unsigned NextLB(int sector, FILE *file, struct FAT_32 *fat);
char* formatFileString(char* userInput);
int fileDoesExist(struct DirectoryEntry *dir, char* filename);


FILE* open(char *fileName, struct FAT_32 *img, struct DirectoryEntry *dir);
void ls(FILE *file, struct FAT_32 *fat, struct DirectoryEntry *dir);
void stat(struct DirectoryEntry *dir, FILE *file, char* userFileName);
void get(FILE *file, struct DirectoryEntry *dir, struct FAT_32 *fat, char* userCleanName, char* userOriginalName);
void readFile(FILE *file, struct FAT_32 *fat, struct DirectoryEntry dir, int offset, int numOfBytes);
void readDirectory(int cluster, FILE *file, struct DirectoryEntry *dir, struct FAT_32 *fat);



int main()
{
  int currentOffset = 0;
  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
  char * currentFile = NULL;
  int hasFileClosed = 0;

  FILE *IMG = NULL;

  struct DirectoryEntry *dir = (struct DirectoryEntry *)malloc(sizeof(struct DirectoryEntry) * 16);


  struct FAT_32 *fat = (struct FAT_32 *)malloc(sizeof(struct FAT_32));

  while( 1 )
  {


    printf ("mfs> ");


    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );


    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;


    char *arg_ptr;

    char *working_str  = strdup( cmd_str );


    char *working_root = working_str;

        while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) &&
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }


    if ( token[0] == NULL){

    }

    else if ( strcmp(token[0], "open") == 0){
      if(currentFile == NULL && IMG == NULL){
        if(token[1] != NULL){
          IMG = open(token[1], fat, dir);

          if(IMG != NULL){
            hasFileClosed = 0;
            currentFile = (char *)malloc(sizeof(token[1]));
            strcpy(currentFile, token[1]);
          }
        }
      }else{
        if(strcmp(currentFile, token[1]) == 0){
          printf("Error: File system image is already open.\n");
        }
      }
    }

    else if ( strcmp(token[0], "close") == 0){

      if(hasFileClosed != 1){
        if(IMG != NULL){
          int res = fclose(IMG);

          if(res == 0){

            currentFile = NULL;
            IMG = NULL;
            hasFileClosed = 1;

          }
        }else{
          printf("%s\n", "Error: File system not open.");
        }
      }else{
        printf("%s\n", "Error: File system image must be opened first.");
      }
    }
    else if (strcmp(token[0], "info") == 0){
      if(IMG != NULL){
        if(token[1] != NULL){
          printf("BPB_BytsPerSec:%6d %6x\n", fat->BPB_BytsPerSec, fat->BPB_BytsPerSec);
          printf("BPB_SecPerClus:%6d %6x\n", fat->BPB_SecPerClus, fat->BPB_SecPerClus);
          printf("BPB_RsvdSecCnt:%6d %6x\n", fat->BPB_RsvdSecCnt, fat->BPB_RsvdSecCnt);
          printf("BPB_NumFATS:%9d %6x\n", fat->BPB_NumFATS, fat->BPB_NumFATS);
          printf("BPB_FATSz32:%9d %6x\n", fat->BPB_FATSz32, fat->BPB_FATSz32);
        }
      }
    }
    else if (strcmp(token[0], "stat") == 0){
      if(IMG != NULL){
          if(token[1] != 0){
          char *clean = NULL;
          clean = formatFileString(token[1]);
          stat(dir, IMG, clean);
        }
      }
    }
    else if (strcmp(token[0], "get") == 0){
     if(IMG != NULL){
        if(token[1] != NULL){
          char *clean = NULL;
          clean = formatFileString(token[1]);
          get(IMG, dir, fat, clean, token[1]);
        }
      }
    }

    else if (strcmp(token[0], "cd") == 0) {
      char * cleanFileName = NULL;
      int fileIndex;

      char * del = (char *)"/";
      char buffer[strlen(token[1])];
      char * fileToken;
      char * fileTokens[50];


      strcpy(buffer, token[1]);

      int i = 0;
      int maxTokenCount = 0;


      fileToken = strtok ( buffer, del);
      while (fileToken != NULL) {
        fileTokens[maxTokenCount] = (char *)malloc(sizeof(strlen(fileToken)));
        strcpy(fileTokens[maxTokenCount], fileToken);
        fileToken = strtok(NULL, del);
        maxTokenCount++;
      }


      if (IMG != NULL) {

        for ( i = 0; i < maxTokenCount; i++ ) {

          cleanFileName = formatFileString(fileTokens[i]);

          if ((fileIndex = fileDoesExist(dir, cleanFileName)) != -1) {
                        if (dir[fileIndex].DIR_Attr == 0x10) {
              readDirectory(dir[fileIndex].DIR_FirstClusterLow, IMG, dir, fat);
              currentOffset = dir[fileIndex].DIR_FirstClusterLow;

            } else if (dir[fileIndex].DIR_Name[0] == '.') {
              readDirectory(dir[fileIndex].DIR_FirstClusterLow, IMG, dir, fat);
              currentOffset = dir[fileIndex].DIR_FirstClusterLow;

            }
          }
        }
      }
    }
    else if (strcmp(token[0], "ls") == 0) {
      if(IMG != NULL){
      int fileIndex;
      int directoryDepth = 0;
      char * cleanFileName = NULL;

      char * fileToken;
      char * fileTokens[50];
      char * del = (char *)"/";
      int i = 0;
      int maxTokenCount = 0;
      int  dotCounts= 0;
            if (token[1] != NULL ) {
        char buffer[strlen(token[1])];
        strcpy(buffer, token[1]);
        fileToken = strtok ( buffer, del);


        while (fileToken != NULL) {

          fileTokens[maxTokenCount] = (char *)malloc(sizeof(strlen(fileToken)));
          strcpy(fileTokens[maxTokenCount], fileToken);

          fileToken = strtok(NULL, del);

          if (strcmp(fileTokens[maxTokenCount], "..") == 0) {
            directoryDepth--;
          } else if  (strcmp(fileTokens[maxTokenCount], ".") == 0) {
            dotCounts++;
          }
          else {
            directoryDepth++;
          }

          maxTokenCount++;
        }
      }

        if ( maxTokenCount != 0 ) {

          for ( i = 0; i < maxTokenCount; i++ ) {
          cleanFileName = formatFileString(fileTokens[i]);

            if ((fileIndex = fileDoesExist(dir, cleanFileName)) != -1) {
              if (dir[fileIndex].DIR_Attr == 0x10) {
                readDirectory(dir[fileIndex].DIR_FirstClusterLow, IMG, dir, fat);
              } else if (dir[fileIndex].DIR_Name[0] == '.') {
                readDirectory(dir[fileIndex].DIR_FirstClusterLow, IMG, dir, fat);
              }
            }
          }

            ls(IMG, fat, dir);


            for (i = 0; i < directoryDepth; i++){
              readDirectory(dir[1].DIR_FirstClusterLow, IMG, dir, fat);
            }

          }else {
            ls(IMG, fat, dir);
          }
      }
    }
    else if (strcmp(token[0], "read") == 0) {
      int offset;
      int numOfBytes;
      char *cleanFileName = NULL;

      if(token[1] != NULL || token[2] != NULL || token[3] != NULL){
        cleanFileName = formatFileString(token[1]);
        offset = atoi(token[2]);
        numOfBytes = atoi(token[3]);
        int fileIndex;
        if(IMG != NULL){
          if((fileIndex = fileDoesExist(dir, cleanFileName)) != -1){
                        readFile(IMG, fat, dir[fileIndex], offset, numOfBytes);
          }
        }
      }
    }
    else if (strcmp(token[0], "volume") == 0) {
      if(IMG != NULL){
        // Volume
        char * volume;
        volume = (char *)malloc(sizeof(11));

        // memset(volume, 0, 12);
        strcpy(volume, fat->BS_VolLab);
        if( volume != NULL){
          printf("Volume is :%s\n", volume);
        }else{
          printf("%s\n", " volume name not found.");
        }
      }
    }

    free( working_root );
  }
  return 0;
}


FILE* open(char *fileName, struct FAT_32 *img, struct DirectoryEntry *dir){
  FILE *file;
  if(!(file=fopen(fileName, "r"))){
    printf(" File system image not found.\n");
    return 0;
  }

  fseek(file, 11, SEEK_SET);

  fread(&img->BPB_BytsPerSec, 2, 1, file);


  fseek(file, 13, SEEK_SET);

  fread(&img->BPB_SecPerClus, 1, 1, file);


  fseek(file, 14, SEEK_SET);

  fread(&img->BPB_RsvdSecCnt, 2, 1, file);


  fseek(file, 16, SEEK_SET);

  fread(&img->BPB_NumFATS, 1, 1, file);


  fseek(file, 36, SEEK_SET);

  fread(&img->BPB_FATSz32, 4, 1, file);


  fseek(file, 3, SEEK_SET);
  fread(&img->BS_OEMNAME, 8, 1, file);


  fseek(file, 71, SEEK_SET);
  fread(img->BS_VolLab, 11, 1, file);


  fseek(file, 17, SEEK_SET);
  fread(&img->BPB_RootEntCnt, 2, 1, file);


  fseek(file, 44, SEEK_SET);
  fread(&img->BPB_RootEntCnt, 2, 1, file);


  img->RootDirSectors = 0;
  img->FirstDataSector = 0;
  img->FirstSectorofCluster =  0;

  img->root_offset = (img->BPB_NumFATS * img->BPB_FATSz32 * img->BPB_BytsPerSec) + (img->BPB_RsvdSecCnt * img->BPB_BytsPerSec);
  img->bytesPerCluster = (img->BPB_SecPerClus * img->BPB_BytsPerSec);


  fseek(file, img->root_offset, SEEK_SET);
  int i = 0;

  for(i=0; i<16; i++){
    fread(&dir[i], 32, 1, file);
  }


  return file;
}


void readFile(FILE *file, struct FAT_32 *fat, struct DirectoryEntry dir, int offset, int numOfBytes){
  uint8_t value;
  int userOffset = offset;
  int cluster = dir.DIR_FirstClusterLow;
  int fileOffset = LBAToOffset(cluster, fat);
  fseek(file, fileOffset, SEEK_SET);



  while(userOffset > fat->BPB_BytsPerSec){
     cluster = NextLB(cluster, file, fat);
     userOffset -= fat->BPB_BytsPerSec;
  }

  fileOffset = LBAToOffset(cluster, fat);
  fseek(file, fileOffset + userOffset, SEEK_SET);
  int i = 0;
  for(i = 0; i < numOfBytes; i++){
    fread(&value, 1, 1, file);
    printf("%d", value);
  }
  printf("\n");
}


void ls(FILE *file, struct FAT_32 *fat, struct DirectoryEntry *dir){

  int i = 0;
  signed char firstByteOfDIRName=  dir[2].DIR_Name[0];

  for(i=0; i < 16; i++){


    signed char firstByteOfDIRName=  dir[i].DIR_Name[0];
    if ( firstByteOfDIRName == (char)0xe5 ) {
      int j = 1;
    }
    else if (dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20 || dir[i].DIR_Attr == 0x01 ||  dir[i].DIR_Name[0] == '.')  {

      char fileName[12];
      memset(fileName, 0, 12);
      strncpy(fileName, dir[i].DIR_Name, 11);
      printf("%2s\n", fileName);
    }
  }
}


void stat(struct DirectoryEntry *dir, FILE *file, char* userFileName){
  int fileIndex;
  if((fileIndex = fileDoesExist(dir, userFileName)) != -1){

    printf("File Size: %d\n", dir[fileIndex].DIR_FileSize);
    printf("First Cluster with Low: %d\n",  dir[fileIndex].DIR_FirstClusterLow);
    printf("DIR_ATTR: %d\n", dir[fileIndex].DIR_Attr);
    printf("First Cluster with high: %d\n", dir[fileIndex].DIR_FirstClusterHigh);

  } else {
    printf("%s\n", "File not found.");
  }
}


void get(FILE *file, struct DirectoryEntry *dir, struct FAT_32 *fat, char* userCleanName, char* userOriginalName){

  int fileIndex;

  if((fileIndex = fileDoesExist(dir, userCleanName)) != -1){

    FILE *localFile;
    int nextCluster;
    localFile = fopen(userOriginalName, "w");
    int size = dir[fileIndex].DIR_FileSize;
    int cluster = dir[fileIndex].DIR_FirstClusterLow;
    int offset = LBAToOffset(cluster, fat);

    fseek(file, offset, SEEK_SET);
    nextCluster = cluster;
    uint8_t value[512];

    while(size > 512){
      fread(&value, 512, 1, file);
      fwrite(&value, 512, 1, localFile);
      size -= 512;
      nextCluster = NextLB(nextCluster, file, fat);
      fseek(file, LBAToOffset(nextCluster, fat), SEEK_SET);
    }


    fread(&value, size, 1, file);
    fwrite(&value, size, 1, localFile);
    fclose(localFile);

  }else{
    printf("%s\n", "File not found.");
  }
}


int LBAToOffset(int sector, struct FAT_32 *fat){
  return ((sector - 2) * fat->BPB_BytsPerSec) + (fat->BPB_BytsPerSec * fat->BPB_RsvdSecCnt) + (fat->BPB_NumFATS * fat->BPB_FATSz32 * fat->BPB_BytsPerSec);
}


unsigned NextLB(int sector, FILE *file, struct FAT_32 *fat){
  int FATAddress = (fat->BPB_BytsPerSec * fat->BPB_RsvdSecCnt) + (sector * 4);
  unsigned val;
  fseek(file, FATAddress, SEEK_SET);
  fread(&val, 2, 1, file);
  return val;
}



char* formatFileString(char* userInput) {
  char copyOfUser[strlen(userInput)];

  strcpy(copyOfUser, userInput);

  char *filename;
  char *extension;

  char *token;
  char *toFATStr;

  toFATStr = (char*)malloc(sizeof(char) * 11);

   char * del = (char *) ".\n";

  int numOfSpaces;
  int numOfExtSpaces = 3;

     if ( copyOfUser[0] == '.' && copyOfUser[1] == '.'){
    toFATStr = (char *) "..         ";

  } else if ( copyOfUser[0] == '.' ) {
    toFATStr = (char *) ".          ";

  } else {


    token = strtok(copyOfUser,del);
    filename = (char *)malloc(sizeof(token));
    strcpy(filename, token);


    int lenOfExtension;

    if((token = strtok(NULL, del)) != NULL) {
      extension = (char *)malloc(sizeof(token));
      strcpy(extension, token);
      lenOfExtension = strlen(extension);
      numOfExtSpaces = 3 - lenOfExtension;
    } else {

      extension = (char *)malloc(sizeof(0));
      extension = (char *) "";
      numOfExtSpaces = 3;
    }


    int lenOfFilename = strlen(filename);

    numOfSpaces = 8 - lenOfFilename;


    strcat(toFATStr, filename);

    if(numOfSpaces > 0){
      int i = 0;

      for(i = 0; i < numOfSpaces; i++){
        strcat(toFATStr, " ");
      }
    }


    strcat(toFATStr, extension);

    if(numOfExtSpaces > 0){
      int i = 0;

      for(i = 0; i < numOfExtSpaces; i++){
        strcat(toFATStr, " ");
      }
    }

    int i = 0;
    for(i = 0; i < strlen(toFATStr); i++){
      toFATStr[i] = toupper(toFATStr[i]);
    }

    return toFATStr;
  }
  return toFATStr;
}


int fileDoesExist(struct DirectoryEntry *dir, char* filename){
  int i = 0;
  for(i=0; i < 16; i++){
    if(dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20 || dir[i].DIR_Name[0] == '.'){

      char dirFileName[12];
      memset(dirFileName, 0, 12);
      strncpy(dirFileName, dir[i].DIR_Name, 11);
      if(strcasecmp(dirFileName, filename) == 0){
        return i;
      }
    }
  }
  return -1;
}


void readDirectory(int cluster, FILE *file, struct DirectoryEntry *dir, struct FAT_32 *fat) {
  int offset;
  if (cluster == 0) {
    offset = fat->root_offset;
  } else {
    offset = LBAToOffset(cluster, fat);
  }
  fseek(file, offset, SEEK_SET);
  int i;

  for(i=0; i<16; i++){
    fread(&dir[i], 32, 1, file);

  }
}
