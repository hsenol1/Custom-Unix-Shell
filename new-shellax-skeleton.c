#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h> // termios, TCSANOW, ECHO, ICANON
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <assert.h>
#define BUFFER_SIZE 1024
#define _OPEN_SYS_ITOA_EXT


const char *sysname = "shellax";

enum return_codes {
  SUCCESS = 0,
  EXIT = 1,
  UNKNOWN = 2,
};

struct command_t {
  char *name;
  bool background;
  bool auto_complete;
  int arg_count;
  char **args;
  char *redirects[3];     // in/out redirection
  struct command_t *next; // for piping
};

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command) {
  int i = 0;
  printf("Command: <%s>\n", command->name);
  printf("\tIs Background: %s\n", command->background ? "yes" : "no");
  printf("\tNeeds Auto-complete: %s\n", command->auto_complete ? "yes" : "no");
  printf("\tRedirects:\n");
  for (i = 0; i < 3; i++)
    printf("\t\t%d: %s\n", i,
           command->redirects[i] ? command->redirects[i] : "N/A");
  printf("\tArguments (%d):\n", command->arg_count);
  for (i = 0; i < command->arg_count; ++i)
    printf("\t\tArg %d: %s\n", i, command->args[i]);
  if (command->next) {
    printf("\tPiped to:\n");
    print_command(command->next);
  }
}
/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command) {
  if (command->arg_count) {
    for (int i = 0; i < command->arg_count; ++i)
      free(command->args[i]);
    free(command->args);
  }
  for (int i = 0; i < 3; ++i)
    if (command->redirects[i])
      free(command->redirects[i]);
  if (command->next) {
    free_command(command->next);
    command->next = NULL;
  }
  free(command->name);
  free(command);
  return 0;
}
/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt() {
  char cwd[1024], hostname[1024];
  gethostname(hostname, sizeof(hostname));
  getcwd(cwd, sizeof(cwd));
  printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
  return 0;
}
/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command) {
  const char *splitters = " \t"; // split at whitespace
  int index, len;
  len = strlen(buf);
  while (len > 0 && strchr(splitters, buf[0]) != NULL) // trim left whitespace
  {
    buf++;
    len--;
  }
  while (len > 0 && strchr(splitters, buf[len - 1]) != NULL)
    buf[--len] = 0; // trim right whitespace

  if (len > 0 && buf[len - 1] == '?') // auto-complete
    command->auto_complete = true;
  if (len > 0 && buf[len - 1] == '&') // background
    command->background = true;

  char *pch = strtok(buf, splitters);
  if (pch == NULL) {
    command->name = (char *)malloc(1);
    command->name[0] = 0;
  } else {
    command->name = (char *)malloc(strlen(pch) + 1);
    strcpy(command->name, pch);
  }

  command->args = (char **)malloc(sizeof(char *));

  int redirect_index;
  int arg_index = 0;
  char temp_buf[1024], *arg;
  while (1) {
    // tokenize input on splitters
    pch = strtok(NULL, splitters);
    if (!pch)
      break;
    arg = temp_buf;
    strcpy(arg, pch);
    len = strlen(arg);

    if (len == 0)
      continue; // empty arg, go for next
    while (len > 0 && strchr(splitters, arg[0]) != NULL) // trim left whitespace
    {
      arg++;
      len--;
    }
    while (len > 0 && strchr(splitters, arg[len - 1]) != NULL)
      arg[--len] = 0; // trim right whitespace
    if (len == 0)
      continue; // empty arg, go for next

    // piping to another command
    if (strcmp(arg, "|") == 0) {
      struct command_t *c = malloc(sizeof(struct command_t));
      int l = strlen(pch);
      pch[l] = splitters[0]; // restore strtok termination
      index = 1;
      while (pch[index] == ' ' || pch[index] == '\t')
        index++; // skip whitespaces
      //printf("HEYHEY: \n%s\n",buf);
      //printf("%s\n", pch + index); 
      parse_command(pch + index, c);
      pch[l] = 0; // put back strtok termination
      command->next = c;
      continue;
    }

    // background process
    if (strcmp(arg, "&") == 0)
      continue; // handled before

    // handle input redirection
    redirect_index = -1;
    if (arg[0] == '<')
      redirect_index = 0;
    if (arg[0] == '>') {
      if (len > 1 && arg[1] == '>') {
        redirect_index = 2;
        arg++;
        len--;
      } else
        redirect_index = 1;
    }
    if (redirect_index != -1) {
      command->redirects[redirect_index] = malloc(len);
      strcpy(command->redirects[redirect_index], arg + 1);
      continue;
    }

    // normal arguments
    if (len > 2 &&
        ((arg[0] == '"' && arg[len - 1] == '"') ||
         (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
    {
      arg[--len] = 0;
      arg++;
    }
    
    command->args = (char **) realloc(command->args,sizeof(char*)*(arg_index+1));
    command->args[arg_index] = (char *)malloc(len + 1);
    strcpy(command->args[arg_index++], arg);
  }
  command->arg_count = arg_index;
  return 0;
}

void prompt_backspace() {
  putchar(8);   // go back 1
  putchar(' '); // write empty over
  putchar(8);   // go back 1 again
}
/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command) {
  int index = 0;
  char c;
  char buf[4096];
  static char oldbuf[4096];

  // tcgetattr gets the parameters of the current terminal
  // STDIN_FILENO will tell tcgetattr that it should write the settings
  // of stdin to oldt
  static struct termios backup_termios, new_termios;
  tcgetattr(STDIN_FILENO, &backup_termios);
  new_termios = backup_termios;
  // ICANON normally takes care that one line at a time will be processed
  // that means it will return if it sees a "\n" or an EOF or an EOL
  new_termios.c_lflag &=
      ~(ICANON |
        ECHO); // Also disable automatic echo. We manually echo each char.
  // Those new settings will be set to STDIN
  // TCSANOW tells tcsetattr to change attributes immediately.
  tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

  show_prompt();
  buf[0] = 0;
  while (1) {
    c = getchar();
    // printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

    if (c == 9) // handle tab
    {
      buf[index++] = '?'; // autocomplete
      break;
    }

    if (c == 127) // handle backspace
    {
      if (index > 0) {
        prompt_backspace();
        index--;
      }
      continue;
    }

    if (c == 27 || c == 91 || c == 66 || c == 67 || c == 68) {
      continue;
    }

    if (c == 65) // up arrow
    {
      while (index > 0) {
        prompt_backspace();
        index--;
      }

      char tmpbuf[4096];
      printf("%s", oldbuf);
      strcpy(tmpbuf, buf);
      strcpy(buf, oldbuf);
      strcpy(oldbuf, tmpbuf);
      index += strlen(buf);
      continue;
    }

    putchar(c); // echo the character
    buf[index++] = c;
    if (index >= sizeof(buf) - 1)
      break;
    if (c == '\n') // enter key
      break;
    if (c == 4) // Ctrl+D
      return EXIT;
  }
  if (index > 0 && buf[index - 1] == '\n') // trim newline from the end
    index--;
  buf[index++] = '\0'; // null terminate string

  strcpy(oldbuf, buf);

  parse_command(buf, command);

  // print_command(command); // DEBUG: uncomment for debugging

  // restore the old settings
  tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
  return SUCCESS;
}
int process_command(struct command_t *command);
void run_command(struct command_t *command);
int main() {
  while (1) {
    struct command_t *command = malloc(sizeof(struct command_t));
    memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

    int code;
    code = prompt(command);
    if (code == EXIT)
      break;

    code = process_command(command);
    if (code == EXIT)
      break;

    free_command(command);
  }

  printf("\n");
  return 0;
}
void wiseman(struct command_t * command) {
	char* temp;
	struct command_t *new_command = malloc(sizeof(struct command_t));
	memset(new_command, 0, sizeof(struct command_t));
	temp = "echo";
	new_command->name = (char *)malloc(strlen(temp) + 1);
    	strcpy(new_command->name, temp);
	new_command->arg_count = 1;
	temp = (char*) malloc(sizeof(char)*29);
	strcat(temp, "*/");
	strcat(temp, command->args[0]);
	strcat(temp, " * * * * fortune | espeak");
	new_command->args = (char **) malloc(sizeof(char*));
	new_command->args[0] = temp;
	
	struct command_t *crontab_command = malloc(sizeof(struct command_t));
	memset(crontab_command, 0, sizeof(struct command_t));
	temp = "crontab";
	crontab_command->name = (char *)malloc(strlen(temp) + 1);
    	strcpy(crontab_command->name, temp);
	crontab_command->arg_count = 1;
    	temp = "-";
	crontab_command->args = (char **) malloc(sizeof(char*));
	crontab_command->args[0] = temp;
	
	new_command->next = crontab_command;
	process_command(new_command);
}

int draw_tree(char* pid,char* output_file) {
	FILE *in = fopen("process_info.txt", "r");		
	FILE *out = fopen("graph.dot","w");
	fprintf(out,"graph G{\n");
	char line[100];
	int flag = 0;
	int safety = 0;
	unsigned long x = 9999999999;
	char *ptr;
	int line_counter = 0;
	char buffer[sizeof(unsigned long) * 8 +1];
	char selected[24] = "random_string";
	
	while (fgets(line,sizeof(line),in)) {
			char* tok = strtok(line,"]");
			char* tok4 = strtok(NULL,"]");
			char* tok1 = strtok(tok4," ");
			char* tok2 = strtok(NULL," ");
			char* tok3 = strtok(NULL," ");
			line_counter += 1;
			
			if (strcmp(tok2,pid) == 0) {
				unsigned long iterator = strtoul(tok3,&ptr,10);
				if (iterator <= x) {
					x = iterator;
				}
			}
	}
	
	const int n = snprintf(NULL, 0, "%lu", x);
	assert(n > 0);
	char buf[n+1];
	int c = snprintf(buf, n+1, "%lu", x);
	assert(buf[n] == '\0');
	assert(c == n);
	
	fclose(in);
	FILE *bin = fopen("process_info.txt","r");
	while (fgets(line,sizeof(line),bin)) {
	
		
		if (line_counter == 0) {
			printf("File is empty.");
			}
			
		else if (line_counter == 1) {
			char* tok = strtok(line," ");
			char* tok1 = strtok(NULL," "); // pid
			char* tok2 = strtok(NULL," "); // ppid
			char* tok3 = strtok(NULL," "); // time.

			fprintf(out, "%s  -- %s\n", tok1,tok2);
			break;
		}
		else {
			if (flag == 0) {
				flag += 1;
			}
			else {
				char* tok = strtok(line,"]");
				char* tok4 = strtok(NULL,"]");
				char* tok1 = strtok(tok4," ");
				char* tok2 = strtok(NULL," ");
				 char* tok3 = strtok(NULL," ");
				 char* p = strchr(tok3, '\n');
				 if (p != NULL) {
				 	*p = '\0';
				 }
				 
				 if (strcmp(buf,tok3) == 0) {
				 	strcpy(selected,tok1);
				 
				 }
				if (strcmp(tok1,pid) != 0) {
				fprintf(out, "\"%s\" -- \"%s\"\n", tok1 ,tok2);}
				fprintf(out, "\"%s\" [label= \"%s time= %s\"]\n",tok1,tok1, tok3);
			}
		}
	}
	fprintf(out,"%s [color = \"red\"]\n", selected);
	fprintf(out, "}\n");
	fclose(in);
	fclose(out);
	char buffer2[80] = "dot -Tpng graph.dot -o ";
	strcat(buffer2,output_file);
	strcat(buffer2,".png");
	system(buffer2);
	return 0;
}


int psvis(char* pid,char* output) {
	char command[100] = "make;";
	strcat(command, "sudo dmesg -c;");
	strcat(command, "sudo insmod mymodule.ko pid=");
	strcat(command, pid);
	strcat(command,";");
	strcat(command,"sudo dmesg -c > process_info.txt;");
	strcat(command, "sudo rmmod mymodule");
	system(command);

	int counter = 0;
	char* output_name = output;

	draw_tree(pid,output_name);

	return 0;
}



void uniq(struct command_t *command) {
  char *str = NULL;
  size_t size = 0;
  size_t lines = 0;
  ssize_t len= 0;
  char** arr = NULL;
  char** distinct_strings = NULL;

  while ((len= getline(&str,&size, stdin)) != -1) {
    arr = realloc(arr, sizeof * arr * lines + 1);
    arr[lines++] = strdup(str);
  }

  int* occurence= NULL;
  char* new;
  char* old = "random_string";
  int num_results = 0;
  int counter = -1;
  int lines2 = 0;

  for (int i = 0; i < lines; i++) {
    new = arr[i];
    if (strcmp(new,old) != 0) {
      old = new;
      distinct_strings = realloc(distinct_strings, sizeof * distinct_strings * lines2 + 1);
      distinct_strings[lines2++] = strdup(old);
      num_results++;
      counter++;
      occurence = realloc(occurence, sizeof * occurence * counter +1);
      occurence[counter] = 1; 
    } else { occurence[counter] += 1;}
  }

  for (int k = 0; k < num_results; k++) {
    if (command->arg_count) {
      if ((strcmp(command->args[0],"-c")==0) || (strcmp(command->args[0],"--count")==0) ) {printf("%d ",occurence[k]);}
    }
    printf("%s",distinct_strings[k]);
  }
}

void openai(struct command_t *command) { // custom command 
  // give the input with - in place of spaces
  // e.g. openai a-man-giving-his-son-money-for-school
  // necessary libraries must be installed for python
  size_t init = 0;
  char *inp = NULL;
  if (command->arg_count == 0) {getline(&inp, &init, stdin);} 
  else {inp = command->args[0];}
  printf("%s\n",inp);
  execlp("python", "python", "openai_code.py",inp,(char*) NULL);
}


void get_crypto_info(struct command_t *command) { // custom command
// Explained in report. It gives desired parameters and information about selected financial instrument.

	char* stock = command->args[0];

	
	char* screener = command->args[1];
	
	char* exchange = command->args[2];

	
	char run_it[150] = "python3 crypto.py ";
	
	strcat(run_it,stock);
	strcat(run_it," ");
	strcat(run_it,screener);
	strcat(run_it," ");
	strcat(run_it,exchange);
	system(run_it);
	
}
int directoryExists(const char *path)
{
  struct stat stats;
  stat(path, &stats);
  if (S_ISDIR(stats.st_mode))
        return 1;

  return 0;
}
bool fileExists(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    bool is_exist = false;
    if (fp != NULL)
    {
        is_exist = true;
        fclose(fp); // close the file
    }
    return is_exist;
}
void chatroom(struct command_t *command) {
  char room_name_buf[32];
  char hist_name_buf[32];
  char user_pipe_buf[32];
  char buff[1024];
  char msg[1024];
  char* reader = NULL;
  FILE *name_fp;
  if (directoryExists("tmp") == 0) {int check0 = mkdir("tmp", 0777);}
  strcpy(room_name_buf,"tmp/");
  strcat(room_name_buf,command->args[0]);
  strcpy(hist_name_buf, "tmp/");
  strcat(hist_name_buf, command->args[0]);
  strcat(hist_name_buf, "/history");
  strcpy(user_pipe_buf,room_name_buf);
  strcat(user_pipe_buf,"/");
  strcat(user_pipe_buf, command->args[1]);
  if (directoryExists(room_name_buf) == 0) {int check = mkdir(room_name_buf, 0777);}
  if (fileExists(user_pipe_buf) == 0) {
    mkfifo(user_pipe_buf, 0777);
    char welcome_msg[1024];
    //strcpy(welcome_msg,"Welcome to ");
    //strcat(welcome_msg,command->args[0]);
    //strcat(welcome_msg,"!");
    //strcat(welcome_msg,"\0");
    //int fd = open(user_pipe_buf, O_NONBLOCK | O_RDWR);
    //write(fd,welcome_msg, strlen(welcome_msg));
    //close(fd);
  }

  int pid = fork();
  if (pid == 0) {
    int fd_child;
    fd_child = open(user_pipe_buf, O_RDONLY);
    while (1) {
      reader = realloc(reader,1024*sizeof(char));
      memset(reader, 0, 1024*sizeof(char));
      read(fd_child, reader, 1024);
      if (strlen(reader)>0) {
        printf("\33[2K");
        printf("\r");
        printf("%s\n",reader);
        printf("[");
        printf("%s",command->args[0]);
        printf("] ");
        printf("%s",command->args[1]);
        printf(" > ");
        fflush(stdout);
      }
      fflush(stdout);
    }
    close(fd_child);
    exit(0);
  } else {
    int fd_parent;
    char file_name[256];
    struct dirent *de;
    DIR *dr;
    while (1) {
      strcpy(buff,"[");
      strcat(buff,command->args[0]);
      strcat(buff,"] ");
      strcat(buff,command->args[1]);
      strcat(buff," > ");
      printf("%s ", buff);
      fflush(stdout);
      scanf("%s",msg);
      if (strcmp(msg,"exit")==0) {break;}
      strcat(buff, msg);
      strcat(buff,"\0");
      dr = opendir(room_name_buf);
      while ((de = readdir(dr)) != NULL) {
        if ((isalpha(de->d_name[0]) != 0) && (strcmp(de->d_name,command->args[1])!=0)) {
          strcpy(file_name, room_name_buf);
          strcat(file_name, "/");
          strcat(file_name, de->d_name);
          fd_parent = open(file_name, O_NONBLOCK | O_WRONLY);
          write(fd_parent, buff, strlen(buff));
          close(fd_parent);
        }
        file_name[0] = '\0';
      }
      closedir(dr);
      buff[0] = '\0';
    }
    remove(user_pipe_buf);
    kill(pid, SIGKILL);
    exit(0);
  }
}


void run_command(struct command_t * command) {
      
      if (command->redirects[0] != NULL) {
      	dup2(fileno(fopen(command->redirects[0],"r")),STDIN_FILENO);
      }
      if (command->redirects[1] != NULL) {
      	dup2(fileno(fopen(command->redirects[1],"w")),STDOUT_FILENO);
      }
      if (command->redirects[2] != NULL) {
    	dup2(fileno(fopen(command->redirects[2],"a")),STDOUT_FILENO);
      }

      // TODO: do your own exec with path resolving using execv()
      // do so by replacing the execvp call below
      
      if (strcmp(command->name, "wiseman") == 0) {
      	wiseman(command);
  	    exit(0);
      }
      if (strcmp(command->name, "uniq") == 0) {
  	    uniq(command);
        exit(0);
      }
      if (strcmp(command->name, "openai") == 0) {
        openai(command);
        exit(0);
      } 
      if (strcmp(command->name, "chatroom") == 0) {
        chatroom(command);
        exit(0);
      }
      if (strcmp(command->name, "crypto") == 0) {   
        get_crypto_info(command);
        exit(0);
      } 
      if (strcmp(command->name, "psvis") == 0) {
  	char *pid = command->args[0];
  	char *output = command->args[1];
  	psvis(pid,output);
  	exit(0);
      }

      
      // increase args size by 2
      command->args = (char **)realloc(
      command->args, sizeof(char *) * (command->arg_count += 2));

      // shift everything forward by 1
      for (int i = command->arg_count - 2; i > 0; --i)
        command->args[i] = command->args[i - 1];

      // set args[0] as a copy of name
      command->args[0] = strdup(command->name);
      // set args[arg_count-1] (last) to NULL
      command->args[command->arg_count - 1] = NULL;
     

      if (command->args[0][0] == '.')
      {
        command->args[0] += 2;
        execv(command->args[0], command->args);
      }
      else
      {
        char *general_path = getenv("PATH");
        char *token = strtok(general_path, ":");
        int len2 = strlen(command->args[0]);
        while (true)
        {
          int len1 = strlen(token);
          char *command_path = malloc(len1 + len2 + 2);
          strcpy(command_path, token);
          strcat(command_path, "/");
          strcat(command_path, command->args[0]);
          execv(command_path, command->args);
          free(command_path);
          token = strtok(NULL, ":");
          if (token == NULL) {break;}
        }
        free(general_path);
      }
}

int process_command(struct command_t *command) {
  int r;
  if (strcmp(command->name, "") == 0)
    return SUCCESS;

  if (strcmp(command->name, "exit") == 0)
    return EXIT;

  if (strcmp(command->name, "cd") == 0) {
    if (command->arg_count > 0) {
      r = chdir(command->args[0]);
      if (r == -1)
        printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
      return SUCCESS;
    }
  }
  
  
  pid_t pid = fork();
  if (pid == 0) // child
  { 
    while (command->next != NULL) {
      int fd[2];
      int pid_2;
      if (pipe(fd) < 0) {fprintf(stderr,"Pipe failed.");}
      pid_2 = fork();
      if (pid_2 == 0) {
      	dup2(fd[1], STDOUT_FILENO);
      	close(fd[0]);
      	run_command(command);
      	exit(0);
      } else {
        wait(NULL);
      	close(fd[1]);
      	dup2(fd[0],STDIN_FILENO);
      	command = command->next;
      }
      
    }
    run_command(command);
    exit(0);
  } else {
      if (!command->background) {waitpid(pid, NULL, 0);}
    return SUCCESS;
  }

  printf("-%s: %s: command not found\n", sysname, command->name);
  return UNKNOWN;

}







