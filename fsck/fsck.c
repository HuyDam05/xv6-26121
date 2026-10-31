#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kernel/types.h"
#include "kernel/fs.h"

static FILE *img;
static int nerrors;
static uchar *used_blocks;

static void
report(const char *fmt, ...)
{
  va_list ap;

  printf("ERROR: ");
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  printf("\n");

  nerrors++;
}

static void
rblock(uint bn, void *buf)
{
  if(fseek(img, (long)bn * BSIZE, SEEK_SET) != 0 ||
     fread(buf, BSIZE, 1, img) != 1){
    fprintf(stderr, "fsck: cannot read block %u\n", bn);
    exit(2);
  }
}

static void
check_superblock(struct superblock *sb, long imgbytes)
{
  if(sb->magic != FSMAGIC)
    report("bad magic 0x%x (expected 0x%x)",
           sb->magic, FSMAGIC);

  if((long)sb->size * BSIZE != imgbytes)
    report("size %u blocks does not match image size %ld bytes",
           sb->size, imgbytes);
}

static void
mark_block(struct superblock *sb, uint bno, uint inum)
{
  uint nbitmap = (sb->size + BPB - 1) / BPB;
  uint datastart = sb->bmapstart + nbitmap;

  if(bno == 0)
    return;

  if(bno < datastart || bno >= sb->size){
    report("inode %u references invalid block %u",
           inum, bno);
    return;
  }

  if(used_blocks[bno]){
    report("block %u is referenced more than once",
           bno);
    return;
  }

  used_blocks[bno] = 1;
}

static void
check_inodes(struct superblock *sb)
{
  uchar buf[BSIZE];

  for(uint inum = 1; inum < sb->ninodes; inum++){
    uint bn = IBLOCK(inum, (*sb));

    rblock(bn, buf);

    struct dinode *dip =
      ((struct dinode *)buf) + (inum % IPB);

    if(dip->type == 0)
      continue;

    if(dip->type < 1 || dip->type > 3){
      report("inode %u has invalid type %d",
             inum, dip->type);
      continue;
    }

    /*
     * Direct blocks.
     */
    for(int i = 0; i < NDIRECT; i++){
      uint bno = dip->addrs[i];

      if(bno != 0)
        mark_block(sb, bno, inum);
    }

    /*
     * Indirect block.
     */
    uint indirect = dip->addrs[NDIRECT];

    if(indirect != 0){
      uint nbitmap = (sb->size + BPB - 1) / BPB;
      uint datastart = sb->bmapstart + nbitmap;

      if(indirect < datastart || indirect >= sb->size){
        report("inode %u has bad indirect block %u",
               inum, indirect);
        continue;
      }

      /*
       * The indirect block itself is allocated.
       */
      mark_block(sb, indirect, inum);

      uchar ibuf[BSIZE];
      rblock(indirect, ibuf);

      uint *addrs = (uint *)ibuf;

      for(int j = 0; j < NINDIRECT; j++){
        uint bno = addrs[j];

        if(bno != 0)
          mark_block(sb, bno, inum);
      }
    }
  }
}

static int
bitmap_is_set(struct superblock *sb, uint bno)
{
  uchar buf[BSIZE];

  uint bitmap_block = BBLOCK(bno, (*sb));

  rblock(bitmap_block, buf);

  uint bit = bno % BPB;

  return (buf[bit / 8] >> (bit % 8)) & 1;
}

static void
check_bitmap(struct superblock *sb)
{
  uint nbitmap = (sb->size + BPB - 1) / BPB;
  uint datastart = sb->bmapstart + nbitmap;

  /*
   * Metadata blocks are always allocated:
   *
   * boot
   * superblock
   * log
   * inode blocks
   * bitmap blocks
   */
  for(uint b = 0; b < datastart; b++)
    used_blocks[b] = 1;

  for(uint b = 0; b < sb->size; b++){
    int bit = bitmap_is_set(sb, b);

    if(used_blocks[b] && !bit){
      report("block %u is used but marked free in bitmap",
             b);
    }

    if(!used_blocks[b] && bit){
      report("block %u is marked used in bitmap but not referenced",
             b);
    }
  }
}

int
main(int argc, char *argv[])
{
  uchar buf[BSIZE];
  struct superblock sb;
  long imgbytes;
  char *path;

  if(argc < 2 || argc > 3 ||
     (argc == 3 && strcmp(argv[1], "-n") != 0)){
    fprintf(stderr, "usage: %s [-n] fs.img\n", argv[0]);
    return 2;
  }

  path = argv[argc - 1];

  img = fopen(path, "rb");
  if(img == NULL){
    perror(path);
    return 2;
  }

  if(fseek(img, 0, SEEK_END) != 0){
    perror("fseek");
    fclose(img);
    return 2;
  }

  imgbytes = ftell(img);

  rblock(1, buf);
  memcpy(&sb, buf, sizeof(sb));

  check_superblock(&sb, imgbytes);

  if(nerrors != 0){
    printf("fsck: %d error(s) found\n", nerrors);
    fclose(img);
    return 1;
  }

  used_blocks = calloc(sb.size, sizeof(uchar));

  if(used_blocks == NULL){
    fprintf(stderr, "fsck: cannot allocate block table\n");
    fclose(img);
    return 2;
  }

  check_inodes(&sb);

  if(nerrors == 0)
    check_bitmap(&sb);

  if(nerrors > 0){
    printf("fsck: %d error(s) found\n", nerrors);

    free(used_blocks);
    fclose(img);

    return 1;
  }

  printf("fsck: clean\n");

  free(used_blocks);
  fclose(img);

  return 0;
}
