/*
 *  Copyright (c) 2026 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef CLI_PARSER_H
#define CLI_PARSER_H

#include "CliOptions.h"

class CliParser {
 public:
  void parse(int argc, const char** argv, CliOptions& options) const;
  void printUsage() const;
};

#endif // CLI_PARSER_H
