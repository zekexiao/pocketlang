/*
 *  Copyright (c) 2026 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include "CliParser.h"

#define _ARGPARSE_IMPL
#include "argparse.h"
#undef _ARGPARSE_IMPL

namespace {

const char* const kUsage[] = {
  "pocket ... [-c cmd | file] ...",
  nullptr,
};

} // namespace

void CliParser::parse(int argc, const char** argv, CliOptions& options) const {
  struct argparse_option cli_opts[] = {
    OPT_STRING('c', "cmd", (void*)&options.cmd,
      "Evaluate and run the passed string.", nullptr, 0, 0),

    OPT_BOOLEAN('d', "debug", (void*)&options.debug,
      "Compile and run the debug version.", nullptr, 0, 0),

    OPT_BOOLEAN('h', "help", (void*)&options.help,
      "Prints this help message and exit.", nullptr, 0, 0),

    OPT_BOOLEAN('q', "quiet", (void*)&options.quiet,
      "Don't print version and copyright statement on REPL startup.",
      nullptr, 0, 0),

    OPT_BOOLEAN('v', "version", &options.version,
      "Prints the pocketlang version and exit.", nullptr, 0, 0),
    OPT_END(),
  };

  struct argparse argparse;
  argparse_init(&argparse, cli_opts, kUsage, 0);
  options.argc = argparse_parse(&argparse, argc, argv);
  options.argv = argv;
}

void CliParser::printUsage() const {
  const char* cmd = nullptr;
  int debug = false, help = false, quiet = false, version = false;
  struct argparse_option cli_opts[] = {
    OPT_STRING('c', "cmd", (void*)&cmd,
      "Evaluate and run the passed string.", nullptr, 0, 0),

    OPT_BOOLEAN('d', "debug", (void*)&debug,
      "Compile and run the debug version.", nullptr, 0, 0),

    OPT_BOOLEAN('h', "help", (void*)&help,
      "Prints this help message and exit.", nullptr, 0, 0),

    OPT_BOOLEAN('q', "quiet", (void*)&quiet,
      "Don't print version and copyright statement on REPL startup.",
      nullptr, 0, 0),

    OPT_BOOLEAN('v', "version", &version,
      "Prints the pocketlang version and exit.", nullptr, 0, 0),
    OPT_END(),
  };

  struct argparse argparse;
  argparse_init(&argparse, cli_opts, kUsage, 0);
  argparse_usage(&argparse);
}
