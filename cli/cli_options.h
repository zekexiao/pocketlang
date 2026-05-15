/*
 *  Copyright (c) 2026 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef CLI_OPTIONS_H
#define CLI_OPTIONS_H

class CliOptions {
 public:
  CliOptions() = default;
  ~CliOptions() = default;

  CliOptions(const CliOptions&) = default;
  CliOptions(CliOptions&&) noexcept = default;
  CliOptions& operator=(const CliOptions&) = default;
  CliOptions& operator=(CliOptions&&) noexcept = default;

  [[nodiscard]] const char* command() const noexcept { return command_; }
  [[nodiscard]] bool debug() const noexcept { return debug_; }
  [[nodiscard]] bool help() const noexcept { return help_; }
  [[nodiscard]] bool quiet() const noexcept { return quiet_; }
  [[nodiscard]] bool version() const noexcept { return version_; }
  [[nodiscard]] int argc() const noexcept { return argc_; }
  [[nodiscard]] const char* const* argv() const noexcept { return argv_; }

  void setCommand(const char* command) noexcept { command_ = command; }
  void setDebug(bool debug) noexcept { debug_ = debug; }
  void setHelp(bool help) noexcept { help_ = help; }
  void setQuiet(bool quiet) noexcept { quiet_ = quiet; }
  void setVersion(bool version) noexcept { version_ = version; }
  void setArgc(int argc) noexcept { argc_ = argc; }
  void setArgv(const char** argv) noexcept { argv_ = argv; }

 private:
  const char* command_ = nullptr;
  bool debug_ = false;
  bool help_ = false;
  bool quiet_ = false;
  bool version_ = false;
  int argc_ = 0;
  const char* const* argv_ = nullptr;
};

#endif // CLI_OPTIONS_H
