/*
 *  Copyright (c) 2026 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef CLI_OPTIONS_H
#define CLI_OPTIONS_H

struct CliOptions {
  const char* cmd = nullptr;
  int debug = false;
  int help = false;
  int quiet = false;
  int version = false;
  int argc = 0;
  const char** argv = nullptr;
};

#endif // CLI_OPTIONS_H
