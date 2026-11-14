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



/* W04: Repair bitmap only after all structural checks pass. */
static int
repair_bitmap(struct superblock *sb)
{
  uchar buf[BSIZE];
  uint nbitmap = (sb->size + BPB - 1) / BPB;
  uint datastart = sb->bmapstart + nbitmap;
  int repaired = 0;

  for(uint b = 0; b < datastart; b++)
    used_blocks[b] = 1;

  for(uint k = 0; k < nbitmap; k++){
    uint bitmap_bn = sb->bmapstart + k;
    int changed = 0;

    rblock(bitmap_bn, buf);

    for(uint bit = 0; bit < BPB; bit++){
      uint bno = k * BPB + bit;
      if(bno >= sb->size)
        break;

      uchar mask = (uchar)(1U << (bit % 8));
      int actual = (buf[bit / 8] & mask) != 0;
      int expected = used_blocks[bno] != 0;

      if(actual == expected)
        continue;

      if(expected)
        buf[bit / 8] |= mask;
      else
        buf[bit / 8] &= (uchar)~mask;

      changed = 1;
      repaired++;
    }

    if(changed){
      if(fseek(img, (long)bitmap_bn * BSIZE, SEEK_SET) != 0 ||
         fwrite(buf, BSIZE, 1, img) != 1 ||
         fflush(img) != 0){
        fprintf(stderr, "fsck: bitmap write failed\n");
        exit(2);
      }
    }
  }

  return repaired;
}

/* Read an inode from the disk image. */
static void
read_disk_inode(struct superblock *sb, uint inum,
                struct dinode *out)
{
  uchar buf[BSIZE];

  rblock(IBLOCK(inum, (*sb)), buf);
  memcpy(out, buf + (inum % IPB) * sizeof(*out),
         sizeof(*out));
}

/* Read one directory entry, including indirect blocks. */
static int
read_directory_entry(struct superblock *sb,
                     struct dinode *dir,
                     uint offset, struct dirent *entry)
{
  uchar buf[BSIZE];
  uint index = offset / BSIZE;
  uint blockno;
  uint datastart =
    sb->bmapstart + (sb->size + BPB - 1) / BPB;

  if(index < NDIRECT){
    blockno = dir->addrs[index];
  } else {
    index -= NDIRECT;
    uint indirect = dir->addrs[NDIRECT];

    if(index >= NINDIRECT ||
       indirect < datastart || indirect >= sb->size)
      return -1;

    rblock(indirect, buf);
    memcpy(&blockno, buf + index * sizeof(uint),
           sizeof(blockno));
  }

  if(blockno < datastart || blockno >= sb->size ||
     offset % BSIZE + sizeof(*entry) > BSIZE)
    return -1;

  rblock(blockno, buf);
  memcpy(entry, buf + offset % BSIZE, sizeof(*entry));

  return 0;
}

/* Count actual directory links and compare with nlink. */
static void
check_nlink(struct superblock *sb, short *wanted, uchar *fix)
{
  uint *links = calloc(sb->ninodes, sizeof(uint));

  if(links == NULL){
    fprintf(stderr, "fsck: out of memory\n");
    exit(2);
  }

  for(uint inum = 1; inum < sb->ninodes; inum++){
    struct dinode dir;
    read_disk_inode(sb, inum, &dir);

    if(dir.type != 1)
      continue;

    if(dir.size > MAXFILE * BSIZE ||
       dir.size % sizeof(struct dirent) != 0){
      report("directory inode %u has invalid size", inum);
      continue;
    }

    for(uint off = 0; off < dir.size;
        off += sizeof(struct dirent)){
      struct dirent de;

      if(read_directory_entry(sb, &dir, off, &de) != 0){
        report("cannot read directory inode %u", inum);
        break;
      }

      if(de.inum == 0)
        continue;

      /* "." does not contribute to nlink. */
      if(de.name[0] == '.' && de.name[1] == '\0')
        continue;

      /* ".." contributes to its parent nlink. */
      if(de.inum >= sb->ninodes){
        report("directory %u has invalid inode %u",
               inum, de.inum);
        continue;
      }

      struct dinode target;
      read_disk_inode(sb, de.inum, &target);

      if(target.type == 0){
        report("directory %u references free inode %u",
               inum, de.inum);
        continue;
      }

      links[de.inum]++;
    }
  }

  for(uint inum = 1; inum < sb->ninodes; inum++){
    struct dinode dip;
    read_disk_inode(sb, inum, &dip);

    if(dip.type == 0)
      continue;

    if(dip.nlink != (int)links[inum]){
      if(wanted != NULL && fix != NULL &&
         links[inum] <= 32767){
        wanted[inum] = (short)links[inum];
        fix[inum] = 1;
      } else {
        report("inode %u nlink mismatch (disk=%d, expected=%u)",
               inum, dip.nlink, links[inum]);
      }
    }
  }

  free(links);
}


/* W04: Repair nlink after directory validation. */
static int
repair_nlink(struct superblock *sb, short *wanted, uchar *fix)
{
  uchar buf[BSIZE];
  int count = 0;

  for(uint inum = 1; inum < sb->ninodes; inum++){
    if(!fix[inum])
      continue;

    uint bn = IBLOCK(inum, (*sb));
    rblock(bn, buf);

    struct dinode *dip =
      ((struct dinode *)buf) + (inum % IPB);

    if(dip->type == 0){
      fprintf(stderr, "fsck: unsafe inode\n");
      exit(2);
    }

    dip->nlink = wanted[inum];

    if(fseek(img, (long)bn * BSIZE, SEEK_SET) != 0 ||
       fwrite(buf, BSIZE, 1, img) != 1 ||
       fflush(img) != 0){
      fprintf(stderr, "fsck: nlink write failed\n");
      exit(2);
    }

    count++;
  }

  return count;
}

static void
check_directories(struct superblock *sb)
{
  uint *parent = calloc(sb->ninodes, sizeof(uint));
  uint *dotdot = calloc(sb->ninodes, sizeof(uint));
  uint *dots = calloc(sb->ninodes, sizeof(uint));
  uint *dotdots = calloc(sb->ninodes, sizeof(uint));

  if(!parent || !dotdot || !dots || !dotdots){
    fprintf(stderr, "fsck: out of memory\n");
    exit(2);
  }

  for(uint inum = 1; inum < sb->ninodes; inum++){
    struct dinode dir;
    read_disk_inode(sb, inum, &dir);

    if(dir.type != 1)
      continue;

    if(dir.size > MAXFILE * BSIZE ||
       dir.size % sizeof(struct dirent) != 0){
      report("directory %u has invalid size", inum);
      continue;
    }

    for(uint off = 0; off < dir.size;
        off += sizeof(struct dirent)){
      struct dirent de;

      if(read_directory_entry(sb, &dir, off, &de) != 0){
        report("directory %u has unreadable entry", inum);
        break;
      }

      if(de.inum == 0)
        continue;

      if(de.inum >= sb->ninodes){
        report("directory %u has invalid dirent inode %u",
               inum, de.inum);
        continue;
      }

      struct dinode target;
      read_disk_inode(sb, de.inum, &target);

      if(target.type == 0){
        report("directory %u references free inode %u",
               inum, de.inum);
        continue;
      }

      if(de.name[0] == '.' && de.name[1] == '\0'){
        dots[inum]++;
        if(de.inum != inum)
          report("directory %u has invalid dot entry", inum);
        continue;
      }

      if(de.name[0] == '.' &&
         de.name[1] == '.' && de.name[2] == '\0'){
        dotdots[inum]++;
        dotdot[inum] = de.inum;

        if(target.type != 1)
          report("directory %u has non-directory parent", inum);
        continue;
      }

      if(de.name[0] == '\0')
        report("directory %u has empty entry name", inum);

      if(target.type == 1){
        if(de.inum == ROOTINO)
          report("root directory has invalid parent link");

        if(parent[de.inum] != 0)
          report("directory inode %u has multiple parent links",
                 de.inum);
        else
          parent[de.inum] = inum;
      }
    }
  }

  for(uint inum = 1; inum < sb->ninodes; inum++){
    struct dinode dir;
    read_disk_inode(sb, inum, &dir);

    if(dir.type != 1)
      continue;

    if(dots[inum] != 1)
      report("directory %u has %u dot entries",
             inum, dots[inum]);

    if(dotdots[inum] != 1)
      report("directory %u has %u dotdot entries",
             inum, dotdots[inum]);

    if(inum == ROOTINO){
      if(dotdot[inum] != ROOTINO)
        report("root directory has invalid dotdot");
    } else if(parent[inum] != 0 &&
              dotdot[inum] != parent[inum]){
      report("directory %u has incorrect dotdot parent",
             inum);
    }

    uint cur = inum;
    for(uint steps = 0; steps < sb->ninodes; steps++){
      if(parent[cur] == 0)
        break;

      cur = parent[cur];

      if(cur == inum){
        report("directory cycle detected at inode %u", inum);
        break;
      }

      if(steps == sb->ninodes - 1)
        report("directory parent chain cycle at inode %u",
               inum);
    }
  }

  free(parent);
  free(dotdot);
  free(dots);
  free(dotdots);
}


/*
 * D3: Find inodes unreachable from the root directory.
 * An unreferenced inode with nlink == 0 may be pending
 * reclamation after a crash.
 */
static void
check_orphans(struct superblock *sb)
{
  uchar *reachable = calloc(sb->ninodes, sizeof(uchar));
  uint *queue = calloc(sb->ninodes, sizeof(uint));
  uint head = 0, tail = 0;

  if(reachable == NULL || queue == NULL){
    fprintf(stderr, "fsck: out of memory\n");
    free(reachable);
    free(queue);
    exit(2);
  }

  struct dinode root;
  read_disk_inode(sb, ROOTINO, &root);

  if(root.type != 1){
    report("root inode is not a directory");
    free(reachable);
    free(queue);
    return;
  }

  reachable[ROOTINO] = 1;
  queue[tail++] = ROOTINO;

  while(head < tail){
    uint inum = queue[head++];
    struct dinode dir;
    read_disk_inode(sb, inum, &dir);

    if(dir.type != 1)
      continue;

    if(dir.size > MAXFILE * BSIZE ||
       dir.size % sizeof(struct dirent) != 0)
      continue;

    for(uint off = 0; off < dir.size;
        off += sizeof(struct dirent)){
      struct dirent de;

      if(read_directory_entry(sb, &dir, off, &de) != 0)
        break;

      if(de.inum == 0 || de.inum >= sb->ninodes)
        continue;

      if(de.name[0] == '.' && de.name[1] == '\0')
        continue;

      if(de.name[0] == '.' &&
         de.name[1] == '.' && de.name[2] == '\0')
        continue;

      struct dinode target;
      read_disk_inode(sb, de.inum, &target);

      if(target.type == 0 || reachable[de.inum])
        continue;

      reachable[de.inum] = 1;

      if(target.type == 1)
        queue[tail++] = de.inum;
    }
  }

  for(uint inum = 1; inum < sb->ninodes; inum++){
    struct dinode dip;
    read_disk_inode(sb, inum, &dip);

    if(dip.type == 0 || reachable[inum])
      continue;

    if(dip.nlink > 0){
      report("orphan inode %u: nlink=%d, unreachable from root",
             inum, dip.nlink);
    } else if(dip.nlink == 0){
      printf("WARNING: inode %u has nlink=0 and is "
             "unreachable (possibly pending reclamation)\n",
             inum);
    } else {
      report("inode %u has negative nlink %d",
             inum, dip.nlink);
    }
  }

  free(reachable);
  free(queue);
}


int
main(int argc, char *argv[])
{
  uchar buf[BSIZE];
  struct superblock sb;
  long imgbytes;
  char *path;
  int repair_mode = 0;
  int repaired = 0;
  int nlink_repaired = 0;
  short *wanted = NULL;
  uchar *fix = NULL;

  if(argc == 3){
    if(strcmp(argv[1], "-y") == 0)
      repair_mode = 1;
    else if(strcmp(argv[1], "-n") != 0){
      fprintf(stderr, "usage: %s [-n|-y] image\n", argv[0]);
      return 2;
    }
  } else if(argc != 2){
    fprintf(stderr, "usage: %s [-n|-y] image\n", argv[0]);
    return 2;
  }

  path = argv[argc - 1];

  if(repair_mode){
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;

    if(strcmp(base, "fs.img") == 0){
      fprintf(stderr, "fsck: refusing to repair fs.img; use a copy\n");
      return 2;
    }
  }

  img = fopen(path, repair_mode ? "r+b" : "rb");
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

  if(nerrors == 0 && repair_mode){
    uint nbitmap = (sb.size + BPB - 1) / BPB;

    if(sb.ninodes <= ROOTINO ||
       sb.logstart < 2 ||
       sb.inodestart <= sb.logstart ||
       sb.bmapstart <= sb.inodestart ||
       (unsigned long long)sb.bmapstart + nbitmap >= sb.size)
      report("unsafe filesystem geometry");
  }

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

  if(repair_mode){
    if(nerrors == 0){
      wanted = calloc(sb.ninodes, sizeof(short));
      fix = calloc(sb.ninodes, sizeof(uchar));

      if(wanted == NULL || fix == NULL){
        fprintf(stderr, "fsck: out of memory\n");
        exit(2);
      }

      check_nlink(&sb, wanted, fix);
      check_directories(&sb);
      check_orphans(&sb);
    }

    if(nerrors == 0){
      nlink_repaired = repair_nlink(&sb, wanted, fix);
      repaired = repair_bitmap(&sb);
      check_bitmap(&sb);
    }
  } else {
    /* Preserve the W03 read-only check order. */
    if(nerrors == 0)
      check_bitmap(&sb);

    if(nerrors == 0){
      check_nlink(&sb, NULL, NULL);
      check_directories(&sb);
      check_orphans(&sb);
    }
  }

  if(nerrors > 0){
    printf("fsck: %d error(s) found\n", nerrors);
    free(used_blocks);
    fclose(img);
    return 1;
  }

  if(repair_mode){
    printf("fsck: repaired %d bitmap bit(s)\n", repaired);
    printf("fsck: repaired %d nlink(s)\n", nlink_repaired);
  }

  printf("fsck: clean\n");
  free(used_blocks);
  fclose(img);
  return 0;
}
