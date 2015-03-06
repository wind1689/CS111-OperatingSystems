// UCLA CS 111 Lab 1 main program

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

#include <errno.h>
#include <error.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>

#include "command.h"

static char const *program_name;
static char const *script_name;

static void
usage (void)
{
  error (1, 0, "usage: %s [-p PROF-FILE | -t] SCRIPT-FILE", program_name);
}

static int
get_next_byte (void *stream)
{
  return getc (stream);
}

int
main (int argc, char **argv)
{
  int command_number = 1;
  bool print_tree = false;
  char const *profile_name = 0;
  program_name = argv[0];

  for (;;)
    switch (getopt (argc, argv, "p:t"))
      {
      case 'p': profile_name = optarg; break;  //When the command is "profsh -p xx.sh";
      case 't': print_tree = true; break;   //When the command is "profsh -t xx.sh";
      default: usage (); break;
      case -1: goto options_exhausted;
      }
 options_exhausted:;

  // There must be exactly one file argument.
  if (optind != argc - 1)
    usage ();

  //Get the script name;
  script_name = argv[optind];               
  FILE *script_stream = fopen (script_name, "r");
  //When the script can not open;
  if (! script_stream)
    error (1, errno, "%s: cannot open", script_name);  
  //Build up a command stream(command linked list) based on the script;
  command_stream_t command_stream =
    make_command_stream (get_next_byte, script_stream); 
  //Profile function is for lab 1c
  int profiling = -1;
  if (profile_name)
    {
      profiling = prepare_profiling (profile_name);
      if (profiling < 0)
	     error (1, errno, "%s: cannot open", profile_name);
    }

  //Print out or execute the commands
  command_t last_command = NULL;
  command_t command;
  while ((command = read_command_stream (command_stream)))
    {
      //When it is "profsh -t xx.sh", print out the command tree;
      if (print_tree)
	    {
	     printf ("# %d\n", command_number++);
	     print_command (command);
	    }
      //When it is "profsh xx.sh", execute the command stream;
      else
	    {
	     last_command = command;
	     execute_command (command, profiling);
	    }
    }

  return print_tree || !last_command ? 0 : command_status (last_command);
}
