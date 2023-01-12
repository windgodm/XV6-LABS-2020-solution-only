// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
} bcache;

// hash table

struct buf *table[NBUCKET];
struct spinlock bucketlock[NBUCKET];

static
void headinsert(struct buf **pphead, struct buf *pb)
{
  pb->next = *pphead; // set next
  if(pb->next)
    pb->next->prev = pb; // set new next block
  pb->prev = 0; // set prev
  *pphead = pb; // as new head
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  int i = 0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    headinsert(&table[i], b);
    i = (i + 1) % NBUCKET;
  }

  // init hash table
  for(int i = 0; i < NBUCKET; i++){
    initlock(&bucketlock[i], "bcache.bucket");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // find in hashtable
  int key = blockno % NBUCKET;
  acquire(&bucketlock[key]);
  for(b = table[key]; b != 0; b = b->next) {
    if (dev == b->dev && blockno == b->blockno)
      break;
  }

  // the block already cached
  if(b){
    b->refcnt++;
    release(&bucketlock[key]);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bucketlock[key]);

  // not cached, serialize eviction, check again
  acquire(&bcache.lock); // don't hold any bucket lock before bcache.lock to avoid dead lock
  acquire(&bucketlock[key]);
  for(b = table[key]; b != 0; b = b->next) {
    if (dev == b->dev && blockno == b->blockno)
      break;
  }

  // others cached between first check and serialize eviction
  if(b){
    b->refcnt++;
    release(&bucketlock[key]);
    release(&bcache.lock);
    acquiresleep(&b->lock);
    return b;
  }

  // acquire each bucket, and find lru by timestamp
  uint ts = -1;
  uint key2 = key;
  for(int i = 0; i < NBUCKET; i++){
    if(i != key && i != key2)
      acquire(&bucketlock[i]);
    for(struct buf * e = table[i]; e != 0; e = e->next){
      uint ts0 = e->timestamp;
      if(e->refcnt == 0 && ts0 < ts){
        ts = ts0; // lru timestamp
        b = e; // lru block
        if(key2 != i && key2 != key)
          release(&bucketlock[key2]); // release last find lru bucket
        key2 = i; // lru bucket
      }
    }
    if(i != key && i != key2)
      release(&bucketlock[i]);
  }

  // eviction
  if(b){
    if(b->next) 
      b->next->prev = b->prev; // set old next block
    if(b->prev) 
      b->prev->next = b->next; // set old prev block
    else
      table[key2] = b->next; // as old head
    if(key2 != key)
      release(&bucketlock[key2]); // release old bucket
    headinsert(&table[key], b);
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&bucketlock[key]);
    release(&bcache.lock); // end serialize eviction
    acquiresleep(&b->lock);
    return b;
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int key = b->blockno % NBUCKET;
  acquire(&bucketlock[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    int ts;
    acquire(&tickslock);
    ts = ticks;
    release(&tickslock);
    b->timestamp = ts;
  }
  release(&bucketlock[key]);
}

void
bpin(struct buf *b) {
  int key = b->blockno % NBUCKET;
  acquire(&bucketlock[key]);
  b->refcnt++;
  release(&bucketlock[key]);
}

void
bunpin(struct buf *b) {
  int key = b->blockno % NBUCKET;
  acquire(&bucketlock[key]);
  b->refcnt--;
  release(&bucketlock[key]);
}


