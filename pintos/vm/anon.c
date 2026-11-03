/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */
#include <string.h>
#include <bitmap.h>
#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/mmu.h"

#define SECTOR_COUNT (disk_size(swap_disk))
#define SECTORS_PER_SLOT (PGSIZE / DISK_SECTOR_SIZE)
#define SLOT_COUNT (SECTOR_COUNT / SECTORS_PER_SLOT)
#define SECTOR_START(slot_no) (SECTORS_PER_SLOT * (slot_no))


/* DO NOT MODIFY BELOW LINE */
static struct lock swap_lock;
static struct disk *swap_disk;
static struct bitmap *swap_table;

static void set_swap_disk();
static void set_swap_table();

static bool anon_swap_in (struct page *page, void *kva);
static void load_from_disk(void *kva, size_t swap_slot);

static bool anon_swap_out (struct page *page);
static bool is_slot_available(size_t swap_slot);
static size_t get_swap_slot();
static void write_back_to_disk(void *kva, size_t swap_slot);

static void anon_destroy (struct page *page);
static void remove_from_swap_table(size_t swap_slot);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};


/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	set_swap_disk();
  set_swap_table();
}

static void
set_swap_disk(){
  swap_disk = disk_get(1, 1);
  if (swap_disk == NULL)
    PANIC("swap disk init failed.");
}

static void
set_swap_table(){
  swap_table = bitmap_create(SLOT_COUNT);
  if (swap_table == NULL)
    PANIC("swap table creation failed.");
  
  lock_init(&swap_lock);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
  anon_page->is_stack = IS_STACK(type);
  anon_page->swap_slot = BITMAP_ERROR;

  return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;

  load_from_disk(kva, anon_page->swap_slot);
  anon_page->swap_slot = BITMAP_ERROR;

  return true;
}

static void
load_from_disk(void *kva, size_t swap_slot){
  void *buff = kva;
  disk_sector_t sector = SECTOR_START(swap_slot); 
  for (int i = 0; i <SECTORS_PER_SLOT; i++){
    disk_read(swap_disk, sector, buff); 
    buff += DISK_SECTOR_SIZE;
    sector ++;
  }

  remove_from_swap_table(swap_slot);
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
  struct anon_page *anon_page = &page->anon;
	
  anon_page->swap_slot = get_swap_slot();

  if (!is_slot_available(anon_page->swap_slot)) return false;

  write_back_to_disk(page->frame->kva, anon_page->swap_slot);

  return true;
}

static bool
is_slot_available(size_t swap_slot){
  return swap_slot != BITMAP_ERROR;
}

static size_t
get_swap_slot(){
  lock_acquire(&swap_lock); 
  size_t available = bitmap_scan_and_flip(swap_table, 0, 1, false);
  lock_release(&swap_lock);

  return available;
}

static void
write_back_to_disk(void *kva, size_t swap_slot){
  void *buff = kva;
  disk_sector_t sector = SECTOR_START(swap_slot); 
  for (int i = 0; i <SECTORS_PER_SLOT; i++){
    disk_write(swap_disk, sector, buff); 
    buff += DISK_SECTOR_SIZE;
    sector ++;
  }
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;

  if (page->frame){ 
    release_frame(page->frame, thread_current()->pml4);
  }
  else{
    remove_from_swap_table(anon_page->swap_slot);
    anon_page->swap_slot = BITMAP_ERROR;
  }
}

static void
remove_from_swap_table(size_t swap_slot){
  lock_acquire(&swap_lock);
  bitmap_reset(swap_table, swap_slot);
  lock_release(&swap_lock);
}
