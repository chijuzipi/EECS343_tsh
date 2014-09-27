#include <stdio.h>


char* single_param(char *st);
//void parser_single(char *c, int sz, commandT** cd, int bg);

int main()
{
  //char *first;
  char input[] ="sdfa sadfads";
  //first = input;
  //printf("%d", single_param(&input));
  printf(single_param(input));
  /*
  printf("\n");
  first = first+ 7;
  printf(single_param(first));
  */
  return 0;
}

/*Parse the single command and call single_param to parse each word in the command*/
/*
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
    (*cd) -> argv[i] = strdup(single_param(tmp));
    while ((*tmp) != '\0') tmp++;
    tmp ++;
  }
  if(in){
    (*cd) -> is_redirect_in = 1;
    (*cd) -> redirect_in = strdup(single_param(in));
  }
  if(out){
    (*cd) -> is_redirect_out = 1;
    (*cd) -> redirect_out = strdup(single_param(out));
  }
}
*/

/*Parse a single word from the param. Get rid of '"' or '''*/
char* single_param(char *st)
{
  int quot1 = 0,quot2 = 0, start = 0;
  char *t = st;
  static int idx;

  idx = 0;
  while(1){
    if(start == 1){
      if(st[idx] == '\0') return t;
      if(st[idx] == '<' || st[idx] == '>') {st[idx] = '\0'; return t;}
      if(st[idx] == ' ' && quot1 == 0 && quot2 == 0) {st[idx] = '\0';return t;}
      if(st[idx] == '\'' && quot1 == 1) {st[idx] = '\0';return t;}
      if(st[idx] == '"' && quot2 == 1) {st[idx] = '\0';return t;}
    }
    else{
      if(st[idx] == ' ' || st[idx] =='\0');
      else if(st[idx] == '"') {quot2 = 1; start = 1; t = &(st[idx+1]);}
      else if(st[idx] == '\'') {quot1 = 1; start = 1; t = &(st[idx+1]);}
      else {start = 1; t = &(st[idx]);}
    }
    idx++;
  }
}
