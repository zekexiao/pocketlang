/*
 *  Copyright (c) 2026 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include "CliParser.h"

#include <array>

#define _ARGPARSE_IMPL
#include "argparse.h"
#undef _ARGPARSE_IMPL

namespace {

const char* const kUsage[] = {
  "pocket ... [-c cmd | file] ...",
  nullptr,
};

struct CliOptionState {
  const char** cmd;
  int* debug;
  int* help;
  int* quiet;
  int* version;
};

std::array<argparse_option, 6> makeCliOptions(const CliOptionState& state) {
  return { {
    OPT_STRING('c', "cmd", (void*) state.cmd,
      "Evaluate and run the passed string.", nullptr, 0, 0),

    OPT_BOOLEAN('d', "debug", (void*) state.debug,
      "Compile and run the debug version.", nullptr, 0, 0),

    OPT_BOOLEAN('h', "help", (void*) state.help,
      "Prints this help message and exit.", nullptr, 0, 0),

    OPT_BOOLEAN('q', "quiet", (void*) state.quiet,
      "Don't print version and copyright statement on REPL startup.",
      nullptr, 0, 0),

    OPT_BOOLEAN('v', "version", state.version,
      "Prints the pocketlang version and exit.", nullptr, 0, 0),
    OPT_END(),
  } };
}

} // namespace

void CliParser::parse(int argc, const char** argv, CliOptions& options) const {
  auto cli_opts = makeCliOptions({
    &options.cmd,
    &options.debug,
    &options.help,
    &options.quiet,
    &options.version,
  });

  struct argparse argparse;
  argparse_init(&argparse, cli_opts.data(), kUsage, 0);
  options.argc = argparse_parse(&argparse, argc, argv);
  options.argv = argv;
}

void CliParser::printUsage() const {
  const char* cmd = nullptr;
  int debug = false, help = false, quiet = false, version = false;
  auto cli_opts = makeCliOptions({
    &cmd,
    &debug,
    &help,
    &quiet,
    &version,
  });

  struct argparse argparse;
  argparse_init(&argparse, cli_opts.data(), kUsage, 0);
  argparse_usage(&argparse);
}
