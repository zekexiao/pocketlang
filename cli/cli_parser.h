/*
 *  Copyright (c) 2026 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef CLI_PARSER_H
#define CLI_PARSER_H

#include "cli_options.h"

class CliParser {
 public:
  CliParser() = default;
  ~CliParser() = default;

  CliParser(const CliParser&) = default;
  CliParser(CliParser&&) noexcept = default;
  CliParser& operator=(const CliParser&) = default;
  CliParser& operator=(CliParser&&) noexcept = default;

  void parse(int argc, const char** argv, CliOptions& options) const;
  void printUsage() const;
};

#endif // CLI_PARSER_H
