/*
 *  Copyright (c) 2026 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef POCKET_VM_H
#define POCKET_VM_H

#include <pocketlang.h>

class PocketVm {
 public:
  PocketVm();
  ~PocketVm();

  PocketVm(const PocketVm&) = delete;
  PocketVm& operator=(const PocketVm&) = delete;

  int runString(const char* source) const;
  int runFile(const char* path) const;
  int runRepl() const;

 private:
  PKVM* vm_ = nullptr;
};

#endif // POCKET_VM_H
