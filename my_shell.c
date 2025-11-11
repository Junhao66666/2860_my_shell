#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

/* Print the prompt ">>> " and read a line of characters
   from stdin. */
int getcmd(char *buf, int nbuf) {

  fprintf(2, ">>> ");
  gets(buf, nbuf);
   if (buf[0] == 0){
    return -1;
   }
  return 0;
}

/*
  A recursive function which parses the command
  at *buf and executes it.
*/
__attribute__((noreturn))
void run_command(char *buf, int nbuf, int *pcp) {

  /* Useful data structures and flags. */
  char *arguments[10];
  int numargs = 0;
  /* Flags to mark word start/end */
  int ws = 1;
  int we = 0;

  /* Flags to mark redirection direction */
  int redirection_left = 0;
  int redirection_right = 0;

  /* File names supplied in the command */
  char *file_name_l = 0;
  char *file_name_r = 0;

  int p[2];
  int pipe_cmd = 0;

  /* Flag to mark sequence command */
  int sequence_cmd = 0;

  int i = 0;
  /* Parse the command character by character. */
  for (; i < nbuf; i++) {
    if(buf[i] == 0 || buf[i] == '\n' || buf[i] == '\r'){
      buf[i] = 0;
      break;
    }
    if(buf[i] == ';'){
      buf[i] = 0;
      sequence_cmd = 1;
      break;
    }
    if (buf[i] == '|'){
      buf[i] = 0;
      pipe_cmd = 1;
      break;
    }

    if (!(redirection_left || redirection_right)) {
      /* No redirection, continue parsing command. */
      if(buf[i] == '<'){
        redirection_left = 1;
        buf[i] = 0;
        ws = 1;
        continue;
      }
      if(buf[i] == '>'){
        redirection_right = 1;
        buf[i] = 0;
        ws = 1;
        continue;
      }
      if(buf[i] == ' '){
        buf[i] = 0;
        ws = 1;
        we = 1;
        continue;
      }
      if (ws == 1){
        arguments[numargs++] = &buf[i];
        ws = 0;
        we = 0;
      }
      if (numargs >= 10) break;

    } else {
      /* Redirection command. Capture the file names. */
      if(buf[i] == ' '){
        redirection_left = 0;
        redirection_right = 0;
        ws = 1;
        continue;
      }

      if (redirection_left && file_name_l == 0) {
        file_name_l = &buf[i];
      }
      if (redirection_right && file_name_r == 0) {
        file_name_r = &buf[i];
      }
    }
  }
  arguments[numargs] = 0;
  if(we){ we = 0; }
  /*
    Sequence command. Continue this command in a new process.
    Wait for it to complete and execute the command following ';'.
  */
  if (sequence_cmd) {
    sequence_cmd = 0;
    if (fork() != 0) {
      wait(0);
      run_command(&buf[i+1], nbuf-(i+1), pcp);
      exit(0);
    }
    else{
      run_command(buf, nbuf, pcp);
      exit(0);
    }
  }

  /*
    If this is a redirection command,
    tie the specified files to std in/out.
  */
  if (redirection_left) {
    close(0);
      open(file_name_l, O_RDONLY);
  }
  if (redirection_right) {
    close(1);
    open(file_name_r, O_WRONLY | O_CREATE);
  }

  /* Parsing done. Execute the command. */

  /*
    If this command is a CD command, write the arguments to the pcp pipe
    and exit with '2' to tell the parent process about this.
  */
  if (strcmp(arguments[0], "cd") == 0) {
    if(numargs < 2){
      fprintf(2, "cd: Missing\n");
      exit(0);
    }
    write(pcp[1], arguments[1], strlen(arguments[1]));
    write(pcp[1], "\n", 1);
    exit(2); 
  } else {
    /*
      Pipe command: fork twice. Execute the left hand side directly.
      Call run_command recursion for the right side of the pipe.
    */
    if (pipe_cmd) {
      pipe(p);

      if (fork() == 0) {
        close(1);
        dup(p[1]);
        close(p[0]);
        close(p[1]);
        if (arguments[0]) {
          exec(arguments[0], arguments);
          printf("exec failed\n");
        }
        exit(1);
      }

      if (fork() == 0) {
        close(0);
        dup(p[0]);
        close(p[1]);
        close(p[0]);
        run_command(&buf[i+1], nbuf-(i+1), pcp);
      }

      close(p[0]);
      close(p[1]);
      wait(0);
      wait(0);
      exit(0);
    }
    else {
      if(fork() == 0){
        exec(arguments[0], arguments);
        printf("exec failed\n");
        exit(1);
      }
      wait(0);
    }
  }
  exit(0);
}

int main(void) {

  static char buf[100];

  int pcp[2];
  pipe(pcp);

  /* Read and run input commands. */
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(fork() == 0){
      run_command(buf, 100, pcp);
    }
    /*
      Check if run_command found this is
      a CD command and run it if required.
    */
    int child_status;
    wait(&child_status);

    if (child_status == 2) {
      char path[100];
      int n = read(pcp[0], path, sizeof(path));
      if (n > 0) {
        path[n - 1] = 0;
        chdir(path);
      }
    }
  }
  exit(0);
}

