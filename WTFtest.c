#include "wtf.h"
#include <sys/wait.h>

int main(int argc, char**argv){
    pid_t child = fork();
    if(child < 0){
        perror("fork() error\n");
        exit(-1);
    }
    if(child != 0){
        wait(NULL);
    } else {
      execlp("./WTFserver", "7000");
      sleep(5);
        execlp("./WTF", "configure", "lisp.cs.rutgers.edu", "7000");
      sleep(5);
      execlp("./WTF", "create", "File1");
      sleep(5);
      execlp("./WTF", "add", "File1","cs.txt");
      sleep(5);
      execlp("./WTF", "add", "File2","adru.txt");
      sleep(5);
      execlp("./WTF", "add", "File2", "File2/adru.txt");
      sleep(5);
      execlp("./WTF", "commit", "File1");
      sleep(5);
      execlp("./WTF", "push", "File2");
      sleep(5);
      execlp("./WTF", "remove", "File2", "adru.txt");
      sleep(5);
      execlp("./WTF", "update", "File1");
      sleep(5);
      execlp("./WTF", "upgrade", "File1");
      sleep(5);
      execlp("./WTF", "history", "File1");
      sleep(5);
      execlp("./WTF", "currentversion", "File1");
      sleep(5);
      execlp("./WTF", "commit", "File1", "1");
      sleep(5);
      execlp("./WTF", "push", "File1");
      sleep(5);
    }

