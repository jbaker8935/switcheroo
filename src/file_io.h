#ifndef FILE_IO_H
#define FILE_IO_H

#include <stdint.h>
// File I/O module for saving/loading game state

// Initialize file I/O - called on application startup
void file_io_init(void);

// Save game state - called on application exit
void file_io_save(void);

#endif // FILE_IO_H
