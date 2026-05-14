/*
 *  Copyright (c) 2026 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include "pocket_app.h"

#include "cli_options.h"
#include "cli_parser.h"
#include "pocket_vm.h"

#include <pocketlang.h>

#include <cstdio>

namespace {

#define CLI_NOTICE                                                            \
  "PocketLang " PK_VERSION_STRING " (https://github.com/ThakeeNathees/pocketlang/)\n" \
  "Copyright (c) 2020-2021 ThakeeNathees\n"                                   \
  "Copyright (c) 2021-2022 Pocketlang Contributors\n"                         \
  "Free and open source software under the terms of the MIT license.\n"

} // namespace

int PocketApp::run(int argc, const char** argv) const {
  CliOptions options;
  CliParser parser;
  parser.parse(argc, argv, options);

  if (options.help) {
    parser.printUsage();
    return 0;
  }

  if (options.version) {
    std::fprintf(stdout, "pocketlang %s\n", PK_VERSION_STRING);
    return 0;
  }

  PocketVm vm;
  if (options.cmd != nullptr) {
    return vm.runString(options.cmd);
  }

  if (options.argc == 0) {
    if (!options.quiet) {
      std::printf("%s", CLI_NOTICE);
    }
    return vm.runRepl();
  }

  return vm.runFile(options.argv[0]);
}
