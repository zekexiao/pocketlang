/*
 *  Copyright (c) 2026 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include "pocket_vm.h"

#ifdef _WIN32
  #include <Windows.h>
  #include <io.h>
  #define isatty _isatty
  #define fileno _fileno
#else
  #include <unistd.h>
#endif

#include <cstdio>

PocketVm::PocketVm() {
  PkConfiguration config = pkNewConfiguration();

  if (!!isatty(fileno(stderr))) {
#ifdef _WIN32
    DWORD outmode = 0;
    HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
    GetConsoleMode(handle, &outmode);
    SetConsoleMode(handle, outmode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
    config.use_ansi_escape = true;
  }

  vm_ = pkNewVM(&config);
}

PocketVm::PocketVm(PocketVm&& other) noexcept : vm_(other.release()) {}

PocketVm::~PocketVm() { reset(); }

PocketVm& PocketVm::operator=(PocketVm&& other) noexcept {
  if (this != &other) reset(other.release());
  return *this;
}

int PocketVm::runString(const char* source) const {
  return (int) pkRunString(vm_, source);
}

int PocketVm::runFile(const char* path) const {
  return (int) pkRunFile(vm_, path);
}

int PocketVm::runRepl() const {
  return pkRunREPL(vm_);
}

PKVM* PocketVm::release() noexcept {
  PKVM* vm = vm_;
  vm_ = nullptr;
  return vm;
}

void PocketVm::reset(PKVM* vm) noexcept {
  if (vm_ != nullptr) pkFreeVM(vm_);
  vm_ = vm;
}
