#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
  if(argc < 3){
    fprintf(stderr, "Usage: corrupt <fs.img> <mode>\n");
    return 1;
  }
  printf("corrupt: target %s mode %s\n", argv[1], argv[2]);
  return 0;
}
