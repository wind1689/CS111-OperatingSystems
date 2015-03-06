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

/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */

void IOsetting_TRUNC(command_t c);
void IOsetting_APPEND(command_t c);
void execute_if_command (command_t c);
void execute_until_command (command_t c);
void execute_while_command (command_t c);
void execute_subshell_command (command_t c);
void execute_pipe_command (command_t c);
void execute_sequence_command (command_t c);
void execute_simple_command (command_t c);

//For lab 1c
int
prepare_profiling (char const *name)
{
  /* FIXME: Replace this with your implementation.  You may need to
     add auxiliary functions and otherwise modify the source code.
     You can also use external functions defined in the GNU C Library.  */
  error (0, 0, "warning: profiling not yet implemented");
  return -1;
}

int
command_status (command_t c)
{
  return c->status;
}

void
execute_command (command_t c, int profiling)
{
	//Execute the command based on its command type;
	switch (c->type)
	{
		case IF_COMMAND: execute_if_command (c);
										 break;
		case UNTIL_COMMAND: execute_until_command (c);
										 break;
		case WHILE_COMMAND: execute_while_command (c);
										 break;
		case SUBSHELL_COMMAND: execute_subshell_command (c);
										 break;
		case PIPE_COMMAND: execute_pipe_command (c);
										 break;
		case SEQUENCE_COMMAND: execute_sequence_command (c);
										 break;
		case SIMPLE_COMMAND: execute_simple_command (c);
										 break;
		default:	error (1, 0, "The type of command can not be recognized");
	}
}

//Execute the if command;
void execute_if_command (command_t c)
{
	int status;
	int profile = 0;
	int pid1, pid2, pid3;

	//Create child1 process to execute command A;
	pid1 = fork();

	//In child1 process;
	if(pid1 == 0)
	{
		//Seting the IO stream to file
		IOsetting_TRUNC(c);
		//Execute the command A after the "if" token; 
		execute_command(c->u.command[0], profile);
		_exit(c->u.command[0]->status);
	}

	//In parent process;
	else if(pid1 > 0)
	{
		//Wait until the child1 process exits;
		waitpid(pid1, &status, 0);

		//If the command A was executed successfully, execute command B;
		if (status == 0)
		{
			//Create child2 process to execute command B;
			pid2 = fork();

			//In child2 process;
			if (pid2 == 0)
			{
				//Seting the IO stream to file
				IOsetting_APPEND(c);
				//Execute the command B after the "then" token; 
				execute_command(c->u.command[1], profile);
				_exit(c->u.command[1]->status);
			}
			//In parent process;
			else if (pid2 > 0)
			{
				//Wait until the child2 process exits;
				waitpid(pid2, &status, 0);
				//Assign the status = command B exit status;
				c->status = WEXITSTATUS(status);
			}
			else
				error (1, 0, "Error on creating a new process");
		}

		//If command A was not executed successfully, check command C;
		//If command C exists, create child3 process to execute command C;
		else if (c->u.command[2] != NULL)
		{
			//Create child3 process to execute command C;
			pid3 = fork();

			//In child3 process;
			if (pid3 == 0)
			{
				//Seting the IO stream to file
				IOsetting_APPEND(c);
				//Execute the command C after the "else" token; 
				execute_command(c->u.command[2], profile);
				_exit(c->u.command[2]->status);
			}
			//In parent process;
			else if (pid3 > 0)
			{
				//Wait until the child3 process exits;
				waitpid(pid3, &status, 0);
				//Assign the status = command C exit status;
				c->status = WEXITSTATUS(status);
			}
			else
				error (1, 0, "Error on creating a new process");
		}

		//If command A was not executed successfully and command C does not exist,
		//simply assign the status = 0;
		else
			c->status = 0;
		
	}
	else
		error (1, 0, "Error on creating a new process");
}

//Execute the until command;
void execute_until_command (command_t c)
{
	int status;
	int profile = 0;

	pid_t pid1;
	pid_t pid2;

	//Clear the output file first;
	if (c->output != NULL)
	{
		int fd;
		if ((fd = open(c->output, O_TRUNC, 0644)) < 0)
			error (1, 0, "Error on opening output file");
		if (close(fd) < 0)	error (1, 0, "Error on closing file");
	}

	while(1)
	{
		//Create child1 process to execute command A;
		pid1 = fork();

		//In child1 process;
		if (pid1 == 0)
		{
			//Seting the IO stream to file
			IOsetting_APPEND(c);
			//Execute the command A after the "until" token; 
			execute_command(c->u.command[0], profile);
			_exit(c->u.command[0]->status);
		}

		//In parent process;
		else if (pid1 > 0)
		{
			//Wait until the child1 process exits;
			waitpid(pid1, &status, 0);

			//If the command A was not executed successfully, execute command B;
			if (status != 0)
			{
				//Create child2 process to execute command B;
				pid2 = fork();

				//In child2 process;
				if (pid2 == 0)
				{
					//Seting the IO stream to file
					IOsetting_APPEND(c);
					//Execute the command B after the "do" token; 
					execute_command(c->u.command[1], profile);
					_exit(c->u.command[1]->status);
				}
				//In parent process;
				else if (pid2 > 0)
				{
					//Wait until the child2 process exits;
					waitpid(pid2, &status, 0);
					//Assign the status = command B exit status;
					c->status = WEXITSTATUS(status);
				}
				else
					error (1, 0, "Error on creating a new process");
			}

			//If the command A was executed successfully;
			//Assign status = 0 and exit the whlie loop;
			else
			{
				c->status = 0;
				break;
			}
		}

		else
			error (1, 0, "Error on creating a new process");
	}
}

//Execute the while command;
void execute_while_command (command_t c)
{
	int status;
	int profile = 0;

	pid_t pid1;
	pid_t pid2;

	//Clear the output file first;
	if (c->output != NULL)
	{
		int fd;
		if ((fd = open(c->output, O_TRUNC, 0644)) < 0)
			error (1, 0, "Error on opening output file");
		if (close(fd) < 0)	error (1, 0, "Error on closing file");
	}

	while(1)
	{
		//Create child1 process to execute command A;
		pid1 = fork();

		//In child1 process;
		if (pid1 == 0)
		{
			//Seting the IO stream to file
			IOsetting_APPEND(c);
			//Execute the command A after the "while" token; 
			execute_command(c->u.command[0], profile);
			_exit(c->u.command[0]->status);
		}

		//In parent process;
		else if (pid1 > 0)
		{
			//Wait until the child1 process exits;
			waitpid(pid1, &status, 0);

			//If the command A was executed successfully, execute command B;
			if (status == 0)
			{
				//Create child2 process to execute command B;
				pid2 = fork();

				//In child2 process;
				if (pid2 == 0)
				{
					//Seting the IO stream to file
					IOsetting_APPEND(c);
					//Execute the command B after the "do" token; 
					execute_command(c->u.command[1], profile);
					_exit(c->u.command[1]->status);
				}
				//In parent process;
				else if (pid2 > 0)
				{
					//Wait until the child2 process exits;
					waitpid(pid2, &status, 0);
					//Assign the status = command B exit status;
					c->status = WEXITSTATUS(status);
				}
				else
					error (1, 0, "Error on creating a new process");
			}

			//If the command A was not executed successfully;
			//Assign status = 0 and exit the whlie loop;
			else
			{
				c->status = 0;
				break;
			}
		}

		else
			error (1, 0, "Error on creating a new process");
	}
}

//Execute the subshell command;
void execute_subshell_command (command_t c)
{
	int status;
	int profile = 0;

	//Create a child process to execute the command;
	pid_t pid = fork();
	//In child process;
	if (pid == 0)
	{
		//Seting the IO stream to file
		IOsetting_TRUNC(c);
		//Execute the command A;
		execute_command(c->u.command[0], profile);
		_exit(c->u.command[0]->status);
	}
	//In parent process;
	else if (pid > 0)
	{
		//Wait until the child process exits;
		waitpid(pid, &status, 0);
		c->status = status;
	}
	else
		error (1, 0, "Error on creating a new process");
}

//Execute the pipe command;
void execute_pipe_command (command_t c)
{
	pid_t pid1, pid2;
	int status;
	int profile = 0;
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
		execute_command(c->u.command[0], profile);
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
			execute_command(c->u.command[1], profile);
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
void execute_sequence_command (command_t c)
{
	int status;
	int profile = 0;

	//Create a child process to execute the commands;
	pid_t pid = fork();
	//In child process;
	if (pid == 0)
	{
		pid = fork();
		//Execute the command A;
		if (pid == 0)
		{
			execute_command(c->u.command[0], profile);
			_exit(c->u.command[0]->status);
		}
		//Execute the command B;
		else if (pid > 0)
		{
			waitpid(pid, NULL, 0);
			execute_command(c->u.command[1], profile);
			_exit(c->u.command[1]->status);
		}
		else
			error (1, 0, "Error on creating a new process");
	}
	//In parent process;
	else if (pid > 0)
	{
		waitpid(pid, &status, 0);
		//Store the exit status of command B;
		c->status = WEXITSTATUS(status);
	}
	else
		error (1, 0, "Error on creating a new process");
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
		IOsetting_TRUNC(c);

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
		// printf("parent status:%d\n", c->status);
	}
	else
		error (1, 0, "Error on creating a new process");
}

//Setting the command's input and output;
void IOsetting_TRUNC(command_t c)
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

//Setting the command's input and output;
void IOsetting_APPEND(command_t c)
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
		if ((fd_out = open(c->output, O_CREAT | O_WRONLY | O_APPEND, 0644)) < 0)
			error (1, 0, "Error on opening output file");
		//Connect the standard output to fd;
		if (dup2(fd_out, 1) < 0)	error (1, 0, "Error on using dup2");
		//Close the fd;
		if (close(fd_out) < 0)	error (1, 0, "Error on closing file");
	}
}