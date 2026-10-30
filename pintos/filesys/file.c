#include "filesys/file.h"
#include <debug.h>
#include "filesys/inode.h"
#include "threads/malloc.h"

/* 열린 파일(open file). */
struct file {
	struct inode *inode;        /* 파일의 inode. */
	off_t pos;                  /* 현재 위치. */
	bool deny_write;            /* file_deny_write()가 호출됐는지 여부. */
};

/* 주어진 INODE에 대한 파일을 열고, INODE의 소유권을 넘겨받아
 * 새 파일을 반환함. 할당(allocation)에 실패하거나 INODE가
 * 널 포인터(null pointer)이면 널 포인터를 반환함. */
struct file *
file_open (struct inode *inode) {
	struct file *file = calloc (1, sizeof *file);
	if (inode != NULL && file != NULL) {
		file->inode = inode;
		file->pos = 0;
		file->deny_write = false;
		return file;
	} else {
		inode_close (inode);
		free (file);
		return NULL;
	}
}

/* FILE과 같은 inode에 대한 새 파일을 열어 반환함.
 * 실패하면 널 포인터를 반환함. */
struct file *
file_reopen (struct file *file) {
	return file_open (inode_reopen (file->inode));
}

/* 속성(attribute)까지 포함해 파일 객체를 복제하고, FILE과 같은
 * inode에 대한 새 파일을 반환함. 실패하면 널 포인터를 반환함. */
struct file *
file_duplicate (struct file *file) {
	struct file *nfile = file_open (inode_reopen (file->inode));
	if (nfile) {
		nfile->pos = file->pos;
		if (file->deny_write)
			file_deny_write (nfile);
	}
	return nfile;
}

/* FILE을 닫음. */
void
file_close (struct file *file) {
	if (file != NULL) {
		file_allow_write (file);
		inode_close (file->inode);
		free (file);
	}
}

/* FILE이 감싸고 있는 inode를 반환함. */
struct inode *
file_get_inode (struct file *file) {
	return file->inode;
}

/* FILE의 현재 위치부터 SIZE 바이트를 BUFFER로 읽음.
 * 실제로 읽은 바이트 수를 반환하며, 파일 끝(end of file)에
 * 도달하면 SIZE보다 작을 수 있음.
 * FILE의 위치를 읽은 바이트 수만큼 앞으로 이동시킴. */
off_t
file_read (struct file *file, void *buffer, off_t size) {
	off_t bytes_read = inode_read_at (file->inode, buffer, size, file->pos);
	file->pos += bytes_read;
	return bytes_read;
}

/* 파일의 오프셋(offset) FILE_OFS부터 SIZE 바이트를 BUFFER로 읽음.
 * 실제로 읽은 바이트 수를 반환하며, 파일 끝(end of file)에
 * 도달하면 SIZE보다 작을 수 있음.
 * 파일의 현재 위치는 변하지 않음. */
off_t
file_read_at (struct file *file, void *buffer, off_t size, off_t file_ofs) {
	return inode_read_at (file->inode, buffer, size, file_ofs);
}

/* BUFFER의 SIZE 바이트를 FILE의 현재 위치부터 씀.
 * 실제로 쓴 바이트 수를 반환하며, 파일 끝(end of file)에
 * 도달하면 SIZE보다 작을 수 있음.
 * (원래는 이 경우 파일을 늘려야 하지만, 파일 확장(file growth)은
 * 아직 구현되지 않았음.)
 * FILE의 위치를 읽은 바이트 수만큼 앞으로 이동시킴. */
off_t
file_write (struct file *file, const void *buffer, off_t size) {
	off_t bytes_written = inode_write_at (file->inode, buffer, size, file->pos);
	file->pos += bytes_written;
	return bytes_written;
}

/* BUFFER의 SIZE 바이트를 파일의 오프셋(offset) FILE_OFS부터 씀.
 * 실제로 쓴 바이트 수를 반환하며, 파일 끝(end of file)에
 * 도달하면 SIZE보다 작을 수 있음.
 * (원래는 이 경우 파일을 늘려야 하지만, 파일 확장(file growth)은
 * 아직 구현되지 않았음.)
 * 파일의 현재 위치는 변하지 않음. */
off_t
file_write_at (struct file *file, const void *buffer, off_t size,
		off_t file_ofs) {
	return inode_write_at (file->inode, buffer, size, file_ofs);
}

/* file_allow_write()가 호출되거나 FILE이 닫힐 때까지
 * FILE의 기반 inode에 대한 쓰기 연산을 막음. */
void
file_deny_write (struct file *file) {
	ASSERT (file != NULL);
	if (!file->deny_write) {
		file->deny_write = true;
		inode_deny_write (file->inode);
	}
}

/* FILE의 기반 inode에 대한 쓰기 연산을 다시 허용함.
 * (같은 inode를 연 다른 파일이 있으면 그쪽에서 쓰기가
 * 여전히 막혀 있을 수 있음.) */
void
file_allow_write (struct file *file) {
	ASSERT (file != NULL);
	if (file->deny_write) {
		file->deny_write = false;
		inode_allow_write (file->inode);
	}
}

/* FILE의 크기를 바이트 단위로 반환함. */
off_t
file_length (struct file *file) {
	ASSERT (file != NULL);
	return inode_length (file->inode);
}

/* FILE의 현재 위치를 파일 시작으로부터 NEW_POS 바이트 지점으로
 * 설정함. */
void
file_seek (struct file *file, off_t new_pos) {
	ASSERT (file != NULL);
	ASSERT (new_pos >= 0);
	file->pos = new_pos;
}

/* FILE의 현재 위치를 파일 시작으로부터의 바이트 오프셋(byte offset)으로
 * 반환함. */
off_t
file_tell (struct file *file) {
	ASSERT (file != NULL);
	return file->pos;
}
