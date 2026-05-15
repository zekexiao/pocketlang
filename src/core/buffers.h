/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_BUFFERS_TEMPLATE_H
#define PK_BUFFERS_TEMPLATE_H

#ifndef PK_AMALGAMATED
#include "internal.h"
#endif

// A buffer of type 'T' contains a heap allocated array of 'T'. When the
// capacity is filled, it will grow to the next power of two.
template <typename T>
class pkBuffer {
public:
  T* data;
  uint32_t count;
  uint32_t capacity;
};

static inline uint32_t pkBufferNextCapacity(size_t size) {
  ASSERT(size <= UINT32_MAX, OOPS);
  uint32_t capacity = MIN_CAPACITY;
  while (capacity < size) capacity *= 2;
  return capacity;
}

template <typename T>
void pkBufferInit(pkBuffer<T>* self) {
  self->data = NULL;
  self->count = 0;
  self->capacity = 0;
}

template <typename T>
void pkBufferClear(pkBuffer<T>* self, PKVM* vm) {
  vmRealloc(vm, self->data, self->capacity * sizeof(T), 0);
  self->data = NULL;
  self->count = 0;
  self->capacity = 0;
}

template <typename T>
void pkBufferReserve(pkBuffer<T>* self, PKVM* vm, size_t size) {
  if (self->capacity < size) {
    uint32_t capacity = pkBufferNextCapacity(size);
    self->data = (T*) vmRealloc(vm, self->data,
                                self->capacity * sizeof(T),
                                capacity * sizeof(T));
    self->capacity = capacity;
  }
}

template <typename T, typename U>
void pkBufferFill(pkBuffer<T>* self, PKVM* vm, U data, int count) {
  pkBufferReserve(self, vm, self->count + count);
  for (int i = 0; i < count; i++) {
    self->data[self->count++] = (T) data;
  }
}

template <typename T, typename U>
void pkBufferWrite(pkBuffer<T>* self, PKVM* vm, U data) {
  pkBufferFill(self, vm, data, 1);
}

template <typename T>
void pkBufferConcat(pkBuffer<T>* self, PKVM* vm, const pkBuffer<T>* other) {
  pkBufferReserve(self, vm, self->count + other->count);
  memcpy(self->data + self->count, other->data, other->count * sizeof(T));
  self->count += other->count;
}

#endif // PK_BUFFERS_TEMPLATE_H
