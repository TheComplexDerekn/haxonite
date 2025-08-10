//========================================================================
//
// runtime_system.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "runtime_system.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string>
#include "aconf.h"
#include "runtime_datetime.h"
#include "runtime_String.h"
#include "runtime_StringBuf.h"
#include "runtime_Vector.h"

//------------------------------------------------------------------------

static Cell commandLineArgsVector = cellNilHeapPtrInit;

//------------------------------------------------------------------------

// commandLineArgs() -> Vector[String]
static NativeFuncDefn(runtime_commandLineArgs) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  engine.push(commandLineArgsVector);
}

// exit(exitCode: Int)
static NativeFuncDefn(runtime_exit_I) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsInt(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &exitCodeCell = engine.arg(0);

  exit((int)cellInt(exitCodeCell));
}

// pathExists(path: String) -> Bool
static NativeFuncDefn(runtime_pathExists_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);

  std::string path = stringToStdString(pathCell);

  struct stat st;
  engine.push(cellMakeBool(!lstat(path.c_str(), &st)));
}

// pathIsDir(path: String) -> Bool
static NativeFuncDefn(runtime_pathIsDir_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);

  std::string path = stringToStdString(pathCell);

  struct stat st;
  engine.push(cellMakeBool(!stat(path.c_str(), &st) && S_ISDIR(st.st_mode)));
}

// pathIsFile(path: String) -> Bool
static NativeFuncDefn(runtime_pathIsFile_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);

  std::string path = stringToStdString(pathCell);

  struct stat st;
  engine.push(cellMakeBool(!stat(path.c_str(), &st) && S_ISREG(st.st_mode)));
}

// modTime(path: String) -> Result[Timestamp]
static NativeFuncDefn(runtime_modTime_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);

  std::string path = stringToStdString(pathCell);

  struct stat st;
  if (stat(path.c_str(), &st)) {
    engine.push(cellMakeError());
  } else {
#if HAVE_STAT_ST_MTIM
    int64_t ns = (int64_t)st.st_mtim.tv_nsec;
#elif HAVE_STAT_ST_MTIMESPEC
    int64_t ns = (int64_t)st.st_mtimespec.tv_nsec;
#else
    int64_t ns = 0;
#endif
    engine.push(timestampMake((int64_t)st.st_mtime, ns, engine));
  }
}

// currentDir() -> String
static NativeFuncDefn(runtime_currentDir) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif

  char s[PATH_MAX + 1];
  if (!getcwd(s, sizeof(s))) {
    s[0] = '\0';
  }
  engine.push(stringMake((const uint8_t *)s, (int64_t)strlen(s), engine));
}

// homeDir() -> String
static NativeFuncDefn(runtime_homeDir) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif

  const char *s;
  if (!(s = getenv("HOME"))) {
    struct passwd *pw;
    char *user = getenv("USER");
    pw = user ? getpwnam(user) : getpwuid(getuid());
    if (pw) {
      s = pw->pw_dir;
    } else {
      s = ".";
    }
  }
  engine.push(stringMake((const uint8_t *)s, (int64_t)strlen(s), engine));
}

// createDir(path: String) -> Result[]
static NativeFuncDefn(runtime_createDir_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);

  std::string path = stringToStdString(pathCell);

  if (mkdir(path.c_str(), 0755) == 0) {
    engine.push(cellMakeInt(0));
  } else {
    engine.push(cellMakeError());
  }
}

// delete(path: String) -> Result[]
static NativeFuncDefn(runtime_delete_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);

  std::string path = stringToStdString(pathCell);

  if (unlink(path.c_str()) == 0) {
    engine.push(cellMakeInt(0));
  } else {
    engine.push(cellMakeError());
  }
}

// deleteDir(path: String) -> Result[]
static NativeFuncDefn(runtime_deleteDir_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);

  std::string path = stringToStdString(pathCell);

  if (rmdir(path.c_str()) == 0) {
    engine.push(cellMakeInt(0));
  } else {
    engine.push(cellMakeError());
  }
}

// rename(oldPath: String, newPath: String) -> Result[]
static NativeFuncDefn(runtime_rename_SS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &oldPathCell = engine.arg(0);
  Cell &newPathCell = engine.arg(1);

  std::string oldPath = stringToStdString(oldPathCell);
  std::string newPath = stringToStdString(newPathCell);

  if (rename(oldPath.c_str(), newPath.c_str()) == 0) {
    engine.push(cellMakeInt(0));
  } else {
    engine.push(cellMakeError());
  }
}

// readFile(path: String) -> Result[String]
static NativeFuncDefn(runtime_readFile_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);

  std::string path = stringToStdString(pathCell);

  FILE *f = fopen(path.c_str(), "rb");
  if (f) {
    char buf[4096];
    std::string sBuf;
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
      sBuf.append(buf, n);
    }
    fclose(f);
    engine.push(stringMake((const uint8_t *)sBuf.c_str(), (int64_t)sBuf.size(), engine));
  } else {
    engine.push(cellMakeError());
  }
}

// readFile(path: String, sb: StringBuf) -> Result[]
static NativeFuncDefn(runtime_readFile_ST) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsHeapPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);
  Cell &sbCell = engine.arg(1);

  std::string path = stringToStdString(pathCell);

  FILE *f = fopen(path.c_str(), "rb");
  if (f) {
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
      stringBufAppend(sbCell, (uint8_t *)buf, n, engine);
    }
    fclose(f);
    engine.push(sbCell);
  } else {
    engine.push(cellMakeError());
  }
}

// writeFile(path: String, s: String) -> Result[]
static NativeFuncDefn(runtime_writeFile_SS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);
  Cell &sCell = engine.arg(1);

  std::string path = stringToStdString(pathCell);

  FILE *f = fopen(path.c_str(), "wb");
  if (f) {
    int64_t n = stringByteLength(sCell);
    if (fwrite(stringData(sCell), 1, n, f) == n) {
      engine.push(cellMakeInt(0));
    } else {
      engine.push(cellMakeError());
    }
    fclose(f);
  } else {
    engine.push(cellMakeError());
  }
}

// writeFile(path: String, sb: StringBuf) -> Result[]
static NativeFuncDefn(runtime_writeFile_ST) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsHeapPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);
  Cell &sbCell = engine.arg(1);

  std::string path = stringToStdString(pathCell);

  FILE *f = fopen(path.c_str(), "wb");
  if (f) {
    int64_t n = stringBufLength(sbCell);
    if (fwrite(stringBufData(sbCell), 1, n, f) == n) {
      engine.push(cellMakeInt(0));
    } else {
      engine.push(cellMakeError());
    }
    fclose(f);
  } else {
    engine.push(cellMakeError());
  }
}

// readDir(path: String) -> Result[Vector[String]]
static NativeFuncDefn(runtime_readDir_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);

  std::string path = stringToStdString(pathCell);

  DIR *dir = opendir(path.c_str());
  if (dir) {
    Cell vCell = vectorMake(engine);
    engine.pushGCRoot(vCell);
    struct dirent *de;
    while ((de = readdir(dir))) {
      Cell entryCell = stringMake((uint8_t *)de->d_name, strlen(de->d_name), engine);
      vectorAppend(vCell, entryCell, engine);
    }
    closedir(dir);
    engine.push(vCell);
    engine.popGCRoot(vCell);
  } else {
    engine.push(cellMakeError());
  }
}

// copyFile(src: String, dest: String) -> Result[]
static NativeFuncDefn(runtime_copyFile_SS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &srcCell = engine.arg(0);
  Cell &destCell = engine.arg(1);

  std::string srcPath = stringToStdString(srcCell);
  std::string destPath = stringToStdString(destCell);

  FILE *src = fopen(srcPath.c_str(), "rb");
  if (!src) {
    engine.push(cellMakeError());
    return;
  }

  FILE *dest = fopen(destPath.c_str(), "wb");
  if (!dest) {
    fclose(src);
    engine.push(cellMakeError());
    return;
  }

  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
    if (fwrite(buf, 1, n, dest) != n) {
      fclose(dest);
      fclose(src);
      engine.push(cellMakeError());
      return;
    }
  }

  fclose(dest);
  fclose(src);

  engine.push(cellMakeInt(0));
}

// run(command: Vector[String]) -> Result[Int]
static NativeFuncDefn(runtime_run_VS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &commandCell = engine.arg(0);

  //--- fork
  pid_t child = fork();
  if (child < 0) {
    engine.push(cellMakeError());
    return;
  }

  //--- child process
  if (child == 0) {

    //--- construct argv
    int64_t argc = vectorLength(commandCell);
    if (argc < 1) {
      BytecodeEngine::fatalError("Invalid argument");
    }
    if (argc > INT64_MAX - 1) {
      BytecodeEngine::fatalError("Integer overflow");
    }
    char **argv = new char*[argc + 1];
    for (int64_t i = 0; i < argc; ++i) {
      Cell argCell = vectorGet(commandCell, i);
      std::string argString = stringToStdString(argCell);
      argv[i] = new char[argString.size() + 1];
      memcpy(argv[i], argString.c_str(), argString.size() + 1);
    }
    argv[argc] = nullptr;

    //--- exec
    execvp(argv[0], argv);
    kill(getpid(), SIGABRT);  // just in case execvp fails
  }

  //--- parent process
  int status;
  if (waitpid(child, &status, 0) < 0 ||
      !WIFEXITED(status)) {
    engine.push(cellMakeError());
    return;
  }
  engine.push(cellMakeInt(WEXITSTATUS(status)));
}

// sleep(useconds: Int)
static NativeFuncDefn(runtime_sleep_I) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsInt(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &usecondsCell = engine.arg(0);

  usleep(cellInt(usecondsCell));

  engine.push(cellMakeInt(0));
}

// heapSize() -> Int
static NativeFuncDefn(runtime_heapSize) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif

  engine.push(cellMakeInt((int64_t)engine.currentHeapSize()));
}

//------------------------------------------------------------------------

void runtime_system_init(BytecodeEngine &engine) {
  engine.pushGCRoot(commandLineArgsVector);

  engine.addNativeFunction("commandLineArgs", &runtime_commandLineArgs);
  engine.addNativeFunction("exit_I", &runtime_exit_I);
  engine.addNativeFunction("pathExists_S", &runtime_pathExists_S);
  engine.addNativeFunction("pathIsDir_S", &runtime_pathIsDir_S);
  engine.addNativeFunction("pathIsFile_S", &runtime_pathIsFile_S);
  engine.addNativeFunction("modTime_S", &runtime_modTime_S);
  engine.addNativeFunction("currentDir", &runtime_currentDir);
  engine.addNativeFunction("homeDir", &runtime_homeDir);
  engine.addNativeFunction("createDir_S", &runtime_createDir_S);
  engine.addNativeFunction("delete_S", &runtime_delete_S);
  engine.addNativeFunction("deleteDir_S", &runtime_deleteDir_S);
  engine.addNativeFunction("rename_SS", &runtime_rename_SS);
  engine.addNativeFunction("readFile_S", &runtime_readFile_S);
  engine.addNativeFunction("readFile_ST", &runtime_readFile_ST);
  engine.addNativeFunction("writeFile_SS", &runtime_writeFile_SS);
  engine.addNativeFunction("writeFile_ST", &runtime_writeFile_ST);
  engine.addNativeFunction("readDir_S", &runtime_readDir_S);
  engine.addNativeFunction("copyFile_SS", &runtime_copyFile_SS);
  engine.addNativeFunction("run_VS", &runtime_run_VS);
  engine.addNativeFunction("sleep_I", &runtime_sleep_I);
  engine.addNativeFunction("heapSize", &runtime_heapSize);
}

//------------------------------------------------------------------------
// support functions
//------------------------------------------------------------------------

void setCommandLineArgs(int argc, char *argv[], BytecodeEngine &engine) {
  commandLineArgsVector = vectorMake(engine);
  for (int i = 0; i < argc; ++i) {
    Cell sCell = stringMake((uint8_t *)argv[i], strlen(argv[i]), engine);
    engine.pushGCRoot(sCell);
    vectorAppend(commandLineArgsVector, sCell, engine);
    engine.popGCRoot(sCell);
  }
}
