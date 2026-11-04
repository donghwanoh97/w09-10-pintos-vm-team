/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */
#include <string.h>
#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/mmu.h"
#include "lib/kernel/bitmap.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

struct bitmap *swap_table;
struct lock swap_table_lock;

#define PAGE_SECTOR_CNT (PGSIZE / DISK_SECTOR_SIZE)

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1,1);

	size_t swap_table_size = disk_size(swap_disk) / PAGE_SECTOR_CNT;
	swap_table = bitmap_create(swap_table_size);

	if (!swap_table) PANIC("swap table create");
	lock_init(&swap_table_lock);
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
	size_t idx = anon_page->swap_slot;
	if (idx == BITMAP_ERROR) return false;

	lock_acquire(&swap_table_lock);
	if (!bitmap_test(swap_table, idx)) {
		lock_release(&swap_table_lock);
		return false;
	}
	bitmap_reset(swap_table, idx);

	for (size_t i = 0; i < PAGE_SECTOR_CNT; i++) {
		disk_read(swap_disk, idx * PAGE_SECTOR_CNT + i ,kva + (DISK_SECTOR_SIZE * i));
	}

	lock_release(&swap_table_lock);

	anon_page->swap_slot = BITMAP_ERROR;

	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	lock_acquire(&swap_table_lock);
	size_t idx = bitmap_scan_and_flip(swap_table, 0, 1, false);
	lock_release(&swap_table_lock);

	if (idx == BITMAP_ERROR) PANIC("bitmap scan and flip");
	anon_page->swap_slot = idx;

	for (size_t i = 0; i < PAGE_SECTOR_CNT; i++) {
		disk_write(swap_disk, idx * PAGE_SECTOR_CNT + i ,page->frame->kva + (DISK_SECTOR_SIZE * i));
	}

	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;

  if (page->frame){ 
		remove_frame_table_elem(&page->frame->elem);
    release_frame(page->frame, page->pml4);
  } 
	else if (anon_page->swap_slot != BITMAP_ERROR) {
		lock_acquire(&swap_table_lock);
		bitmap_reset(swap_table, anon_page->swap_slot);
		lock_release(&swap_table_lock);
	}
}
