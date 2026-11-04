/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */
#include <string.h>
#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/mmu.h"
#include "vm/anon.h"
#include "lib/kernel/bitmap.h"
#include "threads/synch.h"

/* DO NOT MODIFY BELOW LINE */
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);
static void release_slot (size_t slot);
static void write_at_swap_disk (struct page *page);
static void read_from_swap_disk(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

#define SECTOR_PER_SLOT (PGSIZE / DISK_SECTOR_SIZE)
#define SECTOR_CNT disk_size(swap_disk) 
static struct lock swap_lock; 
static struct bitmap *swap_table;
static struct disk *swap_disk;

static disk_sector_t
slot_to_sector (size_t slot) {
	return slot * SECTOR_PER_SLOT;
}

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	swap_disk = disk_get(1, 1);
	if (swap_disk == NULL) 
		PANIC("swap disk not found");
	size_t total_slot_cnt = SECTOR_CNT / SECTOR_PER_SLOT;
	swap_table = bitmap_create(total_slot_cnt);
	if (swap_table == NULL)
		PANIC ("swap table create failed");
	lock_init(&swap_lock); 
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;
  anon_page->is_stack = IS_STACK(type);
	anon_page->swap_slot_idx = BITMAP_ERROR;  
	return true;
}

static bool
anon_swap_out (struct page *page) { 
	struct anon_page *anon_page = &page->anon;
	lock_acquire(&swap_lock);
	anon_page->swap_slot_idx = bitmap_scan_and_flip(swap_table, 0, 1, false);
	lock_release(&swap_lock);
	if (anon_page->swap_slot_idx == BITMAP_ERROR) 
		PANIC("no empty slot"); 
	write_at_swap_disk(page);
	remove_pte_from_pml4(page->frame, page->owner->pml4);
	unlink_page_and_frame(page->frame);
	return true;
}

static void
write_at_swap_disk (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	char *buf_start = page->frame->kva;
	for (int k = 0; k < SECTOR_PER_SLOT; k++) {
		disk_write(swap_disk, slot_to_sector (anon_page->swap_slot_idx) + k, buf_start + DISK_SECTOR_SIZE * k);
	}
}

static bool
anon_swap_in (struct page *page, void *kva) {
  struct anon_page *anon_page = &page->anon;
	ASSERT (page->frame != NULL); 
	ASSERT (page->anon.swap_slot_idx != BITMAP_ERROR); 
	read_from_swap_disk(page);
	// 슬롯을 비우고 비트값을 유효하지 않은 것으로 한다. 
	release_slot(anon_page->swap_slot_idx);
	anon_page->swap_slot_idx = BITMAP_ERROR;
	return true;
}

static void
read_from_swap_disk(struct page *page) {
	struct anon_page *anon_page = &page->anon;
	char *buf_start = page->frame->kva;	
	for (int k = 0; k < SECTOR_PER_SLOT; k++) {
		disk_read(swap_disk, slot_to_sector (anon_page->swap_slot_idx) + k, buf_start + DISK_SECTOR_SIZE * k);
	}
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	if (page->frame != NULL)
		release_frame(page->frame, page->owner->pml4);
	else if (anon_page->swap_slot_idx != BITMAP_ERROR) 
		release_slot(anon_page->swap_slot_idx);
}

static void
release_slot (size_t swap_slot_idx) {
	lock_acquire(&swap_lock); 
	bitmap_reset(swap_table, swap_slot_idx);
	lock_release(&swap_lock);
}
	
