/*
    gmm2json: program that reads Gridmonger's GMM file and converts it into
   JSON format
    Copyright (C) 2025 Jagholin (github.com/Jagholin)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see
   <https://www.gnu.org/licenses/>
*/
#ifndef DYNARRAY_H
#define DYNARRAY_H

#include <memory.h>
#include <stdlib.h>

typedef struct Dynarray {
  unsigned int len;
  unsigned int cap;
  size_t elsize;
  char *data; // Bytes of data
} Dynarray;

static inline Dynarray make_dynarray(size_t elsize, unsigned int n) {
  Dynarray result;
  result.data = (char *)malloc(elsize * n);
  result.len = 0;
  result.cap = n;
  result.elsize = elsize;
  return result;
}

static inline void dynarray_free(Dynarray *arr) { free(arr->data); }

static inline void *dynarray_push_inplace(Dynarray *arr) {
  unsigned int new_cap = arr->cap;
  unsigned int new_len = arr->len + 1;
  // determine new capacity
  while (new_cap <= new_len)
    new_cap <<= 1;
  // new_cap is now > n.
  if (new_cap != arr->cap) {
    char *new_data = (char *)realloc(arr->data, arr->elsize * new_cap);
    if (new_data == NULL) {
      // Out of memory
      // free(arr.data); We don't free memory before exit.
      exit(EXIT_FAILURE);
    }
    arr->data = new_data;
  }
  arr->len = new_len;
  return &arr->data[arr->elsize * (new_len - 1)];
}

static inline void dynarray_set(Dynarray *arr, unsigned int n, void *d) {
  if (n >= arr->len) {
    unsigned int new_cap = arr->cap;
    unsigned int new_len = n + 1;
    // determine new capacity
    while (new_cap <= new_len)
      new_cap <<= 1;
    // new_cap is now > n.
    if (new_cap != arr->cap) {
      char *new_data = (char *)realloc(arr->data, arr->elsize * new_cap);
      if (new_data == NULL) {
        // Out of memory
        // free(arr.data); We don't free memory before exit.
        exit(EXIT_FAILURE);
      }
      arr->data = new_data;
    }
    arr->len = new_len;
  }
  memcpy(arr->data + arr->elsize * n, d, arr->elsize);
}

static inline void *dynarray_get(Dynarray *arr, unsigned int n) {
  if (arr->len <= n)
    return NULL;
  return (void *)&arr->data[arr->elsize * n];
}

static inline unsigned int dynarray_size(Dynarray *arr) { return arr->len; }

static inline void dynarray_push(Dynarray *arr, void *d) {
  dynarray_set(arr, arr->len, d);
}

static inline void dynarray_pop(Dynarray *arr) {
  if (arr->len > 0)
    arr->len--;
}

#endif // DYNARRAY_H
