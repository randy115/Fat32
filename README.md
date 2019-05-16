# Fat32

Created a shell like environment to read and write to a fat32


DOWNLOAD THE IMAGE AND BEFORE DOING ANYTHING COMMANDS YOU MUST TYPE "open fat32.img"

commands in the code 

1). info - gets the system information of the fat32 and prints it out.
    
2). stat - print the attributes and starting cluster number of the file or directory name. 

3). get <filename> - gets the file from fat32 and places it in your current working directory.
  
4). put <filename> - gets file from working directory and places it into fat32.
  
5). cd <directory> - changes current working directory to given directory.
  
6). ls - lists current directory contents.

7). read <filename> <position> <number of bytes> - reads file at given position and outputs number of bytes requested. 
  
8). close - closes what image you have open so you can load up another image.   
    
