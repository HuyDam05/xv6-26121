// xv6-26121: Host-side tool for controlled corruption of a COPY of fs.img.
// Usage: ./fsck/corrupt <mode> <image-file>
// Also accepts the original argument order: <image-file> <mode>.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kernel/types.h"
#include "kernel/fs.h"

static FILE *image;
static struct superblock sb;

static void
die(const char *message)
{
  fprintf(stderr, "corrupt: %s\n", message);
  exit(1);
}

static void
read_block(uint bno, void *buffer)
{
  if(bno >= sb.size || fseek(image, (long)bno * BSIZE, SEEK_SET) != 0 ||
     fread(buffer, 1, BSIZE, image) != BSIZE)
    die("cannot read disk block");
}

static void
write_block(uint bno, const void *buffer)
{
  if(bno >= sb.size || fseek(image, (long)bno * BSIZE, SEEK_SET) != 0 ||
     fwrite(buffer, 1, BSIZE, image) != BSIZE || fflush(image) != 0)
    die("cannot write disk block");
}

static uint
data_start(void)
{
  return sb.bmapstart + (sb.size + BPB - 1) / BPB;
}

static int
is_data_block(uint bno)
{
  return bno >= data_start() && bno < sb.size;
}

static void
read_inode(uint inum, struct dinode *node)
{
  uchar block[BSIZE];
  if(inum == 0 || inum >= sb.ninodes)
    die("invalid inode number");
  read_block(IBLOCK(inum, sb), block);
  memcpy(node, block + (inum % IPB) * sizeof(*node), sizeof(*node));
}

static void
write_inode(uint inum, const struct dinode *node)
{
  uchar block[BSIZE];
  if(inum == 0 || inum >= sb.ninodes)
    die("invalid inode number");
  uint bno = IBLOCK(inum, sb);
  read_block(bno, block);
  memcpy(block + (inum % IPB) * sizeof(*node), node, sizeof(*node));
  write_block(bno, block);
}

static int
bitmap_set(uint bno, int used)
{
  uchar block[BSIZE];
  uint bmap = BBLOCK(bno, sb);
  uint bit = bno % BPB;
  uchar mask = (uchar)(1U << (bit % 8));
  if(bno >= sb.size)
    die("invalid bitmap block number");
  read_block(bmap, block);
  int old = (block[bit / 8] & mask) != 0;
  if(used)
    block[bit / 8] |= mask;
  else
    block[bit / 8] &= (uchar)~mask;
  if(old != used)
    write_block(bmap, block);
  return old;
}

static int
bitmap_get(uint bno)
{
  uchar block[BSIZE];
  uint bit = bno % BPB;
  if(bno >= sb.size)
    die("bitmap lookup outside image");
  read_block(BBLOCK(bno, sb), block);
  return (block[bit / 8] >> (bit % 8)) & 1;
}

// Return block storing data at logical block index in an inode.
static uint
inode_data_block(const struct dinode *node, uint index)
{
  if(index < NDIRECT)
    return node->addrs[index];
  index -= NDIRECT;
  if(index >= NINDIRECT || !is_data_block(node->addrs[NDIRECT]))
    return 0;
  uint addresses[NINDIRECT];
  read_block(node->addrs[NDIRECT], addresses);
  return addresses[index];
}

// Find the first non-directory file linked directly from root.
static int
find_root_file(uint *file_inum, uint *dir_block, uint *dir_offset)
{
  struct dinode root;
  uchar block[BSIZE];
  read_inode(ROOTINO, &root);
  if(root.type != 1)
    die("root inode is not a directory");

  for(uint offset = 0; offset + sizeof(struct dirent) <= root.size;
      offset += sizeof(struct dirent)){
    uint disk = inode_data_block(&root, offset / BSIZE);
    if(!is_data_block(disk))
      die("invalid root directory data block");
    read_block(disk, block);
    uint pos = offset % BSIZE;
    struct dirent entry;
    memcpy(&entry, block + pos, sizeof(entry));
    if(entry.inum == 0 || entry.inum >= sb.ninodes)
      continue;
    struct dinode child;
    read_inode(entry.inum, &child);
    if(child.type == 2){ // T_FILE
      *file_inum = entry.inum;
      *dir_block = disk;
      *dir_offset = pos;
      return 1;
    }
  }
  return 0;
}

static void
corrupt_bitmap_clear(void)
{
  struct dinode root;
  read_inode(ROOTINO, &root);
  uint bno = root.addrs[0];
  if(!is_data_block(bno) || !bitmap_get(bno))
    die("root data block unavailable or already free");
  bitmap_set(bno, 0);
  printf("corrupt: bitmap-clear block %u (was allocated)\n", bno);
}

static void
corrupt_bitmap_mark_used(void)
{
  for(uint bno = data_start(); bno < sb.size; bno++){
    if(!bitmap_get(bno)){
      bitmap_set(bno, 1);
      printf("corrupt: bitmap-set block %u (was free)\n", bno);
      return;
    }
  }
  die("no free data block to corrupt");
}

static void
corrupt_nlink(void)
{
  struct dinode root;
  read_inode(ROOTINO, &root);
  if(root.type != 1 || root.nlink >= 32767)
    die("root inode invalid or nlink out of range");
  short old = root.nlink;
  root.nlink++;
  write_inode(ROOTINO, &root);
  printf("corrupt: nlink-bump inode %d: %d -> %d\n", ROOTINO, old,
         root.nlink);
}

static void
corrupt_dirent_drop(void)
{
  uint inum, bno, pos;
  uchar block[BSIZE];
  if(!find_root_file(&inum, &bno, &pos))
    die("no regular file in root directory");
  read_block(bno, block);
  // Remove the directory link without freeing the inode or its blocks.
  struct dirent entry;
  memset(&entry, 0, sizeof(entry));
  memcpy(block + pos, &entry, sizeof(entry));
  write_block(bno, block);
  printf("corrupt: dirent-drop root link to inode %u\n", inum);
}

static void
corrupt_duplicate_block(void)
{
  uint first_inode = 0, second_inode = 0, source_bno = 0;
  struct dinode first, second;
  for(uint inum = 1; inum < sb.ninodes; inum++){
    struct dinode node;
    read_inode(inum, &node);
    if(node.type != 2 || !is_data_block(node.addrs[0]))
      continue;
    if(first_inode == 0){
      first_inode = inum;
      first = node;
      source_bno = first.addrs[0];
    } else if(node.addrs[0] != source_bno){
      second_inode = inum;
      second = node;
      break;
    }
  }
  if(second_inode == 0 || first_inode == 0)
    die("need two regular files with distinct first data blocks");

  uint old_bno = second.addrs[0];
  second.addrs[0] = source_bno;
  write_inode(second_inode, &second);
  // The old data block no longer belongs to the second inode.
  // Clear its bitmap bit to avoid introducing a separate bitmap leak.
  if(bitmap_get(old_bno))
    bitmap_set(old_bno, 0);
  printf("corrupt: dup-block inode %u and inode %u share block %u\n",
         first_inode, second_inode, source_bno);
}

static int
known_mode(const char *s)
{
  return strcmp(s, "bitmap-clear") == 0 || strcmp(s, "bitmap-set") == 0 ||
         strcmp(s, "nlink-bump") == 0 || strcmp(s, "dirent-drop") == 0 ||
         strcmp(s, "dup-block") == 0;
}

int
main(int argc, char *argv[])
{
  if(argc != 3){
    fprintf(stderr, "Usage: %s <mode> <copy-of-fs.img>\n", argv[0]);
    return 2;
  }
  const char *mode = argv[1];
  const char *path = argv[2];
  if(!known_mode(mode) && known_mode(argv[2])){
    mode = argv[2];
    path = argv[1];
  }
  if(!known_mode(mode))
    die("unknown corruption mode");

  // Guard against accidentally modifying the live/default filesystem image.
  const char *basename = strrchr(path, '/');
  basename = basename ? basename + 1 : path;
  if(strcmp(basename, "fs.img") == 0)
    die("refusing to modify fs.img; copy it to another filename first");

  image = fopen(path, "r+b");
  if(image == NULL){
    perror(path);
    return 2;
  }

  if(fseek(image, 0, SEEK_END) != 0)
    die("cannot inspect image size");
  long image_size = ftell(image);
  if(image_size < 2L * BSIZE || fseek(image, BSIZE, SEEK_SET) != 0 ||
     fread(&sb, sizeof(sb), 1, image) != 1)
    die("not a valid xv6 filesystem image");

  if(sb.magic != FSMAGIC || sb.size == 0 ||
     image_size != (long)sb.size * BSIZE ||
     sb.ninodes <= ROOTINO || sb.inodestart <= sb.logstart ||
     sb.bmapstart <= sb.inodestart || data_start() >= sb.size)
    die("bad superblock or inconsistent image geometry");

  if(strcmp(mode, "bitmap-clear") == 0)
    corrupt_bitmap_clear();
  else if(strcmp(mode, "bitmap-set") == 0)
    corrupt_bitmap_mark_used();
  else if(strcmp(mode, "nlink-bump") == 0)
    corrupt_nlink();
  else if(strcmp(mode, "dirent-drop") == 0)
    corrupt_dirent_drop();
  else
    corrupt_duplicate_block();

  if(fclose(image) != 0)
    die("failed to close image");
  return 0;
}
