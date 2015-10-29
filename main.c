/*******************************************************************************

  @file        main.c

  @author      Ethan Turkeltaub

  @date        20 October 2015

  @brief       eshell

*******************************************************************************/

#include <stdio.h>

int main(void) {
  int c;

  while((c = getchar()) != EOF) putchar(c);

  return 0;
}
