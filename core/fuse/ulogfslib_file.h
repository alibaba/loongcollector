/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>

#if defined(_MSC_VER)
#include <stdint.h>
typedef int64_t ssize_t;
#endif

/**
 * Open ulogfs file in readonly mode
 *
 * @param name
 *   The name of ulogfs file.
 * @return
 *   The file descriptor of an opened ulogfs file, or negative value on error.
 */
int ulogfs_open_ro(const char* name);


/**
 * Open ulogfs file in rw mode
 *
 * @param name
 *   The name of ulogfs file.
 * @return
 *   The file descriptor of an opened ulogfs file, or a negative value on error.
 */
int ulogfs_open(const char* name);


/**
 * Close ulogfs file
 *
 * @param fd
 *   The file descriptor of previously opened file.
 * @return
 *   0 on success, or a negative value on error.
 */
int ulogfs_close(int fd);


/**
 * Read data from previously opened file with offset.
 *
 * @param fd
 *   The file descriptor of previously opened file.
 * @param buf
 *   The memory where to hold the data.
 * @param count
 *   The number of data in byte to read from file.
 * @param offset
 *   The offset of file, would be modified if file was ftruncated.
 * @return
 *   Positive or 0 on success indicates bytes read from file,
 *   negative value on error.
 */
ssize_t ulogfs_pread(int fd, void* buf, size_t count, off_t* offset);


/**
 * 1/31/2019, after rewrite the pdflush mechanism, the data might be
 * stored either in tmpfs or in datadir. we combine both parts of data
 * together.
 *
 * 4/10/2019, if the caller didn't pass datadir parameter, we will read it
 * from environment variales.
 *
 * Read data from previously opened file with offset, and combine both tmpfs and
 * datadir parts of data together.
 *
 * @param fd
 *   The file descriptor of previously opened file.
 * @param datadir
 *   the path of datadir directory, if the caller didn't pass datadir parameter,
 *   it will read from environment variales.
 * @param buf
 *   The memory where to hold the data.
 * @param count
 *   The number of data in byte to read from file.
 * @param offset
 *   The offset of file, would be modified if file was ftruncated.
 * @return
 *   Positive or 0 on success indicates bytes read from file,
 *   negative value on error.
 */
ssize_t ulogfs_pread2(int fd, const char* datadir, void* buf, size_t count, off_t* offset);


/**
 * Reposition a fd.
 *
 * @param fd
 *   The file descriptor of previously opened file.
 * @param offset
 *   The offset to whence.
 * @param whence
 *   The position to stub.
 * @return
 *   Positive or 0 on success, negative value on error.
 */
off_t ulogfs_seek(int fd, off_t offset, int whence);


/**
 * Get stat info of file.
 *
 * @param fd
 *   The file descriptor of previously opened file.
 * @param buf
 *   The memory where to hold the data.
 * @return
 *   Positive or 0 on success, negative value on error.
 */
#if defined(__linux__)
int ulogfs_stat(int fd, struct stat* buf);
#elif defined(_MSC_VER)
int ulogfs_stat(int fd, struct _stat64* buf);
#endif


/**
 * Get current file ptr offset.
 *
 * @param fd
 *   The file descriptor of previously opened file.
 * @return
 *   Positive or 0 on success indicates size of file,
 *   negative value on error.
 */
int64_t ulogfs_tell(int fd);

/**
 * Inform ulogfs of data handled successfully.
 *
 * @param fd
 *   The file descriptor
 * @param name
 *   The file name.
 * @param offset
 *   The offset of buffer
 * @param length
 *   The lenth of buffer
 * @param force
 *      //
 * @return
 *   0 on success, or a negative value on error.
 */
int ulogfs_sparse(int fd, const char* name, off_t offset, int64_t length);


/**
 * Inform ulogfs that the data of file can be fully flushed
 *
 * @param fd
 *   The file descriptor
 * @param name
 *   the file name
 * @return
 *   0 on success, or a negative value on error.
 */
int ulogfs_fflush(int fd, const char* name);

int ulogfs_fflush2(int fd, const char* name);

/**
  * Dump ulogfs inodes or files data
  *
  * @param type
  *   dump file or inode
  * @param ino
  *   inode number, if you want to dump all inodes info, set ino to 0.
  * @param buf
  *   data buffer
  * @param len
  *   buffer length
  * @return
  *   0 on success, or a negative value on error.
  */
int ulogfs_dump(int type, long ino, void* buf, int64_t* len);

/* send HELLO request to daemon and receive HELLO_REPLY */
int ulogfs_hello();

int ulogfs_upgrade();

#ifdef __cplusplus
}
#endif
