#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct file_page {
  struct mmap_file *mmap_file;
  struct page *head_page;
  off_t offset;
  uint32_t zero_bytes;
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void do_munmap (void *va);

bool is_mmap_addr(void *addr);
struct file *reopen_file(struct file * file);
struct mmap_file* duplicate_mmap_file(struct file *file);

struct mmap_file{
  struct file *file;
  int count;
};

struct load_file_args{
  struct mmap_file *file;
  off_t ofs;
  uint32_t read_bytes;
  uint32_t zero_bytes;
  void *mmap_addr;
};

#define AUX_TO_ARGS(aux_) ((struct load_file_args *)((struct aux *) (aux_))->aux_pt)

#endif
