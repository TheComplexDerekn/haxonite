//========================================================================
//
// CairoXCBGfx.cpp
//
// Cairo/XCB implementation of the windowing/graphics library.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "Gfx.h"
#include <limits.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/shm.h>
#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>
#include <cairo/cairo-xcb.h>
#include <fontconfig/fontconfig.h>
#include <jpeglib.h>
#include <png.h>
#include <xcb/xcb.h>
#include <xcb/randr.h>
#define explicit xexplicit
#include <xcb/xkb.h>
#undef explicit
#include <xcb/xcb_icccm.h>
#include <xcb/shm.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <X11/keysym.h>
#include <algorithm>
#include <deque>
#include <vector>
#include "NumConversion.h"
#include "runtime_String.h"
#include "runtime_Vector.h"

//------------------------------------------------------------------------

// these are defined in newer verions of xcb.h
#ifndef XCB_TRUE
#  define XCB_TRUE 1
#endif
#ifndef XCB_FALSE
#  define XCB_FALSE 0
#endif

union xkb_generic_event {
  struct {
    uint8_t response_type;
    uint8_t xkbType;
    uint16_t sequence;
    xcb_timestamp_t time;
    uint8_t deviceID;
  } any;
  xcb_xkb_new_keyboard_notify_event_t new_keyboard_notify;
  xcb_xkb_map_notify_event_t map_notify;
  xcb_xkb_state_notify_event_t state_notify;
};

//------------------------------------------------------------------------

#define shmPad 65536

//------------------------------------------------------------------------

// Max time between clicks, in ms, to be counted as double/triple/etc
// click.
#define multiClickMaxInterval 400

// Max distance between clicks (in pixels, along either axis) to be
// counted as double/triple/etc click.
#define multiClickMaxDistance 10

//------------------------------------------------------------------------

// Number of microseconds to wait for a paste (SelectionNotify event).
#define pasteTimeout 10000

//------------------------------------------------------------------------

#define fallbackSerifFont           "DejaVu Serif"
#define fallbackSerifBoldFont       "DejaVu Serif Bold"
#define fallbackSerifItalicFont     "DejaVu Serif Italic"
#define fallbackSerifBoldItalicFont "DejaVu Serif Bold Italic"
#define fallbackSansFont            "DejaVu Sans"
#define fallbackSansBoldFont        "DejaVu Sans Bold"
#define fallbackSansItalicFont      "DejaVu Sans Oblique"
#define fallbackSansBoldItalicFont  "DejaVu Sans Bold Oblique"
#define fallbackMonoFont            "DejaVu Sans Mono"
#define fallbackMonoBoldFont        "DejaVu Sans Mono Bold"
#define fallbackMonoItalicFont      "DejaVu Sans Mono Oblique"
#define fallbackMonoBoldItalicFont  "DejaVu Sans Mono Bold Oblique"

//------------------------------------------------------------------------

struct GfxMatrix {
  float a, b, c, d, tx, ty;
};

//------------------------------------------------------------------------

struct GfxFont {
  ResourceObject resObj;

  cairo_font_face_t *font;
};

//------------------------------------------------------------------------

struct Font {
  uint64_t hdr;
  Cell gfxFont;       // resource pointer -> GfxFont
};

#define fontNCells (sizeof(Font) / sizeof(Cell) - 1)

//------------------------------------------------------------------------

enum class GfxImageKind {
  image,   // plain image surface, no xcb connection
  pixmap   // connected to an xcb pixmap
};

struct GfxImage {
  ResourceObject resObj;

  GfxImageKind kind;

  //--- only for kind = pixmap
  size_t shmSize;
  int shmId;
  uint32_t *shmPixels;
  xcb_shm_seg_t shmSeg;
  xcb_pixmap_t pixmap;

  cairo_surface_t *surface;
  cairo_t *cairo;
  int w, h;
  std::vector<GfxMatrix> savedStates;
  GfxMatrix mat;
};

//------------------------------------------------------------------------

struct Image {
  uint64_t hdr;
  Cell gfxImg;        // resource pointer -> GfxImage
  Cell font;	      // heap pointer -> Font
};

#define imageNCells (sizeof(Image) / sizeof(Cell) - 1)

//------------------------------------------------------------------------

struct GfxWindow {
  ResourceObject resObj;

  xcb_window_t window;
  xcb_gcontext_t gc;
  int w, h;
  uint32_t backgroundColor;
};

//------------------------------------------------------------------------

struct Window {
  uint64_t hdr;
  Cell gfxWin;        // resource pointer -> GfxWindow
  Cell frontBuf;      // heap pointer -> Image
  Cell backBuf;       // heap pointer -> Image
  Cell next;	      // heap pointer -> Window
};

#define windowNCells (sizeof(Window) / sizeof(Cell) - 1)

//------------------------------------------------------------------------

// This type is not visible outside of this module.
struct GfxApplication {
  ResourceObject resObj;

  //--- config
  std::string genericSerifFont;
  std::string genericSerifBoldFont;
  std::string genericSerifItalicFont;
  std::string genericSerifBoldItalicFont;
  std::string genericSansFont;
  std::string genericSansBoldFont;
  std::string genericSansItalicFont;
  std::string genericSansBoldItalicFont;
  std::string genericMonoFont;
  std::string genericMonoBoldFont;
  std::string genericMonoItalicFont;
  std::string genericMonoBoldItalicFont;
  float defaultFontSize;

  //--- xcb connection
  xcb_connection_t *connection;
  int screenNum;
  xcb_screen_t *screen;
  xcb_visualtype_t *visual;
  int screenDPI;

  //--- atoms
  xcb_atom_t protocolsAtom;       // WM_PROTOCOLS
  xcb_atom_t deleteWindowAtom;    // WM_DELETE_WINDOW
  xcb_atom_t clipboardAtom;       // CLIPBOARD
  xcb_atom_t utf8StringAtom;      // UTF8_STRING
  xcb_atom_t targetsAtom;         // TARGETS
  xcb_atom_t haxSelectionAtom;    // HAX_SELECTION

  //--- event queue
  std::deque<xcb_generic_event_t*> xevents;

  //--- xkb
  uint8_t xkbEvent;
  xkb_context *xkbContext;
  int32_t xkbDevice;
  xkb_keymap *xkbKeymap;
  xkb_state *xkbState;
  xkb_compose_table *xkbComposeTable;
  xkb_compose_state *xkbComposeState;
  xkb_mod_index_t xkbModShift;
  xkb_mod_index_t xkbModCtrl;
  xkb_mod_index_t xkbModAlt;
  xkb_mod_index_t xkbModSuper;

  //--- mouse button press count tracking
  xcb_timestamp_t prevButtonPressTime;
  int16_t prevButtonPressX;
  int16_t prevButtonPressY;
  int64_t buttonPressCount;

  //--- clipboard
  std::string clipText;
};

//------------------------------------------------------------------------

struct Application {
  uint64_t hdr;
  Cell gfxApp;        // resource pointer -> GfxApplication
  Cell winList;       // heap pointer -> head of window list
};

#define applicationNCells (sizeof(Application) / sizeof(Cell) - 1)

//------------------------------------------------------------------------

static Cell appCell = cellNilHeapPtrInit;

//------------------------------------------------------------------------

static void resetState(Image *img, GfxImage *gfxImg);
static void setMatrix(GfxImage *gfxImg);
static void convertPath(cairo_t *cairo, Path *path);
static void finalizeFont(ResourceObject *resObj);
static void closeFont(GfxFont *gfxFont);
static void finalizeImage(ResourceObject *resObj);
static void closeImage(GfxImage *gfxImg);
static bool imageIsPNG(char *buf, size_t nBytes);
static bool imageIsJPEG(char *buf, size_t nBytes);
static Cell readPNG(FILE *in, BytecodeEngine &engine);
static Cell readJPEG(FILE *in, BytecodeEngine &engine);
static Cell makePixmapImage(int w, int h, GfxWindow *gfxWin, BytecodeEngine &engine);
static void resizePixmapImage(Image *img, int w, int h, GfxWindow *gfxWin);
static bool checkPixmapSize(int w, int h);
static bool allocPixmapMem(GfxImage *gfxImg, int w, int h);
static void freePixmapMem(GfxImage *gfxImg);
static bool initPixmapXcb(GfxImage *gfxImg, int w, int h, GfxWindow *win);
static void closePixmapXcb(GfxImage *gfxImg);
static void finalizeWindow(ResourceObject *resObj);
static void closeWindow(GfxWindow *gfxWin);
static void redrawWindow(Window *win, int x, int y, int w, int h);
static void resizeWindow(Window *win, int w, int h);
static bool initWindowXcb(GfxWindow *gfxWin, const std::string &title, int w, int h);
static void closeWindowXcb(GfxWindow *gfxWin);
static void initApp(BytecodeEngine &engine);
static void finalizeApp(ResourceObject *resObj);
static void closeApp(GfxApplication *gfxApp);
static Window *findWindow(xcb_window_t xcbWindow);
static bool initAppXcb(GfxApplication *gfxApp);
static void closeAppXcb(GfxApplication *gfxApp);
static xcb_atom_t atom(xcb_connection_t *connection, const char *name, bool create);
static void enqueueEvents(GfxApplication *gfxApp);
static void doPoll(GfxApplication *gfxApp, int64_t timeout);
static Cell processEvent(GfxApplication *gfxApp, BytecodeEngine &engine);
static Cell makeResizeEvent(Cell &winCell, int64_t w, int64_t h, BytecodeEngine &engine);
static Cell makeCloseEvent(Cell &winCell, BytecodeEngine &engine);
static Cell makeMouseButtonPressEvent(Cell &winCell, int64_t x, int64_t y,
				      int64_t button, int64_t count, int64_t modifiers,
				      BytecodeEngine &engine);
static Cell makeMouseButtonReleaseEvent(Cell &winCell, int64_t x, int64_t y,
					int64_t button, int64_t modifiers, BytecodeEngine &engine);
static Cell makeMouseScrollWheelEvent(Cell &winCell, int64_t x, int64_t y,
				      int64_t scroll, int64_t modifiers, BytecodeEngine &engine);
static Cell makeMouseMoveEvent(Cell &winCell, int64_t x, int64_t y,
			       int64_t button, int64_t modifiers, BytecodeEngine &engine);
static Cell makeKeyPressEvent(Cell &winCell, int64_t key, int64_t unicode, int64_t modifiers,
			      BytecodeEngine &engine);
static Cell makeKeyReleaseEvent(Cell &winCell, int64_t key, int64_t unicode, int64_t modifiers,
				BytecodeEngine &engine);
static bool initXkb(GfxApplication *gfxApp);
static bool initXkbKeymap(GfxApplication *gfxApp);
static void closeXkb(GfxApplication *gfxApp);
static bool getKey(GfxApplication *gfxApp, bool press, xkb_keycode_t keyCode,
		   int64_t &key, int64_t &unicode, int64_t &modifiers);
static int64_t mapKeysym(xkb_keysym_t xkeysym);
static void handleSelectionRequest(GfxApplication *gfxApp,
				   xcb_selection_request_event_t *selectionRequest);

//------------------------------------------------------------------------
// initialization
//------------------------------------------------------------------------

void gfxInit(BytecodeEngine &engine) {
  //--- allocate the Application object and make it visible to the GC
  appCell = cellMakeHeapPtr(engine.heapAllocTuple(applicationNCells, 0));
  engine.pushGCRoot(appCell);
  Application *app = (Application *)cellHeapPtr(appCell);
  app->gfxApp = cellMakeNilResourcePtr();
  app->winList = cellMakeNilHeapPtr();
}

//------------------------------------------------------------------------
// cairo state setup
//------------------------------------------------------------------------

// Reset [img], [gfxImg], and [gfxImg]->cairo to the initial state as
// defined by Haxonite. This function is called in three different
// contexts:
// (1) after creating a new Image and GfxImage, which involves
//     creating a new cairo object
// (2) after resizing a pixmap Image, which involves creating a new
//     cairo object, but reusing the Image and GfxImage objects
// (3) when swapping the front and back buffers, which involves
//     resetting an existing cairo object
// The caller is responsible for setting the GfxImage w and h fields
// before calling this function.
static void resetState(Image *img, GfxImage *gfxImg) {
  img->font = cellMakeNilHeapPtr();

  while (!gfxImg->savedStates.empty()) {
    cairo_restore(gfxImg->cairo);
    gfxImg->savedStates.pop_back();
  }
  gfxImg->mat.a  = 1;  gfxImg->mat.b  = 0;
  gfxImg->mat.c  = 0;  gfxImg->mat.d  = 1;
  gfxImg->mat.tx = 0;  gfxImg->mat.ty = 0;

  cairo_identity_matrix(gfxImg->cairo);
  cairo_reset_clip(gfxImg->cairo);
  cairo_set_source_rgba(gfxImg->cairo, 0.0, 0.0, 0.0, 1.0);
  cairo_set_fill_rule(gfxImg->cairo, CAIRO_FILL_RULE_WINDING);
  cairo_set_line_width(gfxImg->cairo, 1.0);
  cairo_set_font_face(gfxImg->cairo, nullptr);
  cairo_set_font_size(gfxImg->cairo, 10.0);
}

//------------------------------------------------------------------------
// state save/restore
//------------------------------------------------------------------------

void gfxPushState(Cell &destCell) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  cairo_save(gfxImg->cairo);
  gfxImg->savedStates.push_back(gfxImg->mat);
}

void gfxPopState(Cell &destCell) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  if (!gfxImg->savedStates.empty()) {
    cairo_restore(gfxImg->cairo);
    gfxImg->mat = gfxImg->savedStates.back();
    gfxImg->savedStates.pop_back();
  }
}

//------------------------------------------------------------------------
// state modification
//------------------------------------------------------------------------

static inline double argbGetA(int64_t argb) {
  return ((argb >> 24) & 0xff) / 255.0;
}

static inline double argbGetR(int64_t argb) {
  return ((argb >> 16) & 0xff) / 255.0;
}

static inline double argbGetG(int64_t argb) {
  return ((argb >> 8) & 0xff) / 255.0;
}

static inline double argbGetB(int64_t argb) {
  return (argb & 0xff) / 255.0;
}

static inline int64_t makeARGB(double a, double r, double g, double b) {
  return ((int64_t)(a * 255) << 24) |
         ((int64_t)(r * 255) << 16) |
         ((int64_t)(g * 255) <<  8) |
         ((int64_t)(b * 255)      );
}

// Our transform:
//           [a  b  0]
// [x y 1] * [c  d  0] = [(x*a + y*c + tx) (x*b + y*d + ty)  1]
//           [tx ty 1]

// Cairo's transform:
//           [xx yx 0]
// [x y 1] * [xy yy 0] = [(x*xx + y*xy + x0) (x*yx + y*yy + y0) 1]
//           [x0 y0 1]

static void setMatrix(GfxImage *gfxImg) {
  cairo_matrix_t m;
  m.xx = gfxImg->mat.a;
  m.yx = gfxImg->mat.b;
  m.xy = gfxImg->mat.c;
  m.yy = gfxImg->mat.d;
  m.x0 = gfxImg->mat.tx;
  m.y0 = gfxImg->mat.ty;
  cairo_set_matrix(gfxImg->cairo, &m);
}

void gfxSetMatrix(Cell &destCell, Cell &matrixCell) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);
  Matrix *mat = (Matrix *)cellHeapPtr(matrixCell);
  BytecodeEngine::failOnNilPtr(mat);

  gfxImg->mat.a = cellFloat(mat->a);
  gfxImg->mat.b = cellFloat(mat->b);
  gfxImg->mat.c = cellFloat(mat->c);
  gfxImg->mat.d = cellFloat(mat->d);
  gfxImg->mat.tx = cellFloat(mat->tx);
  gfxImg->mat.ty = cellFloat(mat->ty);

  setMatrix(gfxImg);
}

void gfxConcatMatrix(Cell &destCell, Cell &matrixCell) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);
  Matrix *mat = (Matrix *)cellHeapPtr(matrixCell);
  BytecodeEngine::failOnNilPtr(mat);

  float a = cellFloat(mat->a);
  float b = cellFloat(mat->b);
  float c = cellFloat(mat->c);
  float d = cellFloat(mat->d);
  float tx = cellFloat(mat->tx);
  float ty = cellFloat(mat->ty);

  // current           * m
  // [mat.a  mat.b  0]   [a  b  0]
  // [mat.c  mat.d  0] * [c  d  0]
  // [mat.tx mat.ty 1]   [tx ty 1]

  GfxMatrix newMat;
  newMat.a = gfxImg->mat.a * a + gfxImg->mat.b * c;
  newMat.b = gfxImg->mat.a * b + gfxImg->mat.b * d;
  newMat.c = gfxImg->mat.c * a + gfxImg->mat.d * c;
  newMat.d = gfxImg->mat.c * b + gfxImg->mat.d * d;
  newMat.tx = gfxImg->mat.tx * a + gfxImg->mat.ty * c + tx;
  newMat.ty = gfxImg->mat.tx * b + gfxImg->mat.ty * d + ty;
  gfxImg->mat = newMat;

  setMatrix(gfxImg);
}

void gfxSetClipRect(Cell &destCell, float x, float y, float w, float h) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  // the cairo clipping functions use the transform matrix, so reset
  // it here, and restore it after
  cairo_identity_matrix(gfxImg->cairo);

  cairo_reset_clip(gfxImg->cairo);
  cairo_rectangle(gfxImg->cairo, x, y, w, h);
  cairo_clip(gfxImg->cairo);

  setMatrix(gfxImg);
}

void gfxIntersectClipRect(Cell &destCell, float x, float y, float w, float h) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  // the cairo clipping functions use the transform matrix, so reset
  // it here, and restore it after
  cairo_identity_matrix(gfxImg->cairo);

  cairo_rectangle(gfxImg->cairo, x, y, w, h);
  cairo_clip(gfxImg->cairo);

  setMatrix(gfxImg);
}

void gfxSetColor(Cell &destCell, int64_t color) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  cairo_set_source_rgba(gfxImg->cairo,
			argbGetR(color),
			argbGetG(color),
			argbGetB(color),
			argbGetA(color));
}

void gfxSetFillRule(Cell &destCell, int64_t rule) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  cairo_set_fill_rule(gfxImg->cairo, (rule == fillRuleEvenOdd) ? CAIRO_FILL_RULE_EVEN_ODD
		                                               : CAIRO_FILL_RULE_WINDING);
}

void gfxSetStrokeWidth(Cell &destCell, float width) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  cairo_set_line_width(gfxImg->cairo, width);
}

void gfxSetFont(Cell &destCell, Cell &fontCell) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);
  Font *font = (Font *)cellHeapPtr(fontCell);
  BytecodeEngine::failOnNilPtr(font);
  GfxFont *gfxFont = (GfxFont *)cellResourcePtr(font->gfxFont);

  img->font = fontCell;
  cairo_set_font_face(gfxImg->cairo, gfxFont->font);
}

void gfxSetFontSize(Cell &destCell, float fontSize) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  cairo_set_font_size(gfxImg->cairo, fontSize);
}

//------------------------------------------------------------------------
// state accessors
//------------------------------------------------------------------------

Cell gfxMatrix(Cell &destCell, BytecodeEngine &engine) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  Matrix *mat = (Matrix *)engine.heapAllocTuple(matrixNCells, 0);
  mat->a = cellMakeFloat(gfxImg->mat.a);
  mat->b = cellMakeFloat(gfxImg->mat.b);
  mat->c = cellMakeFloat(gfxImg->mat.c);
  mat->d = cellMakeFloat(gfxImg->mat.d);
  mat->tx = cellMakeFloat(gfxImg->mat.tx);
  mat->ty = cellMakeFloat(gfxImg->mat.ty);
  return cellMakeHeapPtr(mat);
}

Cell gfxClipRect(Cell &destCell, BytecodeEngine &engine) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  // cairo_clip_extents() returns a rectangle in user space, so reset
  // the transform matrix here, and restore it after
  cairo_identity_matrix(gfxImg->cairo);

  double x1, y1, x2, y2;
  cairo_clip_extents(gfxImg->cairo, &x1, &y1, &x2, &y2);

  Rect *rect = (Rect *)engine.heapAllocTuple(rectNCells, 0);
  rect->x = cellMakeFloat(x1);
  rect->y = cellMakeFloat(y1);
  rect->w = cellMakeFloat(x2 - x1);
  rect->h = cellMakeFloat(y2 - y1);

  setMatrix(gfxImg);

  return cellMakeHeapPtr(rect);
}

int64_t gfxColor(Cell &destCell) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  double r, g, b, a;
  cairo_pattern_get_rgba(cairo_get_source(gfxImg->cairo), &r, &g, &b, &a);
  return makeARGB(a, r, g, b);
}

int64_t gfxFillRule(Cell &destCell) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  return cairo_get_fill_rule(gfxImg->cairo) == CAIRO_FILL_RULE_EVEN_ODD ? fillRuleEvenOdd :
                                                                          fillRuleNZWN;
}

float gfxStrokeWidth(Cell &destCell) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);
  return (float)cairo_get_line_width(gfxImg->cairo);
}

Cell gfxFont(Cell &destCell) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  return img->font;
}

float gfxFontSize(Cell &destCell) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  cairo_matrix_t mat;
  cairo_get_font_matrix(gfxImg->cairo, &mat);
  return (float)(0.5 * (mat.xx + mat.yy));
}

//------------------------------------------------------------------------
// path drawing
//------------------------------------------------------------------------

void gfxStroke(Cell &destCell, Cell &pathCell) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);
  Path *path = (Path *)cellHeapPtr(pathCell);
  BytecodeEngine::failOnNilPtr(path);

  convertPath(gfxImg->cairo, path);
  cairo_stroke(gfxImg->cairo);
  // NB: cairo_stroke() clears the path
}

void gfxFill(Cell &destCell, Cell &pathCell) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);
  Path *path = (Path *)cellHeapPtr(pathCell);
  BytecodeEngine::failOnNilPtr(path);

  convertPath(gfxImg->cairo, path);
  cairo_fill(gfxImg->cairo);
  // NB: cairo_fill() clears the path
}

void gfxStrokeLine(Cell &destCell, float x0, float y0, float x1, float y1) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  cairo_move_to(gfxImg->cairo, x0, y0);
  cairo_line_to(gfxImg->cairo, x1, y1);
  cairo_stroke(gfxImg->cairo);
  // NB: cairo_stroke() clears the path
}

void gfxStrokeRect(Cell &destCell, float x, float y, float w, float h) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  cairo_rectangle(gfxImg->cairo, x, y, w, h);
  cairo_stroke(gfxImg->cairo);
  // NB: cairo_stroke() clears the path
}

void gfxFillRect(Cell &destCell, float x, float y, float w, float h) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  cairo_rectangle(gfxImg->cairo, x, y, w, h);
  cairo_fill(gfxImg->cairo);
  // NB: cairo_fill() clears the path
}

static void convertPath(cairo_t *cairo, Path *path) {
  int64_t length = cellInt(path->length);
  PathXYData *xyData = (PathXYData *)cellHeapPtr(path->xy);
  PathFlagsData *flagsData = (PathFlagsData *)cellHeapPtr(path->flags);
  int64_t i = 0;
  while (i < length) {
    uint8_t flags = flagsData->data[i];
    uint8_t kind = flags & pathFlagKindMask;
    if (kind == pathFlagMoveTo) {
      cairo_move_to(cairo, xyData->data[2*i], xyData->data[2*i+1]);
      ++i;
    } else if (kind == pathFlagLineTo) {
      cairo_line_to(cairo, xyData->data[2*i], xyData->data[2*i+1]);
      ++i;
    } else if (kind == pathFlagCurveTo) {
      cairo_curve_to(cairo,
		     xyData->data[2*i  ], xyData->data[2*i+1],
		     xyData->data[2*i+2], xyData->data[2*i+3],
		     xyData->data[2*i+4], xyData->data[2*i+5]);
      // the 'close' flag, if present, is on the last point only
      flags = flagsData->data[i+2];
      i += 3;
    }
    if (flags & pathFlagClose) {
      cairo_close_path(cairo);
    }
  }
}

//------------------------------------------------------------------------
// misc drawing
//------------------------------------------------------------------------

void gfxClear(Cell &destCell) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  cairo_paint(gfxImg->cairo);
}

//------------------------------------------------------------------------
// fonts
//------------------------------------------------------------------------

Cell gfxFontList(BytecodeEngine &engine) {
  FcPattern *pattern = FcPatternBuild(nullptr,
				      FC_OUTLINE, FcTypeBool, FcTrue,
				      FC_SCALABLE, FcTypeBool, FcTrue,
				      nullptr);
  FcObjectSet *objSet = FcObjectSetBuild(FC_FULLNAME, nullptr);
  FcFontSet *fontSet = FcFontList(nullptr, pattern, objSet);
  FcPatternDestroy(pattern);
  FcObjectSetDestroy(objSet);

  Cell v = vectorMake(engine);
  engine.pushGCRoot(v);
  if (fontSet) {
    for (int i = 0; i < fontSet->nfont; ++i) {
      const char *fullName;
      if (FcPatternGetString(fontSet->fonts[i], FC_FULLNAME, 0, (FcChar8 **)&fullName)
	  == FcResultMatch) {
	// NB: this may trigger GC
	Cell s = stringMake((const uint8_t *)fullName, strlen(fullName), engine);
	vectorAppend(v, s, engine);
      }
    }
  }
  engine.popGCRoot(v);

  FcFontSetDestroy(fontSet);

  return v;
}

Cell gfxLoadFont(const std::string &name, BytecodeEngine &engine) {
  GfxFont *gfxFont;
  try {
    gfxFont = new GfxFont();
  } catch (std::bad_alloc) {
    BytecodeEngine::fatalError("Out of memory");
  }
  gfxFont->resObj.finalizer = &finalizeFont;
  gfxFont->font = nullptr;

  //--- construct a Fontconfig pattern
  FcPattern *pattern = FcPatternBuild(nullptr,
				      FC_OUTLINE, FcTypeBool, FcTrue,
				      FC_SCALABLE, FcTypeBool, FcTrue,
				      FC_FULLNAME, FcTypeString, name.c_str(),
				      nullptr);

  //--- search for a matching font, and retrieve the font file and font index
  FcObjectSet *objSet = FcObjectSetBuild(FC_FILE, FC_INDEX, nullptr);
  FcFontSet *fontSet = FcFontList(nullptr, pattern, objSet);
  FcObjectSetDestroy(objSet);
  if (fontSet->nfont == 0) {
    FcFontSetDestroy(fontSet);
    closeFont(gfxFont);
    return cellMakeError();
  }

  //--- ask Cairo to open the font, using the font file and font index
  gfxFont->font = cairo_ft_font_face_create_for_pattern(fontSet->fonts[0]);
  FcFontSetDestroy(fontSet);
  if (cairo_font_face_status(gfxFont->font) != CAIRO_STATUS_SUCCESS) {
    closeFont(gfxFont);
    return cellMakeError();
  }

  //--- allocate the Font
  Font *font = (Font *)engine.heapAllocTuple(fontNCells, 0);
  font->gfxFont = cellMakeResourcePtr(gfxFont);
  engine.addResourceObject(&gfxFont->resObj);

  return cellMakeHeapPtr(font);
}

Cell gfxGenericFont(int64_t family, bool bold, bool italic, BytecodeEngine &engine) {
  initApp(engine);
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);

  std::string name;
  if (family == genericFontSerif) {
    if (bold) {
      if (italic) {
	name = gfxApp->genericSerifBoldItalicFont;
      } else {
	name = gfxApp->genericSerifBoldFont;
      }
    } else {
      if (italic) {
	name = gfxApp->genericSerifItalicFont;
      } else {
	name = gfxApp->genericSerifFont;
      }
    }
  } else if (family == genericFontSansSerif) {
    if (bold) {
      if (italic) {
	name = gfxApp->genericSansBoldItalicFont;
      } else {
	name = gfxApp->genericSansBoldFont;
      }
    } else {
      if (italic) {
	name = gfxApp->genericSansItalicFont;
      } else {
	name = gfxApp->genericSansFont;
      }
    }
  } else { // family == genericFontMono
    if (bold) {
      if (italic) {
	name = gfxApp->genericMonoBoldItalicFont;
      } else {
	name = gfxApp->genericMonoBoldFont;
      }
    } else {
      if (italic) {
	name = gfxApp->genericMonoItalicFont;
      } else {
	name = gfxApp->genericMonoFont;
      }
    }
  }
  Cell fontRes = gfxLoadFont(name, engine);
  if (cellIsError(fontRes)) {
    BytecodeEngine::fatalError("Couldn't open generic font");
  }
  return fontRes;
}

void gfxCloseFont(Cell &fontCell, BytecodeEngine &engine) {
  Font *font = (Font *)cellHeapPtr(fontCell);
  BytecodeEngine::failOnNilPtr(font);
  GfxFont *gfxFont = (GfxFont *)cellResourcePtr(font->gfxFont);
  engine.removeResourceObject(&gfxFont->resObj);
  font->gfxFont = cellMakeNilResourcePtr();
  closeFont(gfxFont);
}

static void finalizeFont(ResourceObject *resObj) {
  GfxFont *gfxFont = (GfxFont *)resObj;
  closeFont(gfxFont);
}

static void closeFont(GfxFont *gfxFont) {
  if (gfxFont->font) {
    cairo_font_face_destroy(gfxFont->font);
  }
  delete gfxFont;
}

//------------------------------------------------------------------------
// text drawing
//------------------------------------------------------------------------

void gfxDrawText(Cell &destCell, const std::string &s, float x, float y) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);
  
  cairo_move_to(gfxImg->cairo, x, y);
  cairo_show_text(gfxImg->cairo, s.c_str());
}

//------------------------------------------------------------------------
// font/text information
//------------------------------------------------------------------------

void gfxFontMetrics(Cell &destCell, float &ascent, float &descent, float &lineSpacing) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  cairo_font_extents_t ext;
  cairo_scaled_font_extents(cairo_get_scaled_font(gfxImg->cairo), &ext);

  ascent = (float)-ext.ascent;
  descent = (float)ext.descent;
  lineSpacing = (float)ext.height;
}

void gfxTextBox(Cell &destCell, const std::string &s,
		float &x, float &y, float &w, float &h) {
  Image *img = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  cairo_text_extents_t ext;
  cairo_text_extents(gfxImg->cairo, s.c_str(), &ext);

  x = (float)ext.x_bearing;
  y = (float)ext.y_bearing;
  // use max of width and x_advance to account for trailing space chars
  w = (float)std::max(ext.width, ext.x_advance);
  h = (float)ext.height;
}

//------------------------------------------------------------------------
// images
//------------------------------------------------------------------------

Cell gfxMakeImage(int w, int h, int64_t color, BytecodeEngine &engine) {
  GfxImage *gfxImg;
  try {
    gfxImg = new GfxImage();
  } catch (std::bad_alloc) {
    BytecodeEngine::fatalError("Out of memory");
  }
  gfxImg->resObj.finalizer = &finalizeImage;
  gfxImg->kind = GfxImageKind::image;
  gfxImg->shmSize = 0;
  gfxImg->shmId = -1;
  gfxImg->shmPixels = nullptr;
  gfxImg->shmSeg = XCB_NONE;
  gfxImg->pixmap = XCB_NONE;
  gfxImg->surface = nullptr;
  gfxImg->cairo = nullptr;
  gfxImg->w = w;
  gfxImg->h = h;

  //--- create image surface
  gfxImg->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
  if (cairo_surface_status(gfxImg->surface) != CAIRO_STATUS_SUCCESS) {
    closeImage(gfxImg);
    BytecodeEngine::fatalError("Out of memory");
  }

  //--- create cairo object
  gfxImg->cairo = cairo_create(gfxImg->surface);
  if (cairo_status(gfxImg->cairo) != CAIRO_STATUS_SUCCESS) {
    closeImage(gfxImg);
    BytecodeEngine::fatalError("Out of memory");
  }

  //--- fill with background color
  cairo_set_source_rgba(gfxImg->cairo,
			argbGetR(color),
			argbGetG(color),
			argbGetB(color),
			argbGetA(color));
  cairo_paint(gfxImg->cairo);

  //--- allocate the Image
  Image *img = (Image *)engine.heapAllocTuple(imageNCells, 0);
  img->gfxImg = cellMakeResourcePtr(gfxImg);
  img->font = cellMakeNilHeapPtr();
  engine.addResourceObject(&gfxImg->resObj);

  //--- set initial state
  resetState(img, gfxImg);

  return cellMakeHeapPtr(img);
}

void gfxCloseImage(Cell &imgCell, BytecodeEngine &engine) {
  Image *img = (Image *)cellHeapPtr(imgCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);
  engine.removeResourceObject(&gfxImg->resObj);
  img->gfxImg = cellMakeNilResourcePtr();
  closeImage(gfxImg);
}

static void finalizeImage(ResourceObject *resObj) {
  GfxImage *gfxImg = (GfxImage *)resObj;
  closeImage(gfxImg);
}

static void closeImage(GfxImage *gfxImg) {
  if (gfxImg->kind == GfxImageKind::pixmap) {
    closePixmapXcb(gfxImg);
    freePixmapMem(gfxImg);
  }
  if (gfxImg->cairo) {
    cairo_destroy(gfxImg->cairo);
  }
  if (gfxImg->surface) {
    cairo_surface_destroy(gfxImg->surface);
  }
  delete gfxImg;
}

int64_t gfxImageWidth(Cell &imgCell) {
  Image *img = (Image *)cellHeapPtr(imgCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);
  return gfxImg->w;
}

int64_t gfxImageHeight(Cell &imgCell) {
  Image *img = (Image *)cellHeapPtr(imgCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);
  return gfxImg->h;
}

Cell gfxReadImage(const std::string &fileName, BytecodeEngine &engine) {
  // read the first few bytes of the file
  FILE *in = fopen(fileName.c_str(), "rb");
  if (!in) {
    return cellMakeError();
  }
  char buf[64];
  size_t nBytes = fread(buf, 1, sizeof(buf), in);
  rewind(in);

  // identify and read the image
  Cell result;
  if (imageIsPNG(buf, nBytes)) {
    result = readPNG(in, engine);
  } else if (imageIsJPEG(buf, nBytes)) {
    result = readJPEG(in, engine);
  } else {
    result = cellMakeError();
  }

  fclose(in);

  return result;
}

static bool imageIsPNG(char *buf, size_t nBytes) {
  static uint8_t pngHdr[8] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
  return nBytes >= 8 && !memcmp(buf, pngHdr, 8);
}

static bool imageIsJPEG(char *buf, size_t nBytes) {
  return nBytes >= 2 && buf[0] == (char)0xff && buf[1] == (char)0xd8;
}

static Cell readPNG(FILE *in, BytecodeEngine &engine) {
  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
					   nullptr, nullptr, nullptr);
  if (!png) {
    return cellMakeError();
  }
  png_infop pngInfo = png_create_info_struct(png);
  if (!pngInfo) {
    png_destroy_read_struct(&png, nullptr, nullptr);
    return cellMakeError();
  }
  if (setjmp(png_jmpbuf(png))) {
    png_destroy_write_struct(&png, &pngInfo);
    return cellMakeError();
  }

  png_init_io(png, in);
  png_read_info(png, pngInfo);

  png_uint_32 w, h;
  int bits, color;
  png_get_IHDR(png, pngInfo, &w, &h, &bits, &color, nullptr, nullptr, nullptr);
  if (color == PNG_COLOR_TYPE_PALETTE) {
    png_set_palette_to_rgb(png);
  }
  if (png_get_valid(png, pngInfo, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(png);
  }
  if (bits == 16) {
    png_set_scale_16(png);
  }
  if (color == PNG_COLOR_TYPE_GRAY ||
      color == PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(png);
  }
#if __BYTE_ORDER == __LITTLE_ENDIAN
  if (color == PNG_COLOR_TYPE_RGB ||
      color == PNG_COLOR_TYPE_RGB_ALPHA) {
    png_set_bgr(png);
  }
#endif
  if (color == PNG_COLOR_TYPE_GRAY ||
      color == PNG_COLOR_TYPE_RGB) {
    png_set_add_alpha(png, 0xffff, PNG_FILLER_AFTER);
  }
#if __BYTE_ORDER == __BIG_ENDIAN
  png_set_swap_alpha(png);
#endif
  png_read_update_info(png, pngInfo);

  Cell imgCell = gfxMakeImage(w, h, 0x00000000, engine);
  Image *img = (Image *)cellHeapPtr(imgCell);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  unsigned char *p = cairo_image_surface_get_data(gfxImg->surface);
  int stride = cairo_image_surface_get_stride(gfxImg->surface);
  png_bytep *rowPointers = new png_bytep[h];
  for (png_uint_32 y = 0; y < h; ++y) {
    rowPointers[y] = p;
    p += stride;
  }

  png_read_image(png, rowPointers);

  delete[] rowPointers;
  png_destroy_read_struct(&png, &pngInfo, nullptr);

  cairo_surface_mark_dirty(gfxImg->surface);

  return imgCell;
}

struct JPEGCustomErrorMgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmpBuf;
};

static void jpegErrorExit(j_common_ptr cinfo) {
  JPEGCustomErrorMgr *errMgr = (JPEGCustomErrorMgr *)cinfo->err;
  longjmp(errMgr->setjmpBuf, 1);
}

static void jpegErrorMessage(j_common_ptr cinfo) {
  // do nothing
}

static Cell readJPEG(FILE *in, BytecodeEngine &engine) {
  JPEGCustomErrorMgr jerr;
  jpeg_decompress_struct decomp;
  decomp.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = &jpegErrorExit;
  jerr.pub.output_message = &jpegErrorMessage;
  if (setjmp(jerr.setjmpBuf)) {
    jpeg_destroy_decompress(&decomp);
    return cellMakeError();
  }

  jpeg_create_decompress(&decomp);

  jpeg_stdio_src(&decomp, in);

  jpeg_read_header(&decomp, TRUE);
#if __BYTE_ORDER == __LITTLE_ENDIAN
  decomp.out_color_space = JCS_EXT_BGRA;
#else
  decomp.out_color_space = JCS_EXT_ARGB;
#endif
  decomp.out_color_components = 3;
  jpeg_start_decompress(&decomp);

  Cell imgCell = gfxMakeImage(decomp.image_width, decomp.image_height, 0x00000000, engine);
  Image *img = (Image *)cellHeapPtr(imgCell);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  unsigned char *p = cairo_image_surface_get_data(gfxImg->surface);
  int stride = cairo_image_surface_get_stride(gfxImg->surface);
  JSAMPROW *rows = new JSAMPROW[decomp.image_height];
  for (uint y = 0; y < decomp.image_height; ++y) {
    rows[y] = (JSAMPROW)p;
    p += stride;
  }
  while (decomp.output_scanline < decomp.image_height) {
    jpeg_read_scanlines(&decomp,
			&rows[decomp.output_scanline],
			decomp.image_height - decomp.output_scanline);
  }
  delete[] rows;

  jpeg_finish_decompress(&decomp);
  jpeg_destroy_decompress(&decomp);

  cairo_surface_mark_dirty(gfxImg->surface);

  return imgCell;
}

bool gfxWritePNG(Cell &imgCell, bool withAlpha, const std::string &fileName) {
  Image *img = (Image *)cellHeapPtr(imgCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  FILE *out = fopen(fileName.c_str(), "wb");
  if (!out) {
    return false;
  }

  cairo_surface_flush(gfxImg->surface);
  unsigned char *data = cairo_image_surface_get_data(gfxImg->surface);
  int w = cairo_image_surface_get_width(gfxImg->surface);
  int h = cairo_image_surface_get_height(gfxImg->surface);
  int stride = cairo_image_surface_get_stride(gfxImg->surface);

  png_structp png;
  png_infop pngInfo;
  if (!(png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
				      nullptr, nullptr, nullptr))) {
    fclose(out);
    return false;
  }
  if (!(pngInfo = png_create_info_struct(png))) {
    png_destroy_write_struct(&png, nullptr);
    fclose(out);
    return false;
  }
  if (setjmp(png_jmpbuf(png))) {
    png_destroy_write_struct(&png, &pngInfo);
    fclose(out);
    return false;
  }

  png_init_io(png, out);
  png_set_IHDR(png, pngInfo, w, h, 8,
	       withAlpha ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB,
	       PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  if (withAlpha) {
    png_color_16 background;
    background.index = 0;
    background.red = 0;
    background.green = 0;
    background.blue = 0;
    background.gray = 0;
    png_set_bKGD(png, pngInfo, &background);
  }
  png_write_info(png, pngInfo);
  if (withAlpha) {
    uint8_t *buf = new uint8_t[w * 4];
    unsigned char *row = data;
    for (int y = 0; y < h; ++y) {
      uint8_t *p = buf;
      uint32_t *pixel = (uint32_t *)row;
      for (int x = 0; x < w; ++x) {
	// divide out the premultiplied alpha
	uint8_t a = (uint8_t)(*pixel >> 24);
	uint8_t r, g, b;
	if (a == 0) {
	  r = g = b = 0;
	} else {
	  r = (((*pixel >> 16) & 0xff) * 255) / a;
	  g = (((*pixel >>  8) & 0xff) * 255) / a;
	  b = (( *pixel        & 0xff) * 255) / a;
	}
	p[0] = r;
	p[1] = g;
	p[2] = b;
	p[3] = a;
	p += 4;
	++pixel;
      }
      png_write_row(png, buf);
      row += stride;
    }
    delete[] buf;
  } else {
    png_set_filler(png, 0, PNG_FILLER_AFTER);
    png_set_bgr(png);
    unsigned char *row = data;
    for (int y = 0; y < h; ++y) {
      png_write_row(png, row);
      row += stride;
    }
  }
  png_write_end(png, pngInfo);
  png_destroy_write_struct(&png, &pngInfo);

  fclose(out);

  return true;
}

bool gfxWriteJPEG(Cell &imgCell, int64_t quality, const std::string &fileName) {
  Image *img = (Image *)cellHeapPtr(imgCell);
  BytecodeEngine::failOnNilPtr(img);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  if (quality < 0 || quality > 100) {
    return false;
  }

  FILE *out = fopen(fileName.c_str(), "wb");
  if (!out) {
    return false;
  }

  cairo_surface_flush(gfxImg->surface);
  unsigned char *data = cairo_image_surface_get_data(gfxImg->surface);
  int w = cairo_image_surface_get_width(gfxImg->surface);
  int h = cairo_image_surface_get_height(gfxImg->surface);
  int stride = cairo_image_surface_get_stride(gfxImg->surface);

  JPEGCustomErrorMgr jerr;
  struct jpeg_compress_struct comp;
  comp.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = &jpegErrorExit;
  jerr.pub.output_message = &jpegErrorMessage;
  if (setjmp(jerr.setjmpBuf)) {
    jpeg_destroy_compress(&comp);
    fclose(out);
    return false;
  }

  jpeg_create_compress(&comp);
  jpeg_stdio_dest(&comp, out);
  comp.image_width = (JDIMENSION)w;
  comp.image_height = (JDIMENSION)h;
#if __BYTE_ORDER == __LITTLE_ENDIAN
  comp.in_color_space = JCS_EXT_BGRA;
#else
  comp.in_color_space = JCS_EXT_ARGB;
#endif
  comp.input_components = 4;
  jpeg_set_defaults(&comp);
  jpeg_set_quality(&comp, (int)quality, TRUE);
  jpeg_start_compress(&comp, TRUE);

  JSAMPROW *rows = new JSAMPROW[comp.image_height];
  unsigned char *row = data;
  for (uint y = 0; y < comp.image_height; ++y) {
    rows[y] = (JSAMPROW)row;
    row += stride;
  }
  while (comp.next_scanline < comp.image_height) {
    jpeg_write_scanlines(&comp, &rows[comp.next_scanline],
			 comp.image_height - comp.next_scanline);
  }

  jpeg_finish_compress(&comp);
  jpeg_destroy_compress(&comp);

  fclose(out);

  return true;
}

//------------------------------------------------------------------------
// pixmap images
//------------------------------------------------------------------------

static Cell makePixmapImage(int w, int h, GfxWindow *gfxWin, BytecodeEngine &engine) {
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);

  GfxImage *gfxImg;
  try {
    gfxImg = new GfxImage();
  } catch (std::bad_alloc) {
    BytecodeEngine::fatalError("Out of memory");
  }
  gfxImg->resObj.finalizer = &finalizeImage;
  gfxImg->kind = GfxImageKind::pixmap;
  gfxImg->shmSize = 0;
  gfxImg->shmId = -1;
  gfxImg->shmPixels = nullptr;
  gfxImg->shmSeg = XCB_NONE;
  gfxImg->pixmap = XCB_NONE;
  gfxImg->surface = nullptr;
  gfxImg->cairo = nullptr;
  gfxImg->w = w;
  gfxImg->h = h;

  //--- create xcb pixmap
  if (!checkPixmapSize(w, h) ||
      !allocPixmapMem(gfxImg, w, h) ||
      !initPixmapXcb(gfxImg, w, h, gfxWin)) {
    closeImage(gfxImg);
    BytecodeEngine::fatalError("Out of memory");
  }

  //--- create image surface
  gfxImg->surface = cairo_image_surface_create_for_data((unsigned char *)gfxImg->shmPixels,
							CAIRO_FORMAT_RGB24,
							w, h, w * 4);
  // In theory, we could use a Cairo xcb surface here, but that causes
  // weird problems (text drawing operations don't work, ...). I
  // suspect that xcb surfaces are meant to work with server-side
  // pixmaps/windows, not shared memory pixmaps (?)
  //gfxImg->surface = cairo_xcb_surface_create(gfxApp->connection, gfxImg->pixmap,
  //					     gfxApp->visual, w, h);
  if (cairo_surface_status(gfxImg->surface) != CAIRO_STATUS_SUCCESS) {
    closeImage(gfxImg);
    BytecodeEngine::fatalError("Out of memory");
  }

  //--- create cairo object
  gfxImg->cairo = cairo_create(gfxImg->surface);
  if (cairo_status(gfxImg->cairo) != CAIRO_STATUS_SUCCESS) {
    closeImage(gfxImg);
    BytecodeEngine::fatalError("Out of memory");
  }

  //--- allocate the Image
  Image *img = (Image *)engine.heapAllocTuple(imageNCells, 0);
  img->gfxImg = cellMakeResourcePtr(gfxImg);
  img->font = cellMakeNilHeapPtr();
  engine.addResourceObject(&gfxImg->resObj);

  //--- set initial state
  resetState(img, gfxImg);

  return cellMakeHeapPtr(img);
}

static void resizePixmapImage(Image *img, int w, int h, GfxWindow *gfxWin) {
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);
  GfxImage *gfxImg = (GfxImage *)cellResourcePtr(img->gfxImg);

  //--- destroy the old cairo object, cairo surface, and pixmap
  cairo_destroy(gfxImg->cairo);
  cairo_surface_destroy(gfxImg->surface);
  closePixmapXcb(gfxImg);

  //--- reallocate memory and create xcb pixmap
  if (!checkPixmapSize(w, h)) {
    BytecodeEngine::fatalError("Out of memory");
  }
  size_t newSize = (size_t)h * (size_t)w * 4;
  if (gfxImg->shmSize < newSize || gfxImg->shmSize > newSize + 2 * shmPad) {
    freePixmapMem(gfxImg);
    if (!allocPixmapMem(gfxImg, w, h)) {
      BytecodeEngine::fatalError("Out of memory");
    }
  }
  if (!initPixmapXcb(gfxImg, w, h, gfxWin)) {
    BytecodeEngine::fatalError("Out of memory");
  }

  //--- create image surface
  gfxImg->surface = cairo_image_surface_create_for_data((unsigned char *)gfxImg->shmPixels,
							CAIRO_FORMAT_RGB24,
							w, h, w * 4);
  // See comment in makePixmapImage()
  //gfxImg->surface = cairo_xcb_surface_create(gfxApp->connection, gfxImg->pixmap,
  //					     gfxApp->visual, w, h);
  if (cairo_surface_status(gfxImg->surface) != CAIRO_STATUS_SUCCESS) {
    BytecodeEngine::fatalError("Out of memory");
  }

  //--- create cairo object
  gfxImg->cairo = cairo_create(gfxImg->surface);
  if (cairo_status(gfxImg->cairo) != CAIRO_STATUS_SUCCESS) {
    closeImage(gfxImg);
    BytecodeEngine::fatalError("Out of memory");
  }

  //--- set initial state
  gfxImg->w = w;
  gfxImg->h = h;
  resetState(img, gfxImg);
}

static bool checkPixmapSize(int w, int h) {
  return w > 0 && w <= UINT16_MAX && h > 0 && h <= UINT16_MAX &&
         (size_t)h <= (SIZE_MAX - shmPad) / w / 4;
}

static bool allocPixmapMem(GfxImage *gfxImg, int w, int h) {
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);

  //--- allocate the shared mem segment
  gfxImg->shmSize = (size_t)h * (size_t)w * 4 + shmPad;
  gfxImg->shmId = shmget(IPC_PRIVATE, gfxImg->shmSize, IPC_CREAT | 0600);
  if (gfxImg->shmId == -1) {
    return false;
  }

  //--- attach shared memory on the client side
  gfxImg->shmPixels = (uint32_t *)shmat(gfxImg->shmId, nullptr, 0);
  if (gfxImg->shmPixels == (uint32_t *)-1) {
    gfxImg->shmPixels = nullptr;
    return false;
  }

  //--- attach shared memory on the X server side
  gfxImg->shmSeg = xcb_generate_id(gfxApp->connection);
  xcb_void_cookie_t shmCookie = xcb_shm_attach_checked(gfxApp->connection,
						       gfxImg->shmSeg, gfxImg->shmId, 0);
  if (xcb_request_check(gfxApp->connection, shmCookie)) {
    return false;
  }

  return true;
}

static void freePixmapMem(GfxImage *gfxImg) {
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);

  if (gfxImg->shmSeg != XCB_NONE) {
    xcb_shm_detach(gfxApp->connection, gfxImg->shmSeg);
    gfxImg->shmSeg = XCB_NONE;
  }
  if (gfxImg->shmPixels) {
    shmdt(gfxImg->shmPixels);
    gfxImg->shmPixels = nullptr;
  }
  if (gfxImg->shmId >= 0) {
    shmctl(gfxImg->shmId, IPC_RMID, nullptr);
    gfxImg->shmId = -1;
  }
}

static bool initPixmapXcb(GfxImage *gfxImg, int w, int h, GfxWindow *win) {
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);

  gfxImg->pixmap = xcb_generate_id(gfxApp->connection);
  xcb_void_cookie_t pixmapCookie = xcb_shm_create_pixmap_checked(gfxApp->connection,
								 gfxImg->pixmap,
								 win->window,
								 (uint16_t)w,
								 (uint16_t)h,
								 gfxApp->screen->root_depth,
								 gfxImg->shmSeg, 0);
  if (xcb_request_check(gfxApp->connection, pixmapCookie)) {
    return false;
  }
  return true;
}

static void closePixmapXcb(GfxImage *gfxImg) {
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);

  if (gfxImg->pixmap != XCB_NONE) {
    xcb_free_pixmap(gfxApp->connection, gfxImg->pixmap);
    gfxImg->pixmap = XCB_NONE;
  }
}

//------------------------------------------------------------------------
// image drawing
//------------------------------------------------------------------------

void gfxDrawImage(Cell &destCell, Cell &srcCell) {
  Image *destImg = (Image *)cellHeapPtr(destCell);
  BytecodeEngine::failOnNilPtr(destImg);
  GfxImage *destGfxImg = (GfxImage *)cellResourcePtr(destImg->gfxImg);
  Image *srcImg = (Image *)cellHeapPtr(srcCell);
  BytecodeEngine::failOnNilPtr(srcImg);
  GfxImage *srcGfxImg = (GfxImage *)cellResourcePtr(srcImg->gfxImg);

  cairo_save(destGfxImg->cairo);

  cairo_set_source_surface(destGfxImg->cairo, srcGfxImg->surface, 0, 0);

  // Cairo uses an inverted matrix here, i.e., the pattern matrix maps
  // user space to pattern space.
  cairo_matrix_t m;
  m.xx = (double)srcGfxImg->w;
  m.yx = 0;
  m.xy = 0;
  m.yy = (double)srcGfxImg->h;
  m.x0 = 0;
  m.y0 = 0;
  cairo_pattern_set_matrix(cairo_get_source(destGfxImg->cairo), &m);

  cairo_rectangle(destGfxImg->cairo, 0, 0, 1, 1);
  cairo_fill(destGfxImg->cairo);

  cairo_restore(destGfxImg->cairo);
}

//------------------------------------------------------------------------
// windows
//------------------------------------------------------------------------

Cell gfxOpenWindow(const std::string &title, int w, int h, BytecodeEngine &engine) {
  initApp(engine);

  GfxWindow *gfxWin;
  try {
    gfxWin = new GfxWindow();
  } catch (std::bad_alloc) {
    BytecodeEngine::fatalError("Out of memory");
  }
  gfxWin->resObj.finalizer = &finalizeWindow;
  gfxWin->window = XCB_NONE;
  gfxWin->gc = XCB_NONE;
  gfxWin->w = w;
  gfxWin->h = h;
  gfxWin->backgroundColor = 0x00000000;

  if (!initWindowXcb(gfxWin, title, w, h)) {
    delete gfxWin;
    return cellMakeError();
  }

  Window *win = (Window *)engine.heapAllocTuple(windowNCells, 0);
  win->gfxWin = cellMakeResourcePtr(gfxWin);
  win->frontBuf = makePixmapImage(w, h, gfxWin, engine);
  win->backBuf = makePixmapImage(w, h, gfxWin, engine);
  engine.addResourceObject(&gfxWin->resObj);

  Application *app = (Application *)cellHeapPtr(appCell);
  win->next = app->winList;
  app->winList = cellMakeHeapPtr(win);

  return cellMakeHeapPtr(win);
}

void gfxSetBackgroundColor(Cell &windowCell, int64_t color) {
  Window *win = (Window *)cellHeapPtr(windowCell);
  BytecodeEngine::failOnNilPtr(win);
  GfxWindow *gfxWin = (GfxWindow *)cellResourcePtr(win->gfxWin);
  gfxWin->backgroundColor = (uint32_t)color;
}

Cell gfxBackBuffer(Cell &windowCell) {
  Window *win = (Window *)cellHeapPtr(windowCell);
  BytecodeEngine::failOnNilPtr(win);
  return win->backBuf;
}

void gfxSwapBuffers(Cell &windowCell) {
  Window *win = (Window *)cellHeapPtr(windowCell);
  BytecodeEngine::failOnNilPtr(win);
  GfxWindow *gfxWin = (GfxWindow *)cellResourcePtr(win->gfxWin);

  std::swap(win->frontBuf, win->backBuf);

  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);
  Image *frontBuf = (Image *)cellHeapPtr(win->frontBuf);
  GfxImage *gfxFrontBuf = (GfxImage *)cellResourcePtr(frontBuf->gfxImg);

  cairo_surface_flush(gfxFrontBuf->surface);
  xcb_clear_area(gfxApp->connection, XCB_TRUE, gfxWin->window, 0, 0, gfxWin->w, gfxWin->h);
  xcb_flush(gfxApp->connection);

  Image *backBuf = (Image *)cellHeapPtr(win->backBuf);
  GfxImage *gfxBackBuf = (GfxImage *)cellResourcePtr(backBuf->gfxImg);
  resetState(backBuf, gfxBackBuf);
}

void gfxCloseWindow(Cell &windowCell, BytecodeEngine &engine) {
  Window *win = (Window *)cellHeapPtr(windowCell);
  BytecodeEngine::failOnNilPtr(win);
  GfxWindow *gfxWin = (GfxWindow *)cellResourcePtr(win->gfxWin);

  Application *app = (Application *)cellHeapPtr(appCell);
  Window *prev = nullptr;
  Window *p = (Window *)cellHeapPtr(app->winList);
  while (p) {
    if (p == win) {
      if (prev) {
	prev->next = p->next;
      } else {
	app->winList = p->next;
      }
      break;
    }
  }

  gfxCloseImage(win->frontBuf, engine);
  gfxCloseImage(win->backBuf, engine);

  engine.removeResourceObject(&gfxWin->resObj);
  win->gfxWin = cellMakeNilResourcePtr();
  closeWindow(gfxWin);
}

static void finalizeWindow(ResourceObject *resObj) {
  GfxWindow *gfxWin = (GfxWindow *)resObj;
  closeWindow(gfxWin);
}

static void closeWindow(GfxWindow *gfxWin) {
  closeWindowXcb(gfxWin);
  delete gfxWin;
}

static void redrawWindow(Window *win, int x, int y, int w, int h) {
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);
  GfxWindow *gfxWin = (GfxWindow *)cellResourcePtr(win->gfxWin);
  Image *frontBuf = (Image *)cellHeapPtr(win->frontBuf);
  GfxImage *gfxFrontBuf = (GfxImage *)cellResourcePtr(frontBuf->gfxImg);
  uint16_t x0 = (uint16_t)std::max(0, std::min(gfxWin->w, std::min(gfxFrontBuf->w, x)));
  uint16_t y0 = (uint16_t)std::max(0, std::min(gfxWin->h, std::min(gfxFrontBuf->h, y)));
  uint16_t x1 = (uint16_t)std::max(0, std::min(gfxWin->w, std::min(gfxFrontBuf->w, x + w)));
  uint16_t y1 = (uint16_t)std::max(0, std::min(gfxWin->h, std::min(gfxFrontBuf->h, y + h)));
  if (x1 > x0 && y1 > y0) {
    xcb_copy_area(gfxApp->connection, gfxFrontBuf->pixmap, gfxWin->window, gfxWin->gc,
		  x0, y0, x0, y0, (uint16_t)(x1 - x0), (uint16_t)(y1 - y0));
    xcb_flush(gfxApp->connection);
  }
}

static void resizeWindow(Window *win, int w, int h) {
  GfxWindow *gfxWin = (GfxWindow *)cellResourcePtr(win->gfxWin);
  Image *frontBuf = (Image *)cellHeapPtr(win->frontBuf);
  Image *backBuf = (Image *)cellHeapPtr(win->backBuf);
  gfxWin->w = w;
  gfxWin->h = h;
  resizePixmapImage(frontBuf, w, h, gfxWin);
  resizePixmapImage(backBuf, w, h, gfxWin);
  uint32_t bg = gfxWin->backgroundColor;
  GfxImage *gfxFrontBuf = (GfxImage *)cellResourcePtr(frontBuf->gfxImg);
  for (size_t i = 0; i < (size_t)gfxFrontBuf->w * (size_t)gfxFrontBuf->h; ++i) {
    gfxFrontBuf->shmPixels[i] = bg;
  }
  GfxImage *gfxBackBuf = (GfxImage *)cellResourcePtr(backBuf->gfxImg);
  for (size_t i = 0; i < (size_t)gfxBackBuf->w * (size_t)gfxBackBuf->h; ++i) {
    gfxBackBuf->shmPixels[i] = bg;
  }
}

static bool initWindowXcb(GfxWindow *gfxWin, const std::string &title, int w, int h) {
  if (w <= 0 || w > UINT16_MAX || h <= 0 || h > UINT16_MAX) {
    return false;
  }

  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);

  gfxWin->window = xcb_generate_id(gfxApp->connection);
  uint32_t winValues[2] = {
    XCB_BACK_PIXMAP_NONE,
    XCB_EVENT_MASK_EXPOSURE
      | XCB_EVENT_MASK_STRUCTURE_NOTIFY
      | XCB_EVENT_MASK_KEY_PRESS
      | XCB_EVENT_MASK_KEY_RELEASE
      | XCB_EVENT_MASK_BUTTON_PRESS
      | XCB_EVENT_MASK_BUTTON_RELEASE
      | XCB_EVENT_MASK_POINTER_MOTION
  };
  xcb_void_cookie_t winCookie = xcb_create_window_checked(
                        gfxApp->connection,
                        XCB_COPY_FROM_PARENT,           // depth
                        gfxWin->window,
                        gfxApp->screen->root,           // parent
                        0, 0,                           // position
                        (uint16_t)w,
                        (uint16_t)h,
                        0,                              // border_width
                        XCB_WINDOW_CLASS_INPUT_OUTPUT,
                        gfxApp->visual->visual_id,
                        XCB_CW_BACK_PIXMAP | XCB_CW_EVENT_MASK,
                        winValues);
  if (xcb_request_check(gfxApp->connection, winCookie)) {
    return false;
  }

  //--- set the title
  xcb_change_property(gfxApp->connection, XCB_PROP_MODE_REPLACE,
                      gfxWin->window,
                      XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                      (uint32_t)std::min(title.size(), (size_t)UINT32_MAX), title.c_str());

  //--- request the close event
  xcb_atom_t atoms[1];
  atoms[0] = gfxApp->deleteWindowAtom;
  xcb_icccm_set_wm_protocols(gfxApp->connection, gfxWin->window,
                             gfxApp->protocolsAtom, 1, atoms);

  //--- create the GC
  gfxWin->gc = xcb_generate_id(gfxApp->connection);
  uint32_t gcValues[1] = {gfxApp->screen->black_pixel};
  xcb_create_gc(gfxApp->connection, gfxWin->gc,
                gfxWin->window, XCB_GC_FOREGROUND, gcValues);

  //--- show the window
  xcb_map_window(gfxApp->connection, gfxWin->window);
  xcb_flush(gfxApp->connection);

  return true;
}

static void closeWindowXcb(GfxWindow *gfxWin) {
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);

  if (gfxWin->gc != XCB_NONE) {
    xcb_free_gc(gfxApp->connection, gfxWin->gc);
  }
  if (gfxWin->window != XCB_NONE) {
    xcb_destroy_window(gfxApp->connection, gfxWin->window);
  }
}

//------------------------------------------------------------------------
// application
//------------------------------------------------------------------------

static void initApp(BytecodeEngine &engine) {
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);
  if (gfxApp) {
    return;
  }

  try {
    gfxApp = new GfxApplication();
  } catch (std::bad_alloc) {
    BytecodeEngine::fatalError("Out of memory");
  }
  gfxApp->resObj.finalizer = &finalizeApp;
  gfxApp->genericSerifFont = fallbackSerifFont;
  gfxApp->genericSerifBoldFont = fallbackSerifBoldFont;
  gfxApp->genericSerifItalicFont = fallbackSerifItalicFont;
  gfxApp->genericSerifBoldItalicFont = fallbackSerifBoldItalicFont;
  gfxApp->genericSansFont = fallbackSansFont;
  gfxApp->genericSansBoldFont = fallbackSansBoldFont;
  gfxApp->genericSansItalicFont = fallbackSansItalicFont;
  gfxApp->genericSansBoldItalicFont = fallbackSansBoldItalicFont;
  gfxApp->genericMonoFont = fallbackMonoFont;
  gfxApp->genericMonoBoldFont = fallbackMonoBoldFont;
  gfxApp->genericMonoItalicFont = fallbackMonoItalicFont;
  gfxApp->genericMonoBoldItalicFont = fallbackMonoBoldItalicFont;
  gfxApp->defaultFontSize = 0;
  gfxApp->connection = nullptr;
  gfxApp->screenNum = 0;
  gfxApp->screen = nullptr;
  gfxApp->visual = nullptr;
  gfxApp->screenDPI = 0;
  gfxApp->protocolsAtom = XCB_ATOM_NONE;
  gfxApp->deleteWindowAtom = XCB_ATOM_NONE;
  gfxApp->clipboardAtom = XCB_ATOM_NONE;
  gfxApp->utf8StringAtom = XCB_ATOM_NONE;
  gfxApp->targetsAtom = XCB_ATOM_NONE;
  gfxApp->haxSelectionAtom = XCB_ATOM_NONE;
  gfxApp->xkbContext = nullptr;
  gfxApp->xkbKeymap = nullptr;
  gfxApp->xkbState = nullptr;
  gfxApp->xkbComposeTable = nullptr;
  gfxApp->xkbComposeState = nullptr;
  gfxApp->xkbModShift = 0;
  gfxApp->xkbModCtrl = 0;
  gfxApp->xkbModAlt = 0;
  gfxApp->xkbModSuper = 0;
  gfxApp->prevButtonPressTime = 0;
  gfxApp->prevButtonPressX = 0;
  gfxApp->prevButtonPressY = 0;
  gfxApp->buttonPressCount = 1;

  if (!initAppXcb(gfxApp)) {
    delete gfxApp;
    BytecodeEngine::fatalError("Couldn't connect to X server");
  }

  if (!initXkb(gfxApp) ||
      !initXkbKeymap(gfxApp)) {
    delete gfxApp;
    BytecodeEngine::fatalError("Couldn't connect to X server");
  }

  ConfigFile::Item *cfgItem;
  if ((cfgItem = engine.configItem("gfx", "genericSerifFont")) &&
      cfgItem->args.size() == 1) {
    gfxApp->genericSerifFont = cfgItem->args[0];
  }
  if ((cfgItem = engine.configItem("gfx", "genericSerifBoldFont")) &&
      cfgItem->args.size() == 1) {
    gfxApp->genericSerifBoldFont = cfgItem->args[0];
  }
  if ((cfgItem = engine.configItem("gfx", "genericSerifItalicFont")) &&
      cfgItem->args.size() == 1) {
    gfxApp->genericSerifItalicFont = cfgItem->args[0];
  }
  if ((cfgItem = engine.configItem("gfx", "genericSerifBoldItalicFont")) &&
      cfgItem->args.size() == 1) {
    gfxApp->genericSerifBoldItalicFont = cfgItem->args[0];
  }
  if ((cfgItem = engine.configItem("gfx", "genericSansFont")) &&
      cfgItem->args.size() == 1) {
    gfxApp->genericSansFont = cfgItem->args[0];
  }
  if ((cfgItem = engine.configItem("gfx", "genericSansBoldFont")) &&
      cfgItem->args.size() == 1) {
    gfxApp->genericSansBoldFont = cfgItem->args[0];
  }
  if ((cfgItem = engine.configItem("gfx", "genericSansItalicFont")) &&
      cfgItem->args.size() == 1) {
    gfxApp->genericSansItalicFont = cfgItem->args[0];
  }
  if ((cfgItem = engine.configItem("gfx", "genericSansBoldItalicFont")) &&
      cfgItem->args.size() == 1) {
    gfxApp->genericSansBoldItalicFont = cfgItem->args[0];
  }
  if ((cfgItem = engine.configItem("gfx", "genericMonoFont")) &&
      cfgItem->args.size() == 1) {
    gfxApp->genericMonoFont = cfgItem->args[0];
  }
  if ((cfgItem = engine.configItem("gfx", "genericMonoBoldFont")) &&
      cfgItem->args.size() == 1) {
    gfxApp->genericMonoBoldFont = cfgItem->args[0];
  }
  if ((cfgItem = engine.configItem("gfx", "genericMonoItalicFont")) &&
      cfgItem->args.size() == 1) {
    gfxApp->genericMonoItalicFont = cfgItem->args[0];
  }
  if ((cfgItem = engine.configItem("gfx", "genericMonoBoldItalicFont")) &&
      cfgItem->args.size() == 1) {
    gfxApp->genericMonoBoldItalicFont = cfgItem->args[0];
  }
  gfxApp->defaultFontSize = std::max(10.0, gfxApp->screenDPI * 0.125);
  if ((cfgItem = engine.configItem("gfx", "defaultFontSize")) &&
      cfgItem->args.size() == 1) {
    float size;
    if (stringToFloatChecked(cfgItem->args[0], size)) {
      gfxApp->defaultFontSize = size;
    }
  }

  app->gfxApp = cellMakeResourcePtr(gfxApp);
  engine.addResourceObject(&gfxApp->resObj);
}

static void finalizeApp(ResourceObject *resObj) {
  GfxApplication *gfxApp = (GfxApplication *)resObj;
  closeApp(gfxApp);
}

static void closeApp(GfxApplication *gfxApp) {
  closeXkb(gfxApp);
  closeAppXcb(gfxApp);
  delete gfxApp;
}

static Window *findWindow(xcb_window_t xcbWindow) {
  Application *app = (Application *)cellHeapPtr(appCell);
  for (Window *win = (Window *)cellHeapPtr(app->winList);
       win;
       win = (Window *)cellHeapPtr(win->next)) {
    GfxWindow *gfxWin = (GfxWindow *)cellResourcePtr(win->gfxWin);
    if (gfxWin->window == xcbWindow) {
      return win;
    }
  }
  return nullptr;
}

static bool initAppXcb(GfxApplication *gfxApp) {
  //--- connection, screen number
  gfxApp->connection = xcb_connect(nullptr, &gfxApp->screenNum);
  if (xcb_connection_has_error(gfxApp->connection)) {
    return false;
  }

  //--- screen
  const xcb_setup_t *setup = xcb_get_setup(gfxApp->connection);
  xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
  for (int i = 0; i < gfxApp->screenNum; ++i) {
    xcb_screen_next(&iter);
  }
  gfxApp->screen = iter.data;

  //--- visual
  for (xcb_depth_iterator_t depthIter = xcb_screen_allowed_depths_iterator(gfxApp->screen);
       depthIter.rem;
       xcb_depth_next(&depthIter)) {
    for (xcb_visualtype_iterator_t visualIter = xcb_depth_visuals_iterator(depthIter.data);
         visualIter.rem;
         xcb_visualtype_next(&visualIter)) {
      if (gfxApp->screen->root_visual == visualIter.data->visual_id) {
        gfxApp->visual = visualIter.data;
      }
    }
  }
  if (!gfxApp->visual) {
    return false;
  }

  //--- require pixel format = 24/32-bit RGB
  if (!((gfxApp->screen->root_depth == 24 ||
         gfxApp->screen->root_depth == 32) &&
        gfxApp->visual->red_mask   == 0x00ff0000 &&
        gfxApp->visual->green_mask == 0x0000ff00 &&
        gfxApp->visual->blue_mask  == 0x000000ff)) {
    return false;
  }

  //--- atoms
  gfxApp->protocolsAtom = atom(gfxApp->connection, "WM_PROTOCOLS", false);
  gfxApp->deleteWindowAtom = atom(gfxApp->connection, "WM_DELETE_WINDOW", false);
  gfxApp->clipboardAtom = atom(gfxApp->connection, "CLIPBOARD", false);
  gfxApp->utf8StringAtom = atom(gfxApp->connection, "UTF8_STRING", false);
  gfxApp->targetsAtom = atom(gfxApp->connection, "TARGETS", false);
  gfxApp->haxSelectionAtom = atom(gfxApp->connection, "HAX_SELECTION", true);

  //--- check for shm pixmap support
  xcb_shm_query_version_cookie_t shmQueryCookie = xcb_shm_query_version(gfxApp->connection);
  xcb_generic_error_t *shmQueryErr;
  xcb_shm_query_version_reply_t *shmQueryReply =
      xcb_shm_query_version_reply(gfxApp->connection, shmQueryCookie, &shmQueryErr);
  bool hasShm = !shmQueryErr && shmQueryReply && shmQueryReply->shared_pixmaps;
  free(shmQueryReply);
  if (!hasShm) {
    return false;
  }

  //--- get screen resolution via the RandR extension
  gfxApp->screenDPI = 96;  // generic default value if we don't have RandR
  xcb_randr_query_version_cookie_t randrQueryCookie =
      xcb_randr_query_version(gfxApp->connection, 1, 5);
  xcb_generic_error_t *randrQueryErr;
  xcb_randr_query_version_reply_t *randrQueryReply =
      xcb_randr_query_version_reply(gfxApp->connection, randrQueryCookie, &randrQueryErr);
  bool hasRandR = !randrQueryErr && randrQueryReply &&
                  randrQueryReply->major_version == 1 && randrQueryReply->minor_version >= 2;
  free(randrQueryReply);
  if (hasRandR) {
    xcb_randr_get_screen_info_cookie_t screenInfoCookie =
        xcb_randr_get_screen_info(gfxApp->connection, gfxApp->screen->root);
    xcb_generic_error_t *screenInfoErr;
    xcb_randr_get_screen_info_reply_t *screenInfoReply =
        xcb_randr_get_screen_info_reply(gfxApp->connection, screenInfoCookie, &screenInfoErr);
    if (!screenInfoErr) {
      xcb_randr_screen_size_iterator_t screenSizeIter =
          xcb_randr_get_screen_info_sizes_iterator(screenInfoReply);
      for (int i = 0; i < screenInfoReply->sizeID; ++i) {
        xcb_randr_screen_size_next(&screenSizeIter);
      }
      double xDotsPerMM = (double)screenSizeIter.data->width
                          / (double)screenSizeIter.data->mwidth;
      double yDotsPerMM = (double)screenSizeIter.data->height
                          / (double)screenSizeIter.data->mheight;
      gfxApp->screenDPI = (int)(25.4 * std::min(xDotsPerMM, yDotsPerMM));
    }
    free(screenInfoReply);
  }

  return true;
}

static void closeAppXcb(GfxApplication *gfxApp) {
  if (gfxApp->connection) {
    xcb_disconnect(gfxApp->connection);
  }
}

static xcb_atom_t atom(xcb_connection_t *connection, const char *name, bool create) {
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, create ? XCB_FALSE : XCB_TRUE,
						    (uint16_t)strlen(name), name);
  xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, nullptr);
  xcb_atom_t a = reply->atom;
  free(reply);
  return a;
}

//------------------------------------------------------------------------
// events
//------------------------------------------------------------------------

int64_t gfxMonoclock() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000 + (int64_t)ts.tv_nsec / 1000;
}

Cell gfxWaitEvent(BytecodeEngine &engine) {
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);

  while (true) {
    enqueueEvents(gfxApp);
    while (!gfxApp->xevents.empty()) {
      Cell eventCell = processEvent(gfxApp, engine);
      if (!cellIsNilHeapPtr(eventCell)) {
	return eventCell;
      }
    }
    doPoll(gfxApp, -1);
  }
}

Cell gfxWaitEvent(int64_t timeLimit, BytecodeEngine &engine) {
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);

  while (true) {
    enqueueEvents(gfxApp);
    while (!gfxApp->xevents.empty()) {
      Cell eventCell = processEvent(gfxApp, engine);
      if (!cellIsNilHeapPtr(eventCell)) {
	return eventCell;
      }
    }
    int64_t delta = timeLimit - gfxMonoclock();
    if (delta <= 0) {
      return cellMakeError();
    }
    doPoll(gfxApp, delta);
  }
}

Cell gfxPollEvent(BytecodeEngine &engine) {
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);

  enqueueEvents(gfxApp);
  while (!gfxApp->xevents.empty()) {
    Cell eventCell = processEvent(gfxApp, engine);
    if (!cellIsNilHeapPtr(eventCell)) {
      return eventCell;
    }
  }
  return cellMakeError();
}

static void enqueueEvents(GfxApplication *gfxApp) {
  xcb_generic_event_t *xev;
  while ((xev = xcb_poll_for_event(gfxApp->connection))) {
    gfxApp->xevents.push_back(xev);
  }
}

static void doPoll(GfxApplication *gfxApp, int64_t timeout) {
  struct pollfd fd;
  fd.fd = xcb_get_file_descriptor(gfxApp->connection);
  fd.events = POLLIN;
  fd.revents = 0;
  int t;
  if (timeout < 0) {
    t = -1;
  } else {
    t = std::max(1, (int)std::min((int64_t)INT_MAX, timeout / 1000));
  }
  poll(&fd, 1, t);
}

static Cell processEvent(GfxApplication *gfxApp, BytecodeEngine &engine) {
  xcb_generic_event_t *xev = gfxApp->xevents.front();
  gfxApp->xevents.pop_front();

  Cell eventCell = cellMakeNilHeapPtr();
  switch (xev->response_type & ~0x80) {

  case 0:
    // ignore errors
    break;

  case XCB_EXPOSE: {
    // expose events are handled here; they do not generate an Event object
    xcb_expose_event_t *expose = (xcb_expose_event_t *)xev;
    Window *win = findWindow(expose->window);
    if (win) {
      redrawWindow(win, expose->x, expose->y, expose->width, expose->height);
    }
    break;
  }

  case XCB_CONFIGURE_NOTIFY: {
    xcb_configure_notify_event_t *cfg = (xcb_configure_notify_event_t *)xev;
    // if there are any later CONFIGURE_NOTIFY events in the queue for
    // the same window, skip this one
    bool skip = false;
    for (xcb_generic_event_t *xev2 : gfxApp->xevents) {
      if ((xev2->response_type & ~0x80) == XCB_CONFIGURE_NOTIFY &&
	  ((xcb_configure_notify_event_t *)xev2)->window == cfg->window) {
	skip = true;
      }
    }
    if (skip) {
      break;
    }
    Window *win = findWindow(cfg->window);
    if (win) {
      GfxWindow *gfxWin = (GfxWindow *)cellResourcePtr(win->gfxWin);
      if (cfg->width != gfxWin->w || cfg->height != gfxWin->h) {
	Cell winCell = cellMakeHeapPtr(win);
	engine.pushGCRoot(winCell);
	resizeWindow(win, cfg->width, cfg->height);
	eventCell = makeResizeEvent(winCell, cfg->width, cfg->height, engine);
	engine.popGCRoot(winCell);
      }
    }
    break;
  }

  case XCB_KEY_PRESS: {
    xcb_key_press_event_t *keyPress = (xcb_key_press_event_t *)xev;
    int64_t key, unicode, modifiers;
    if (getKey(gfxApp, true, keyPress->detail, key, unicode, modifiers)) {
      Window *win = findWindow(keyPress->event);
      if (win) {
	Cell winCell = cellMakeHeapPtr(win);
	engine.pushGCRoot(winCell);
	eventCell = makeKeyPressEvent(winCell, key, unicode, modifiers, engine);
	engine.popGCRoot(winCell);
      }
    }
    break;
  }

  case XCB_KEY_RELEASE: {
    xcb_key_release_event_t *keyRelease = (xcb_key_release_event_t *)xev;
    int64_t key, unicode, modifiers;
    if (getKey(gfxApp, false, keyRelease->detail, key, unicode, modifiers)) {
      Window *win = findWindow(keyRelease->event);
      if (win) {
	Cell winCell = cellMakeHeapPtr(win);
	engine.pushGCRoot(winCell);
	eventCell = makeKeyReleaseEvent(winCell, key, unicode, modifiers, engine);
	engine.popGCRoot(winCell);
      }
    }
    break;
  }

  case XCB_BUTTON_PRESS: {
    xcb_button_press_event_t *btn = (xcb_button_press_event_t *)xev;
    Window *win = findWindow(btn->event);
    if (win) {
      int64_t modifiers = 0;
      if (btn->state & XCB_KEY_BUT_MASK_SHIFT) {
	modifiers |= modShift;
      }
      if (btn->state & XCB_KEY_BUT_MASK_CONTROL) {
	modifiers |= modCtrl;
      }
      if (btn->state & XCB_KEY_BUT_MASK_MOD_1) {
	modifiers |= modAlt;
      }
      if (btn->state & XCB_KEY_BUT_MASK_MOD_4) {
	modifiers |= modSuper;
      }
      if (btn->time - gfxApp->prevButtonPressTime < multiClickMaxInterval &&
	  abs(btn->event_x - gfxApp->prevButtonPressX) < multiClickMaxDistance &&
	  abs(btn->event_y - gfxApp->prevButtonPressY) < multiClickMaxDistance) {
	++gfxApp->buttonPressCount;
      } else {
	gfxApp->buttonPressCount = 1;
      }
      gfxApp->prevButtonPressTime = btn->time;
      gfxApp->prevButtonPressX = btn->event_x;
      gfxApp->prevButtonPressY = btn->event_y;
      Cell winCell = cellMakeHeapPtr(win);
      engine.pushGCRoot(winCell);
      if (btn->detail == 4 || btn->detail == 5) {
	eventCell = makeMouseScrollWheelEvent(winCell, (int64_t)btn->event_x, (int64_t)btn->event_y,
					      (btn->detail == 4) ? 1 : -1, modifiers, engine);
      } else {
	eventCell = makeMouseButtonPressEvent(winCell, (int64_t)btn->event_x, (int64_t)btn->event_y,
					      (int64_t)btn->detail, gfxApp->buttonPressCount,
					      modifiers, engine);
      }
      engine.popGCRoot(winCell);
    }
    break;
  }

  case XCB_BUTTON_RELEASE: {
    xcb_button_release_event_t *btn = (xcb_button_release_event_t *)xev;
    Window *win = findWindow(btn->event);
    if (win) {
      int64_t modifiers = 0;
      if (btn->state & XCB_KEY_BUT_MASK_SHIFT) {
	modifiers |= modShift;
      }
      if (btn->state & XCB_KEY_BUT_MASK_CONTROL) {
	modifiers |= modCtrl;
      }
      if (btn->state & XCB_KEY_BUT_MASK_MOD_1) {
	modifiers |= modAlt;
      }
      if (btn->state & XCB_KEY_BUT_MASK_MOD_4) {
	modifiers |= modSuper;
      }
      Cell winCell = cellMakeHeapPtr(win);
      engine.pushGCRoot(winCell);
      if (btn->detail == 4 || btn->detail == 5) {
	// no event for scroll wheel release
      } else {
	eventCell = makeMouseButtonReleaseEvent(winCell,
						(int64_t)btn->event_x, (int64_t)btn->event_y,
						(int64_t)btn->detail, modifiers, engine);
      }
      engine.popGCRoot(winCell);
    }
    break;
  }

  case XCB_MOTION_NOTIFY: {
    xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *)xev;
    // if there is another MOTION_NOTIFY event in the queue, with no
    // other intervening events (other than exposure events), then
    // skip this one (we don't want to merge MOTION_NOTIFY events
    // across an intervening mouse button event, for example)
    bool skip = false;
    for (xcb_generic_event_t *xev2 : gfxApp->xevents) {
      int type = xev2->response_type & ~0x80;
      if (type == XCB_MOTION_NOTIFY) {
	skip = true;
	break;
      }
      if (type != XCB_EXPOSE &&
	  type != XCB_GRAPHICS_EXPOSURE &&
	  type != XCB_NO_EXPOSURE) {
	break;
      }
    }
    if (skip) {
      break;
    }
    Window *win = findWindow(motion->event);
    if (win) {
      int64_t button = 0;
      if (motion->state & XCB_KEY_BUT_MASK_BUTTON_1) {
	button = 1;
      } else if (motion->state & XCB_KEY_BUT_MASK_BUTTON_2) {
	button = 2;
      } else if (motion->state & XCB_KEY_BUT_MASK_BUTTON_3) {
	button = 3;
      } else if (motion->state & XCB_KEY_BUT_MASK_BUTTON_4) {
	button = 4;
      } else if (motion->state & XCB_KEY_BUT_MASK_BUTTON_5) {
	button = 5;
      }
      int64_t modifiers = 0;
      if (motion->state & XCB_KEY_BUT_MASK_SHIFT) {
	modifiers |= modShift;
      }
      if (motion->state & XCB_KEY_BUT_MASK_CONTROL) {
	modifiers |= modCtrl;
      }
      if (motion->state & XCB_KEY_BUT_MASK_MOD_1) {
	modifiers |= modAlt;
      }
      if (motion->state & XCB_KEY_BUT_MASK_MOD_4) {
	modifiers |= modSuper;
      }
      Cell winCell = cellMakeHeapPtr(win);
      engine.pushGCRoot(winCell);
      eventCell = makeMouseMoveEvent(winCell, (int64_t)motion->event_x, (int64_t)motion->event_y,
				     button, modifiers, engine);
      engine.popGCRoot(winCell);
    }
    break;
  }

  case XCB_SELECTION_REQUEST: {
    xcb_selection_request_event_t *selectionRequest = (xcb_selection_request_event_t *)xev;
    handleSelectionRequest(gfxApp, selectionRequest);
    break;
  }

  case XCB_CLIENT_MESSAGE: {
    xcb_client_message_event_t *msg = (xcb_client_message_event_t *)xev;
    if (msg->data.data32[0] == gfxApp->deleteWindowAtom) {
      Window *win = findWindow(msg->window);
      if (win) {
	Cell winCell = cellMakeHeapPtr(win);
	engine.pushGCRoot(winCell);
	eventCell = makeCloseEvent(winCell, engine);
	engine.popGCRoot(winCell);
      }
    }
    break;
  }

  default:
    if (xev->response_type == gfxApp->xkbEvent) {
      xkb_generic_event *xkbEv = (xkb_generic_event *)xev;
      if (xkbEv->any.deviceID == gfxApp->xkbDevice) {
	switch (xkbEv->any.xkbType) {
	case XCB_XKB_NEW_KEYBOARD_NOTIFY:
	  if (xkbEv->new_keyboard_notify.changed & XCB_XKB_NKN_DETAIL_KEYCODES) {
	    initXkbKeymap(gfxApp);
	  }
	  break;
	case XCB_XKB_MAP_NOTIFY:
	  initXkbKeymap(gfxApp);
	  break;
	case XCB_XKB_STATE_NOTIFY:
	  xkb_state_update_mask(gfxApp->xkbState,
				xkbEv->state_notify.baseMods,
				xkbEv->state_notify.latchedMods,
				xkbEv->state_notify.lockedMods,
				xkbEv->state_notify.baseGroup,
				xkbEv->state_notify.latchedGroup,
				xkbEv->state_notify.lockedGroup);
	  break;
	}
      }
    }
    break;
  }

  free(xev);
  return eventCell;
}

static Cell makeResizeEvent(Cell &winCell, int64_t w, int64_t h, BytecodeEngine &engine) {
  ResizeEvent *event = (ResizeEvent *)engine.heapAllocTuple(resizeEventNCells, 0);
  event->typeID = cellMakeInt(eventResize);
  event->win = winCell;
  event->w = cellMakeInt(w);
  event->h = cellMakeInt(h);
  return cellMakeHeapPtr(event);
}

static Cell makeCloseEvent(Cell &winCell, BytecodeEngine &engine) {
  CloseEvent *event = (CloseEvent *)engine.heapAllocTuple(closeEventNCells, 0);
  event->typeID = cellMakeInt(eventClose);
  event->win = winCell;
  return cellMakeHeapPtr(event);
}

static Cell makeMouseButtonPressEvent(Cell &winCell, int64_t x, int64_t y,
				      int64_t button, int64_t count, int64_t modifiers,
				      BytecodeEngine &engine) {
  MouseButtonPressEvent *event =
      (MouseButtonPressEvent *)engine.heapAllocTuple(mouseButtonPressEventNCells, 0);
  event->typeID = cellMakeInt(eventMouseButtonPress);
  event->win = winCell;
  event->x = cellMakeInt(x);
  event->y = cellMakeInt(y);
  event->button = cellMakeInt(button);
  event->count = cellMakeInt(count);
  event->modifiers = cellMakeInt(modifiers);
  return cellMakeHeapPtr(event);
}

static Cell makeMouseButtonReleaseEvent(Cell &winCell, int64_t x, int64_t y,
					int64_t button, int64_t modifiers, BytecodeEngine &engine) {
  MouseButtonReleaseEvent *event =
      (MouseButtonReleaseEvent *)engine.heapAllocTuple(mouseButtonReleaseEventNCells, 0);
  event->typeID = cellMakeInt(eventMouseButtonRelease);
  event->win = winCell;
  event->x = cellMakeInt(x);
  event->y = cellMakeInt(y);
  event->button = cellMakeInt(button);
  event->modifiers = cellMakeInt(modifiers);
  return cellMakeHeapPtr(event);
}

static Cell makeMouseScrollWheelEvent(Cell &winCell, int64_t x, int64_t y,
				      int64_t scroll, int64_t modifiers, BytecodeEngine &engine) {
  MouseScrollWheelEvent *event =
      (MouseScrollWheelEvent *)engine.heapAllocTuple(mouseScrollWheelEventNCells, 0);
  event->typeID = cellMakeInt(eventMouseScrollWheel);
  event->win = winCell;
  event->x = cellMakeInt(x);
  event->y = cellMakeInt(y);
  event->scroll = cellMakeInt(scroll);
  event->modifiers = cellMakeInt(modifiers);
  return cellMakeHeapPtr(event);
}

static Cell makeMouseMoveEvent(Cell &winCell, int64_t x, int64_t y,
			       int64_t button, int64_t modifiers, BytecodeEngine &engine) {
  MouseMoveEvent *event = (MouseMoveEvent *)engine.heapAllocTuple(mouseMoveEventNCells, 0);
  event->typeID = cellMakeInt(eventMouseMove);
  event->win = winCell;
  event->x = cellMakeInt(x);
  event->y = cellMakeInt(y);
  event->button = cellMakeInt(button);
  event->modifiers = cellMakeInt(modifiers);
  return cellMakeHeapPtr(event);
}

static Cell makeKeyPressEvent(Cell &winCell, int64_t key, int64_t unicode, int64_t modifiers,
			      BytecodeEngine &engine) {
  KeyPressEvent *event = (KeyPressEvent *)engine.heapAllocTuple(keyPressEventNCells, 0);
  event->typeID = cellMakeInt(eventKeyPress);
  event->win = winCell;
  event->key = cellMakeInt(key);
  event->unicode = cellMakeInt(unicode);
  event->modifiers = cellMakeInt(modifiers);
  return cellMakeHeapPtr(event);
}

static Cell makeKeyReleaseEvent(Cell &winCell, int64_t key, int64_t unicode, int64_t modifiers,
				BytecodeEngine &engine) {
  KeyReleaseEvent *event = (KeyReleaseEvent *)engine.heapAllocTuple(keyReleaseEventNCells, 0);
  event->typeID = cellMakeInt(eventKeyRelease);
  event->win = winCell;
  event->key = cellMakeInt(key);
  event->unicode = cellMakeInt(unicode);
  event->modifiers = cellMakeInt(modifiers);
  return cellMakeHeapPtr(event);
}

//------------------------------------------------------------------------
// xkb
//------------------------------------------------------------------------

static bool initXkb(GfxApplication *gfxApp) {
  //--- xkb setup
  if (!xkb_x11_setup_xkb_extension(gfxApp->connection,
				   XKB_X11_MIN_MAJOR_XKB_VERSION,
				   XKB_X11_MIN_MINOR_XKB_VERSION,
				   XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS,
				   nullptr, nullptr,
				   &gfxApp->xkbEvent, nullptr)) {
    return false;
  }
  gfxApp->xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (!gfxApp->xkbContext) {
    return false;
  }
  gfxApp->xkbDevice = xkb_x11_get_core_keyboard_device_id(gfxApp->connection);
  if (gfxApp->xkbDevice == -1) {
    return false;
  }

  //--- compose table
  const char *locale = getenv("LC_ALL");
  if (!locale || !*locale) {
    locale = getenv("LC_CTYPE");
  }
  if (!locale || !*locale) {
    locale = getenv("LANG");
  }
  if (!locale || !*locale) {
    locale = "C";
  }
  gfxApp->xkbComposeTable = xkb_compose_table_new_from_locale(gfxApp->xkbContext, locale,
							      XKB_COMPOSE_COMPILE_NO_FLAGS);
  if (!gfxApp->xkbComposeTable) {
    return false;
  }

  //--- request xkb events
  uint16_t events = XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY |
                    XCB_XKB_EVENT_TYPE_MAP_NOTIFY |
                    XCB_XKB_EVENT_TYPE_STATE_NOTIFY;
  uint16_t mapParts = XCB_XKB_MAP_PART_KEY_TYPES |
                      XCB_XKB_MAP_PART_KEY_SYMS |
                      XCB_XKB_MAP_PART_MODIFIER_MAP |
                      XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS |
                      XCB_XKB_MAP_PART_KEY_ACTIONS |
                      XCB_XKB_MAP_PART_VIRTUAL_MODS |
                      XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP;
  uint16_t stateParts = XCB_XKB_STATE_PART_MODIFIER_BASE |
                        XCB_XKB_STATE_PART_MODIFIER_LATCH |
                        XCB_XKB_STATE_PART_MODIFIER_LOCK |
                        XCB_XKB_STATE_PART_GROUP_BASE |
                        XCB_XKB_STATE_PART_GROUP_LATCH |
                        XCB_XKB_STATE_PART_GROUP_LOCK;
  xcb_xkb_select_events_details_t details;
  memset(&details, 0, sizeof(xcb_xkb_select_events_details_t));
  details.affectNewKeyboard = XCB_XKB_NKN_DETAIL_KEYCODES;
  details.newKeyboardDetails = XCB_XKB_NKN_DETAIL_KEYCODES;
  details.affectState = stateParts;
  details.stateDetails = stateParts;
  xcb_void_cookie_t selectEventsCookie =
                        xcb_xkb_select_events(gfxApp->connection,
					      (xcb_xkb_device_spec_t)gfxApp->xkbDevice,
					      events, 0, 0, mapParts, mapParts, &details);
  xcb_generic_error_t *selectEventsError = xcb_request_check(gfxApp->connection,
							     selectEventsCookie);
  if (selectEventsError) {
    free(selectEventsError);
    return false;
  }

  return true;
}

// This is called at startup, and also whenever we're notified that
// the keymap has changed.
static bool initXkbKeymap(GfxApplication *gfxApp) {
  //--- get the new keymap and state
  xkb_keymap *newKeymap = xkb_x11_keymap_new_from_device(
			      gfxApp->xkbContext, gfxApp->connection,
			      gfxApp->xkbDevice, XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (!newKeymap) {
    return false;
  }
  xkb_state *newXkbState = xkb_x11_state_new_from_device(newKeymap,
							 gfxApp->connection,
							 gfxApp->xkbDevice);
  if (!newXkbState) {
    xkb_keymap_unref(newKeymap);
    return false;
  }
  xkb_compose_state *newComposeState = xkb_compose_state_new(gfxApp->xkbComposeTable,
							     XKB_COMPOSE_STATE_NO_FLAGS);
  if (!newComposeState) {
    xkb_state_unref(newXkbState);
    xkb_keymap_unref(newKeymap);
    return false;
  }

  //--- replace the old values
  if (gfxApp->xkbComposeState) {
    xkb_compose_state_unref(gfxApp->xkbComposeState);
  }
  if (gfxApp->xkbState) {
    xkb_state_unref(gfxApp->xkbState);
  }
  if (gfxApp->xkbKeymap) {
    xkb_keymap_unref(gfxApp->xkbKeymap);
  }
  gfxApp->xkbKeymap = newKeymap;
  gfxApp->xkbState = newXkbState;
  gfxApp->xkbComposeState = newComposeState;

  //--- xkb modifiers
  gfxApp->xkbModShift = xkb_keymap_mod_get_index(gfxApp->xkbKeymap, "Shift");
  gfxApp->xkbModCtrl = xkb_keymap_mod_get_index(gfxApp->xkbKeymap, "Control");
  gfxApp->xkbModAlt = xkb_keymap_mod_get_index(gfxApp->xkbKeymap, "Mod1");
  gfxApp->xkbModSuper = xkb_keymap_mod_get_index(gfxApp->xkbKeymap, "Mod4");

  return true;
}

static void closeXkb(GfxApplication *gfxApp) {
  if (gfxApp->xkbComposeState) {
    xkb_compose_state_unref(gfxApp->xkbComposeState);
  }
  if (gfxApp->xkbComposeTable) {
    xkb_compose_table_unref(gfxApp->xkbComposeTable);
  }
  if (gfxApp->xkbState) {
    xkb_state_unref(gfxApp->xkbState);
  }
  if (gfxApp->xkbKeymap) {
    xkb_keymap_unref(gfxApp->xkbKeymap);
  }
  if (gfxApp->xkbContext) {
    xkb_context_unref(gfxApp->xkbContext);
  }
}

static bool getKey(GfxApplication *gfxApp, bool press, xkb_keycode_t keyCode,
		   int64_t &key, int64_t &unicode, int64_t &modifiers) {
  // Notes:
  // * capitalization is done by xkb_state_key_get_one_sym()
  // * using xkb_keysym_to_utf32() instead of xkb_state_key_get_utf8()
  //   avoids the control keysym transformation

  xkb_keysym_t xkeysym = xkb_state_key_get_one_sym(gfxApp->xkbState, keyCode);
  if (press) {
    xkb_compose_state_feed(gfxApp->xkbComposeState, xkeysym);
  }
  xkb_compose_status composeStatus = xkb_compose_state_get_status(gfxApp->xkbComposeState);

  bool ret = false;
  if (composeStatus != XKB_COMPOSE_COMPOSING &&
      composeStatus != XKB_COMPOSE_CANCELLED) {
    ret = true;
    if (composeStatus == XKB_COMPOSE_COMPOSED) {
      xkeysym = xkb_compose_state_get_one_sym(gfxApp->xkbComposeState);
    }
    unicode = xkb_keysym_to_utf32(xkeysym);
    // treat all control keys and the delete key as special, i.e.,
    // with no unicode value
    if (unicode < 0x20 || unicode == 0x7f) {
      unicode = 0;
    }
    modifiers = 0;
    if (xkb_state_mod_index_is_active(gfxApp->xkbState, gfxApp->xkbModShift,
				      XKB_STATE_MODS_EFFECTIVE) > 0) {
      modifiers |= modShift;
    }
    if (xkb_state_mod_index_is_active(gfxApp->xkbState, gfxApp->xkbModCtrl,
				      XKB_STATE_MODS_EFFECTIVE) > 0) {
      modifiers |= modCtrl;
    }
    if (xkb_state_mod_index_is_active(gfxApp->xkbState, gfxApp->xkbModAlt,
				      XKB_STATE_MODS_EFFECTIVE) > 0) {
      modifiers |= modAlt;
    }
    if (xkb_state_mod_index_is_active(gfxApp->xkbState, gfxApp->xkbModSuper,
				      XKB_STATE_MODS_EFFECTIVE) > 0) {
      modifiers |= modSuper;
    }
    key = mapKeysym(xkeysym);
  }

  if (!press) {
    if (composeStatus == XKB_COMPOSE_COMPOSED ||
	composeStatus == XKB_COMPOSE_CANCELLED) {
      xkb_compose_state_reset(gfxApp->xkbComposeState);
    }
  }

  return ret;
}

static int64_t mapKeysym(xkb_keysym_t xkeysym) {
  if (xkeysym >= keyRegularMin && xkeysym <= keyRegularMax) {
    return (int64_t)xkeysym;
  }

  switch (xkeysym) {
  case XK_Return:       return keyReturn;
  case XK_Left:         return keyLeft;
  case XK_Up:           return keyUp;
  case XK_Right:        return keyRight;
  case XK_Down:         return keyDown;
  case XK_Page_Up:      return keyPgUp;
  case XK_Page_Down:    return keyPgDn;
  case XK_Home:         return keyHome;
  case XK_End:          return keyEnd;
  case XK_Insert:       return keyInsert;
  case XK_Delete:       return keyDelete;
  case XK_BackSpace:    return keyBackspace;
  case XK_Tab:          return keyTab;
    // tab and ctrl-tab send XK_Tab; shift-tab and ctrl-shift-tab send
    // XK_ISO_Left_tab -- I have no idea why, but treat them the same
  case XK_ISO_Left_Tab: return keyTab;
  case XK_Escape:       return keyEsc;
  case XK_F1:           return keyF1;
  case XK_F2:           return keyF2;
  case XK_F3:           return keyF3;
  case XK_F4:           return keyF4;
  case XK_F5:           return keyF5;
  case XK_F6:           return keyF6;
  case XK_F7:           return keyF7;
  case XK_F8:           return keyF8;
  case XK_F9:           return keyF9;
  case XK_F10:          return keyF10;
  case XK_F11:          return keyF11;
  case XK_F12:          return keyF12;
  case XK_F13:          return keyF13;
  case XK_F14:          return keyF14;
  case XK_F15:          return keyF15;
  case XK_F16:          return keyF16;
  case XK_F17:          return keyF17;
  case XK_F18:          return keyF18;
  case XK_F19:          return keyF19;
  case XK_F20:          return keyF20;
  case XK_F21:          return keyF21;
  case XK_F22:          return keyF22;
  case XK_F23:          return keyF23;
  case XK_F24:          return keyF24;
  case XK_F25:          return keyF25;
  case XK_F26:          return keyF26;
  case XK_F27:          return keyF27;
  case XK_F28:          return keyF28;
  case XK_F29:          return keyF29;
  case XK_F30:          return keyF30;
  case XK_F31:          return keyF31;
  case XK_F32:          return keyF32;
  case XK_F33:          return keyF33;
  case XK_F34:          return keyF34;
  case XK_F35:          return keyF35;
  case XK_Shift_L:      return keyShift;
  case XK_Shift_R:      return keyShift;
  case XK_Control_L:    return keyControl;
  case XK_Control_R:    return keyControl;
  case XK_Alt_L:        return keyAlt;
  case XK_Alt_R:        return keyAlt;
  case XK_Super_L:      return keySuper;
  case XK_Super_R:      return keySuper;
  case XK_Caps_Lock:    return keyCapsLock;
  default:              return keyNone;
  }
}

//------------------------------------------------------------------------
// clipboard
//------------------------------------------------------------------------

void gfxCopyToClipboard(Cell &windowCell, const std::string &s) {
  Window *win = (Window *)cellHeapPtr(windowCell);
  BytecodeEngine::failOnNilPtr(win);
  GfxWindow *gfxWin = (GfxWindow *)cellResourcePtr(win->gfxWin);
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);

  if (s.size() > INT_MAX) {
    gfxApp->clipText = s.substr(0, INT_MAX);
  } else {
    gfxApp->clipText = s;
  }

  xcb_set_selection_owner(gfxApp->connection, gfxWin->window, gfxApp->clipboardAtom,
			  XCB_CURRENT_TIME);
  xcb_flush(gfxApp->connection);
}

static void handleSelectionRequest(GfxApplication *gfxApp,
				   xcb_selection_request_event_t *selectionRequest) {
  xcb_selection_notify_event_t notify = {0};
  notify.response_type = XCB_SELECTION_NOTIFY;
  notify.time = selectionRequest->time;
  notify.requestor = selectionRequest->requestor;
  notify.selection = selectionRequest->selection;
  notify.target = selectionRequest->target;
  notify.property = XCB_NONE;

  xcb_atom_t prop = selectionRequest->property;
  if (prop == XCB_NONE) {
    prop = selectionRequest->target;
  }

  if (selectionRequest->target == gfxApp->utf8StringAtom) {
    xcb_change_property(gfxApp->connection, XCB_PROP_MODE_REPLACE, selectionRequest->requestor,
			prop, selectionRequest->target, 8, (uint32_t)gfxApp->clipText.size(),
			gfxApp->clipText.c_str());
    notify.property = selectionRequest->property;
  } else if (selectionRequest->target == gfxApp->targetsAtom) {
    xcb_atom_t targets[] = {
      gfxApp->utf8StringAtom,
      gfxApp->targetsAtom
    };
    xcb_change_property(gfxApp->connection, XCB_PROP_MODE_REPLACE, selectionRequest->requestor,
			prop, XCB_ATOM_ATOM, sizeof(xcb_atom_t) * 8,
			sizeof(targets) / sizeof(xcb_atom_t), targets);
    notify.property = selectionRequest->property;
  }

  xcb_send_event(gfxApp->connection, XCB_FALSE, selectionRequest->requestor,
		 0, (const char *)&notify);
  xcb_flush(gfxApp->connection);
}

bool gfxPasteFromClipboard(Cell &windowCell, std::string &s) {
  Window *win = (Window *)cellHeapPtr(windowCell);
  BytecodeEngine::failOnNilPtr(win);
  GfxWindow *gfxWin = (GfxWindow *)cellResourcePtr(win->gfxWin);
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);

  //--- check that selection is owned
  xcb_get_selection_owner_cookie_t ownerCookie =
      xcb_get_selection_owner(gfxApp->connection, gfxApp->clipboardAtom);
  xcb_get_selection_owner_reply_t *ownerReply =
      xcb_get_selection_owner_reply(gfxApp->connection, ownerCookie, nullptr);
  bool hasOwner = ownerReply && ownerReply->owner != XCB_NONE;
  free(ownerReply);
  if (!hasOwner) {
    return false;
  }

  //--- request selection
  xcb_convert_selection(gfxApp->connection, gfxWin->window, gfxApp->clipboardAtom,
			gfxApp->utf8StringAtom, gfxApp->haxSelectionAtom, XCB_CURRENT_TIME);
  xcb_flush(gfxApp->connection);

  //--- wait for a reply
  int64_t timeLimit = gfxMonoclock() + pasteTimeout;
  size_t nextEvent = 0;
  while (true) {
    enqueueEvents(gfxApp);

    //--- look for a SelectionNotify event
    for (; nextEvent < gfxApp->xevents.size(); ++nextEvent) {
      xcb_generic_event_t *xev = gfxApp->xevents[nextEvent];
      if ((xev->response_type & ~0x80) == XCB_SELECTION_NOTIFY) {
	xcb_selection_notify_event_t *notify = (xcb_selection_notify_event_t *)xev;

	//--- get the selection
	s.clear();
	bool more = true;
	bool ok = true;
	do {
	  //~ this doesn't handle INCR
	  xcb_get_property_cookie_t propCookie =
	      xcb_get_property(gfxApp->connection, XCB_TRUE, gfxWin->window,
			       gfxApp->haxSelectionAtom, XCB_ATOM_ANY, s.size() / 4, 65536 / 4);
	  xcb_get_property_reply_t *propReply =
	      xcb_get_property_reply(gfxApp->connection, propCookie, nullptr);
	  if (!propReply) {
	    ok = false;
	    break;
	  }
	  int n = xcb_get_property_value_length(propReply);
	  s.append((char *)xcb_get_property_value(propReply), n * (propReply->format / 8));
	  more = propReply->bytes_after > 0;
	  free(propReply);
	} while (more);

	gfxApp->xevents.erase(gfxApp->xevents.begin() + nextEvent);
	free(notify);
	return ok;
      }
    }

    int64_t delta = timeLimit - gfxMonoclock();
    if (delta <= 0) {
      return false;
    }
    doPoll(gfxApp, delta);
  }
}

//------------------------------------------------------------------------
// screen info
//------------------------------------------------------------------------

int gfxScreenDPI(BytecodeEngine &engine) {
  initApp(engine);
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);
  return gfxApp->screenDPI;
}

//------------------------------------------------------------------------
// config info
//------------------------------------------------------------------------

float gfxDefaultFontSize(BytecodeEngine &engine) {
  initApp(engine);
  Application *app = (Application *)cellHeapPtr(appCell);
  GfxApplication *gfxApp = (GfxApplication *)cellResourcePtr(app->gfxApp);
  return gfxApp->defaultFontSize;
}
