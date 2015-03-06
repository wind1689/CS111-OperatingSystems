// UCLA CS 111 Lab 1 command execution

// Copyright 2012-2014 Paul Eggert.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "command.h"
#include "command-internals.h"
#include "alloc.h"
#include <error.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>

/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */

void IOsetting(command_t c);
void execute_if_command (command_t c, int profiling);
void execute_until_command (command_t c, int profiling);
void execute_while_command (command_t c, int profiling);
void execute_subshell_command (command_t c, int profiling);
void execute_pipe_command (command_t c, int profiling);
void execute_sequence_command (command_t c, int profiling);
void execute_simple_command (command_t c);
char *command_str (command_t c);

//Return the file descriptor for profiling;
int
prepare_profiling (char const *name)
{
  //Open the file and write only;
	return open(name, O_CREAT | O_WRONLY | O_TRUNC, 0644);
}

int
command_status (command_t c)
{
  return c->status;
}

void
execute_command (command_t c, int profiling)
{
	int status;

	//Create a chil process to execute this command;
	pid_t pid = fork();

	//In child process;
	if (pid == 0)
	{
		struct rusage usage;
		double user_time, sys_time;
		struct timespec start, end;
		double end_time, real_time;
		char *buffer = checked_malloc(sizeof(char)*1024);

		//Mark start time;
		clock_gettime(CLOCK_REALTIME, &start);

		//Execute the command based on its command type;
		switch (c->type)
		{
			case IF_COMMAND: execute_if_command (c, profiling);
											 break;
			case UNTIL_COMMAND: execute_until_command (c, profiling);
											 break;
			case WHILE_COMMAND: execute_while_command (c, profiling);
											 break;
			case SUBSHELL_COMMAND: execute_subshell_command (c, profiling);
											 break;
			case PIPE_COMMAND: execute_pipe_command (c, profiling);
											 break;
			case SEQUENCE_COMMAND: execute_sequence_command (c, profiling);
											 break;
			case SIMPLE_COMMAND: execute_simple_command (c);
											 break;
			default:	error (1, 0, "The type of command can not be recognized");
		}

		//Mark and record the end time;
		clock_gettime(CLOCK_REALTIME, &end);
		end_time = end.tv_sec + end.tv_nsec*1e-9;
		//Get the real time, user time and system time for command;
		real_time = end.tv_sec - start.tv_sec + (end.tv_nsec - start.tv_nsec)*1e-9;
		getrusage(RUSAGE_CHILDREN, &usage);
		user_time = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec * 1e-6;
		sys_time = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec * 1e-6;
		//Write the time informations into the log file;
		sprintf(buffer, "%.2f %f %f %f %s\n", end_time, real_time, user_time, sys_time, command_str(c));
		write(profiling, buffer, strlen(buffer));

		_exit(c->status);
	}
	//In parent process;
	else if (pid > 0)
	{
		//Wait until the command is done;
		waitpid(pid, &status, 0);
		//Save the command's status;
		c->status = status;
	}
	else
		error (1, 0, "Error on creating a new process");
}

//Execute the if command;
void execute_if_command (command_t c, int profiling)
{
	// Seting the IO stream to file
	IOsetting(c);

	// Execute command A;
	execute_command(c->u.command[0], profiling);
	if (c->u.command[0]->status == 0)
	{
		//Execute command B;
		execute_command(c->u.command[1], profiling);
		c->status = c->u.command[1]->status;
	}
	else if (c->u.command[2] != NULL)
	{
		//Execute command C if it has;
		execute_command(c->u.command[2], profiling);
		c->status = c->u.command[2]->status;
	}
	else
		c->status = 0;
}

//Execute the until command;
void execute_until_command (command_t c, int profiling)
{
	//Seting the IO stream to file
	IOsetting(c);

	//Execute command A;
	execute_command(c->u.command[0], profiling);
	c->status = 0;
	while (c->u.command[0]->status > 0)
	{
		//Execute command B;
		execute_command(c->u.command[1], profiling);
		c->status = c->u.command[1]->status;
		//Execute command A again;
		execute_command(c->u.command[0], profiling);
	}
}

//Execute the while command;
void execute_while_command (command_t c, int profiling)
{
	//Seting the IO stream to file
	IOsetting(c);

	//Execute command A;
	execute_command(c->u.command[0], profiling);
	c->status = 0;
	while (c->u.command[0]->status == 0)
	{
		//Execute command B;
		execute_command(c->u.command[1], profiling);
		c->status = c->u.command[1]->status;
		//Execute command A again;
		execute_command(c->u.command[0], profiling);
	}
}

//Execute the subshell command;
void execute_subshell_command (command_t c, int profiling)
{
	//Seting the IO stream to file
	IOsetting(c);
	//Execute command A;
	execute_command(c->u.command[0], profiling);
	c->status = c->u.command[0]->status;
}

//Execute the pipe command;
void execute_pipe_command (command_t c, int profiling)
{
	int status;

	pid_t pid1, pid2;
	//Establish a pipe;
	int fd[2];
	if (pipe(fd) < 0) error (1, 0, "Error on creating a pipe");
	//Create child1 process to execute command A;
	pid1 = fork();
	//In child1 process;
	if (pid1 == 0)
	{
		close(fd[0]);
		//Connect the standard output to the input of pipe;
		if (dup2(fd[1],1) < 0)	error (1, 0, "Error on using dup2");
		//Execute the command A while a process is reading on the other side of pipe;
		execute_command(c->u.command[0], profiling);
		_exit(c->u.command[0]->status);
	}
	//For parent process;
	else if(pid1 > 0)
	{
		//Create child2 process to execute command B;
		pid2 = fork();
		//In child2 process;
		if (pid2 == 0)
		{
			close(fd[1]);
			//Connect the out of pipe to the standard input;
			if (dup2(fd[0], 0) < 0)	error (1, 0, "Error on using dup2");
			//Execute the command B while a process is writing on the other side of pipe;
			execute_command(c->u.command[1], profiling);
			_exit(c->u.command[1]->status);
		}
		//In parent process;
		else if (pid2 > 0)
		{
			close(fd[0]);
			close(fd[1]);
			//Parent process waits until both child processes end;
			waitpid(pid1, NULL, 0);
			waitpid(pid2, &status,0);

			//Store child2 process(command B)'s exit status(0-Succeed; 1-Fail)
			c->status = WEXITSTATUS(status);
		}
		else
			error (1, 0, "Error on creating a new process");
	}
	else
		error (1, 0, "Error on creating a new process");
}

//Execute the sequence command;
void execute_sequence_command (command_t c, int profiling)
{
	//Execute the command A;
	execute_command(c->u.command[0], profiling);
	//Execute the command B;
	execute_command(c->u.command[1], profiling);
	c->status = c->u.command[1]->status;
}

//Execute the simple command;
void execute_simple_command (command_t c)
{
	int status;

	//Create a child process to execute this simple command;
	pid_t pid = fork();

	//In child process, we execute the simple command by using "execvp" function;
	if (pid == 0)
	{
		//Seting the IO stream to file
		IOsetting(c);

		int i;
		while (i < 100000000)
		{
			i += 1;
		}

		//If the simple command is ":", finish this process;
		if (strcmp(c->u.word[0],":") == 0)
			_exit(0);

		//Child process will end in this instruction if the command can be executed.
		execvp(c->u.word[0], c->u.word);
		//When the command can not be executed above, 
		//Child process will not end, and we need to throw an error.
		// _exit(1);
		error (1, 0, "Invalid simple command which can not be executed!");
	}

	//In parent process;
	else if(pid > 0)
	{
		//Parent process waits until the child process ends;
		waitpid(pid, &status,0);
		//Store the child process's exit status(0-Succeed; 1-Fail)
		c->status = WEXITSTATUS(status);
	}
	else
		error (1, 0, "Error on creating a new process");
}

//Setting the command's input and output;
void IOsetting(command_t c)
{
	if (c->input != NULL)
	{
		int fd_in;
		//Open the file and read only;
		if ((fd_in = open(c->input, O_RDONLY, 0644)) < 0)
			error (1, 0, "Error on opening input file");
		//Connect the standard input to fd;
		if (dup2(fd_in, 0) < 0)	error (1, 0, "Error on using dup2");
		//Close the fd;
		if (close(fd_in) < 0)	error (1, 0, "Error on closing file");
	}

	if (c->output != NULL)
	{
		int fd_out;
		//Open the file and write only;
		if ((fd_out = open(c->output, O_CREAT | O_WRONLY | O_TRUNC, 0644)) < 0)
			error (1, 0, "Error on opening output file");
		//Connect the standard output to fd;
		if (dup2(fd_out, 1) < 0)	error (1, 0, "Error on using dup2");
		//Close the fd;
		if (close(fd_out) < 0)	error (1, 0, "Error on closing file");
	}
}

//Construct the command into a string;
char *command_str (command_t c)
{
	char *buffer_c = checked_malloc(1024);
  switch (c->type)
    {
    case IF_COMMAND:
    case UNTIL_COMMAND:
    case WHILE_COMMAND:
    	strcpy(buffer_c, (c->type == IF_COMMAND ? "if "
	       : c->type == UNTIL_COMMAND ? "until " : "while "));
	    strcpy(buffer_c, strcat(buffer_c, command_str (c->u.command[0])));
      strcpy(buffer_c, strcat(buffer_c, (c->type == IF_COMMAND ? " then " : " do ")));
      strcpy(buffer_c, strcat(buffer_c, command_str (c->u.command[1])));
      if (c->type == IF_COMMAND && c->u.command[2])
			{

				strcpy(buffer_c, strcat(buffer_c, " else "));
	  		strcpy(buffer_c, strcat(buffer_c, command_str (c->u.command[2])));
			}

			strcpy(buffer_c, strcat(buffer_c, (c->type == IF_COMMAND ? " fi" : " done")));
      break;

    case SEQUENCE_COMMAND:
    case PIPE_COMMAND:
      {
      	strcpy(buffer_c, command_str (c->u.command[0]));
				strcpy(buffer_c, strcat(buffer_c, (c->type == SEQUENCE_COMMAND ? " ; " : " | ")));
				strcpy(buffer_c, strcat(buffer_c, command_str (c->u.command[1])));
				break;
      }

    case SIMPLE_COMMAND:
      {
				char **w = c->u.word;
				strcpy(buffer_c, *w);
				while (*++w){
					strcpy(buffer_c, strcat(buffer_c, " "));
	  			strcpy(buffer_c, strcat(buffer_c, *w));
	  		}
				break;
      }

    case SUBSHELL_COMMAND:
      strcpy(buffer_c, "( ");
      strcpy(buffer_c, strcat(buffer_c, command_str (c->u.command[0])));
      strcpy(buffer_c, strcat(buffer_c, " )"));
      break;

    default:
      abort ();
    }

  if (c->input){
  	strcpy(buffer_c, strcat(buffer_c, "<"));
  	strcpy(buffer_c, strcat(buffer_c, c->input));
  }
  if (c->output){
  	strcpy(buffer_c, strcat(buffer_c, ">"));
  	strcpy(buffer_c, strcat(buffer_c, c->output));
  }
  return buffer_c;
}