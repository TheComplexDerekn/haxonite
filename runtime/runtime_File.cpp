//========================================================================
//
// runtime_File.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "runtime_File.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "runtime_String.h"
#include "runtime_StringBuf.h"

//------------------------------------------------------------------------

#define fileModeRead   0
#define fileModeWrite  1
#define fileModeAppend 2

//------------------------------------------------------------------------

struct FileResource {
  ResourceObject resObj;
  FILE *f;
};

struct File {
  uint64_t hdr;
  Cell fileResource;    // resource pointer -> FileResource
};

#define fileNCells (sizeof(File) / sizeof(Cell) - 1)

struct TempFile {
  uint64_t hdr;
  Cell path;
  Cell file;
};

#define tempFileNCells (sizeof(TempFile) / sizeof(Cell) - 1)

//------------------------------------------------------------------------

static Cell makeFileObject(FILE *f, BytecodeEngine &engine);
static void finalizeFile(ResourceObject *resObj);
static void closeFile(FileResource *fileResource);

//------------------------------------------------------------------------

// openFile(path: String, mode: FileMode) -> Result[File]
static NativeFuncDefn(runtime_openFile_S8FileMode) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);
  Cell &modeCell = engine.arg(1);

  const char *mode;
  switch (cellInt(modeCell)) {
  case fileModeRead:
    mode = "rb";
    break;
  case fileModeWrite:
    mode = "wb";
    break;
  case fileModeAppend:
    mode = "ab";
    break;
  default:
    BytecodeEngine::fatalError("Invalid argument");
  }

  std::string path = stringToStdString(pathCell);

  FILE *f = fopen(path.c_str(), mode);

  if (f) {
    Cell fileCell = makeFileObject(f, engine);
    engine.push(fileCell);
  } else {
    engine.push(cellMakeError());
  }
}

// openTempFile(prefix: String) -> Result[TempFile]
static NativeFuncDefn(runtime_openTempFile_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &prefixCell = engine.arg(0);

  std::string prefix = stringToStdString(prefixCell);

  const char *tmpDir = getenv("TMPDIR");
  if (!tmpDir) {
    tmpDir = "/tmp";
  }
  std::string tmpl = std::string(tmpDir) + "/" + prefix + "XXXXXX";
  int fd = mkstemp(tmpl.data());
  if (fd < 0) {
    engine.push(cellMakeError());
    return;
  }
  FILE *f = fdopen(fd, "wb");
  if (!f) {
    close(fd);
    engine.push(cellMakeError());
    return;
  }

  // NB: this may trigger GC
  Cell pathCell = stringMake((const uint8_t *)tmpl.c_str(), tmpl.size(), engine);
  engine.pushGCRoot(pathCell);

  // NB: this may trigger GC
  Cell fileCell = makeFileObject(f, engine);
  engine.pushGCRoot(fileCell);

  // NB: this may trigger GC
  TempFile *tempFile = (TempFile *)engine.heapAllocTuple(tempFileNCells, 0);
  tempFile->path = pathCell;
  tempFile->file = fileCell;

  engine.push(cellMakeHeapPtr(tempFile));

  engine.popGCRoot(fileCell);
  engine.popGCRoot(pathCell);
}

static Cell makeFileObject(FILE *f, BytecodeEngine &engine) {
  FileResource *fileResource;
  try {
    fileResource = new FileResource();
  } catch (std::bad_alloc) {
    BytecodeEngine::fatalError("Out of memory");
  }
  fileResource->resObj.finalizer = &finalizeFile;
  fileResource->f = f;

  // NB: this may trigger GC
  File *file = (File *)engine.heapAllocTuple(fileNCells, 0);
  file->fileResource = cellMakeResourcePtr(fileResource);
  engine.addResourceObject(&fileResource->resObj);
  return cellMakeHeapPtr(file);
}

// close(f: File)
static NativeFuncDefn(runtime_close_4File) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &fCell = engine.arg(0);

  File *file = (File *)cellHeapPtr(fCell);
  engine.failOnNilPtr(file);
  FileResource *fileResource = (FileResource *)cellResourcePtr(file->fileResource);
  engine.removeResourceObject(&fileResource->resObj);
  file->fileResource = cellMakeNilResourcePtr();
  closeFile(fileResource);

  engine.push(cellMakeInt(0));
}

static void finalizeFile(ResourceObject *resObj) {
  FileResource *fileResource = (FileResource *)resObj;
  closeFile(fileResource);
}

static void closeFile(FileResource *fileResource) {
  fclose(fileResource->f);
  delete fileResource;
}

// read(f: File, sb: StringBuf, n: Int) -> Result[Int]
static NativeFuncDefn(runtime_read_4FileTI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1)) ||
      !cellIsInt(engine.arg(2))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &fCell = engine.arg(0);
  Cell &sbCell = engine.arg(1);
  Cell &nCell = engine.arg(2);

  File *file = (File *)cellHeapPtr(fCell);
  engine.failOnNilPtr(file);
  FileResource *fileResource = (FileResource *)cellResourcePtr(file->fileResource);

  size_t n = (size_t)cellInt(nCell);
  std::unique_ptr<uint8_t[]> buf = std::unique_ptr<uint8_t[]>(new uint8_t[n]);
  clearerr(fileResource->f);
  size_t nBytesRead = fread(buf.get(), 1, n, fileResource->f);
  if (nBytesRead < n && ferror(fileResource->f)) {
    engine.push(cellMakeError());
  } else {
    if (nBytesRead > 0) {
      // NB: this may trigger GC
      stringBufAppend(sbCell, buf.get(), (int64_t)nBytesRead, engine);
    }
    engine.push(cellMakeInt((int64_t)nBytesRead));
  }
}

// readLine(f: File) -> Result[String]
static NativeFuncDefn(runtime_readLine_4File) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &fCell = engine.arg(0);

  File *file = (File *)cellHeapPtr(fCell);
  engine.failOnNilPtr(file);
  FileResource *fileResource = (FileResource *)cellResourcePtr(file->fileResource);

  std::string s;
  int c;
  do {
    c = fgetc(fileResource->f);
    if (c < 0) {
      break;
    }
    s.push_back((char)c);
  } while (c != '\n');
  // NB: this may trigger GC
  engine.push(stringMake((uint8_t *)s.c_str(), (int64_t)s.size(), engine));
}

// write(f: File, s: String) -> Result[Int]
static NativeFuncDefn(runtime_write_4FileS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &fCell = engine.arg(0);
  Cell &sCell = engine.arg(1);

  File *file = (File *)cellHeapPtr(fCell);
  engine.failOnNilPtr(file);
  FileResource *fileResource = (FileResource *)cellResourcePtr(file->fileResource);

  uint8_t *s = stringData(sCell);
  int64_t n = stringByteLength(sCell);
  size_t nBytesWritten = fwrite(s, 1, n, fileResource->f);
  if (nBytesWritten == (size_t)n) {
    engine.push(cellMakeInt((int64_t)nBytesWritten));
  } else {
    engine.push(cellMakeError());
  }
}

// write(f: File, sb: StringBuf) -> Result[Int]
static NativeFuncDefn(runtime_write_4FileT) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &fCell = engine.arg(0);
  Cell &sbCell = engine.arg(1);

  File *file = (File *)cellHeapPtr(fCell);
  engine.failOnNilPtr(file);
  FileResource *fileResource = (FileResource *)cellResourcePtr(file->fileResource);

  uint8_t *s = stringBufData(sbCell);
  int64_t n = stringBufLength(sbCell);
  size_t nBytesWritten = fwrite(s, 1, n, fileResource->f);
  if (nBytesWritten == (size_t)n) {
    engine.push(cellMakeInt((int64_t)nBytesWritten));
  } else {
    engine.push(cellMakeError());
  }
}

// read(sb: StringBuf, n: Int) -> Result[Int]
static NativeFuncDefn(runtime_read_TI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sbCell = engine.arg(0);
  Cell &nCell = engine.arg(1);

  size_t n = (size_t)cellInt(nCell);
  std::unique_ptr<uint8_t[]> buf = std::unique_ptr<uint8_t[]>(new uint8_t[n]);
  clearerr(stdin);
  size_t nBytesRead = fread((char *)buf.get(), 1, n, stdin);
  if (nBytesRead < n && ferror(stdin)) {
    engine.push(cellMakeError());
  } else {
    if (nBytesRead > 0) {
      // NB: this may trigger GC
      stringBufAppend(sbCell, buf.get(), nBytesRead, engine);
    }
    engine.push(cellMakeInt((int64_t)nBytesRead));
  }
}

// readLine() -> Result[String]
static NativeFuncDefn(runtime_readLine) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif

  std::string s;
  int c;
  do {
    c = fgetc(stdin);
    if (c < 0) {
      break;
    }
    s.push_back((char)c);
  } while (c != '\n');
  // NB: this may trigger GC
  engine.push(stringMake((uint8_t *)s.c_str(), (int64_t)s.size(), engine));
}

// write(s: String) -> Result[Int]
static NativeFuncDefn(runtime_write_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);

  uint8_t *s = stringData(sCell);
  int64_t n = stringByteLength(sCell);
  size_t nBytesWritten = fwrite(s, 1, n, stdout);
  if (nBytesWritten == (size_t)n) {
    engine.push(cellMakeInt((int64_t)nBytesWritten));
  } else {
    engine.push(cellMakeError());
  }
}

// ewrite(s: String) -> Result[Int]
static NativeFuncDefn(runtime_ewrite_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);

  uint8_t *s = stringData(sCell);
  int64_t n = stringByteLength(sCell);
  size_t nBytesWritten = fwrite(s, 1, n, stderr);
  if (nBytesWritten == (size_t)n) {
    engine.push(cellMakeInt((int64_t)nBytesWritten));
  } else {
    engine.push(cellMakeError());
  }
}

void runtime_File_init(BytecodeEngine &engine) {
  engine.addNativeFunction("openFile_S8FileMode", &runtime_openFile_S8FileMode);
  engine.addNativeFunction("openTempFile_S", &runtime_openTempFile_S);
  engine.addNativeFunction("close_4File", &runtime_close_4File);
  engine.addNativeFunction("read_4FileTI", &runtime_read_4FileTI);
  engine.addNativeFunction("readLine_4File", &runtime_readLine_4File);
  engine.addNativeFunction("write_4FileS", &runtime_write_4FileS);
  engine.addNativeFunction("write_4FileT", &runtime_write_4FileT);
  engine.addNativeFunction("read_TI", &runtime_read_TI);
  engine.addNativeFunction("readLine", &runtime_readLine);
  engine.addNativeFunction("write_S", &runtime_write_S);
  engine.addNativeFunction("ewrite_S", &runtime_ewrite_S);
}
