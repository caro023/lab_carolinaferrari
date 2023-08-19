#ifndef RW_H
#define RW_H

#include "tabella.h"

void *Reader(void* arg);
void *Writer(void* arg);
void* Capo(void* arg);

#endif  // RW_H