/* -*-C-*-
 *******************************************************************************
 *
 * File:         interpreter.c
 * RCS:          $Id: interpreter.c,v 1.1 2009/10/09 04:38:08 npb853 Exp $
 * Description:  Handles the input from stdin
 * Author:       Stefan Birrer
 *               AquaLab Research Group
 *               EECS
 *               Northwestern University
 * Created:      Fri Oct 06, 2006 at 13:33:58
 * Modified:     Fri Oct 06, 2006 at 13:39:14 fabianb@cs.northwestern.edu
 * Language:     C
 * Package:      N/A
 * Status:       Experimental (Do Not Distribute)
 *
 * (C) Copyright 2006, Northwestern University, all rights reserved.
 *
 *******************************************************************************
 */
#define __INTERPRETER_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
/************Private include**********************************************/
#include "interpreter.h"
#include "io.h"
#include "runtime.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */
typedef struct string_l {
  char* s;
  struct string_l* next;
} stringL;

/*Parse a single word from the param. Get rid of '"' or '''*/
char* single_param(char *st)
{
  int quot1 = 0,quot2 = 0, start = 0;
  char *t = st;
  static int idx;
  bool firstletterisQuot = 0;
  idx = 0;
  while(1){
    if(start == 1){
      if(st[idx] == '\0') return t;
      if(st[idx] == '<' || st[idx] == '>') {st[idx] = '\0'; return t;}
      if(st[idx] == ' ' && quot1 == 0 && quot2 == 0) {st[idx] = '\0';return t;}
      if(st[idx] == '\'' && quot1 == 1) {
        if(firstletterisQuot == 1){
          st[idx] = '\0';
          return t;
        }
        else {
          st[idx+1] = '\0';
          return t;
        }
      }        
      if(st[idx] == '\'' && quot1 == 0) {quot1 = 1;}
      if(st[idx] == '"' && quot2 == 1) {
        if (firstletterisQuot == 1) {
          st[idx] = '\0'; 
        return t;
        }
        else{
          st[idx+1] = '\0';
          return t;
        }
      }
      if(st[idx] == '"' && quot2 == 0) {quot2 = 1;}
    }
    else{
      if(st[idx] == ' ' || st[idx] =='\0');
      else if(st[idx] == '"') {quot2 = 1; start = 1; t = &(st[idx+1]); firstletterisQuot = 1;}
      else if(st[idx] == '\'') {quot1 = 1; start = 1; t = &(st[idx+1]); firstletterisQuot = 1;}
      else {start = 1; t = &(st[idx]);}
    }
    idx++;
  }
}

char * translatePath(char * c) {
  if (c[0] == '~') {
  //printf("in translatePath \n");
	char * newpath = (char *)malloc(sizeof(char) * (MAXLINE));

  newpath = strdup(getenv("HOME"));
  //printf("left is %s\n", newpath);

	char *right = c + 1;
  //printf("right is %s\n", right);
	
  newpath = strcat(newpath, right);

  //c[strlen(left)] = rigth;
	//printf("path is %s\n", newpath);
	return newpath;    
	}
  else return c;
}
/*Parse the single command and call single_param to parse each word in the command*/
//parser_single(&(cmdLine[i-j]), j, &(command[task]),bg);
void parser_single(char *c, int sz, commandT** cd, int bg)
{
  int i, task_argc = 0, quot1 = 0, quot2 = 0;
  char *in = NULL, * out = NULL, *tmp;
  int cmd_length;
  c[sz] = '\0';
  for(i = 0; i < sz; i++)
    if(c[i] != ' ') break;
  c = &(c[i]);
  sz = sz - i;
  cmd_length = sz;
  for(i = 0; i < sz; i++){
    if(c[i] == '\''){
      if(quot2) continue;
      else if(quot1){
        quot1 = 0;continue;
      }
      else quot1 = 1;
    }
    if(c[i] == '"'){
      if(quot1) continue;
      else if(quot2){
        quot2 = 0;continue;
      }
      else quot2 = 1;
    }
    if(c[i] == '<' && quot1 != 1 && quot2 != 1){
      if(cmd_length == sz) cmd_length = i; 
      while(i < (sz - 1) && c[i + 1] == ' ') i++;
     in = &(c[i+1]);
    }
    if(c[i] == '>' && quot1 != 1 && quot2 != 1){
      if(cmd_length == sz) cmd_length = i;
      while(i < (sz - 1) && c[i+1] == ' ') i++;
      out = &(c[i+1]);
    }
    if(c[i] == ' ' && quot1 != 1 && quot2 != 1){
      if(cmd_length == sz) task_argc++;
      while(i < (sz - 1) && c[i+1] == ' ') i++;
    }
  }
  if(c[cmd_length - 1] != ' ') task_argc++;
  //printf("%d\n",task_argc);
  (*cd) = CreateCmdT(task_argc);
  (*cd) -> bg = bg;
  (*cd)->cmdline = strdup(c);
  tmp = c;
  for(i = 0; i < task_argc; i++){
    char * newpath = translatePath(strdup(single_param(tmp)));
    (*cd) -> argv[i] = strdup(newpath);
    //free(newpath);
    while ((*tmp) != '\0') tmp++;
    tmp ++;
  }
  if(in){
    (*cd) -> is_redirect_in = 1;
    char * newpath = translatePath(strdup(single_param(in)));
    (*cd) -> redirect_in = strdup(newpath);
    //free(newpath);
  }
  if(out){
    (*cd) -> is_redirect_out = 1;
    char * newpath = translatePath(strdup(single_param(out)));
    (*cd) -> redirect_out = strdup(newpath);
    //free(newpath);
  }
}


/**************Implementation***********************************************/
/*Parse the whole command line and split commands if a piped command is sent.*/
void Interpret(char* cmdLine)
{
  int task = 1;
  int bg = 0, i,k,j = 0, quotation1 = 0, quotation2 = 0;
  commandT **command;

  if(cmdLine[0] == '\0') return;

  for(i = 0; i < strlen(cmdLine); i++){
    if(cmdLine[i] == '\''){
      if(quotation2) continue;
      else if(quotation1){
        quotation1 = 0;continue;
      }
      else quotation1 = 1;
    }
    if(cmdLine[i] == '"'){
      if(quotation1) continue;
      else if(quotation2){
        quotation2 = 0;continue;
      }
      else quotation2 = 1;
    }

    if(cmdLine[i] == '|' && quotation1 != 1 && quotation2 != 1){
      task ++;
    }
  }

  command = (commandT **) malloc(sizeof(commandT *) * task);
  i = strlen(cmdLine) - 1;
  while(i >= 0 && cmdLine[i] == ' ') i--;
  if(cmdLine[i] == '&'){
    if(i == 0) return;
    bg = 1;
    cmdLine[i] = '\0';
  }

  quotation1 = quotation2 = 0;
  task = 0;
  k = strlen(cmdLine);
  for(i = 0; i < k; i++, j++){
    if(cmdLine[i] == '\''){
      if(quotation2) continue;
      else if(quotation1){
        quotation1 = 0;continue;
      }
      else quotation1 = 1;
    }
    if(cmdLine[i] == '"'){
      if(quotation1) continue;
      else if(quotation2){
        quotation2 = 0;continue;
      }
      else quotation2 = 1;
    }
    if(cmdLine[i] == '|' && quotation1 != 1 && quotation2 != 1){
      parser_single(&(cmdLine[i-j]), j, &(command[task]),bg);
      task++;
      j = -1;
    }
  }
  parser_single(&(cmdLine[i-j]), j, &(command[task]),bg);
  
  /* test what is in the command struct
  printf("the name is : %s \n", command[task]->name);
  printf("the cmdline is : %s \n", command[task]->cmdline);
  printf("the argc is : %d \n", command[task]->argc);
  printf("the bg is : %d \n", command[task]->bg);
  printf("the argv1 is : %s \n", command[task]->argv[0]);
  printf("the argv2 is : %s \n", command[task]->argv[1]);
  printf("the argv3 is : %s \n", command[task]->argv[2]);
  printf("the bg is : %d \n", command[task]->bg);
  */
  RunCmd(command, task+1);
  free(command);
}
