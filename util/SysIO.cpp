//========================================================================
//
// SysIO.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "SysIO.h"
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

//------------------------------------------------------------------------

std::string getEnvVar(const std::string &var) {
  char *val = getenv(var.c_str());
  if (!val) {
    return std::string();
  }
  return std::string(val);
}

void setEnvVar(const std::string &var, const std::string &value) {
  setenv(var.c_str(), value.c_str(), 1);
}

std::string configDir() {
  char *s;
  struct passwd *pw;
  if ((s = getenv("HOME"))) {
    return s;
  } else if ((s = getenv("USER")) &&
	     (pw = getpwnam(s))) {
    return pw->pw_dir;
  } else if ((pw = getpwuid(getuid()))) {
    return pw->pw_dir;
  } else {
    return ".";
  }
}

bool pathIsFile(const std::string &path) {
  struct stat st;
  return !stat(path.c_str(), &st) && S_ISREG(st.st_mode);
}

DateTime pathModTime(const std::string &path) {
  struct stat st;
  if (stat(path.c_str(), &st)) {
    return DateTime();
  }
  return DateTime(st.st_mtime, st.st_mtim.tv_nsec);
}

bool readFile(const std::string &path, std::string &contents) {
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    return false;
  }
  std::string data;
  char buf[65536];
  while (1) {
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n == 0) {
      break;
    }
    if (n < 0) {
      close(fd);
      return false;
    }
    contents.append(buf, n);
  }
  close(fd);
  return true;
}

bool createDir(const std::string &path) {
  return mkdir(path.c_str(), 0755) == 0;
}

bool run(std::vector<std::string> cmd, int &exitStatus) {
  size_t n = cmd.size();
  const char **argv = new const char*[n + 1];
  for (size_t i = 0; i < n; ++i) {
    argv[i] = cmd[i].c_str();
  }
  argv[n] = nullptr;

  pid_t child = fork();
  if (child < 0) {
    delete[] argv;
    return false;
  }

  //--- child process
  if (child == 0) {
    execvp(argv[0], (char **)argv);
    kill(getpid(), SIGABRT);
  }

  //--- parent process
  delete[] argv;
  int status;
  if (waitpid(child, &status, 0) < 0 ||
      !WIFEXITED(status)) {
    exitStatus = -1;
    return false;
  }
  exitStatus = WEXITSTATUS(status);
  return true;
}
