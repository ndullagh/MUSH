#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include "mush.h"

extern char *readLongString(FILE *infile);
void parse(pipeline pipeln);
void int_handler(int signum);

int main(int argc, char *argv[])
{
	char *buf;
	int i;
	FILE * fp;
	pipeline pipeln;
	struct sigaction act;
	/*stuff for handling SIGINTs*/
	void (*handler)(int) = &int_handler;
	sigset_t mask;
	if(sigemptyset(&mask) != 0)
	{
		perror("sigemptyset");
		exit(EXIT_FAILURE);
	}
	if(sigaddset(&mask, SIGINT) != 0)
	{
		perror("sigaddset");
		exit(EXIT_FAILURE);
	}
	act.sa_handler = handler;
	act.sa_flags = 0;
	act.sa_mask = mask;
	if(sigaction(SIGINT, &act, NULL) != 0)
	{
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
	if(argc == 1)
	{
		while(isatty(STDIN_FILENO) && 
			isatty(STDOUT_FILENO) && !feof(stdin)) 
		/*if stdin or stdout are not ttys, something is VERY WRONG.*/
		{
			printf("8-P "); /*prompt*/
			if((buf = readLongString(stdin)) == NULL && !feof(stdin))
			{
				if(errno != EINTR) /*ensure readLongString was not interrupted*/
				{
					perror("readLongString");
					exit(EXIT_FAILURE);

				}
				else /*move on if interrupted*/
				{
					continue;
				}
			}
			else if(feof(stdin))/*exit right away if end reached*/
			{
				printf("\n");
				free(buf);
				exit(EXIT_SUCCESS);
			}
			
			
			if(strlen(buf) > 0 && (pipeln = crack_pipeline(buf)) == NULL)
			{ /*if the buffer is not empty and the pipeline 
				doesn't exist, be sad and move on (error happened somewhere)*/
				free(buf);
				if(errno != EINTR)
				{
					
					continue;
				}	
			}
			
			if(strlen(buf) > 0) /*otherwise, parse the pipeline, 
									then free it and be happy*/
			{
				parse(pipeln);
				if(pipeln != NULL)
					free_pipeline(pipeln);		
				free(buf);
			}
			

		}
	}
	else /*if given file(s) to read commands from*/
	{	
		for(i = 1; i < argc; i++) /*read each file, 
									treat each line as a new pipeline, 
									execute as above (minus prompt)*/
		{
			if((fp = fopen(argv[i], "r")) == NULL)
			{
				perror("fopen");
				continue;	
			}
			
			while((buf = readLongString(fp)) != NULL)
			{
				if((pipeln = crack_pipeline(buf)) == NULL)
				{
					
					if(buf != NULL)
						free(buf);

					if(errno != EINTR)
					{
						continue;
					}
					
					
				}
				else if(strlen(buf) > 0)
				{
					parse(pipeln);
					if(pipeln != NULL)
						free_pipeline(pipeln);		
					free(buf);

				}

				
			}
			if(!feof(fp))
			{
				perror("readLongString");
				exit(EXIT_FAILURE);
			}
			fclose(fp);
			
			

		}
		
	}
	return 0;
	
}

void parse(pipeline pipeln)
{
	int i, j;
	int pid;
	struct clstage curstage;
	uid_t uid = getuid();
	struct passwd *pw;
	char *env;
	int length = pipeln -> length;
	int **pipearr;
	char **argv_term;
	int fd;
	
	
	if(strcmp((pipeln -> stage)[0].argv[0], "cd") == 0)
	{
		if(pipeln -> length > 1)
		{
			printf("usage: cd may not be part of a multi-stage pipeline\n");
			exit(EXIT_FAILURE);
		}
		else
		{
			
			if((pipeln -> stage)[0].argc == 1)
			{
				/*chdir to home*/
				if((env = getenv("HOME")) == NULL)
				{
					/*try again using user pw*/
					if((pw = getpwuid(uid)) != NULL)
					{
						if(chdir(pw -> pw_dir) == -1)
						{
							perror("chdir");
						}

					}
					else
					{
						perror("getpwuid");
					}

				}
				/*give up*/
				else
				{
					if(chdir(env) == -1)
					{
						perror("chdir");
						
					}

				}
			}
			else
			{
				/*chdir to given directory*/	
				if(chdir((pipeln -> stage)[0].argv[1]) == -1)
				{
					perror("chdir");
					
				}
			}
		}

	}
	else
	{ /*anything other than cd*/
		/*create array of pipes*/
		if((pipearr = malloc((length - 1) * sizeof(int *))) == NULL)
		{
			perror("malloc");
			exit(EXIT_FAILURE);
		}
		for(i = 0; i < length -1; i++)
		{
			if((pipearr[i] = malloc(2 * sizeof(int))) == NULL)
			{
				perror("malloc");
				exit(EXIT_FAILURE);
			}
			if(pipe(pipearr[i]) == -1)
			{
				perror("pipe");
				exit(EXIT_FAILURE);
			}
		}

		for(i = 0; i < length; i++)
		{
			curstage = (pipeln -> stage)[i];

			if((argv_term = malloc((curstage.argc + 1) * sizeof(char *))) == NULL)
			{
				perror("malloc");
				exit(EXIT_FAILURE);
			}
			
			/*null-terminate argv*/
			for(j = 0; j < curstage.argc; j++)
			{
				argv_term[j] = curstage.argv[j];
			}
			argv_term[curstage.argc] = NULL;


			pid = fork();
	
			if(pid == 0)
			{
				/*child process reads from curstage's in to its out*/
				if(i == 0 && curstage.inname != NULL)
				{ /*IO redirection (if present)*/
					fd = open(curstage.inname, O_RDONLY | O_CREAT, 
						S_IRUSR | S_IWUSR | S_IRGRP 
						| S_IWGRP | S_IROTH | S_IWOTH);
					if(dup2(fd, STDIN_FILENO) == -1)
					{
						perror("dup2");
						exit(EXIT_FAILURE);
					}
					close(fd);

				}
				if(i > 0) /*pipe the in if not first*/
				{
					if(dup2(pipearr[i-1][0], STDIN_FILENO) == -1)
					{
						perror("dup2");
						exit(EXIT_FAILURE);
					}
				}
				if(i == length - 1 && curstage.outname != NULL)
				{
					fd = open(curstage.outname, O_WRONLY | O_CREAT, 
						S_IRUSR | S_IWUSR | S_IRGRP 
						| S_IWGRP | S_IROTH | S_IWOTH);
					if(dup2(fd, STDOUT_FILENO) == -1)
					{
						perror("dup2");
						exit(EXIT_FAILURE);
					}
					close(fd);

				}
				if(i < length - 1) /*pipe the out if not last*/
				{
					if(dup2(pipearr[i][1], STDOUT_FILENO) == -1)
					{
						perror("dup2");
						exit(EXIT_FAILURE);
					}
				}
				for(j = 0; j < length - 1; j++) /*close all pipes*/
				{
					close(pipearr[j][0]);
					close(pipearr[j][1]);
				}
				
				execvp(curstage.argv[0], argv_term);
				
				perror("execvp");
				exit(EXIT_FAILURE);

				
			}
			free(argv_term);

		}

		for(i = 0; i < length -1; i++) /*close all pipes in parent*/
		{
			close(pipearr[i][0]);
			close(pipearr[i][1]);
			free(pipearr[i]);
		}
		free(pipearr);
		for(i = 0; i < length; i++)
		{
			if(wait(NULL) == -1) /*wait for all children*/
			{
				if(errno != EINTR)
				{
					perror("wait");
				}
				else /*just in case!*/
				{
					while(wait(NULL) == -1)
					{
						if(errno != EINTR)
						{
							perror("wait");
						}

					}
				} 
			}
		}
	}				



	
}

/*hilariously simple SIGINT handler*/
void int_handler(int signum)
{
	if(signum == SIGINT)
	{
		printf("\n");	
	}
}
