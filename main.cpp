#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <iostream>


#include "uthreads.h"

bool in = false;

void thread1(void) {
  int i = 0;
  while (true) {
  
      
  }
}


void thread3(){
  int i = 0;
  while (true) {
  }
}


void thread4(void) {
  int i = 0;
  while (true) {
  if(i == 2500){
    uthread_terminate(0);
    
    }
    i++;
  }
}



void thread2(void){
  int i = 0;
  while (true) {
    if(i == 5000){
      uthread_terminate(1);
      in = true;
      
    }
    i++;
  }

}



int main(void) {

  uthread_init(3000000);
  uthread_spawn(thread1);
  uthread_spawn(thread2);
  
  std::cout << "I am here" << std::endl;
  for(;;){
    if(in){
      uthread_terminate(2);
      in = false;
    }
  }
  return 0;
}
