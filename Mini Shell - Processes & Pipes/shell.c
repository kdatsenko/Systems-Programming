#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <mcheck.h>

#include "parser.h"
#include "shell.h"

/**
 * Program that simulates a simple shell.
 * The shell covers basic commands, including builtin commands 
 * (cd and exit only), standard I/O redirection and piping (|). 
 */

#define MAX_DIRNAME 100
#define MAX_COMMAND 1024
#define MAX_TOKEN 128
#define PIPE_READ 0 /* pipe end for reading */
#define PIPE_WRITE 1 /* pipe end for writing */
#define SYS_ERROR -1 /* System call error - used in execute_complex_command to differentiate 
						system call errors from incorrecly formulated shell command errors 
						for recursive feedback */

static pid_t init_process_id; /* process id of origin process */

/* Functions to implement, see below after main */
int execute_cd(char** words);
int execute_nonbuiltin(simple_command *s);
int execute_simple_command(simple_command *cmd);
int execute_complex_command(command *cmd);

/* Helper functions */
int redirect_stdio(int filedes, int stdio, char * filename);
int pipe_commands(int * pdf, int pipe_end);

int main(int argc, char** argv) {
	
	char cwd[MAX_DIRNAME];           /* Current working directory */
	char command_line[MAX_COMMAND];  /* The command */
	char *tokens[MAX_TOKEN];         /* Command tokens (program name, 
					  * parameters, pipe, etc.) */
	init_process_id = getpid();
	while (1) {

		/* Display prompt */		
		getcwd(cwd, MAX_DIRNAME-1);
		printf("%s> ", cwd);
		
		/* Read the command line */
		fgets(command_line, MAX_COMMAND, stdin);
		/* Strip the new line character */
		if (command_line[strlen(command_line) - 1] == '\n') {
			command_line[strlen(command_line) - 1] = '\0';
		}
		
		/* Parse the command into tokens */
		parse_line(command_line, tokens);

		/* Check for empty command */
		if (!(*tokens)) {
			continue;
		}
		
		/* Construct chain of commands, if multiple commands */
		command *cmd = construct_command(tokens);
		//print_command(cmd, 0);
    
		int exitcode = 0;
		if (cmd->scmd) {
			exitcode = execute_simple_command(cmd->scmd);
			if (exitcode == -1) {
				break;
			}
		}
		else {
			exitcode = execute_complex_command(cmd);
			if (exitcode == -1) {
				break;
			}
		}
		release_command(cmd);
	}
    
	return 0;
}


/**
 * Changes directory to a path specified in the words argument;
 * For example: words[0] = "cd"
 *              words[1] = "csc209/assignment3/"
 * Your command should handle both relative paths to the current 
 * working directory, and absolute paths relative to root,
 * e.g., relative path:  cd csc209/assignment3/
 *       absolute path:  cd /u/bogdan/csc209/assignment3/
 */
int execute_cd(char** words) {
	
	 /* Checks for possible errors in command construction */
	 if (!words || !words[0] || !words[1] || strcmp(words[0], "cd")){
	 	if (!words[1])
	 		fprintf(stderr, "Usage: cd <directory name>");
	 	return(EXIT_FAILURE);
	 }

	 /* Changes the directory to the path specified */

	 //chdir can handle both relative and absolute addresses 
	 if (chdir(words[1]) == -1){
	 	perror(words[1]); //No such directory
	 	return (EXIT_FAILURE);
	 } else{
	 	return (EXIT_SUCCESS);
	 }
}


/**
 * Executes a program, based on the tokens provided as 
 * an argument.
 * For example, "ls -l" is represented in the tokens array by 
 * 2 strings "ls" and "-l", followed by a NULL token.
 * The command "ls -l | wc -l" will contain 5 tokens, 
 * followed by a NULL token. 
 */
int execute_command(char **tokens) {
	
	/**
	 * Function returns only in case of a failure (EXIT_FAILURE).
	 */
	 if (execvp(tokens[0], tokens) == -1){
	 	perror(tokens[0]);
	 	return (EXIT_FAILURE);
	 }
	 return 0; //default
}

/**
 * Redirects standard I/O to the corresponding file,
 * and checks for errors. Filename is provided for error
 * msg in case of non-existent file descriptor.
 */
int redirect_stdio(int filedes, int stdio, char * filename){
	if (filedes == -1){
		perror(filename);
	}
	if(dup2(filedes, stdio) == -1) {
        perror("dup2"); //Cannot open/dup2 input file
        return (EXIT_FAILURE);
    }
	if((close(filedes)) == -1) {
    	perror("close"); //Error closing file descriptor
    	return (EXIT_FAILURE);
	}
	return 0;
}


/**
 * Executes a non-builtin command.
 */
int execute_nonbuiltin(simple_command *s) {

	/**
	 * Checks if the in, out, and err fields are set (not NULL) and in each 
	 * case opens a new file descriptor and redirects stdin/stdout/stderr 
	 * to the corresponding file.
	 **/
	int filedes; 
	if (s->in){ //redirect std input from a file
		filedes = open(s->in, O_RDONLY); 
		if (redirect_stdio(filedes, fileno(stdin), s->in))
			return EXIT_FAILURE;
	}
	if (s->out){ //redirect std output to a file
		//Permissions equivalent to rw-rw-r--
		filedes = open(s->out, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
		if (redirect_stdio(filedes, fileno(stdout), s->in))
			return EXIT_FAILURE;
	}
	if (s->err){ //redirect std error to a file
		filedes = open(s->err, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
		if (redirect_stdio(filedes, fileno(stderr), s->in))
			return EXIT_FAILURE;
	}
	/* executes the command, returns if execution fails */
	return execute_command(s->tokens);
}


/**
 * Executes a simple command (no pipes).
 */
int execute_simple_command(simple_command *cmd) {

	//Execution of builtin commands: BUILTIN_CD, BUILTIN_EXIT
	if (cmd->builtin){ //the command is built-in
	 	switch(cmd->builtin){
    		case BUILTIN_CD: //change directory command
       			return execute_cd(cmd->tokens);
    		case BUILTIN_EXIT: //exit command
       			exit(EXIT_SUCCESS);
		}
	}
	//Execution of non-builtin commands
	pid_t r;
	int status;
	if((r = fork()) == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	} else if (r == 0){ /* child */
		execute_nonbuiltin(cmd);
		/* if function above^ returns (doesn't terminate), something
		went wrong with the execution of the command. Then -1 is always
		returned to main to break the loop and terminate this child process. */
		exit(EXIT_FAILURE);
	} else { /* parent */
		if(wait(&status) == -1) { /* waits for the child */
			perror("wait"); 
			exit(EXIT_FAILURE);
		}
		if(!WIFEXITED(status)) {
			fprintf(stderr, "[%d] Child exited abnormally\n", r);
			exit(EXIT_FAILURE);
		} else {
			return WEXITSTATUS(status);
		}
	}
	return 0; 	
}


/**
 * Executes a complex command.  A complex command is two commands chained 
 * together with a pipe operator.
 */
int execute_complex_command(command *c) {

	 /* BASE CASE: simple_command 
	 Reached a non-chained command (no pipe). Executes non-builin commands ONLY. */
	 if (c->scmd){
	 	if (c->scmd->builtin){ /* Improperly formed command - contains builtin cmd */
	 		/* Ignores builtin commands */
	 		fprintf(stderr, "Improperly formed command. Builtin cmd %s does not belong in piped cmd.\n", 
	 			c->scmd->tokens[0]);
	 	} else {
	 		execute_nonbuiltin(c->scmd); //any error msg would have already been printed 
	 		//If execution reaches here, something went wrong with execution of the command.
	 		exit(EXIT_FAILURE);
	 	}
	 	exit(0);
	 }
	
	int pfd[2]; /* pipe */
	if (!strcmp(c->oper, "|")) { //the command is piped (complex)

		 /* Create the pipe */
    	if((pipe(pfd)) == -1) {
        	perror("pipe"); //Could not create pipe
        	exit(SYS_ERROR);
    	}
    	/**
		 * Parent process: Forks two new process to execute 
		 * complex command cmd1 and cmd2 recursively and waits 
		 * for both children to finish.
		 */
		pid_t child1_pid;
		if((child1_pid = fork()) > 0) { /* parent */
			pid_t child2_pid;
		 	if ((child2_pid = fork()) > 0){ /* still in PARENT */

				int sys_error_code; /* Code to exit with in case of system error*/
				//initial process returns standard failure constant 1.
				if (getpid() == init_process_id) 
					sys_error_code = EXIT_FAILURE; 
				else 
					sys_error_code = SYS_ERROR;

		 		/* Parent doesn't need the pipe. Closes both ends. */
		 		if(close(pfd[0]) == -1){
            		perror("close");
            		exit(sys_error_code);
        		}
        		if (close(pfd[1]) == -1) {
        			perror("close");
            		exit(sys_error_code);
        		}
            	int ch1_status, ch2_status; //status for child1, child2	
        		if (waitpid(child1_pid, &ch1_status, 0) == -1) //wait for cmd1 to terminate
        			exit(sys_error_code);
        		if (waitpid(child2_pid, &ch2_status, 0) == -1) //wait for cmd2 to terminate
        			exit(sys_error_code);
 				
        		if(!WIFEXITED(ch1_status) | !WIFEXITED(ch2_status)) { //did not terminate normally
        			//generally, shouldn't occur due to error handling throughout the program code
        			if (!WIFEXITED(ch1_status)) 
        				fprintf(stderr, "[%d] Child exited abnormally\n", child1_pid);
        			if (!WIFEXITED(ch2_status))
        				fprintf(stderr, "[%d] Child exited abnormally\n", child2_pid);
        			if (getpid() == init_process_id)
        				return EXIT_FAILURE;
        			else
        				exit(EXIT_FAILURE); 

				} else if ((WEXITSTATUS(ch1_status) == 255) // (255 is -1 in 2's complement)
							| (WEXITSTATUS(ch2_status == 255))) { //something went wrong in the system
        			if (getpid() == init_process_id)
        				return -1; //return to main, and break the loop
        			else 
        				exit(SYS_ERROR); //transfer system error exit status 
        								 //back through the recursive stack
				}

				//SUCCESS
        		/* If the current process is not the initial process,
        		execution should NOT return to main, but EXIT here. */
        		if (getpid() == init_process_id){
        			return 0; //safe to return with status other than -1.
        		} else {
        			exit(0);
        		}

		 	} else if(child2_pid == 0){ /* CHILD2 will run cmd2 */
		 		//connect read end of pipe to stdin.
		 		if (pipe_commands(pfd, PIPE_READ)){
		 			//unable to connect to pipe
		 			exit(SYS_ERROR);
		 		}
        		/* execute complex command cmd2 recursively */
        		execute_complex_command(c->cmd2);

		 	} else { /* error */
		 		perror("fork cmd2"); 
        		exit(SYS_ERROR);
		 	}

    	} else if(child1_pid == 0){      /* CHILD1 will run cmd1 */ 
    		//connect stdout to write end of pipe
		 	if (pipe_commands(pfd, PIPE_WRITE)){
		 		//unable to connect to pipe
		 		exit(SYS_ERROR);
		 	}
        	/* execute complex command cmd1 recursively */
        	execute_complex_command(c->cmd1);

    	} else { /* error */
    		perror("fork cmd1"); 
        	exit(SYS_ERROR);
    	}	
	}
	return 0;
}


/** 
 * Connects stdin/stdout of a process to a corresponding pipe end
 * and checks for errors. Takes care of closing all hanging descriptors.
 * Arguments:
 *  - pdf: the array of file descriptors for the pipe.
 * 	- pipe_end: can be the macro values PIPE_READ or PIPE_WRITE. 
 * 	These indicate which pipe connection to make: read/stdin or write/stdout.
 **/
int pipe_commands(int * pfd, int pipe_end){
	/* File descriptor for standard input/output */
	int stdio_filedes = pipe_end; //Specifies fileno(stdout/in).
	/* The child process won't be using the other pipe end */
    if(close(pfd[!pipe_end]) == -1) {
        perror("close");
        return(1);
    }
    /* Connects the stdio to the other end of the pipe 
    (the one that was not closed). */
    if(dup2(pfd[pipe_end], stdio_filedes) == -1) {
    	if (pipe_end == PIPE_READ)
    		perror("dup2(Cannot connect RD end of pipe to stdin)");
    	else
        	perror("dup2(Cannot connect WR end of pipe to stdout)");
        return(1);
    }
    /* close writing/reading fd (the one that was used), 
    since all writing/reading goes to stdio */
    if(close(pfd[pipe_end]) == -1) {
        perror("close");
        return(1);
        //TODO!!!
    }
    return 0;
}