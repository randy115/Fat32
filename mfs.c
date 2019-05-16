/*
   Name: James McAllister
   ID: 1001078112

   Name: Randy Bui
   ID: 1001338008
*/

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <stdint.h>

#define WHITESPACE " \t\n" // We want to split our command line up into tokens \
                           // so we need to define what delimits our tokens.   \
                           // In this case  white space                        \
                           // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255 // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5 // Mav shell only supports five arguments
int ultimate; 
//Initialize Info variables
char BS_OEMName[8];
int16_t BPB_BytsPerSec;
int8_t BPB_SecPerClus;
int16_t BPB_RsvdSecCnt;
int8_t BPB_NumFATs;
int16_t BPB_RootEntCnt;
char BS_VolLab[11];
int32_t BPB_FATSz32;
int32_t BPB_RootClus;
int PastRoot = 0;
int32_t RootDirSectors = 0;
int32_t FirstDataSector = 0;
int32_t FirstSectorofCluster = 0;

FILE *fp, *fp2;
struct __attribute__((__packed__)) DirectoryEntry
{
  char DIR_Name[11];
  uint8_t DIR_Attr;
  uint8_t Unused1[8];
  uint16_t DIR_FirstClusterHigh;
  uint8_t Unused2[4];
  uint16_t DIR_FirstClusterLow;
  uint32_t DIR_FileSize;
};

struct DirectoryEntry dir[16];

//finds the starting address of a block of
//data given the sector number

int LBAToOffset(int32_t sector)
{
  return ((sector - 2) * BPB_BytsPerSec) + (BPB_BytsPerSec * BPB_RsvdSecCnt) + (BPB_NumFATs * BPB_FATSz32 * BPB_BytsPerSec);
}

//given a logical block address
//look up into the first FAT and
//return the logical block address
//of the block in the file

int16_t NextLB(uint32_t sector)
{
  uint32_t FATAddress = (BPB_BytsPerSec * BPB_RsvdSecCnt) + (sector * 4);
  int16_t val;
  fseek(fp, FATAddress, SEEK_SET);
  fread(&val, 2, 1, fp);
  return val;
}

//function to see if user input matches
//with the name in the fat32
//First paramater is the name from the fat 32 image
//Second paramter is the name taken from the user input
//Will return 1 or 0 depending on if it is true or not
int CheckMatch(char IMG_Name[], char USR_Input[])
{
  int matches = 0;
  int match = 0;
  if (USR_Input[0] == '.')
  {
    int a = 0;
    for (a = 0; a < strlen(USR_Input); a++)
    {
      if (USR_Input[a] == '.' && IMG_Name[a] == '.')
      {
        matches++;
      }
    }
    if (matches == strlen(USR_Input))
    {
      return 1;
    }
    else
    {
      return 0;
    }
  }
  char expanded_name[12];
  memset(expanded_name, ' ', 12);

  char *token = strtok(USR_Input, ".");

  strncpy(expanded_name, token, strlen(token));

  token = strtok(NULL, ".");

  if (token)
  {
    strncpy((char *)(expanded_name + 8), token, strlen(token));
  }

  expanded_name[11] = '\0';

  int i;
  for (i = 0; i < 11; i++)
  {
    expanded_name[i] = toupper(expanded_name[i]);
  }

  if (strncmp(expanded_name, IMG_Name, 11) == 0)
  {
    match = 1;
  }
  else
  {
    match = 0;
  }

  return match;
}

int main()
{
  //byt used for checking different conditions
  unsigned int byt = 0x00;
  //Simple signal to let use for a check inside of different conditionals
  int signal = 0;
  //Code below is all
  char *cmd_str = (char *)malloc(MAX_COMMAND_SIZE);

  while (1)
  {
    // Print out the msf prompt
    printf("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while (!fgets(cmd_str, MAX_COMMAND_SIZE, stdin))
      ;

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;

    char *working_str = strdup(cmd_str);

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while (((arg_ptr = strsep(&working_str, WHITESPACE)) != NULL) &&
           (token_count < MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup(arg_ptr, MAX_COMMAND_SIZE);
      if (strlen(token[token_count]) == 0)
      {
        token[token_count] = NULL;
      }
      token_count++;
    }

    //Collecting information on our fat 32 image for when client enters info
    if (strcmp(token[0], "info") == 0)
    {
      if (signal == 0)
      {
        printf("Error: File system image must be opened first\n");
      }
      else
      {
        //Output the desired information on the fat32
        printf("BPB_BytsPerSec: %d %x\n", BPB_BytsPerSec, BPB_BytsPerSec);
        printf("BPB_SecPerClus: %d %x\n", BPB_SecPerClus, BPB_SecPerClus);
        printf("BPB_RsvdSecCnt: %d %x\n", BPB_RsvdSecCnt, BPB_RsvdSecCnt);
        printf("BPB_NumFATs: %d %x\n", BPB_NumFATs, BPB_NumFATs);
        printf("BPB_FATSz32: %d %x\n", BPB_FATSz32, BPB_FATSz32);
      }
    }
    //read function output the number of bytes specified outprinting byte by byte per the user from offset to offset plus the number of bytes
    //Next logical block is used here
    else if (strcmp(token[0], "read") == 0)
    {
      if (signal == 0)
      {
        printf("Error: File system image must be opened first\n");
      }
      else
      {
        int i;

        //copy variable is for holding the name from the structure

        char copy[104];

        //for loop iterates through the structure and checks
        //if there is a match for the name of the structure
        //we are looking for

        for (i = 0; i < 16; i++)
        {
          strcpy(copy, token[1]);
          if (CheckMatch(dir[i].DIR_Name, copy) == 1)
          {
            uint8_t show = 0x00;
            int position = atoi(token[2]);     //starting position in file
            float position1 = (float)position; //copy of starting position
            int bytes = atoi(token[3]);        //number of bytes we are supposed read
            int j;
            char b;

            //offset variable holds the starting address of the
            //block we are looking for

            int offset = LBAToOffset(dir[i].DIR_FirstClusterLow);

            //block is a copy of the low cluster number for the structure
            //we are looking for

            int16_t block = dir[i].DIR_FirstClusterLow;

            //for loop starts at the address that we get from the LBAToOffset function
            //and reads and write byte by byte

            for (j = 0; j < bytes; j++)
            {

              //while we are on the block if we exceed reach the end of the block(sector)
              //we go on into this if statement

              if (position >= 512)
              {
                //copy of our position
                position1 = (float)position;

                //while loop condition divides the position by 512 to see how many times
                //we need to get a new block
                //we divide the copy of our position by 512
                //because when our position reaches over 512
                //we need find the next logical block
                //it may be the case sometimes we need to get multiple
                //logical blocks
                //position1 is to keep track of how many blocks we need
                while (position1 / 512 > 1)
                {

                  //puts beginning address into NextLB to get the next logical block
                  block = NextLB(block);

                  //get the beginning address of the next logical block
                  offset = LBAToOffset(block);

                  //divide copy of position by 512 to update
                  //that we are at that logical block
                  position1 /= 512;

                  //position is subtracted by 512 to
                  //give new current position in the new
                  //logical block
                  position = position - 512;
                }
              }
              //offset+position is the starting address of the
              //new block plus the current position we are at to
              //find exact spot in block
              fseek(fp, offset + position, SEEK_SET);
              fread(&show, 1, 1, fp);
              printf("%X", show);
              //increment position by 1 to get next position in sector
              //and to read the next byte
              position++;
            }
          }
        }
        printf("\n");
      }
    }
    else if (strcmp(token[0], "cd") == 0)
    {
      if (signal == 0)
      {
        printf("Error: File system image must be opened first\n");
      }
      else
      {
        //set fp to our current working root
        fseek(fp, PastRoot, SEEK_SET);
        //create a tokenized string
        char *tokenStruct = strtok(token[1], "/");
        //while the string can still be tokenized traverse the tokenstruct
        //Each continuation is an additional cd requested by the user
        while (tokenStruct != NULL)
        {
          //File Name Conversion Mainly copied from our check match function to reuse code
          char expanded_name[12];
          memset(expanded_name, ' ', 11);
          strncpy(expanded_name, tokenStruct, strlen(tokenStruct));
          expanded_name[11] = '\0';

          int j;
          for (j = 0; j < 11; j++)
          {
            expanded_name[j] = toupper(expanded_name[j]);
          }
          tokenStruct = strtok(NULL, "/");
          //End file name conversion and tokenization

          //CD functions
          //seeks to our current root directory that we are in
          fseek(fp, PastRoot, SEEK_SET);
          int i, foundIt = 0;
          for (i = 0; i < 16; i++)
          {
            //reread structure to ensure we have the right stuff
            fread(&dir[i], 32, 1, fp);
            //Check that the file can be replaced based off of our conditions
            if (dir[i].DIR_Name[0] > 0)
            {
              if (dir[i].DIR_Attr == 0x10)
              {
                char File[11];
                memset(File, ' ', 11);
                strncpy(File, dir[i].DIR_Name, 11);

                if (strncmp(expanded_name, File, 11) == 0)
                {
                  foundIt = 0;
                  if (dir[i].DIR_FirstClusterLow == 0){
                    PastRoot = RootDirSectors;
                  }
                  else{
                    PastRoot = LBAToOffset(dir[i].DIR_FirstClusterLow);
                  }
                  break;
                }
              }
            }
          }
        }
      }
      int i;
      //reset the dir structure to ensure it has the right information for other functions
      fseek(fp, PastRoot, SEEK_SET);
      for (i = 0; i < 16; i++)
        fread(&dir[i], 1, 32, fp);
    }
    else if (strcmp(token[0], "ls") == 0)
    {
      if (signal == 0)
      {
        printf("Error: File system image must be opened first\n");
      }
      else
      {
        //for loop goes through the structure array
        //and prints out the Dir_name and print
        //it out on screen. Also checks the first byte
        //in Dir name if it is a deleted file
        int i, j;
        for (i = 0; i < 16; i++)
        {
          unsigned int byte;
          char nameArray[12];
          strcpy(nameArray, dir[i].DIR_Name);
          nameArray[11] = '\0';
          byte = (unsigned char)dir[i].DIR_Name[0];
          if (byte != 0xE5 && (dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 
          || dir[i].DIR_Attr == 0x20))
            printf("%s\n", nameArray);
        }
      }
    }
    else if (strcmp(token[0], "put") == 0)
    {
      if (signal == 0)
      {
        printf("Error: File system image must be opened first\n");
      }
      else
      {
        //Open file to read into the fat32.img
        fp2 = fopen(token[1], "r+");
        struct stat buf;
        stat(token[1], &buf);
        //get the file size using st_size
        int FileSize = buf.st_size;
        char buffer[FileSize];
        //read in the file to our buffer
        for (int i = 0; i < FileSize; i++)
          fread(&buffer[i], 1, 1, fp2);
        //File Name Conversion
        char expanded_name[12];
        memset(expanded_name, ' ', 12);
        char *tokenStruct = strtok(token[1], ".");
        strncpy(expanded_name, tokenStruct, strlen(tokenStruct));
        tokenStruct = strtok(NULL, ".");
        if (tokenStruct)
        {
          strncpy((char *)(expanded_name + 8), tokenStruct, strlen(tokenStruct));
        }

        expanded_name[11] = '\0';

        int j;
        for (j = 0; j < 11; j++)
        {
          expanded_name[j] = toupper(expanded_name[j]);
        }
        //End of file name conversion
        int x;
        unsigned c = 0x00;
        //Run through dir structure
        for (x = 0; x < 16; x++)
        {
          c = (unsigned char)dir[x].DIR_Name[0];
          if (c == 0xE5 || c == 0x00)
          {
            int y;
            //update the structure with new information
            for (y = 0; y < 11; y++)
              dir[x].DIR_Name[y] = expanded_name[y];
            dir[x].DIR_Attr = 0x20;
            dir[x].DIR_FileSize = FileSize;
            //compare bytes
            int32_t compare = 0x00;
            int32_t compare1 = 0x00;
            uint8_t holder = 0x00;
            int file_parser = 0;
            //This is our flag
            int _bool = 0;
            //next logical block
            int32_t nxt_lb_e = 0;
            int offset_to_new = 0;
            //start of fat
            int s_of_fat = BPB_RsvdSecCnt * BPB_BytsPerSec;
            //copy of fat start
            int start_pt_fat = s_of_fat;
            //second copy2 of start
            int future_ptr = start_pt_fat;
            uint16_t low;
            //end point of fat
            int stop = BPB_NumFATs * BPB_BytsPerSec * BPB_FATSz32;
            //traversing the whole fat
            while (s_of_fat <= stop)
            {
              fseek(fp, s_of_fat, SEEK_SET);
              fread(&compare, 1, 4, fp);
              //if there is a empty spot start looking
              //for another 0 in fat
              if (compare == 0x00)
              {
                future_ptr = s_of_fat + 4;
                nxt_lb_e = future_ptr - start_pt_fat;
                fseek(fp, future_ptr, SEEK_SET);
                fread(&compare1, 1, 4, fp);
                //if not a valid spot in fat32 then move
                //forward
                while (nxt_lb_e % 4 != 0 || compare1 != 0x00)
                {
                  future_ptr += 4;
                  nxt_lb_e = future_ptr - start_pt_fat;
                  fseek(fp, future_ptr, SEEK_SET);
                  fread(&compare1, 1, 4, fp);
                }
                nxt_lb_e /= 4;
                fseek(fp, s_of_fat, SEEK_SET);
                fwrite(&nxt_lb_e, 1, 4, fp);
                fseek(fp, s_of_fat, SEEK_SET);
                fread(&low, 1, 2, fp);
                //store first logical block into structure
                if (_bool == 0)
                {
                  dir[x].DIR_FirstClusterLow = low;
                  _bool++;
                }
                //new starting address of logical block
                offset_to_new = LBAToOffset(low);
                int z;
                //reading and writing byte by byte until we
                //use the entire logical block
                for (z = 0; z < 512; z++)
                {
                  fseek(fp2, file_parser, SEEK_SET);
                  fread(&holder, 1, 1, fp2);
                  fseek(fp, offset_to_new + z, SEEK_SET);
                  fwrite(&holder, 1, 1, fp);
                  file_parser++;
                }
                FileSize -= 512;
                //still need to keep writing 
                //because there is something in
                //file
                if (FileSize >0)
                {
                  s_of_fat = future_ptr;
                  continue;
                }
                //file has been written into fat32
                else
                {
                  _bool = -1;
                  fseek(fp, future_ptr, SEEK_SET);
                  fwrite(&_bool, 1, 4, fp);
                  fseek(fp, PastRoot + (x * 32), SEEK_SET);
                  fwrite(&dir[x], 32, 1, fp);
                  break;
                }
              }
              //traverse fat32 by 32 bits 
              fseek(fp, s_of_fat, SEEK_SET);
              fread(&compare, 1, 4, fp);
              s_of_fat += 4;
            }
            break;
          }

        }
      }
    }
    else if (strcmp(token[0], "get") == 0)
    {
      if (signal == 0)
      {
        printf("Error: File system image must be opened first\n");
      }
      else
      {
        int i, j, foundIt=0;
        for (i = 0; i < 16; i++)
        {
          char input[7];
          char output;
          strcpy(input, token[1]);
          //use checkmatch function to find
          //matching user input in the dir
          //structure array
          if (CheckMatch(dir[i].DIR_Name, input))
          {
            foundIt=1;
            //byt used to check the first byte
            //in the directory structure name
            //to see what kind of file it is
            //and accept if it is
            //read only and file is not deleted
            //or it is not a directory
            byt = (unsigned char)dir[i].DIR_Name[0];
            if ((dir[i].DIR_Name[0] != 0xE2 || byt != 0xE5 ) &&
                ( dir[i].DIR_Attr == 0x20 || dir[i].DIR_Attr == 0x010 || dir[i].DIR_Attr == 0x01))
            {
              j = 0;

              //file size of the directory we are looking at
              int file_size = dir[i].DIR_FileSize;

              //opening a file and write to it
              fp2 = fopen(token[1], "w");

              //currentSpot is the position we are at
              //in the file
              int currentSpot = 0;

              //nextBlock is a copy of our LBA
              int16_t nextBlock = dir[i].DIR_FirstClusterLow;

              //buffer for sending 2 bytes at a time
              uint8_t getShow = 0x00;

              //offset is the starting address of the block
              //we are looking at
              int offset = LBAToOffset(dir[i].DIR_FirstClusterLow);

              //starting from 0 and writing to the file
              //until it reaches the end of the file size
              for (j = 0; j < file_size; j++)
              {
                //reaching the end of LBA
                //and calculating the new LBA
                //to start writing at
                if (currentSpot == 512)
                {
                  nextBlock = NextLB(nextBlock);
                  if(nextBlock =-1)
                    break;
                  offset = LBAToOffset(nextBlock);
                  currentSpot = 0;
                }
                //move the fp head to point to the
                //starting position/new calculated
                //position in the file
                fseek(fp, (offset + currentSpot), SEEK_SET);
                fread(&getShow, 1, 1, fp);
                fwrite(&getShow, sizeof(getShow), 1, fp2);
                currentSpot++;
                foundIt = 1;
              }
              fclose(fp2);
              break;
            }
          }
        }
        if (foundIt != 1)
          printf("Error: File not found \n ");
      }
    }
    else if (strcmp(token[0], "stat") == 0)
    {
      if (signal == 0)
      {
        printf("Error: File system image must be opened first\n");
      }
      else
      {
        int i = 0, foundIt = 0;
        while (i < 16 && foundIt == 0)
        {
          char input[100];
          strcpy(input, token[1]);
          //Find the file we want to stat then give the user the information requested
          if (CheckMatch(dir[i].DIR_Name, input) == 1)
          {
            printf("Attributes: %d \nStarting Cluster Number: %d\n", dir[i].DIR_Attr, dir[i].DIR_FirstClusterLow);
            foundIt = 1;
          }
          i++;
        }
        if (foundIt != 1)
          printf("Error: File not found.\n");
      }
    }
    else if (strcmp(token[0], "open") == 0)
    {
      if (signal == 1)
      {
        printf("Error: File system image already opened\n");
      }
      else
      {
        int i, j;
        fp = fopen(token[1], "r+");
        if (fp == NULL)
        {
          printf("Error: Image not found\n");
        }
        else
        {
          //BytsPerSec reading in
          fseek(fp, 11, SEEK_SET);
          fread(&BPB_BytsPerSec, 1, 2, fp);
          //SecPerClus reading in
          fseek(fp, 13, SEEK_SET);
          fread(&BPB_SecPerClus, 1, 1, fp);
          //RsvdSecCnt reading in
          fseek(fp, 14, SEEK_SET);
          fread(&BPB_RsvdSecCnt, 1, 2, fp);
          //NumFats reading in
          fseek(fp, 16, SEEK_SET);
          fread(&BPB_NumFATs, 1, 1, fp);
          //FATSz32 reading in
          fseek(fp, 36, SEEK_SET);
          fread(&BPB_FATSz32, 1, 4, fp);
          signal = 1;
          RootDirSectors = (BPB_NumFATs * BPB_FATSz32 * BPB_BytsPerSec) + (BPB_RsvdSecCnt * BPB_BytsPerSec);
          PastRoot = RootDirSectors;
          i = 0;
          while (i < 16)
          {
            fseek(fp, RootDirSectors + (i * 32), SEEK_SET);
            fread(&dir[i], 1, 32, fp);
            i++;
          }
        }
      }
    }
    //close out the file
    else if (strcmp(token[0], "close") == 0)
    {
      if (signal == 0)
      {
        printf("Error: File system not open\n");
      }
      else
      {
        fclose(fp);
        printf("Closed succesfully\n");
      }
      signal = 0;
    }
    else
    {
      printf("%s", token[0]);
    }
    free(working_root);
  }
  return 0;
}
