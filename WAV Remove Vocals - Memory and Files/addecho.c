#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#define HEADER_SIZE 44
#define FILE_READ 0
#define FILE_WRITE 1

/* Checks if correct number of items were read or written into a file. */
void file_action(int result, int expected, int action_type);

int main(int argc, char *argv[]) {

    long delay = 8000; //default values
    short volume_scale = 4;
    
    FILE *fp; //input file
    FILE *foutput; //output file
    long int orig_num_samples; //number of samples in the source file
    char header[HEADER_SIZE]; //buffer for the header of the wav file
    short *echo_buff; //Echo buffer

    int opt; //option character
    char *ep; //pointer to invalid character in optarg
    long fileSize;

    //Parsing command line arguments 
    const char *optstring = "d:v:";
    while ((opt = getopt(argc, argv, optstring)) != -1) {
        switch (opt) {                   
        case 'd':
            errno = 0;	
            delay = strtol(optarg, &ep, 10);
            if (*ep != 0 || errno != 0 || delay <= 0){ //invalid or empty argument 
            	fprintf(stderr, "Invalid argument after option -%c\n", optopt);
				exit(1);
            }			
            break;
        case 'v':
            errno = 0;	
            volume_scale = (short) strtol(optarg, &ep, 10);
            if (*ep != 0 || errno != 0 || volume_scale <= 0){ //invalid or empty argument 
                fprintf(stderr, "Invalid argument after option -%c\n", optopt);
				exit(1);
            }	
            break;
        default: /* '?' */
            fprintf(stderr, "Usage: %s [-d delay] [-v volume_scale] sourcewav destwav\n",
                    argv[0]);
            exit(1);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Expected arguments after options <sourcewav> <destwav>\n");
        exit(EXIT_FAILURE);
    }

    //checks if input file and ouput file can be opened successfully.
    if((fp = fopen(argv[optind++], "rb")) == NULL) { //does it exist?
        perror(argv[optind++]);
        exit(1);
    }
    if((foutput = fopen(argv[optind], "w")) == NULL) {
        perror(argv[optind]);
        exit(1);
    }
    
    //gets the size of the file in in order to 
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
    //initializes the number of samples found in the file
    orig_num_samples = fileSize; 
    
    //copying and modifying header of wav fle
    int *sizeptr; //pointer to size of file stored as an int in the header  
    file_action(fread(header, 1, HEADER_SIZE, fp), HEADER_SIZE, FILE_READ);
    /* The next line says that we want to treat the memory starting at the
     * address header + 4 as if it were an unsigned int */ 
    sizeptr = (int *)(header + 4);	
    //Adds the right amount of additional size in anticipation of echo 
    //for the header of the output file .
    *sizeptr += (delay * 2); 
    sizeptr = (int *)(header + 40);   
    *sizeptr += (delay * 2);
    file_action(fwrite(header, 1, HEADER_SIZE, foutput), HEADER_SIZE, FILE_WRITE);
    
    //Allocates memory for the echo buffer based on the delay value.
    echo_buff = malloc(delay*sizeof(short)); // one buffer!
    if (echo_buff == NULL){ //check for failure of malloc
    	fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    //MAIN ALGORITHM
       
    long buffer_counter = 0; //buffer index
    short orig_sample[1]; //temp storage
    
    long i; 
    for (i = 1; i <= orig_num_samples; i++){
        file_action(fread(orig_sample, sizeof(short), 1, fp), 1, FILE_READ);
        if (i <= delay){
            //write sound without echo (echo hasn't begun yet)
            file_action(fwrite(orig_sample, sizeof(short), 1, foutput), 1, FILE_WRITE);
        } else {
            //at delay samples or later, mix in the echo.
            echo_buff[buffer_counter] += orig_sample[0]; 
            file_action(fwrite(echo_buff + buffer_counter, sizeof(short), 1, foutput), 1, FILE_WRITE);
        }
        //scale down the volume of the orginal sound for echo.
        echo_buff[buffer_counter] = orig_sample[0] / volume_scale;
        //Prevents running off the end of the buffer,
        //restores writing point to the beginning.
        buffer_counter++;
        if (buffer_counter >= delay){
            buffer_counter = 0;
        }
    }
     
    //Writes the rest of the echo buffer left over after mixing
    //with the original size of the wav file. 
    //**For the case when the delay is greater file sample size, writes empty 
    //(zero/silent) values (delay - orig_num_samples) from buffer 
    //until end, and starts writing from beginning of the buffer for echo
    //INVARIANT: echo sample size = orig_num_samples + delay.
    for (i = 0; i < delay; i++){
        file_action(fwrite(echo_buff + buffer_counter, sizeof(short), 1, foutput), 1, FILE_WRITE);
        buffer_counter++;
        //last echo value may have been written in the middle of the buffer.
        //Restores buffer index to the beginning when it runs off the end
        //in order to write exactly "delay" number of samples to output file.
        if (buffer_counter >= delay){
            buffer_counter = 0;
        }  
    }
 
    fclose(fp);
    fclose(foutput);
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