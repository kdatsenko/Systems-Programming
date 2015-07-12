#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define HEADER_SIZE 44
#define FILE_READ 0
#define FILE_WRITE 1

/* Checks if correct number of items were read or written into a file. */
void file_action(int result, int expected, int action_type);

int main(int argc, char *argv[]) {

    FILE *fp; //input file
    FILE *fo; //output file
    char header_buffer[HEADER_SIZE]; //buffer for the header of the wav file
    long fileSize; //size of input wav file (samples)
    short buff[2]; //buffer for input vocals wav file

    if(argc != 3) { //checks if number of arguments are correct
        fprintf(stderr, "Usage: %s <filename> <filename>\n", argv[0]);
        exit(1);
    }

    //checks if input file and ouput file can be opened successfully.
    if((fp = fopen(argv[1], "rb")) == NULL) {
        perror(argv[1]);
        exit(1);
    }
    if((fo = fopen(argv[2], "w")) == NULL) {
        perror(argv[optind]);
        exit(1);
    }
    
    //gets the size of the file in order to 
    //calculate the number of samples of the file.
    if (fseek(fp, 0, SEEK_END) != 0){
        fprintf(stderr, "An error occurred while setting position of the file stream of\n");
        exit(1);
    }
    if ((fileSize = ftell(fp)) == -1){
       fprintf(stderr, "An error occurred while retrieving position of the file stream\n");
        exit(1); 
    }
    rewind(fp);
    
    //number of samples (excluding header)
    fileSize = (fileSize - HEADER_SIZE) / 2; 

    file_action(fread(header_buffer, 1, HEADER_SIZE, fp), HEADER_SIZE, FILE_READ);
    file_action(fwrite(header_buffer, 1, HEADER_SIZE, fo), HEADER_SIZE, FILE_WRITE);
    
    //Reads 2 shorts at a time, subtracting right from left, and writing 
    //two copies to output in order to produce a wav file with minimal vocals.
    long i;
    for (i = 1; i <= fileSize; i += 2) { 
        file_action(fread(buff, sizeof(short), 2, fp), 2, FILE_READ);
        buff[0] = (buff[0] - buff[1])/2;
        buff[1] = buff[0];
        file_action(fwrite(buff, sizeof(short), 2, fo), 2, FILE_WRITE);
    }

    fclose(fp);
    fclose(fo);	
    
    return 0;
}

/**
* Checks if correct number of items were read or written into a file.
* @param result Number of items actually processed
* @param expected Number of items expected to be processed               
* @param action_ type Type of action: read or write (for error message).
**/
void file_action(int result, int expected, int action_type){
    if (result != expected){
         if (action_type == FILE_READ)
            fprintf(stderr, "An error occurred while reading the file.\n");
         else
            fprintf(stderr, "An error occurred while writing to the file.\n");
        exit(1);
    }
}

