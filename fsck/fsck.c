#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(stderr, "Usage: fsck [-r] <fs.img>\n");
    return 1;
  }
  printf("fsck: checking %s\n", argv[argc - 1]);
  return 0;
}
