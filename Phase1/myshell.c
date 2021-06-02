/* $begin shellmain */
#include "myshell.h"
#include<errno.h>
#define MAXARGS   128
int argc;            /* Number of args */
/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);
void command_ls(char *const argv[]);
void command_cat(char *const argv[]);
void command_touch(char *const argv[]);
void command_echo(char *const argv[]);
int main()
{
    char *pt,cmdline[MAXLINE]; /* Command line */

    while (1) {
    /* Read */
    printf("CSE20171700-SP-P#4> ");
    pt = fgets(cmdline, MAXLINE, stdin);
    if (feof(stdin))
        exit(0);

    /* Evaluate */
    eval(cmdline);
    }
    return 0;
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    
    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
    return;   /* Ignore empty lines */
    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
        execute(argv[0], argv, environ);  //ex) /bin/ls ls -al &

        /* Parent waits for foreground job to terminate */
        if (!bg){
            int status;
        }
        else//when there is backgrount process!
            printf("%d %s", pid, cmdline);
    }
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv)
{
    if (!strcmp(argv[0], "exit")) /* quit command */
    exit(0);
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
    return 1;
    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv)
{
    char *delim;         /* Points to first space delimiter */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
    buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
    argv[argc++] = buf;
    *delim = '\0';
    buf = delim + 1;
    while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
    return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
    argv[--argc] = NULL;

    return bg;
}
/* $end parseline */
/*-------------------------------------------------*/
void unix_error(char *msg) /* Unix-style error */
{
    printf("%s: %s\n", msg, strerror(errno));
    exit(0);
}
pid_t Waitpid(pid_t pid, int *iptr, int options)
{
    pid_t retpid;
    if ((retpid  = waitpid(pid, iptr, options)) < 0)
    unix_error("Waitpid error");
    if(!WIFEXITED(*iptr)){ // child terminate abnormally
        printf("Child %d terminate abnormally\n",retpid);
    }
    return(retpid);
}
pid_t Fork(void){
    pid_t pid;
    if ((pid = fork()) < 0)
    unix_error("Fork error");
    return pid;
}
void command_ls(char *const argv[]){
    
}
void command_cat(char *const argv[]){
    FILE *fp;
    char str[MAXLINE];
    int ch,i;
    if(argv[1] == NULL){ // only "cat" command
        while(fgets(str, MAXLINE, stdin) != NULL && (str[0] != 4)){ //Ctrl + d : exit
            fputs(str,stdout);
            fflush(stdout);
        }
    }
    else{
        if(!(strcmp(argv[1],">"))){ // create file or cover existing file
            if((fp = fopen(argv[2],"w")) == NULL)
                printf("File write failed.\n");
            while(fgets(str, MAXLINE, stdin) != NULL){
                if(str[0] == 4) break; //Ctrl + d : exit
                fprintf(fp,"%s",str);
            }
            fclose(fp);
        }
        else if(!(strcmp(argv[1],">>"))){ //create file or append existing file
            if((fp = fopen(argv[2],"a")) == NULL)
                printf("File write failed.\n");
            while(fgets(str, MAXLINE, stdin) != NULL){
                if(str[0] == 4) break; //Ctrl + d : exit
                fprintf(fp,"%s",str);
            }
            fclose(fp);
        }
        else{ // print files
            for(i=1;i<argc;i++){
                if(argv[i] == NULL) break;
                if((fp = fopen(argv[i],"r")) == NULL)
                    perror(argv[1]);
                else{
                    while((ch = getc(fp)) != EOF)
                        putchar(ch);
                }
                fclose(fp);
            }
        }
    }
    return;
}
void command_touch(char *const argv[]){
    FILE *fp;
    int i;
    if(argv[1] != NULL){ //create files
        for(i=1;i<argc;i++){
            if(argv[i] == NULL) break;
            if((fp = fopen(argv[i],"a")) == NULL)
                perror(argv[1]);
            fclose(fp);
        }
    }
    else{ // print usage
        printf("usage:\ntouch [file]\n");
    }
}
void command_echo(char *const argv[]){
    int i;
    char *ev,*pt,str[MAXLINE],temp[MAXLINE];
    if(argv[1] != NULL){ // echo [string]
        for(i=1;i<argc;i++){
            if(argv[i] == NULL) break;
            strcpy(temp,argv[i]);
            pt = argv[i];
            if((pt[0] == '\''&&pt[strlen(pt)-1] == '\'') || (pt[0] == '\"'&&pt[strlen(pt)-1] == '\"')){
                (argv[i])[strlen(argv[i])-1] = '\0';
                strcpy(temp,argv[i]+1);
            }
            else if(pt[strlen(pt)-1] == '\"' || pt[strlen(pt)-1] == '\''){
                (argv[i])[strlen(argv[i])-1] = '\0';
                strcpy(temp,argv[i]);
            }
            else if(pt[0] == '\"' || pt[0] == '\''){
                strcpy(temp,argv[i]+1);
            }
            else if(pt[0] == '`'){
                /*어케하지*/
            }
            else if(pt[0] == '$' && strlen(pt) != 1){ //'echo 환경변수' 일때
                strcpy(temp,argv[i]+1);
                ev = getenv(temp);
                strcpy(temp,ev);
            }
            if(!(strcmp(pt,"\\\\")))
                strcpy(temp,"\\");
    
            strcat(str,temp);
            strcat(str," ");
           // sprintf(str,"%s ",argv[i]);
        }
        printf("%s\n",str);
    }
    else{ // only "echo" command
        printf("\n");
    }
}
void execute(const char *filename, char *const argv[], char *const envp[]){
    /*if (execve(filename, argv, envp) < 0){
    unix_error("Execve error");
    }*/
    pid_t wpid,pid;
    int status;
    
    if(!(strcmp(filename,"cd"))){ // child에서 어케 구현하냐
       // pid = Fork();
       // if(pid == 0){ //case of child
            
        if(argv[1] == NULL){ // go to home directory
                if(chdir(getenv("HOME"))<0)
                    perror(argv[1]); // error message
        }
        else if(chdir(argv[1]) < 0)
            perror(argv[1]); // error message
        //exit(0);
       // }
        // case of parent
        //wpid = Waitpid(pid,&status,0);
    }
    else{
        pid = Fork();
        if(pid == 0){//case of child
          // printf("<Get in child process>\n");
            if(!(strcmp(filename,"ls"))){
                if(execvp(filename,argv)<0)
                    unix_error("Execvp error");
            }
            else if(!(strcmp(filename,"mkdir"))){
                if(argv[1] == NULL)
                    perror(argv[1]);
                else{
                    if(mkdir(argv[1], 0755))
                        perror(argv[1]);
                }
            }
            else if(!(strcmp(filename,"rmdir"))){
                if(argv[1] == NULL)
                    perror(argv[1]);
                else{
                    if(rmdir(argv[1]))
                        perror(argv[1]);
                }
            }
            else if(!(strcmp(filename,"cat"))){
                command_cat(argv);
            }
            else if(!(strcmp(filename,"touch"))){
                command_touch(argv);
            }
            else if(!(strcmp(filename,"echo"))){
                command_echo(argv);
            }
            else if(!(strcmp(filename,"exit")))
                exit(1);
            else //command isn't exist
                printf("%s: Command not found.\n",filename);
            exit(0);
        }
        else// case of parent
        {
           // printf("<Get in parent process>\n");
            wpid = Waitpid(pid,&status,0);
            //printf("<Get out child process>\n");
        }
    }
    
}
