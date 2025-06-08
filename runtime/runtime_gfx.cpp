//========================================================================
//
// runtime_gfx.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "runtime_gfx.h"
#include <limits.h>
#include <math.h>
#include <string.h>
#include "BytecodeDefs.h"
#include "Gfx.h"
#include "runtime_String.h"

//------------------------------------------------------------------------
// color
//------------------------------------------------------------------------

// argb(a: Int, r: Int, g: Int, b: Int) -> ARGB
static NativeFuncDefn(runtime_argb_IIII) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 4 ||
      !cellIsInt(engine.arg(0)) ||
      !cellIsInt(engine.arg(1)) ||
      !cellIsInt(engine.arg(2)) ||
      !cellIsInt(engine.arg(3))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &aCell = engine.arg(0);
  Cell &rCell = engine.arg(1);
  Cell &gCell = engine.arg(2);
  Cell &bCell = engine.arg(3);
  int64_t a = cellInt(aCell);
  int64_t r = cellInt(rCell);
  int64_t g = cellInt(gCell);
  int64_t b = cellInt(bCell);
  if (a < 0 || a > 255 || r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
    BytecodeEngine::fatalError("Invalid argument");
  }
  int64_t argb = (a << 24) | (r << 16) | (g << 8) | b;
  engine.push(cellMakeInt(argb));
}

// rgb(r: Int, g: Int, b: Int) -> ARGB
static NativeFuncDefn(runtime_rgb_III) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsInt(engine.arg(0)) ||
      !cellIsInt(engine.arg(1)) ||
      !cellIsInt(engine.arg(2))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &rCell = engine.arg(0);
  Cell &gCell = engine.arg(1);
  Cell &bCell = engine.arg(2);
  int64_t a = 0xff;
  int64_t r = cellInt(rCell);
  int64_t g = cellInt(gCell);
  int64_t b = cellInt(bCell);
  if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
    BytecodeEngine::fatalError("Invalid argument");
  }
  int64_t argb = (a << 24) | (r << 16) | (g << 8) | b;
  engine.push(cellMakeInt(argb));
}

// a(argb: ARGB) -> Int
static NativeFuncDefn(runtime_a_4ARGB) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsInt(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &argbCell = engine.arg(0);

  int64_t argb = cellInt(argbCell);
  int64_t a = (argb >> 24) & 0xff;

  engine.push(cellMakeInt(a));
}

// r(argb: ARGB) -> Int
static NativeFuncDefn(runtime_r_4ARGB) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsInt(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &argbCell = engine.arg(0);

  int64_t argb = cellInt(argbCell);
  int64_t r = (argb >> 16) & 0xff;

  engine.push(cellMakeInt(r));
}

// g(argb: ARGB) -> Int
static NativeFuncDefn(runtime_g_4ARGB) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsInt(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &argbCell = engine.arg(0);

  int64_t argb = cellInt(argbCell);
  int64_t g = (argb >> 8) & 0xff;

  engine.push(cellMakeInt(g));
}

// b(argb: ARGB) -> Int
static NativeFuncDefn(runtime_b_4ARGB) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsInt(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &argbCell = engine.arg(0);

  int64_t argb = cellInt(argbCell);
  int64_t b = argb & 0xff;

  engine.push(cellMakeInt(b));
}

//------------------------------------------------------------------------
// matrix / point
//------------------------------------------------------------------------

// multiply(mat1: Matrix, mat2: Matrix) -> Matrix
static NativeFuncDefn(runtime_multiply_6Matrix6Matrix) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsHeapPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &mat1Cell = engine.arg(0);
  Cell &mat2Cell = engine.arg(1);

  // NB: this can trigger GC
  Matrix *out = (Matrix *)engine.heapAllocTuple(matrixNCells, 0);

  Matrix *mat1 = (Matrix *)cellHeapPtr(mat1Cell);
  engine.failOnNilPtr(mat1);
  float a1 = cellFloat(mat1->a);
  float b1 = cellFloat(mat1->b);
  float c1 = cellFloat(mat1->c);
  float d1 = cellFloat(mat1->d);
  float tx1 = cellFloat(mat1->tx);
  float ty1 = cellFloat(mat1->ty);
  Matrix *mat2 = (Matrix *)cellHeapPtr(mat2Cell);
  engine.failOnNilPtr(mat2);
  float a2 = cellFloat(mat2->a);
  float b2 = cellFloat(mat2->b);
  float c2 = cellFloat(mat2->c);
  float d2 = cellFloat(mat2->d);
  float tx2 = cellFloat(mat2->tx);
  float ty2 = cellFloat(mat2->ty);

  // mat1        * mat2        = out
  // [a1  b1  0]   [a2  b2  0]   [a  b  0]
  // [c1  d1  0] * [c2  d2  0] = [c  d  0]
  // [tx1 ty1 1]   [tx2 ty2 1]   [tx ty 1]
  out->a = cellMakeFloat(a1 * a2 + b1 * c2);
  out->b = cellMakeFloat(a1 * b2 + b1 * d2);
  out->c = cellMakeFloat(c1 * a2 + d1 * c2);
  out->d = cellMakeFloat(c1 * b2 + d1 * d2);
  out->tx = cellMakeFloat(tx1 * a2 + ty1 * c2 + tx2);
  out->ty = cellMakeFloat(tx1 * b2 + ty1 * d2 + ty2);

  engine.push(cellMakeHeapPtr(out));
}

// transform(pt: Point, mat: Matrix) -> Point
static NativeFuncDefn(runtime_transform_5Point6Matrix) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsHeapPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &ptCell = engine.arg(0);
  Cell &matCell = engine.arg(1);

  // NB: this can trigger GC
  Point *out = (Point *)engine.heapAllocTuple(pointNCells, 0);

  Point *pt = (Point *)cellHeapPtr(ptCell);
  engine.failOnNilPtr(pt);
  float x = cellFloat(pt->x);
  float y = cellFloat(pt->y);
  Matrix *mat = (Matrix *)cellHeapPtr(matCell);
  engine.failOnNilPtr(mat);
  float a = cellFloat(mat->a);
  float b = cellFloat(mat->b);
  float c = cellFloat(mat->c);
  float d = cellFloat(mat->d);
  float tx = cellFloat(mat->tx);
  float ty = cellFloat(mat->ty);

  // pt      * mat       = out
  //           [a  b  0]
  // [x y 1] * [c  d  0] = [x y 1]
  //           [tx ty 1]
  out->x = cellMakeFloat(x * a + y * c + tx);
  out->y = cellMakeFloat(x * b + y * d + ty);

  engine.push(cellMakeHeapPtr(out));
}

// invert(mat: Matrix) -> Matrix
static NativeFuncDefn(runtime_invert_6Matrix) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &matCell = engine.arg(0);

  // NB: this can trigger GC
  Matrix *out = (Matrix *)engine.heapAllocTuple(matrixNCells, 0);

  Matrix *mat = (Matrix *)cellHeapPtr(matCell);
  engine.failOnNilPtr(mat);
  float a = cellFloat(mat->a);
  float b = cellFloat(mat->b);
  float c = cellFloat(mat->c);
  float d = cellFloat(mat->d);
  float tx = cellFloat(mat->tx);
  float ty = cellFloat(mat->ty);

  float det = a * d - b * c;
  if (fabsf(det) < 1e-10) {
    BytecodeEngine::fatalError("Singular matrix");
  }
  float idet = 1.0 / det;
  out->a = cellMakeFloat(d * idet);
  out->b = cellMakeFloat(-b * idet);
  out->c = cellMakeFloat(-c * idet);
  out->d = cellMakeFloat(a * idet);
  out->tx = cellMakeFloat((c * ty - d * tx) * idet);
  out->ty = cellMakeFloat((b * tx - a * ty) * idet);

  engine.push(cellMakeHeapPtr(out));
}

//------------------------------------------------------------------------
// state save/restore
//------------------------------------------------------------------------

// pushState(dest: Image)
static NativeFuncDefn(runtime_pushState_5Image) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);

  gfxPushState(destCell);

  engine.push(cellMakeInt(0));
}

// popState(dest: Image)
static NativeFuncDefn(runtime_popState_5Image) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);

  gfxPopState(destCell);

  engine.push(cellMakeInt(0));
}

//------------------------------------------------------------------------
// state modification
//------------------------------------------------------------------------

// setMatrix(dest: Image, matrix: Matrix)
static NativeFuncDefn(runtime_setMatrix_5Image6Matrix) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsHeapPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &matrixCell = engine.arg(1);

  gfxSetMatrix(destCell, matrixCell);

  engine.push(cellMakeInt(0));
}

// concatMatrix(dest: Image, matrix: Matrix)
static NativeFuncDefn(runtime_concatMatrix_5Image6Matrix) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsHeapPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &matrixCell = engine.arg(1);

  gfxConcatMatrix(destCell, matrixCell);

  engine.push(cellMakeInt(0));
}

// setClipRect(dest: Image, x: Float, y: Float, w: Float, h: Float)
static NativeFuncDefn(runtime_setClipRect_5ImageFFFF) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 5 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsFloat(engine.arg(1)) ||
      !cellIsFloat(engine.arg(2)) ||
      !cellIsFloat(engine.arg(3)) ||
      !cellIsFloat(engine.arg(4))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &xCell = engine.arg(1);
  Cell &yCell = engine.arg(2);
  Cell &wCell = engine.arg(3);
  Cell &hCell = engine.arg(4);

  gfxSetClipRect(destCell, cellFloat(xCell), cellFloat(yCell), cellFloat(wCell), cellFloat(hCell));

  engine.push(cellMakeInt(0));
}

// intersectClipRect(dest: Image, x: Float, y: Float, w: Float, h: Float)
static NativeFuncDefn(runtime_intersectClipRect_5ImageFFFF) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 5 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsFloat(engine.arg(1)) ||
      !cellIsFloat(engine.arg(2)) ||
      !cellIsFloat(engine.arg(3)) ||
      !cellIsFloat(engine.arg(4))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &xCell = engine.arg(1);
  Cell &yCell = engine.arg(2);
  Cell &wCell = engine.arg(3);
  Cell &hCell = engine.arg(4);

  gfxIntersectClipRect(destCell, cellFloat(xCell), cellFloat(yCell),
		       cellFloat(wCell), cellFloat(hCell));

  engine.push(cellMakeInt(0));
}

// setColor(dest: Image, color: ARGB)
static NativeFuncDefn(runtime_setColor_5Image4ARGB) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &colorCell = engine.arg(1);

  int64_t color = cellInt(colorCell);
  gfxSetColor(destCell, color);

  engine.push(cellMakeInt(0));
}

// setFillRule(dest: Image, rule: FillRule)
static NativeFuncDefn(runtime_setFillRule_5Image8FillRule) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &ruleCell = engine.arg(1);

  gfxSetFillRule(destCell, cellInt(ruleCell));

  engine.push(cellMakeInt(0));
}

// setStrokeWidth(dest: Image, width: Float)
static NativeFuncDefn(runtime_setStrokeWidth_5ImageF) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsFloat(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &fontSizeCell = engine.arg(1);

  gfxSetStrokeWidth(destCell, cellFloat(fontSizeCell));

  engine.push(cellMakeInt(0));
}

// setFont(dest: Image, font: Font)
static NativeFuncDefn(runtime_setFont_5Image4Font) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsHeapPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &fontCell = engine.arg(1);

  gfxSetFont(destCell, fontCell);

  engine.push(cellMakeInt(0));
}

// setFontSize(dest: Image, fontSize: Float)
static NativeFuncDefn(runtime_setFontSize_5ImageF) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsFloat(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &fontSizeCell = engine.arg(1);

  gfxSetFontSize(destCell, cellFloat(fontSizeCell));

  engine.push(cellMakeInt(0));
}

//------------------------------------------------------------------------
// state accessors
//------------------------------------------------------------------------

// matrix(dest: Image) -> Matrix
static NativeFuncDefn(runtime_matrix_5Image) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  engine.push(gfxMatrix(destCell, engine));
}

// clipRect(dest: Image) -> Rect
static NativeFuncDefn(runtime_clipRect_5Image) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  engine.push(gfxClipRect(destCell, engine));
}

// color(dest: Image) -> ARGB
static NativeFuncDefn(runtime_color_5Image) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  engine.push(cellMakeInt(gfxColor(destCell)));
}

// fillRule(dest: Image) -> FillRule
static NativeFuncDefn(runtime_fillRule_5Image) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  engine.push(cellMakeInt(gfxFillRule(destCell)));
}

// strokeWidth(dest: Image) -> Float
static NativeFuncDefn(runtime_strokeWidth_5Image) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  engine.push(cellMakeFloat(gfxStrokeWidth(destCell)));
}

// font(dest: Image) -> Font
static NativeFuncDefn(runtime_font_5Image) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  engine.push(gfxFont(destCell));
}

// fontSize(dest: Image) -> Float
static NativeFuncDefn(runtime_fontSize_5Image) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  engine.push(cellMakeFloat(gfxFontSize(destCell)));
}

//------------------------------------------------------------------------
// path
//------------------------------------------------------------------------

// NB: this may trigger GC.
static void pathAppendPoint(Cell &pathCell, float x, float y, uint8_t flags,
			    BytecodeEngine &engine) {
  Path *path = (Path *)cellHeapPtr(pathCell);
  PathXYData *xyData = (PathXYData *)cellHeapPtr(path->xy);
  PathFlagsData *flagsData = (PathFlagsData *)cellHeapPtr(path->flags);
  int64_t length = cellInt(path->length);
  int64_t size = xyData ? heapObjSize(flagsData) : 0;

  // expand the xy and flags arrays, if necessary
  if (length == size) {
    int64_t newSize;
    if (size == 0) {
      newSize = 8;
    } else {
      if (size > bytecodeMaxInt / 2) {
	BytecodeEngine::fatalError("Integer overflow");
      }
      newSize = 2 * size;
    }

    // NB: this may trigger GC
    Cell newXYCell = cellMakeHeapPtr(engine.heapAllocBlob(newSize * 2 * sizeof(float), 0));
    engine.pushGCRoot(newXYCell);
    // NB: this may trigger GC
    Cell newFlagsCell = cellMakeHeapPtr(engine.heapAllocBlob(newSize, 0));
    engine.pushGCRoot(newFlagsCell);
    PathXYData *newXYData = (PathXYData *)cellHeapPtr(newXYCell);
    PathFlagsData *newFlagsData = (PathFlagsData *)cellHeapPtr(newFlagsCell);

    path = (Path *)cellHeapPtr(pathCell);
    if (length > 0) {
      xyData = (PathXYData *)cellHeapPtr(path->xy);
      memcpy(newXYData->data, xyData->data, length * 2 * sizeof(float));
      flagsData = (PathFlagsData *)cellHeapPtr(path->flags);
      memcpy(newFlagsData->data, flagsData->data, length);
    }

    path->xy = newXYCell;
    path->flags = newFlagsCell;
    engine.popGCRoot(newFlagsCell);
    engine.popGCRoot(newXYCell);

    xyData = newXYData;
    flagsData = newFlagsData;
  }

  xyData->data[length*2    ] = x;
  xyData->data[length*2 + 1] = y;
  flagsData->data[length] = flags;
  path->length = cellMakeInt(length + 1);
}

static void pathOrLastFlags(Cell &pathCell, uint8_t flags) {
  Path *path = (Path *)cellHeapPtr(pathCell);
  PathFlagsData *flagsData = (PathFlagsData *)cellHeapPtr(path->flags);
  int64_t length = cellInt(path->length);
  flagsData->data[length - 1] |= flags;
}

// makePath() -> Path
static NativeFuncDefn(runtime_makePath) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif

  Path *path = (Path *)engine.heapAllocTuple(pathNCells, 0);
  path->state = cellMakeInt(pathStateClosed);
  path->currentX = cellMakeFloat(0);
  path->currentY = cellMakeFloat(0);
  path->xy = cellMakeNilHeapPtr();
  path->flags = cellMakeNilHeapPtr();
  path->length = cellMakeInt(0);

  engine.push(cellMakeHeapPtr(path));
}

// moveTo(path: Path, x: Float, y: Float)
static NativeFuncDefn(runtime_moveTo_4PathFF) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsFloat(engine.arg(1)) ||
      !cellIsFloat(engine.arg(2))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);
  Cell &xCell = engine.arg(1);
  Cell &yCell = engine.arg(2);

  Path *path = (Path *)cellHeapPtr(pathCell);
  engine.failOnNilPtr(path);
  path->currentX = xCell;
  path->currentY = yCell;
  path->state = cellMakeInt(pathStateMoved);

  engine.push(cellMakeInt(0));
}

// lineTo(path: Path, x: Float, y: Float)
static NativeFuncDefn(runtime_lineTo_4PathFF) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsFloat(engine.arg(1)) ||
      !cellIsFloat(engine.arg(2))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);
  Cell &xCell = engine.arg(1);
  Cell &yCell = engine.arg(2);

  Path *path = (Path *)cellHeapPtr(pathCell);
  engine.failOnNilPtr(path);
  int64_t state = cellInt(path->state);
  if (state != pathStateClosed) {
    path->state = cellMakeInt(pathStateOpen);
    // NB: pathAppendPoint may trigger GC
    if (state == pathStateMoved) {
      pathAppendPoint(pathCell, cellFloat(path->currentX), cellFloat(path->currentY),
		      pathFlagMoveTo, engine);
    }
    pathAppendPoint(pathCell, cellFloat(xCell), cellFloat(yCell), pathFlagLineTo, engine);
  }

  engine.push(cellMakeInt(0));
}

// curveTo(path: Path, cx1: Float, cy1: Float,
//         cx2: Float, cy2: Float, x: Float, y: Float)
static NativeFuncDefn(runtime_curveTo_4PathFFFFFF) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 7 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsFloat(engine.arg(1)) ||
      !cellIsFloat(engine.arg(2)) ||
      !cellIsFloat(engine.arg(3)) ||
      !cellIsFloat(engine.arg(4)) ||
      !cellIsFloat(engine.arg(5)) ||
      !cellIsFloat(engine.arg(6))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);
  Cell &cx1Cell = engine.arg(1);
  Cell &cy1Cell = engine.arg(2);
  Cell &cx2Cell = engine.arg(3);
  Cell &cy2Cell = engine.arg(4);
  Cell &xCell = engine.arg(5);
  Cell &yCell = engine.arg(6);

  Path *path = (Path *)cellHeapPtr(pathCell);
  engine.failOnNilPtr(path);
  int64_t state = cellInt(path->state);
  if (state != pathStateClosed) {
    path->state = cellMakeInt(pathStateOpen);
    // NB: pathAppendPoint may trigger GC
    if (state == pathStateMoved) {
      pathAppendPoint(pathCell, cellFloat(path->currentX), cellFloat(path->currentY),
		      pathFlagMoveTo, engine);
    }
    pathAppendPoint(pathCell, cellFloat(cx1Cell), cellFloat(cy1Cell), pathFlagCurveTo, engine);
    pathAppendPoint(pathCell, cellFloat(cx2Cell), cellFloat(cy2Cell), pathFlagCurveTo, engine);
    pathAppendPoint(pathCell, cellFloat(xCell), cellFloat(yCell), pathFlagCurveTo, engine);
  }

  engine.push(cellMakeInt(0));
}

// closePath(path: Path)
static NativeFuncDefn(runtime_closePath_4Path) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);

  Path *path = (Path *)cellHeapPtr(pathCell);
  engine.failOnNilPtr(path);
  int64_t state = cellInt(path->state);
  if (state == pathStateOpen) {
    pathOrLastFlags(pathCell, pathFlagClose);
    path->state = cellMakeInt(pathStateClosed);
  }

  engine.push(cellMakeInt(0));
}

//------------------------------------------------------------------------
// path accessors
//------------------------------------------------------------------------

// ifirst(path: Path) -> Int
static NativeFuncDefn(runtime_ifirst_4PathI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);

  Path *path = (Path *)cellHeapPtr(pathCell);
  engine.failOnNilPtr(path);

  engine.push(cellMakeInt(0));
}

// imore(path: Path, iter: Int) -> Bool
static NativeFuncDefn(runtime_imore_4PathI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);
  Cell &iterCell = engine.arg(1);

  Path *path = (Path *)cellHeapPtr(pathCell);
  engine.failOnNilPtr(path);
  int64_t length = cellInt(path->length);
  int64_t iter = cellInt(iterCell);

  engine.push(cellMakeBool(iter < length));
}

// inext(path: Path, iter: Int) -> Int
static NativeFuncDefn(runtime_inext_4PathI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);
  Cell &iterCell = engine.arg(1);

  Path *path = (Path *)cellHeapPtr(pathCell);
  engine.failOnNilPtr(path);
  int64_t length = cellInt(path->length);
  int64_t iter = cellInt(iterCell);
  if (iter < 0 || iter >= length) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  PathFlagsData *flagsData = (PathFlagsData *)cellHeapPtr(path->flags);

  uint8_t kind = flagsData->data[iter] & pathFlagKindMask;
  int64_t next = iter + 1;
  if (kind == pathFlagCurveTo) {
    next = iter + 3;
    if (next > length) {
      BytecodeEngine::fatalError("Invalid path");
    }
  }
  engine.push(cellMakeInt(next));
}

// iget(path: Path, iter: Int) -> PathElem
static NativeFuncDefn(runtime_iget_4PathI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &pathCell = engine.arg(0);
  Cell &iterCell = engine.arg(1);

  Path *path = (Path *)cellHeapPtr(pathCell);
  engine.failOnNilPtr(path);
  int64_t length = cellInt(path->length);
  int64_t iter = cellInt(iterCell);
  if (iter < 0 || iter >= length) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  PathXYData *xyData = (PathXYData *)cellHeapPtr(path->xy);
  PathFlagsData *flagsData = (PathFlagsData *)cellHeapPtr(path->flags);

  PathElem *elem = (PathElem *)engine.heapAllocTuple(pathElemNCells, 0);
  uint8_t kind = flagsData->data[iter] & pathFlagKindMask;
  if (kind == pathFlagMoveTo) {
    elem->kind = cellMakeInt(pathElemKindMove);
    elem->closed = cellMakeBool(false);
    elem->x = cellMakeFloat(xyData->data[2*iter]);
    elem->y = cellMakeFloat(xyData->data[2*iter+1]);
    elem->cx1 = cellMakeFloat(0);
    elem->cy1 = cellMakeFloat(0);
    elem->cx2 = cellMakeFloat(0);
    elem->cy2 = cellMakeFloat(0);
  } else if (kind == pathFlagLineTo) {
    elem->kind = cellMakeInt(pathElemKindLine);
    elem->closed = cellMakeBool((flagsData->data[iter] & pathFlagClose) ? true : false);
    elem->x = cellMakeFloat(xyData->data[2*iter]);
    elem->y = cellMakeFloat(xyData->data[2*iter+1]);
    elem->cx1 = cellMakeFloat(0);
    elem->cy1 = cellMakeFloat(0);
    elem->cx2 = cellMakeFloat(0);
    elem->cy2 = cellMakeFloat(0);
  } else if (kind == pathFlagCurveTo) {
    if (iter > length - 3) {
      BytecodeEngine::fatalError("Invalid path");
    }
    elem->kind = cellMakeInt(pathElemKindCurve);
    elem->closed = cellMakeBool((flagsData->data[iter+2] & pathFlagClose) ? true : false);
    elem->cx1 = cellMakeFloat(xyData->data[2*iter]);
    elem->cy1 = cellMakeFloat(xyData->data[2*iter+1]);
    elem->cx2 = cellMakeFloat(xyData->data[2*iter+2]);
    elem->cy2 = cellMakeFloat(xyData->data[2*iter+3]);
    elem->x = cellMakeFloat(xyData->data[2*iter+4]);
    elem->y = cellMakeFloat(xyData->data[2*iter+5]);
  } else {
    BytecodeEngine::fatalError("Invalid path");
  }

  engine.push(cellMakeHeapPtr(elem));
}

//------------------------------------------------------------------------
// path drawing
//------------------------------------------------------------------------

// stroke(dest: Image, path: Path)
static NativeFuncDefn(runtime_stroke_5Image4Path) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsHeapPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &pathCell = engine.arg(1);

  gfxStroke(destCell, pathCell);

  engine.push(cellMakeInt(0));
}

// fill(dest: Image, path: Path)
static NativeFuncDefn(runtime_fill_5Image4Path) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsHeapPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &pathCell = engine.arg(1);

  gfxFill(destCell, pathCell);

  engine.push(cellMakeInt(0));
}

// strokeLine(dest: Image, x0: Float, y0: Float, x1: Float, y1: Float)
static NativeFuncDefn(runtime_strokeLine_5ImageFFFF) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 5 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsFloat(engine.arg(1)) ||
      !cellIsFloat(engine.arg(2)) ||
      !cellIsFloat(engine.arg(3)) ||
      !cellIsFloat(engine.arg(4))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &x0Cell = engine.arg(1);
  Cell &y0Cell = engine.arg(2);
  Cell &x1Cell = engine.arg(3);
  Cell &y1Cell = engine.arg(4);

  float x0 = cellFloat(x0Cell);
  float y0 = cellFloat(y0Cell);
  float x1 = cellFloat(x1Cell);
  float y1 = cellFloat(y1Cell);
  gfxStrokeLine(destCell, x0, y0, x1, y1);

  engine.push(cellMakeInt(0));
}

// strokeRect(dest: Image, x: Float, y: Float, w: Float, h: Float)
static NativeFuncDefn(runtime_strokeRect_5ImageFFFF) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 5 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsFloat(engine.arg(1)) ||
      !cellIsFloat(engine.arg(2)) ||
      !cellIsFloat(engine.arg(3)) ||
      !cellIsFloat(engine.arg(4))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &xCell = engine.arg(1);
  Cell &yCell = engine.arg(2);
  Cell &wCell = engine.arg(3);
  Cell &hCell = engine.arg(4);

  float x = cellFloat(xCell);
  float y = cellFloat(yCell);
  float w = cellFloat(wCell);
  float h = cellFloat(hCell);
  gfxStrokeRect(destCell, x, y, w, h);

  engine.push(cellMakeInt(0));
}

// fillRect(dest: Image, x: Float, y: Float, w: Float, h: Float)
static NativeFuncDefn(runtime_fillRect_5ImageFFFF) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 5 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsFloat(engine.arg(1)) ||
      !cellIsFloat(engine.arg(2)) ||
      !cellIsFloat(engine.arg(3)) ||
      !cellIsFloat(engine.arg(4))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &xCell = engine.arg(1);
  Cell &yCell = engine.arg(2);
  Cell &wCell = engine.arg(3);
  Cell &hCell = engine.arg(4);

  float x = cellFloat(xCell);
  float y = cellFloat(yCell);
  float w = cellFloat(wCell);
  float h = cellFloat(hCell);
  gfxFillRect(destCell, x, y, w, h);

  engine.push(cellMakeInt(0));
}

//------------------------------------------------------------------------
// misc drawing
//------------------------------------------------------------------------

// clear(dest: Image)
static NativeFuncDefn(runtime_clear_5Image) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);

  gfxClear(destCell);

  engine.push(cellMakeInt(0));
}

//------------------------------------------------------------------------
// fonts
//------------------------------------------------------------------------

// fontList() -> Vector[String]
static NativeFuncDefn(runtime_fontList) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif

  Cell v = gfxFontList(engine);

  engine.push(v);
}

// loadFont(name: String) -> Result[Font]
static NativeFuncDefn(runtime_loadFont_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &nameCell = engine.arg(0);

  std::string name = stringToStdString(nameCell);

  Cell font = gfxLoadFont(name, engine);

  engine.push(font);
}

// genericFont(family: GenericFontFamily, bold: Bool, italic: Bool) -> Font
static NativeFuncDefn(runtime_genericFont_17GenericFontFamilyBB) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsInt(engine.arg(0)) ||
      !cellIsBool(engine.arg(1)) ||
      !cellIsBool(engine.arg(2))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &familyCell = engine.arg(0);
  Cell &boldCell = engine.arg(1);
  Cell &italicCell = engine.arg(2);

  Cell font = gfxGenericFont(cellInt(familyCell), cellBool(boldCell), cellBool(italicCell), engine);

  engine.push(font);
}

// close(font: Font)
static NativeFuncDefn(runtime_close_4Font) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &fontCell = engine.arg(0);

  gfxCloseFont(fontCell, engine);

  engine.push(cellMakeInt(0));
}

//------------------------------------------------------------------------
// text drawing
//------------------------------------------------------------------------

// drawText(dest: Image, s: String, x: Float, y: Float)
static NativeFuncDefn(runtime_drawText_5ImageSFF) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 4 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1)) ||
      !cellIsFloat(engine.arg(2)) ||
      !cellIsFloat(engine.arg(3))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &sCell = engine.arg(1);
  Cell &xCell = engine.arg(2);
  Cell &yCell = engine.arg(3);

  std::string s = stringToStdString(sCell);
  float x = cellFloat(xCell);
  float y = cellFloat(yCell);

  gfxDrawText(destCell, s, x, y);

  engine.push(cellMakeInt(0));
}

//------------------------------------------------------------------------
// text information
//------------------------------------------------------------------------

// fontMetrics(dest: Image) -> FontMetrics
static NativeFuncDefn(runtime_fontMetrics_5Image) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);

  float ascent, descent, lineSpacing;
  gfxFontMetrics(destCell, ascent, descent, lineSpacing);

  FontMetrics *fontMetrics = (FontMetrics *)engine.heapAllocTuple(fontMetricsNCells, 0);
  fontMetrics->ascent = cellMakeFloat(ascent);
  fontMetrics->descent = cellMakeFloat(descent);
  fontMetrics->lineSpacing = cellMakeFloat(lineSpacing);

  engine.push(cellMakeHeapPtr(fontMetrics));
}

// textBox(dest: Image, s: String) -> Rect
static NativeFuncDefn(runtime_textBox_5ImageS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &sCell = engine.arg(1);

  std::string s = stringToStdString(sCell);

  float x, y, w, h;
  gfxTextBox(destCell, s, x, y, w, h);

  Rect *rect = (Rect *)engine.heapAllocTuple(rectNCells, 0);
  rect->x = cellMakeFloat(x);
  rect->y = cellMakeFloat(y);
  rect->w = cellMakeFloat(w);
  rect->h = cellMakeFloat(h);

  engine.push(cellMakeHeapPtr(rect));
}

//------------------------------------------------------------------------
// images
//------------------------------------------------------------------------

// makeImage(w: Int, h: Int, color: ARGB) -> Image
static NativeFuncDefn(runtime_makeImage_II4ARGB) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsInt(engine.arg(0)) ||
      !cellIsInt(engine.arg(1)) ||
      !cellIsInt(engine.arg(2))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &wCell = engine.arg(0);
  Cell &hCell = engine.arg(1);
  Cell &colorCell = engine.arg(2);

  int64_t w = cellInt(wCell);
  int64_t h = cellInt(hCell);
  int64_t color = cellInt(colorCell);
  if (w < 0 || w > INT_MAX || h < 0 || h > INT_MAX) {
    BytecodeEngine::fatalError("Invalid argument");
  }

  Cell imgCell = gfxMakeImage((int)w, (int)h, color, engine);

  engine.push(imgCell);
}

// close(img: Image)
static NativeFuncDefn(runtime_close_5Image) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &imgCell = engine.arg(0);

  gfxCloseImage(imgCell, engine);

  engine.push(cellMakeInt(0));
}

// width(img: Image) -> Int
static NativeFuncDefn(runtime_width_5Image) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &imgCell = engine.arg(0);

  int64_t w = gfxImageWidth(imgCell);

  engine.push(cellMakeInt(w));
}

// height(img: Image) -> Int
static NativeFuncDefn(runtime_height_5Image) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &imgCell = engine.arg(0);

  int64_t h = gfxImageHeight(imgCell);

  engine.push(cellMakeInt(h));
}

// readImage(fileName: String) -> Result[Image]
static NativeFuncDefn(runtime_readImage_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &fileNameCell = engine.arg(0);

  std::string fileName = stringToStdString(fileNameCell);

  engine.push(gfxReadImage(fileName, engine));
}

// writePNG(img: Image, withAlpha: Bool, fileName: String) -> Result[]
static NativeFuncDefn(runtime_writePNG_5ImageBS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsBool(engine.arg(1)) ||
      !cellIsPtr(engine.arg(2))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &imgCell = engine.arg(0);
  Cell &withAlphaCell = engine.arg(1);
  Cell &fileNameCell = engine.arg(2);

  std::string fileName = stringToStdString(fileNameCell);

  if (gfxWritePNG(imgCell, cellBool(withAlphaCell), fileName)) {
    engine.push(cellMakeInt(0));
  } else {
    engine.push(cellMakeError());
  }
}

// writeJPEG(img: Image, quality: Int, fileName: String) -> Result[]
static NativeFuncDefn(runtime_writeJPEG_5ImageIS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1)) ||
      !cellIsPtr(engine.arg(2))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &imgCell = engine.arg(0);
  Cell &qualityCell = engine.arg(1);
  Cell &fileNameCell = engine.arg(2);

  std::string fileName = stringToStdString(fileNameCell);

  if (gfxWriteJPEG(imgCell, cellInt(qualityCell), fileName)) {
    engine.push(cellMakeInt(0));
  } else {
    engine.push(cellMakeError());
  }
}

//------------------------------------------------------------------------
// image drawing
//------------------------------------------------------------------------

// drawImage(dest: Image, src: Image)
static NativeFuncDefn(runtime_drawImage_5Image5Image) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &destCell = engine.arg(0);
  Cell &srcCell = engine.arg(1);

  gfxDrawImage(destCell, srcCell);

  engine.push(cellMakeInt(0));
}

//------------------------------------------------------------------------
// windows
//------------------------------------------------------------------------

// openWindow(title: String, w: Int, h: Int) -> Result[Window]
static NativeFuncDefn(runtime_openWindow_SII) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1)) ||
      !cellIsInt(engine.arg(2))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &titleCell = engine.arg(0);
  Cell &wCell = engine.arg(1);
  Cell &hCell = engine.arg(2);

  std::string title = stringToStdString(titleCell);
  int64_t w = cellInt(wCell);
  int64_t h = cellInt(hCell);
  if (w <= 0 || w > INT_MAX || h <= 0 || h > INT_MAX) {
    BytecodeEngine::fatalError("Invalid argument");
  }

  Cell winCell = gfxOpenWindow(title, (int)w, (int)h, engine);

  engine.push(winCell);
}

// setBackgroundColor(win: Window, color: ARGB)
static NativeFuncDefn(runtime_setBackgroundColor_6Window4ARGB) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &windowCell = engine.arg(0);
  Cell &colorCell = engine.arg(1);

  gfxSetBackgroundColor(windowCell, cellInt(colorCell));

  engine.push(cellMakeInt(0));
}

// backBuffer(win: Window) -> Image
static NativeFuncDefn(runtime_backBuffer_6Window) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &windowCell = engine.arg(0);

  Cell backBufCell = gfxBackBuffer(windowCell);

  engine.push(backBufCell);
}

// swapBuffers(win: Window)
static NativeFuncDefn(runtime_swapBuffers_6Window) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &windowCell = engine.arg(0);

  gfxSwapBuffers(windowCell);

  engine.push(cellMakeInt(0));
}

// close(win: Window)
static NativeFuncDefn(runtime_close_6Window) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &windowCell = engine.arg(0);

  gfxCloseWindow(windowCell, engine);

  engine.push(cellMakeInt(0));
}

//------------------------------------------------------------------------
// events
//------------------------------------------------------------------------

// monoclock() -> Int
static NativeFuncDefn(runtime_monoclock) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif

  int64_t t = gfxMonoclock();

  engine.push(cellMakeInt(t));
}

// waitEvent() -> Event
static NativeFuncDefn(runtime_waitEvent) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif

  Cell eventCell = gfxWaitEvent(engine);

  engine.push(eventCell);
}

// waitEvent(timeLimit: Int) -> Result[Event]
static NativeFuncDefn(runtime_waitEvent_I) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsInt(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &timeLimitCell = engine.arg(0);

  int64_t timeLimit = cellInt(timeLimitCell);
  if (timeLimit <= 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }

  Cell eventCell = gfxWaitEvent(timeLimit, engine);

  engine.push(eventCell);
}

// pollEvent() -> Result[Event]
static NativeFuncDefn(runtime_pollEvent) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif

  Cell eventCell = gfxPollEvent(engine);

  engine.push(eventCell);
}

//------------------------------------------------------------------------
// clipboard
//------------------------------------------------------------------------

// copyToClipboard(win: Window, s: String)
static NativeFuncDefn(runtime_copyToClipboard_6WindowS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &winCell = engine.arg(0);
  Cell &sCell = engine.arg(1);

  std::string s = stringToStdString(sCell);

  gfxCopyToClipboard(winCell, s);
  engine.push(cellMakeInt(0));
}

// pasteFromClipboard(win: Window) -> Result[String]
static NativeFuncDefn(runtime_pasteFromClipboard_6Window) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &winCell = engine.arg(0);

  std::string s;
  if (gfxPasteFromClipboard(winCell, s)) {
    engine.push(stringMake((uint8_t *)s.c_str(), (int64_t)s.size(), engine));
  } else {
    engine.push(cellMakeError());
  }
}

//------------------------------------------------------------------------
// screen info
//------------------------------------------------------------------------

// screenDPI() -> Int
static NativeFuncDefn(runtime_screenDPI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif

  engine.push(cellMakeInt(gfxScreenDPI(engine)));
}

// defaultFontSize() -> Float
static NativeFuncDefn(runtime_defaultFontSize) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif

  engine.push(cellMakeFloat(gfxDefaultFontSize(engine)));
}

//------------------------------------------------------------------------

void runtime_gfx_init(BytecodeEngine &engine) {
  gfxInit(engine);

  //--- color
  engine.addNativeFunction("argb_IIII", &runtime_argb_IIII);
  engine.addNativeFunction("rgb_III", &runtime_rgb_III);
  engine.addNativeFunction("a_4ARGB", &runtime_a_4ARGB);
  engine.addNativeFunction("r_4ARGB", &runtime_r_4ARGB);
  engine.addNativeFunction("g_4ARGB", &runtime_g_4ARGB);
  engine.addNativeFunction("b_4ARGB", &runtime_b_4ARGB);
  //--- matrix / point
  engine.addNativeFunction("multiply_6Matrix6Matrix", &runtime_multiply_6Matrix6Matrix);
  engine.addNativeFunction("transform_5Point6Matrix", &runtime_transform_5Point6Matrix);
  engine.addNativeFunction("invert_6Matrix", &runtime_invert_6Matrix);
  //--- state save/restore
  engine.addNativeFunction("pushState_5Image", &runtime_pushState_5Image);
  engine.addNativeFunction("popState_5Image", &runtime_popState_5Image);
  //--- state modification
  engine.addNativeFunction("setMatrix_5Image6Matrix", &runtime_setMatrix_5Image6Matrix);
  engine.addNativeFunction("concatMatrix_5Image6Matrix", &runtime_concatMatrix_5Image6Matrix);
  engine.addNativeFunction("setClipRect_5ImageFFFF", &runtime_setClipRect_5ImageFFFF);
  engine.addNativeFunction("intersectClipRect_5ImageFFFF", &runtime_intersectClipRect_5ImageFFFF);
  engine.addNativeFunction("setColor_5Image4ARGB", &runtime_setColor_5Image4ARGB);
  engine.addNativeFunction("setFillRule_5Image8FillRule", &runtime_setFillRule_5Image8FillRule);
  engine.addNativeFunction("setStrokeWidth_5ImageF", &runtime_setStrokeWidth_5ImageF);
  engine.addNativeFunction("setFont_5Image4Font", &runtime_setFont_5Image4Font);
  engine.addNativeFunction("setFontSize_5ImageF", &runtime_setFontSize_5ImageF);
  //--- state accessors
  engine.addNativeFunction("matrix_5Image", &runtime_matrix_5Image);
  engine.addNativeFunction("clipRect_5Image", &runtime_clipRect_5Image);
  engine.addNativeFunction("color_5Image", &runtime_color_5Image);
  engine.addNativeFunction("fillRule_5Image", &runtime_fillRule_5Image);
  engine.addNativeFunction("strokeWidth_5Image", &runtime_strokeWidth_5Image);
  engine.addNativeFunction("font_5Image", &runtime_font_5Image);
  engine.addNativeFunction("fontSize_5Image", &runtime_fontSize_5Image);
  //--- path
  engine.addNativeFunction("makePath", &runtime_makePath);
  engine.addNativeFunction("moveTo_4PathFF", &runtime_moveTo_4PathFF);
  engine.addNativeFunction("lineTo_4PathFF", &runtime_lineTo_4PathFF);
  engine.addNativeFunction("curveTo_4PathFFFFFF", &runtime_curveTo_4PathFFFFFF);
  engine.addNativeFunction("closePath_4Path", &runtime_closePath_4Path);
  //--- path accessors
  engine.addNativeFunction("ifirst_4Path", &runtime_ifirst_4PathI);
  engine.addNativeFunction("imore_4PathI", &runtime_imore_4PathI);
  engine.addNativeFunction("inext_4PathI", &runtime_inext_4PathI);
  engine.addNativeFunction("iget_4PathI", &runtime_iget_4PathI);
  //--- path drawing
  engine.addNativeFunction("stroke_5Image4Path", &runtime_stroke_5Image4Path);
  engine.addNativeFunction("fill_5Image4Path", &runtime_fill_5Image4Path);
  engine.addNativeFunction("strokeLine_5ImageFFFF", &runtime_strokeLine_5ImageFFFF);
  engine.addNativeFunction("strokeRect_5ImageFFFF", &runtime_strokeRect_5ImageFFFF);
  engine.addNativeFunction("fillRect_5ImageFFFF", &runtime_fillRect_5ImageFFFF);
  //--- misc drawing
  engine.addNativeFunction("clear_5Image", &runtime_clear_5Image);
  //--- fonts
  engine.addNativeFunction("fontList", &runtime_fontList);
  engine.addNativeFunction("loadFont_S", &runtime_loadFont_S);
  engine.addNativeFunction("genericFont_17GenericFontFamilyBB", &runtime_genericFont_17GenericFontFamilyBB);
  engine.addNativeFunction("close_4Font", &runtime_close_4Font);
  //--- text drawing
  engine.addNativeFunction("drawText_5ImageSFF", &runtime_drawText_5ImageSFF);
  //--- font/text information
  engine.addNativeFunction("fontMetrics_5Image", &runtime_fontMetrics_5Image);
  engine.addNativeFunction("textBox_5ImageS", &runtime_textBox_5ImageS);
  //--- images
  engine.addNativeFunction("makeImage_II4ARGB", &runtime_makeImage_II4ARGB);
  engine.addNativeFunction("close_5Image", &runtime_close_5Image);
  engine.addNativeFunction("width_5Image", &runtime_width_5Image);
  engine.addNativeFunction("height_5Image", &runtime_height_5Image);
  engine.addNativeFunction("readImage_S", &runtime_readImage_S);
  engine.addNativeFunction("writePNG_5ImageBS", &runtime_writePNG_5ImageBS);
  engine.addNativeFunction("writeJPEG_5ImageIS", &runtime_writeJPEG_5ImageIS);
  //--- image drawing
  engine.addNativeFunction("drawImage_5Image5Image", &runtime_drawImage_5Image5Image);
  //--- windows
  engine.addNativeFunction("openWindow_SII", &runtime_openWindow_SII);
  engine.addNativeFunction("setBackgroundColor_6Window4ARGB", &runtime_setBackgroundColor_6Window4ARGB);
  engine.addNativeFunction("backBuffer_6Window", &runtime_backBuffer_6Window);
  engine.addNativeFunction("swapBuffers_6Window", &runtime_swapBuffers_6Window);
  engine.addNativeFunction("close_6Window", &runtime_close_6Window);
  //--- events
  engine.addNativeFunction("monoclock", &runtime_monoclock);
  engine.addNativeFunction("waitEvent", &runtime_waitEvent);
  engine.addNativeFunction("waitEvent_I", &runtime_waitEvent_I);
  engine.addNativeFunction("pollEvent", &runtime_pollEvent);
  //--- clipboard
  engine.addNativeFunction("copyToClipboard_6WindowS", &runtime_copyToClipboard_6WindowS);
  engine.addNativeFunction("pasteFromClipboard_6Window", &runtime_pasteFromClipboard_6Window);
  //--- screen info
  engine.addNativeFunction("screenDPI", &runtime_screenDPI);
  engine.addNativeFunction("defaultFontSize", &runtime_defaultFontSize);
}
