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
  PocketVm(PocketVm&& other) noexcept;
  PocketVm& operator=(const PocketVm&) = delete;
  PocketVm& operator=(PocketVm&& other) noexcept;

  [[nodiscard]] int runString(const char* source) const;
  [[nodiscard]] int runFile(const char* path) const;
  [[nodiscard]] int runRepl() const;

 private:
  PKVM* release() noexcept;
  void reset(PKVM* vm = nullptr) noexcept;

  PKVM* vm_ = nullptr;
};

#endif // POCKET_VM_H
