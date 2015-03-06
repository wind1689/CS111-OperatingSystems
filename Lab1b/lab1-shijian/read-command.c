// UCLA CS 111 Lab 1 command reading

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
#include <error.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Construct a linked list and the addlast function for command tree. */
//Construct a linked list for command tree. 
struct command_node
{
  command_t element;
  struct command_node *next;
};
typedef struct command_node *command_node_t;

struct command_stream
{
  command_node_t current;
  command_node_t head;
  command_node_t tail;
};
//Add the list node to the tail of linked list
void addLast(command_stream_t command_list, command_t command)
{

  if (command_list->tail == NULL)
  {
    command_node_t temp = checked_malloc(sizeof(struct command_node));
    temp->element = command;
    temp->next = NULL;
    command_list->head = temp;
    command_list->tail = temp;
    command_list->current = temp;
  }
  else
  {
    command_list->tail->next = checked_malloc(sizeof(struct command_node));
    command_list->tail->next->element = command;
    command_list->tail->next->next = NULL;
    command_list->tail = command_list->tail->next;
  }
}


int is_special_char(char ch);
char *append_char_to_string(char *str, char ch);
char *get_token_string(void *file_stream, char *ch_for_next_token);
int is_special_token1(char *token);
int is_special_token2(char *token);
char **construct_token_list(void *file_stream, char **token_for_next_command, char *ch_for_next_token, int *token_list_size);
enum command_type recog_command(char **token_list, int begin_index, int end_index, int keyword_index[]);
command_t create_command(char **token_list, int begin_index, int end_index);
command_t create_if_command(char **token_list, int end_index, int keyword_index[]);
command_t create_until_command(char **token_list, int end_index, int keyword_index[]);
command_t create_while_command(char **token_list, int end_index, int keyword_index[]);
command_t create_subshell_command(char **token_list, int end_index, int keyword_index[]);
command_t create_pipe_command(char **token_list, int begin_index, int end_index, int keyword_index[]);
command_t create_sequence_command(char **token_list, int begin_index, int end_index, int keyword_index[]);
command_t create_simple_command(char **token_list, int begin_index, int end_index);



command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{
  //Rename get_next_byte_argument
  FILE *file_stream = get_next_byte_argument;

  //Create a command linked list and initialize it;
  command_stream_t command_list = (command_stream_t)checked_malloc( sizeof(struct command_stream) );
  command_list->current = NULL;
  command_list->head = NULL;
  command_list->tail = NULL;
  
  //Create a command node;
  command_node_t command_list_node = (command_node_t)checked_malloc( sizeof(struct command_node) );
  command_list_node->next = NULL;

  //Create a token list to store the token string;
  char **token_string_list;
  int *token_list_size = checked_malloc(sizeof(int));
  *token_list_size = 0;

  char *ch_for_next_token = checked_malloc(sizeof(char));
  *ch_for_next_token = get_next_byte(file_stream);

  char **token_for_next_command = checked_malloc(sizeof(char*));
  *token_for_next_command = get_token_string(file_stream, ch_for_next_token);


  int begin_index, end_index;
  while (!feof(file_stream))
  {
    //Construct the token list for one command node;
    *token_list_size = 0;
    token_string_list = construct_token_list(file_stream, token_for_next_command, ch_for_next_token, token_list_size);
    
    //Construct the command when the beginning token of token list is not "#";
    if (strcmp(token_string_list[0], "#")!=0)
    {
      //Construct the command node using the token list generated bfore;
      begin_index = 0;
      end_index = *token_list_size - 1;

      //command_list_node = create_command(token_string_list, begin_index, end_index);
      addLast(command_list, create_command(token_string_list, begin_index, end_index));
    }
  }

  return command_list;

}

command_t
read_command_stream (command_stream_t s)
{
  if (s->current != NULL)
  {
    command_t reading_command = checked_malloc( sizeof(struct command) );
    reading_command = s->current->element;
    s->current = s->current->next;
    return reading_command;
  }
  else
    return NULL;
}

char *append_char_to_string(char *str, char ch)
{
  char *cstr = checked_malloc( sizeof(char) * 2 );
  cstr[0] = ch;
  cstr[1] = '\0';
  return strcat(str, cstr);
}

//The special character which can be individually formed a token;
int is_special_char(char ch)
{
  return (ch == ' ') || (ch == '#')
         || (ch == '|') || (ch == ';')
         || (ch == ':') || (ch == '\n')
         || (ch == '(') || (ch == ')')
         || (ch == '<') || (ch == '>')
         || (ch == '&');
}


//Extract the tokens form the file stream
char *get_token_string(void *file_stream, char *ch_for_next_token)
{
  char *token_string = checked_malloc( sizeof(char) * 1);
  char ch;
  char ch_next = *ch_for_next_token;
  //Read the character until it is not ' ';
  while (ch_next == ' ')
  {
    ch = ch_next;
    ch_next = getc(file_stream);
  }
  //Read the character and append it to the token string;
  ch = ch_next;
  ch_next = getc(file_stream);
  while (!is_special_char(ch) && !is_special_char(ch_next) && !feof(file_stream))
  {
    token_string = append_char_to_string(token_string, ch);
    ch = ch_next;
    ch_next = getc(file_stream);
  }
  token_string = append_char_to_string(token_string, ch);
  *ch_for_next_token = ch_next;

  if (strcmp(token_string, "`") == 0) error (1, 0, "Fails!");
  return token_string;
}

//For previous token checking;
int is_special_token1(char* token)
{
  return (strcmp(token, "if")==0) || (strcmp(token, "then")==0)
         || (strcmp(token, "else")==0) || (strcmp(token, "until")==0)
         || (strcmp(token, "while")==0) || (strcmp(token, "do")==0)
         || (strcmp(token, "(")==0) || (strcmp(token, "|")==0)
         || (strcmp(token, ";")==0);
}
//For later token checking;
int is_special_token2(char* token)
{
  return (strcmp(token, "\n")==0) || (strcmp(token, "then")==0)
         || (strcmp(token, "else")==0) || (strcmp(token, "fi")==0)
         || (strcmp(token, "do")==0) || (strcmp(token, "done")==0)
         || (strcmp(token, ")")==0) || (strcmp(token, "|")==0)
         || (strcmp(token, ";")==0);
}

//hahaha
void check_key_token(char *str, int *if_array, int *while_until_array, int *sub_array)
{
  if (strcmp(str, "if") == 0) if_array[0] += 1;
  else if (strcmp(str, "then") == 0)
  {
    if_array[1] += 1;
    if (if_array[1] > if_array[0])
    {
      error (1, 0, "\"then\" token is not in correct order");
    }
  }
  else if (strcmp(str, "else") == 0)
  {
    if_array[2] += 1;
    if (if_array[2] > if_array[0] || if_array[2] > if_array[1])
    {
      error (1, 0, "\"else\" token is not in correct order");
    }
  }
  else if (strcmp(str, "fi") == 0)
  {
    if_array[3] += 1;
    if (if_array[3] > if_array[0] || if_array[3] > if_array[1] || if_array[3] > if_array[0])
    {
      error (1, 0, "\"fi\" token is not in correct order");
    }
  }
  else if (strcmp(str, "while") == 0) while_until_array[0] += 1;
  else if (strcmp(str, "until") == 0) while_until_array[1] += 1;
  else if (strcmp(str, "do") == 0)
  {
    while_until_array[2] += 1;
    int temp1 = while_until_array[0];
    int temp2 = while_until_array[1];
    if (while_until_array[2] > (temp1 + temp2))
    {
      error (1, 0, "\"do\" token is not in correct order");
    }
  }
  else if (strcmp(str, "done") == 0)
  {
    while_until_array[3] += 1;
    int temp1 = while_until_array[0];
    int temp2 = while_until_array[1];
    if (while_until_array[3] > (temp1 + temp2) || while_until_array[3] > while_until_array[2])
    {
      error (1, 0, "\"done\" token is not in correct order");
    }
  }
  else if (strcmp(str, "(") == 0) sub_array[0] += 1;
  else if (strcmp(str, ")") == 0)
  {
    sub_array[1] += 1;
    if (sub_array[1] > sub_array[0])
    {
      error (1, 0, "\")\" token is not in correct order");
    }
  }
}

//Construct a token list from the file stream.
char **construct_token_list(void *file_stream, char **token_for_next_command, char *ch_for_next_token, int *token_list_size)
{

  //For checking the order of the key token;
  int *if_array = checked_malloc(sizeof(int)*4); //if, then, else, fi;
  int *while_until_array = checked_malloc(sizeof(int)*4); //while, until, do, done
  int *sub_array = checked_malloc(sizeof(int)*4); //(, )
  int i;
  for (i = 0; i < 4; ++i)
  {
    if_array[i] = 0;
    while_until_array[i] = 0;
    sub_array[i] = 0;
  }


  char *token = *token_for_next_command;
  char *token_next = get_token_string(file_stream, ch_for_next_token);
  char **token_list = checked_malloc(sizeof(char*) * 100);
  int index = 0;

  //Delete unwanted "\n" between commands;
  while (strcmp(token, "\n")==0)
  {
    token = token_next;
    token_next = get_token_string(file_stream, ch_for_next_token);
  }

  //Check the first token of command
  if (strcmp(token, "<")==0 || strcmp(token, ">")==0 || strcmp(token, ";")==0 || strcmp(token, "|")==0 || strcmp(token, "&")==0)
  {
    error (1, 0, "The first token is invalid");
  }

  while (!feof(file_stream))
  {
    //Delete unwanted "\n" within the commands;
    while (strcmp(token, "\n")==0 && index > 0)
    {
      if (!is_special_token1(token_list[index-1]) && !is_special_token2(token_next))
      {
        token = checked_malloc(sizeof(char)*2);
        token = ";";
      }
      else
      {
        token = token_next;
        token_next = get_token_string(file_stream, ch_for_next_token);
      }
    }

    // &&, ||, << or >> is not allowed be the token
    if ((strcmp(token, "&")==0 || strcmp(token, "<")==0 || strcmp(token, ";")==0 || strcmp(token, ">")==0 )&& index > 0)
    {
      if (strcmp(token_list[index-1], token)==0)
        error (1, 0, "&&, ||, << or >> is not allowed be the token");
    }

    //Write the valid token into the token list.
    token_list[index] = token;
    index += 1;

    //Check the correct order for the key tokens;
    check_key_token(token, if_array, while_until_array, sub_array);

    token = token_next;
    token_next = get_token_string(file_stream, ch_for_next_token);

    //When a complete command is read, jump out of the loop;
    int temp1 = while_until_array[0];
    int temp2 = while_until_array[1];
    int temp3 = while_until_array[3];
    int stop_condition = (if_array[0] == if_array[3]) && ((temp1 + temp2) == temp3) && (sub_array[0] == sub_array[1]);
    if (strcmp(token, "\n")==0 && stop_condition)
      break;
  }

  //Reading the last token in file
  if (feof(file_stream) && strcmp(token, "\n")!=0)
  {
    token_list[index] = token;
    index += 1;
    check_key_token(token, if_array, while_until_array, sub_array);
    int temp1 = while_until_array[0];
    int temp2 = while_until_array[1];
    int temp3 = while_until_array[3];
    if (!((if_array[0] == if_array[3]) && ((temp1 + temp2) == temp3) && (sub_array[0] == sub_array[1])))
      error (1, 0, "Token list is not complete");
  }
  
  *token_list_size = index;
  *token_for_next_command = token_next;

  return token_list;
}

//Recognize the command type base on the read token list, and also return the keyword indices;
enum command_type recog_command(char **token_list, int begin_index, int end_index, int keyword_index[])
{
  int index = begin_index;
  //Recognize the sequence command;
  int if_pair = 0;            // "if" and "fi"
  int while_until_pair = 0;   // "while" "until" and "done"
  int sub_pair = 0;           // "(" and ")"

  int SEQ_last_index = -1;
  for (;index <= end_index; ++index)
  {
    if (strcmp(token_list[index], "if")==0)  if_pair += 1;
    else if (strcmp(token_list[index], "fi")==0)  if_pair -= 1;
    else if (strcmp(token_list[index], "while")==0)  while_until_pair += 1;
    else if (strcmp(token_list[index], "until")==0)  while_until_pair += 1;
    else if (strcmp(token_list[index], "done")==0)  while_until_pair -= 1;
    else if (strcmp(token_list[index], "(")==0)  sub_pair += 1;
    else if (strcmp(token_list[index], ")")==0)  sub_pair -= 1;

    if (strcmp(token_list[index], ";")==0 && if_pair == 0 && while_until_pair == 0 && sub_pair == 0) 
      SEQ_last_index = index;
  }

  index = begin_index;
  if (SEQ_last_index != -1)
  {
    keyword_index[0] = SEQ_last_index;
    return SEQUENCE_COMMAND;
  }
  //Recognize the if command;
  else if (strcmp(token_list[index], "if")==0)
  {
    //Decide the index of the keyword like "if", "then", "else", "fi";
    //Decide index of "if" keyword;
    keyword_index[0] = index;            
    index += 1;

    //Decide index of "then";
    int if_pair = 0;
    index += 1;
    for (; index <= end_index; ++index)
    {
      if (strcmp(token_list[index-1], "if")==0)  if_pair += 1;
      else if (strcmp(token_list[index-1], "fi")==0)  if_pair -= 1;

      if (strcmp(token_list[index], "then") == 0 && if_pair == 0)
      {
        keyword_index[1] = index;
        break;
      }
    }
    if (index == end_index + 1)
    {
      error (1, 0, "IF_COMMAND recognization fails!");
      return 0;
    }

    //Decide index of "else";
    int is_else_exist = 0;
    if_pair = 0;
    index += 1;
    for (; index <= end_index; ++index)
    {
      if (strcmp(token_list[index-1], "if")==0)  if_pair += 1;
      else if (strcmp(token_list[index-1], "fi")==0)  if_pair -= 1;
      if (((strcmp(token_list[index], "else")==0) || (strcmp(token_list[index], "fi")==0)) && if_pair == 0)
      {
        keyword_index[2] = index;
        if (strcmp(token_list[index], "else")==0)
        {
          is_else_exist = 1;
        }
        break;
      }
    }

    //Decide index of "fi";
    if (is_else_exist == 1)
    {
      if_pair = 0;
      index += 1;
      
      for (; index <= end_index; ++index)
      {
        if (strcmp(token_list[index-1], "if")==0)  if_pair += 1;
        else if (strcmp(token_list[index-1], "fi")==0)  if_pair -= 1;

        if (strcmp(token_list[index], "fi")==0 && if_pair == 0)
        {
          keyword_index[3] = index;
          break;
        }
      }
    }

    //Return the type of command;
    return IF_COMMAND;
  }

  //Recognize the until command;
  else if (strcmp(token_list[index], "until")==0)
  {
    //Decide the index of the keyword like "until", "do", "done";
    //Decide index of "until" keyword;
    keyword_index[0] = index;            
    index += 1;

    //Decide index of "do";
    int while_until_pair = 0;
    index += 1;
    for (; index <= end_index; ++index)
    {
      if (strcmp(token_list[index-1], "while")==0)  while_until_pair += 1;
      else if (strcmp(token_list[index-1], "until")==0)  while_until_pair += 1;
      else if (strcmp(token_list[index-1], "done")==0)  while_until_pair -= 1;

      if (strcmp(token_list[index], "do")==0 && while_until_pair == 0)
      {
        keyword_index[1] = index;
        break;
      }
    }
    if (index == end_index + 1)
    {
      error (1, 0, "UNTIL_COMMAND recognization fails!");
      return 0;
    } 

    //Decide index of "done"
    while_until_pair = 0;
    index += 1;
    for (; index <= end_index; ++index)
    {
      if (strcmp(token_list[index-1], "while")==0)  while_until_pair += 1;
      else if (strcmp(token_list[index-1], "until")==0)  while_until_pair += 1;
      else if (strcmp(token_list[index-1], "done")==0)  while_until_pair -= 1;

      if (strcmp(token_list[index], "done")==0 && while_until_pair == 0)
      {
        keyword_index[2] = index;
        break;
      }
    }
    if (index == end_index + 1)
    {
      error (1, 0, "UNTIL_COMMAND recognization fails!");
      return 0;
    } 

    //Return the type of command;
    return UNTIL_COMMAND;
  }

  //Recognize the while command;
  else if (strcmp(token_list[index], "while")==0)
  {
    //Decide the index of the keyword like "while", "do", "done";
    //Decide index of "while" keyword;
    keyword_index[0] = index;            
    index += 1;

    //Decide index of "do";
    int while_until_pair = 0;
    index += 1;
    for (; index <= end_index; ++index)
    {
      if (strcmp(token_list[index-1], "while")==0)  while_until_pair += 1;
      else if (strcmp(token_list[index-1], "until")==0)  while_until_pair += 1;
      else if (strcmp(token_list[index-1], "done")==0)  while_until_pair -= 1;

      if (strcmp(token_list[index], "do")==0 && while_until_pair == 0)
      {
        keyword_index[1] = index;
        break;
      }
    }
    if (index == end_index + 1)
    {
      error (1, 0, "WHILE_COMMAND recognization fails!");
      return 0;
    }

    //Decide index of "done"
    while_until_pair = 0;
    index += 1;
    for (; index <= end_index; ++index)
    {
      if (strcmp(token_list[index-1], "while")==0)  while_until_pair += 1;
      else if (strcmp(token_list[index-1], "until")==0)  while_until_pair += 1;
      else if (strcmp(token_list[index-1], "done")==0)  while_until_pair -= 1;

      if (strcmp(token_list[index], "done")==0 && while_until_pair == 0)
      {
        keyword_index[2] = index;
        break;
      }
    }
    if (index == end_index + 1)
    {
      error (1, 0, "WHILE_COMMAND recognization fails!");
      return 0;
    } 

    //Return the type of command;
    return WHILE_COMMAND;
  }

  //Recognize the subshell command;
  else if (strcmp(token_list[index], "(")==0)
  {
    //Decide the index of the keyword like "(", ")";
    //Decide index of "(" keyword;
    keyword_index[0] = index;
    index += 1;

    //Decide index of ")";
    int subshell_pair = 0;
    index += 1;
    for (; index <= end_index; ++index)
    {
      if (strcmp(token_list[index-1], "(")==0)  subshell_pair += 1;
      else if (strcmp(token_list[index-1], ")")==0)  subshell_pair -= 1;

      if (strcmp(token_list[index], ")")==0 && subshell_pair == 0)
      {
        keyword_index[1] = index;
        break;
      }
    }
    if (index == end_index + 1)
    {
      error (1, 0, "SUBSHELL_COMMAND recognization fails!");
      return 0;
    } 

    //Return the type of command;
    return SUBSHELL_COMMAND;
  }

  //Recognize the pipe command and simple command;
  else
  {
    int PIPE_last_index = -1;
    for (; index <= end_index; ++index)
    {
      if (strcmp(token_list[index], "|")==0) PIPE_last_index = index; 
    }
    if (PIPE_last_index != -1)
    {
      keyword_index[0] = PIPE_last_index;
      return PIPE_COMMAND;
    }
    else
    {
      int input_index = -1;
      int output_index = -1;
      for (; index <= end_index; ++index)
      {
        if (strcmp(token_list[index], "<")==0)  input_index = index;
        else if (strcmp(token_list[index], ">")==0)  output_index = index;
      }
      keyword_index[0] = input_index;
      keyword_index[1] = output_index;
      return SIMPLE_COMMAND;
    }
  }
}

//Create a complete command based on the constructed token list;
command_t create_command(char **token_list, int begin_index, int end_index)
{
  //Recognize the command base on the read token list, and also return the keyword indices;
  int keyword_index[4] = {-1, -1, -1, -1};
  int begin = begin_index;
  int end = end_index;
  while (strcmp(token_list[end], ";")==0)
  {
    end -= 1;
  }
  enum command_type type_of_command = recog_command(token_list, begin, end, keyword_index);

  //After knowing the type of command, create the command of that type;
  switch (type_of_command)
  {
    case IF_COMMAND:  return create_if_command(token_list, end, keyword_index);
    case UNTIL_COMMAND: return create_until_command(token_list, end, keyword_index);
    case WHILE_COMMAND: return create_while_command(token_list, end, keyword_index);
    case SUBSHELL_COMMAND:  return create_subshell_command(token_list, end, keyword_index);
    case PIPE_COMMAND:  return create_pipe_command(token_list, begin, end, keyword_index);
    case SEQUENCE_COMMAND:  return create_sequence_command(token_list, begin, end, keyword_index);
    case SIMPLE_COMMAND:  return create_simple_command(token_list, begin, end);
    default:  {error (1, 0, "This stream of command can not be recognized");
              return 0;}
  }
}

//Create an if command based on the token list;
command_t create_if_command(char **token_list, int end_index, int keyword_index[])
{
  // printf("create_if_command\n");
  command_t if_c = checked_malloc( sizeof(struct command) );
  if_c->type = IF_COMMAND;
  if_c->status = -1;
  if_c->input = NULL;
  if_c->output = NULL;
  int begin = 0;
  int end = 0;

  /*** Create the command A in IF_COMMAND ***/
  begin = keyword_index[0] + 1;
  end = keyword_index[1] - 1;
  if_c->u.command[0] = create_command(token_list, begin, end);

  /*** Create the command B in IF_COMMAND ***/
  begin = keyword_index[1] + 1;
  end = keyword_index[2] - 1;
  if_c->u.command[1] = create_command(token_list, begin, end);

  /*** Create the command C in IF_COMMAND(if it has) ***/
  if (keyword_index[3] != -1)
  {
    begin = keyword_index[2] + 1;
    end = keyword_index[3] - 1;
    if_c->u.command[2] = create_command(token_list, begin, end);
  }

  /*** Create the input and output in IF_COMMAND(if it has) ***/
  int index_of_fi = 0;
  if (keyword_index[2] != end_index && keyword_index[3] !=end_index)
  {
    if (keyword_index[3] == -1)
      index_of_fi = keyword_index[2];
    else
      index_of_fi = keyword_index[3];

    //Check if there are some invalid token after the token "done"
    if (index_of_fi != end_index - 2 && index_of_fi != end_index - 4)
    {
      error (1, 0, "There are invalid tokend behind the if command!");
    }
    else if (index_of_fi == end_index - 2)
    {
      if (strcmp(token_list[index_of_fi + 1], "<")!=0 && strcmp(token_list[index_of_fi + 1], ">")!=0)
        error (1, 0, "There are invalid tokend behind the if command!");
    }
    else
    {
      if (strcmp(token_list[index_of_fi + 1], "<")!=0 || strcmp(token_list[index_of_fi + 3], ">")!=0)
        error (1, 0, "There are invalid tokend behind the if command!");
    }

    //Create input
    if (strcmp(token_list[index_of_fi + 1], "<")==0)
    {
      if (index_of_fi != end_index-2 && index_of_fi != end_index-4)
        error (1, 0, "If command input reading fails!");
      if_c->input = checked_malloc(sizeof(char) * 30);
      if_c->input = token_list[index_of_fi + 2];
      if (index_of_fi == end_index-4)
      {
        if (strcmp(token_list[end_index - 1], ">")!=0)
          error (1, 0, "If command output reading fails!");
        else
        {
          if_c->output = checked_malloc(sizeof(char) * 30);
          if_c->output = token_list[end_index];
        }
      }
    }
    //Create output
    if (strcmp(token_list[index_of_fi + 1], ">")==0)
    {
      if (index_of_fi != end_index-2)
        error (1, 0, "If command output reading fails!");
      if_c->output = checked_malloc(sizeof(char) * 30);
      if_c->output = token_list[end_index];
    }

  }
  return if_c;
}

//Create an until command based on the token list;
command_t create_until_command(char **token_list, int end_index, int keyword_index[])
{
  command_t until_c = checked_malloc( sizeof(struct command) );
  until_c->type = UNTIL_COMMAND;
  until_c->status = -1;
  until_c->input = NULL;
  until_c->output = NULL;
  int begin = 0;
  int end = 0;

  /*** Create the command A in UNTIL_COMMAND ***/
  begin = keyword_index[0] + 1;
  end = keyword_index[1] - 1;
  until_c->u.command[0] = create_command(token_list, begin, end);

  /*** Create the command B in UNTIL_COMMAND ***/
  begin = keyword_index[1] + 1;
  end = keyword_index[2] - 1;
  until_c->u.command[1] = create_command(token_list, begin, end);

  /*** Create the input and output in UNTIL_COMMAND(if it has) ***/
  if (keyword_index[2] != end_index)
  {
    //Check if there are some invalid token after the token "done"
    if (keyword_index[2] != end_index - 2 && keyword_index[2] != end_index - 4)
    {
      error (1, 0, "There are invalid tokend behind the until command!");
    }
    else if (keyword_index[2] == end_index - 2)
    {
      if (strcmp(token_list[keyword_index[2] + 1], "<")!=0 && strcmp(token_list[keyword_index[2] + 1], ">")!=0)
        error (1, 0, "There are invalid tokend behind the until command!");
    }
    else
    {
      if (strcmp(token_list[keyword_index[2] + 1], "<")!=0 || strcmp(token_list[keyword_index[2] + 3], ">")!=0)
        error (1, 0, "There are invalid tokend behind the until command!");
    }

    //Create input
    if (strcmp(token_list[keyword_index[2] + 1], "<")==0)
    {
      if (keyword_index[2] != end_index-2 && keyword_index[2] != end_index-4)
        error (1, 0, "While command input reading fails!");
      until_c->input = checked_malloc(sizeof(char) * 30);
      until_c->input = token_list[keyword_index[2] + 2];
      if (keyword_index[2] == end_index-4)
      {
        if (strcmp(token_list[end_index - 1], ">")!=0)
          error (1, 0, "While command output reading fails!");
        else
        {
          until_c->output = checked_malloc(sizeof(char) * 30);
          until_c->output = token_list[end_index];
        }
      }
    }
    //Create output
    if (strcmp(token_list[keyword_index[2] + 1], ">")==0)
    {
      if (keyword_index[2] != end_index-2)
        error (1, 0, "Fails!");
      until_c->output = checked_malloc(sizeof(char) * 30);
      until_c->output = token_list[end_index];
    }
  }
     
  return until_c;
}

//Create an while command based on the token list;
command_t create_while_command(char **token_list, int end_index, int keyword_index[])
{
  command_t while_c = checked_malloc( sizeof(struct command) );
  while_c->type = WHILE_COMMAND;
  while_c->status = -1;
  while_c->input = NULL;
  while_c->output = NULL;
  int begin = 0;
  int end = 0;

  /*** Create the command A in WHILE_COMMAND ***/
  begin = keyword_index[0] + 1;
  end = keyword_index[1] - 1;
  while_c->u.command[0] = create_command(token_list, begin, end);

  /*** Create the command B in WHILE_COMMAND ***/
  begin = keyword_index[1] + 1;
  end = keyword_index[2] - 1;
  while_c->u.command[1] = create_command(token_list, begin, end);

  /*** Create the input and output in WHILE_COMMAND(if it has) ***/
  if (keyword_index[2] != end_index)
  {
    //Check if there are some invalid token after the token "done"
    if (keyword_index[2] != end_index - 2 && keyword_index[2] != end_index - 4)
    {
      error (1, 0, "There are invalid tokend behind the while command!");
    }
    else if (keyword_index[2] == end_index - 2)
    {
      if (strcmp(token_list[keyword_index[2] + 1], "<")!=0 && strcmp(token_list[keyword_index[2] + 1], ">")!=0)
        error (1, 0, "There are invalid tokend behind the while command!");
    }
    else
    {
      if (strcmp(token_list[keyword_index[2] + 1], "<")!=0 || strcmp(token_list[keyword_index[2] + 3], ">")!=0)
        error (1, 0, "There are invalid tokend behind the while command!");
    }

    //Create input & output
    if (strcmp(token_list[keyword_index[2] + 1], "<")==0)
    {
      if (keyword_index[2] != end_index-2 && keyword_index[2] != end_index-4)
        error (1, 0, "While command input reading fails!");
      while_c->input = checked_malloc(sizeof(char) * 30);
      while_c->input = token_list[keyword_index[2] + 2];
      if (keyword_index[2] == end_index-4)
      {
        if (strcmp(token_list[end_index - 1], ">")!=0)
          error (1, 0, "While command output reading fails!");
        else
        {
          while_c->output = checked_malloc(sizeof(char) * 30);
          while_c->output = token_list[end_index];
        }
      }
    }
    //Create output
    if (strcmp(token_list[keyword_index[2] + 1], ">")==0)
    {
      if (keyword_index[2] != end_index-2)
        error (1, 0, "While command output reading fails!");
      while_c->output = checked_malloc(sizeof(char) * 30);
      while_c->output = token_list[end_index];
    }
  }
  return while_c;
}

//Create an subshell command based on the token list;
command_t create_subshell_command(char **token_list, int end_index, int keyword_index[])
{
  command_t subshell_c = checked_malloc( sizeof(struct command) );
  subshell_c->type = SUBSHELL_COMMAND;
  subshell_c->status = -1;
  subshell_c->input = NULL;
  subshell_c->output = NULL;
  int begin = 0;
  int end = 0;

  /*** Create the command A in SUBSHELL_COMMAND ***/
  begin = keyword_index[0] + 1;
  end = keyword_index[1] - 1;
  subshell_c->u.command[0] = create_command(token_list, begin, end);

  /*** Create the input and output in SUBSHELL_COMMAND(if it has) ***/
  if (keyword_index[1] != end_index)
  {
    //Check if there are some invalid token after the token "done"
    if (keyword_index[1] != end_index - 2 && keyword_index[1] != end_index - 4)
    {
      error (1, 0, "There are invalid tokend behind the subshell command!");
    }
    else if (keyword_index[1] == end_index - 2)
    {
      if (strcmp(token_list[keyword_index[1] + 1], "<")!=0 && strcmp(token_list[keyword_index[1] + 1], ">")!=0)
        error (1, 0, "There are invalid tokend behind the subshell command!");
    }
    else
    {
      if (strcmp(token_list[keyword_index[1] + 1], "<")!=0 || strcmp(token_list[keyword_index[1] + 3], ">")!=0)
        error (1, 0, "There are invalid tokend behind the subshell command!");
    }

    //Create input
    if (strcmp(token_list[keyword_index[1] + 1], "<")==0)
    {
      if (keyword_index[1] != end_index-2 && keyword_index[1] != end_index-4)
        error (1, 0, "Subshell command input reading fails!");
      subshell_c->input = checked_malloc(sizeof(char) * 30);
      subshell_c->input = token_list[keyword_index[1] + 2];
      if (keyword_index[1] == end_index-4)
      {
        if (strcmp(token_list[end_index - 1], ">")!=0)
          error (1, 0, "Subshell command output reading fails!");
        else
        {
          subshell_c->output = checked_malloc(sizeof(char) * 30);
          subshell_c->output = token_list[end_index];
        }
      }
    }
    //Create output
    if (strcmp(token_list[keyword_index[1] + 1], ">")==0)
    {
      if ((keyword_index[1] + 1) != end_index-1)
        error (1, 0, "Subshell command output reading fails!");
      subshell_c->output = checked_malloc(sizeof(char) * 30);
      subshell_c->output = token_list[end_index];
    }
  }

  return subshell_c;
}

//Create an pipe command based on the token list;
command_t create_pipe_command(char **token_list, int begin_index, int end_index, int keyword_index[])
{
  command_t pipe_c = checked_malloc( sizeof(struct command) );
  pipe_c->type = PIPE_COMMAND;
  pipe_c->status = -1;
  pipe_c->input = NULL;
  pipe_c->output = NULL;
  int begin = 0;
  int end = 0;

  /*** Create the command A in PIPE_COMMAND ***/
  begin = begin_index;
  end = keyword_index[0] - 1;
  pipe_c->u.command[0] = create_command(token_list, begin, end);

  /*** Create the command B in PIPE_COMMAND ***/
  begin = keyword_index[0] + 1;
  end = end_index;
  pipe_c->u.command[1] = create_command(token_list, begin, end);
  return pipe_c;
}

//Create an sequence command based on the token list;
command_t create_sequence_command(char **token_list, int begin_index, int end_index, int keyword_index[])
{
  command_t sequence_c = checked_malloc( sizeof(struct command) );
  sequence_c->type = SEQUENCE_COMMAND;
  sequence_c->status = -1;
  sequence_c->input = NULL;
  sequence_c->output = NULL;
  int begin = 0;
  int end = 0;

  /*** Create the command A in SEQUENCE_COMMAND ***/
  begin = begin_index;
  end = keyword_index[0] - 1;
  sequence_c->u.command[0] = create_command(token_list, begin, end);

  /*** Create the command B in SEQUENCE_COMMAND ***/
  begin = keyword_index[0] + 1;
  end = end_index;
  sequence_c->u.command[1] = create_command(token_list, begin, end);
  return sequence_c;
}

//Create an simple command based on the token list;
command_t create_simple_command(char **token_list, int begin_index, int end_index)
{
  command_t simple_c = checked_malloc( sizeof(struct command) );
  simple_c->type = SIMPLE_COMMAND;
  simple_c->status = -1;
  simple_c->input = NULL;
  simple_c->output = NULL;
  int word_index = 0;
  int token_list_index;

  if (begin_index > end_index)
    error (1, 0, "Simple command reading fails!");

  /*** Create the words, input and output in SIMPLE_COMMAND(if it has) ***/
  simple_c->u.word = checked_malloc(sizeof(char*) * (end_index - begin_index + 1 + 5));
  for (word_index = 0, token_list_index = begin_index; token_list_index <= end_index; ++word_index, ++token_list_index)
  {
    if (strcmp(token_list[token_list_index], "<")==0)
    {
      if (token_list_index != end_index-1 && token_list_index != end_index-3)
      {
        error (1, 0, "Simple command input reading fails!");
      }
      simple_c->input = checked_malloc(sizeof(char) * 30);
      simple_c->input = token_list[token_list_index + 1];
      if (token_list_index == end_index-3)
      {
        if (strcmp(token_list[end_index - 1], ">")!=0)
          error (1, 0, "Simple command output reading fails!");
        else
        {
          simple_c->output = checked_malloc(sizeof(char) * 30);
          simple_c->output = token_list[end_index];
        }
      }
      break;
    } 
    else if(strcmp(token_list[token_list_index], ">")==0)
    {
      if (token_list_index != end_index-1)
        error (1, 0, "Simple command output reading fails!");
      else
      {
        simple_c->output = checked_malloc(sizeof(char) * 30);
        simple_c->output = token_list[token_list_index + 1];
        break;
      }
    }
    simple_c->u.word[word_index] = token_list[token_list_index];
  }

  return simple_c;
}