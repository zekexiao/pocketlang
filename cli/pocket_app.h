/*
 *  Copyright (c) 2026 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef POCKET_APP_H
#define POCKET_APP_H

class PocketApp {
 public:
  PocketApp() = default;
  ~PocketApp() = default;

  PocketApp(const PocketApp&) = default;
  PocketApp(PocketApp&&) noexcept = default;
  PocketApp& operator=(const PocketApp&) = default;
  PocketApp& operator=(PocketApp&&) noexcept = default;

  [[nodiscard]] int run(int argc, const char** argv) const;
};

#endif // POCKET_APP_H
