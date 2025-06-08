//========================================================================
//
// Gfx.h
//
// Interface to the platform-dependent windowing/graphics modules.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef Gfx_h
#define Gfx_h

#include <stdint.h>
#include <string>
#include "BytecodeEngine.h"

//------------------------------------------------------------------------

#define genericFontSerif     0
#define genericFontSansSerif 1
#define genericFontMono      2

#define fillRuleNZWN    0
#define fillRuleEvenOdd 1

//------------------------------------------------------------------------

struct Point {
  uint64_t hdr;
  Cell x;             // Float
  Cell y;             // Float
};

#define pointNCells (sizeof(Point) / sizeof(Cell) - 1)

struct Rect {
  uint64_t hdr;
  Cell x;             // Float
  Cell y;             // Float
  Cell w;             // Float
  Cell h;             // Float
};

#define rectNCells (sizeof(Rect) / sizeof(Cell) - 1)

struct Matrix {
  uint64_t hdr;
  Cell a;             // Float
  Cell b;             // Float
  Cell c;             // Float
  Cell d;             // Float
  Cell tx;            // Float
  Cell ty;            // Float
};

#define matrixNCells (sizeof(Matrix) / sizeof(Cell) - 1)

//------------------------------------------------------------------------

struct Path {
  uint64_t hdr;
  Cell state;         // Int -> pathStateXXX constant
  Cell currentX;      // Float
  Cell currentY;      // Float
  Cell xy;            // heap pointer -> blob containing two floats per point
  Cell flags;         // heap pointer -> blob containing one byte per point
  Cell length;	      // Int -> number of valid points
};

#define pathNCells (sizeof(Path) / sizeof(Cell) - 1)

#define pathStateClosed 0   // new path, or last op was closePath
#define pathStateMoved  1   // last op was moveTo
#define pathStateOpen   2   // last op was lineTo or curveTo

#define pathFlagMoveTo   ((uint8_t)0)   // first point in path
#define pathFlagLineTo   ((uint8_t)1)   // line segment
#define pathFlagCurveTo  ((uint8_t)2)   // curved segment - sequence of 3 points
#define pathFlagKindMask ((uint8_t)7)
#define pathFlagClose    ((uint8_t)8)   // last point in path; combined with LineTo or CurveTo

struct PathXYData {
  uint64_t hdr;
  float data[0];
};

struct PathFlagsData {
  uint64_t hdr;
  uint8_t data[0];
};

//------------------------------------------------------------------------

#define pathElemKindMove  0
#define pathElemKindLine  1
#define pathElemKindCurve 2

struct PathElem {
  uint64_t hdr;
  Cell kind;
  Cell closed;
  Cell x;
  Cell y;
  Cell cx1;
  Cell cy1;
  Cell cx2;
  Cell cy2;
};

#define pathElemNCells (sizeof(PathElem) / sizeof(Cell) - 1)

//------------------------------------------------------------------------

struct FontMetrics {
  uint64_t hdr;
  Cell ascent;
  Cell descent;
  Cell lineSpacing;
};

#define fontMetricsNCells (sizeof(FontMetrics) / sizeof(Cell) - 1)

//------------------------------------------------------------------------

struct ResizeEvent {
  uint64_t hdr;
  Cell typeID;        // Int = eventResize
  Cell win;           // heap pointer -> Window
  Cell w;             // Int
  Cell h;             // Int
};

#define resizeEventNCells (sizeof(ResizeEvent) / sizeof(Cell) - 1)

struct CloseEvent {
  uint64_t hdr;
  Cell typeID;        // Int = eventClose
  Cell win;           // heap pointer -> Window
};

#define closeEventNCells (sizeof(CloseEvent) / sizeof(Cell) - 1)

struct MouseButtonPressEvent {
  uint64_t hdr;
  Cell typeID;        // Int = eventMouseButtonPress
  Cell win;           // heap pointer -> Window
  Cell x;             // Int
  Cell y;             // Int
  Cell button;        // Int
  Cell count;         // Int
  Cell modifiers;     // Int
};

#define mouseButtonPressEventNCells (sizeof(MouseButtonPressEvent) / sizeof(Cell) - 1)

struct MouseButtonReleaseEvent {
  uint64_t hdr;
  Cell typeID;        // Int = eventMouseButtonRelease
  Cell win;           // heap pointer -> Window
  Cell x;             // Int
  Cell y;             // Int
  Cell button;        // Int
  Cell modifiers;     // Int
};

#define mouseButtonReleaseEventNCells (sizeof(MouseButtonReleaseEvent) / sizeof(Cell) - 1)

struct MouseScrollWheelEvent {
  uint64_t hdr;
  Cell typeID;        // Int = eventMouseScrollWheel
  Cell win;           // heap pointer -> Window
  Cell x;             // Int
  Cell y;             // Int
  Cell scroll;        // Int
  Cell modifiers;     // Int
};

#define mouseScrollWheelEventNCells (sizeof(MouseScrollWheelEvent) / sizeof(Cell) - 1)

struct MouseMoveEvent {
  uint64_t hdr;
  Cell typeID;        // Int = eventMouseMove
  Cell win;           // heap pointer -> Window
  Cell x;             // Int
  Cell y;             // Int
  Cell button;        // Int
  Cell modifiers;     // Int
};

#define mouseMoveEventNCells (sizeof(MouseMoveEvent) / sizeof(Cell) - 1)

struct KeyPressEvent {
  uint64_t hdr;
  Cell typeID;        // Int = eventKeyPress
  Cell win;           // heap pointer -> Window
  Cell key;           // Int
  Cell unicode;	      // Int
  Cell modifiers;     // Int
};

#define keyPressEventNCells (sizeof(KeyPressEvent) / sizeof(Cell) - 1)

struct KeyReleaseEvent {
  uint64_t hdr;
  Cell typeID;        // Int = eventKeyRelease
  Cell win;           // heap pointer -> Window
  Cell key;           // Int
  Cell unicode;	      // Int
  Cell modifiers;     // Int
};

#define keyReleaseEventNCells (sizeof(KeyReleaseEvent) / sizeof(Cell) - 1)

// for Event.typeID
#define eventResize             0
#define eventClose              1
#define eventMouseButtonPress   2
#define eventMouseButtonRelease 3
#define eventMouseScrollWheel   4
#define eventMouseMove          5
#define eventKeyPress           6
#define eventKeyRelease         7

// for ***Event.modifiers
#define modShift 0x01
#define modCtrl  0x02
#define modAlt   0x04
#define modSuper 0x08

// for KeyPress/ReleaseEvent.key
#define keyNone               0x0000
#define keyRegularMin         0x0020
#define keyRegularMax         0x00ff
#define keyReturn             0x8001
#define keyLeft               0x8002
#define keyUp                 0x8003
#define keyRight              0x8004
#define keyDown               0x8005
#define keyPgUp               0x8006
#define keyPgDn               0x8007
#define keyHome               0x8008
#define keyEnd                0x8009
#define keyInsert             0x800a
#define keyDelete             0x800b
#define keyBackspace          0x800c
#define keyTab                0x800d
#define keyEsc                0x800e
#define keyF1                 0x800f
#define keyF2                 0x8010
#define keyF3                 0x8011
#define keyF4                 0x8012
#define keyF5                 0x8013
#define keyF6                 0x8014
#define keyF7                 0x8015
#define keyF8                 0x8016
#define keyF9                 0x8017
#define keyF10                0x8018
#define keyF11                0x8019
#define keyF12                0x801a
#define keyF13                0x801b
#define keyF14                0x801c
#define keyF15                0x801d
#define keyF16                0x801e
#define keyF17                0x801f
#define keyF18                0x8020
#define keyF19                0x8021
#define keyF20                0x8022
#define keyF21                0x8023
#define keyF22                0x8024
#define keyF23                0x8025
#define keyF24                0x8026
#define keyF25                0x8027
#define keyF26                0x8028
#define keyF27                0x8029
#define keyF28                0x802a
#define keyF29                0x802b
#define keyF30                0x802c
#define keyF31                0x802d
#define keyF32                0x802e
#define keyF33                0x802f
#define keyF34                0x8030
#define keyF35                0x8031
#define keyShift              0x8032
#define keyControl            0x8033
#define keyAlt                0x8034
#define keySuper              0x8035
#define keyCapsLock           0x8036

//------------------------------------------------------------------------

//--- initialization
extern void gfxInit(BytecodeEngine &engine);

//--- state save/restore
extern void gfxPushState(Cell &destCell);
extern void gfxPopState(Cell &destCell);

//--- state modification
extern void gfxSetMatrix(Cell &destCell, Cell &matrixCell);
extern void gfxConcatMatrix(Cell &destCell, Cell &matrixCell);
extern void gfxSetClipRect(Cell &destCell, float x, float y, float w, float h);
extern void gfxIntersectClipRect(Cell &destCell, float x, float y, float w, float h);
extern void gfxSetColor(Cell &destCell, int64_t color);
extern void gfxSetFillRule(Cell &destCell, int64_t rule);
extern void gfxSetStrokeWidth(Cell &destCell, float width);
extern void gfxSetFont(Cell &destCell, Cell &fontCell);
extern void gfxSetFontSize(Cell &destCell, float fontSize);

//--- state accessors
extern Cell gfxMatrix(Cell &destCell, BytecodeEngine &engine);
extern Cell gfxClipRect(Cell &destCell, BytecodeEngine &engine);
extern int64_t gfxColor(Cell &destCell);
extern int64_t gfxFillRule(Cell &destCell);
extern float gfxStrokeWidth(Cell &destCell);
extern Cell gfxFont(Cell &destCell);
extern float gfxFontSize(Cell &destCell);

//--- path drawing
extern void gfxStroke(Cell &destCell, Cell &pathCell);
extern void gfxFill(Cell &destCell, Cell &pathCell);
extern void gfxStrokeLine(Cell &destCell, float x0, float y0, float x1, float y1);
extern void gfxStrokeRect(Cell &destCell, float x, float y, float w, float h);
extern void gfxFillRect(Cell &destCell, float x, float y, float w, float h);

//--- misc drawing
extern void gfxClear(Cell &destCell);

//--- fonts
extern Cell gfxFontList(BytecodeEngine &engine);
extern Cell gfxLoadFont(const std::string &name, BytecodeEngine &engine);
extern Cell gfxGenericFont(int64_t family, bool bold, bool italic, BytecodeEngine &engine);
extern void gfxCloseFont(Cell &fontCell, BytecodeEngine &engine);

//--- text drawing
extern void gfxDrawText(Cell &destCell, const std::string &s, float x, float y);

//--- font/text information
extern void gfxFontMetrics(Cell &destCell, float &ascent, float &descent, float &lineSpacing);
extern void gfxTextBox(Cell &destCell, const std::string &s,
		       float &x, float &y, float &w, float &h);

//--- images
extern Cell gfxMakeImage(int w, int h, int64_t color, BytecodeEngine &engine);
extern void gfxCloseImage(Cell &imgCell, BytecodeEngine &engine);
extern int64_t gfxImageWidth(Cell &imgCell);
extern int64_t gfxImageHeight(Cell &imgCell);
extern Cell gfxReadImage(const std::string &fileName, BytecodeEngine &engine);
extern bool gfxWritePNG(Cell &imgCell, bool withAlpha, const std::string &fileName);
extern bool gfxWriteJPEG(Cell &imgCell, int64_t quality, const std::string &fileName);

//--- image drawing
extern void gfxDrawImage(Cell &destCell, Cell &srcCell);

//--- windows
extern Cell gfxOpenWindow(const std::string &title, int w, int h, BytecodeEngine &engine);
extern void gfxSetBackgroundColor(Cell &windowCell, int64_t color);
extern Cell gfxBackBuffer(Cell &windowCell);
extern void gfxSwapBuffers(Cell &windowCell);
extern void gfxCloseWindow(Cell &windowCell, BytecodeEngine &engine);

//--- events
extern int64_t gfxMonoclock();
extern Cell gfxWaitEvent(BytecodeEngine &engine);
extern Cell gfxWaitEvent(int64_t timeLimit, BytecodeEngine &engine);
extern Cell gfxPollEvent(BytecodeEngine &engine);

//--- clipboard
extern void gfxCopyToClipboard(Cell &windowCell, const std::string &s);
extern bool gfxPasteFromClipboard(Cell &windowCell, std::string &s);

//--- screen info
extern int gfxScreenDPI(BytecodeEngine &engine);

//--- config info
extern float gfxDefaultFontSize(BytecodeEngine &engine);

#endif // Gfx_h
