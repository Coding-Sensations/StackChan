// Working 3-Effect Screensaver - Full Screen
// abstractile.c, aligned_malloc.c, alpha.c

#include <Arduino_GFX_Library.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>

// Hardware pins
#define BL_PIN 21
#define BOOT_PIN 0

// Display setup
Arduino_DataBus *bus = new Arduino_HWSPI(2 /* DC */, 15 /* CS */, 14 /* SCK */, 13 /* MOSI */, 12 /* MISO */);
Arduino_GFX *gfx = new Arduino_ILI9341(bus);

// Touch setup
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
SPIClass touchSPI = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

// Animation modes - 78 EFFECTS
enum AnimationMode {
  BOXED = 0,
  BOXFIT = 1, 
  BRAID = 2,
  BSOD = 3,
  B_SPHERE = 4,
  BUBBLE3D = 5,
  BUILDLWO = 6,
  BUMPS = 7,
  CAGE = 8,
  CAROUSEL = 9,
  CCURVE = 10,
  CELTIC = 11,
  CHESSMODELS = 12,
  CHOMPYTOWER = 13,
  CIRCUIT = 14,
  CITYFLOW = 15,
  CLIENTMSG = 16,
  CLOUDLIFE = 17,
  COLORBARS = 18,
  COLORS = 19,
  COMPANION = 20,
  COMPANION_DISC = 21,
  COMPANION_HEART = 22,
  COMPANION_QUAD = 23,
  COMPASS = 24,
  CORAL = 25,
  COVID19 = 26,
  COW_FACE = 27,
  COW_HIDE = 28,
  COW_HOOFS = 29,
  COW_HORNS = 30,
  COW_TAIL = 31,
  COW_UDDER = 32,
  CRACKBERG = 33,
  CRITICAL = 34,
  CRUMBLER = 35,
  CRYSTAL = 36,
  CUBE21 = 37,
  CUBENETIC = 38,
  CUBESTACK = 39,
  CUBESTORM = 40,
  CUBETWIST = 41,
  CUBICGRID = 42,
  CUBOCTEVERSION = 43,
  CWAVES = 44,
  CYNOSURE = 45,
  DANGERBALL = 46,
  DECAYSCREEN = 47,
  DECO = 48,
  DEEPSTARS = 49,
  DELAUNAY = 50,
  DELUXE = 51,
  DEMO_GTK = 52,
  DEMO_GTK_CONF = 53,
  DEMON = 54,
  DEMO_XM = 55,
  DEMO_XM_WIDGETS = 56,
  DIALOG = 57,
  DISCOBALL = 58,
  DISCRETE = 59,
  DISTORT = 60,
  DNALOGO = 61,
  DOLPHIN = 62,
  DOUBLETIME = 63,
  DRIFT = 64,
  DPMS = 65,
  DPMS_PROTOCOL = 66,
  DROPSHADOW = 67,
  DROSTE = 68,
  DYMAXIONMAP_COORDS = 69,
  EASING = 70,
  ENDGAME = 71,
  ENERGYSTREAM = 72,
  ENGINE = 73,
  EPICYCLE = 74,
  ERUPTION = 75,
  EULER2D = 76,
  FADEPLOT = 77,
  ESPER = 78,
  ETRUSCANVENUS = 79,
  EXEC = 80,
  EXT_IDLE_NOTIFY = 81,
  EXTRUSION = 82,
  EXTRUSION_HELIX2 = 83,
  EXTRUSION_HELIX3 = 84,
  EXTRUSION_HELIX4 = 85,
  EXTRUSION_JOINOFFSET = 86,
  EXTRUSION_SCREW = 87,
  EXTRUSION_TAPER = 88,
  EXTRUSION_TWISTOID = 89,
  EXTS = 90,
  EXT_SESSION_LOCK = 91,
  FADE = 92,
  FFMPEG_OUT = 93,
  FIBERLAMP = 94,
  FILMLEADER = 95,
  FIREWORKX = 96,
  FLAG = 97,
  FLAME = 98,
  FLIPFLOP = 99,
  FLIPSCREEN3D = 100,
  FLIPTEXT = 101,
  FLOW = 102,
  FLUIDBALLS = 103,
  FLURRY = 104,
  FLURRY_SMOKE = 105,
  FLURRY_SPARK = 106,
  FLURRY_STAR = 107,
  FLURRY_TEXTURE = 108,
  FLYINGTOASTERS = 109,
  FONTGLIDE = 110,
  FONT_RETRY = 111,
  FOREST = 112,
  FPS = 113,
  FPS_GL = 114,
  FUZZYFLAKES = 115,
  GALAXY = 116,
  GALLANT12X22_BSD = 117,
  GEARS = 118,
  GEODESIC = 119,
  GEODESICGEARS = 120,
  GLLIST = 121,
  GLMATRIX = 122,
  GLPLANET = 123,
  GLPUZZLE = 124,
  GLSCHOOL = 125,
  GLSLIDESHOW = 126,
  GLSL_UTILS = 127,
  GLSNAKE = 128,
  GLTEXT = 129,
  GLTRACKBALL = 130,
  GLUT_STROKE = 131,
  GLUT_SWIDTH = 132,
  GOOP = 133,
  GRABCLIENT = 134,
  GRAB_XIMAGE = 135,
  GRAV = 136,
  GRAVITYWELL = 137,
  GREYNETIC = 138,
  HALFTONE = 139,
  HALO = 140,
  HANDSY = 141,
  HANDSY_MODEL = 142,
  HEADROOM = 143,
  HEADROOM_MODEL = 144,
  HELIX = 145,
  HEXADROP = 146,
  HEXSTRUT = 147,
  HEXTRAIL = 148,
  HIGHVOLTAGE = 149,
  HIGHVOLTAGE_MODEL = 150,
  HILBERT = 151,
  HOPALONG = 152, 
  HOPFANIMATIONS = 153,
  HOPFFIBRATION = 154,
  HSV = 155,
  HYDROSTAT = 156,
  HYPERBALL = 157,
  HYPERCUBE = 158,
  HYPERTORUS = 159,
  HYPNOWHEEL = 160,
  IDLE_PROTOCOL = 161,
  IFS = 162,
  IMSMAP = 163,
  INTERAGGREGATE = 164,
  INTERFERENCE = 165,
  INTERMOMENTARY = 166,
  INVOLUTE = 167,
  JIGGLYPUFF = 168,
  JIGSAW = 169,
  JUGGLE = 170,
  JUGGLER3D = 171,
  ROTOR = 172,
  ROTZOOMER = 173,
  RUBIKBLOCKS = 174,
  RUBIK = 175,
  SBALLS = 176,
  SCOOTER = 177,
  SETUID = 178,
  SHADEBOBS = 179,
  SHARK = 180,
  SLIDESCREEN = 181,
  SLIP = 182,
  SONAR = 183,
  SPEEDMINE = 184,
  SPHERE = 185,
  SPHEREMONICS = 186,
  SPIRAL = 187,
  SPOTLIGHT = 188,
  SPLITFLAP = 189,
  SPLODESIC = 190,
  SPROINGIES = 191,
  SQUIRAL = 192,
  SQUIRTORUS = 193,
  STAIRS = 194,
  STARFISH = 195,
  STARWARS = 196,
  STONERVIEW = 197,
  STRANGE = 198,
  PETRI = 199,
  PHOSPHOR = 200,
  PHOTOPILE = 201,
  PIECEWISE = 202,
  PINION = 203,
  PIPEOBJS = 204,
  PIPES = 205,
  PLATONICFOLDING = 206,
  POLYHEDRA = 207,
  POLYHEDRA_GL = 208,
  POLYOMINOES = 209,
  POLYTOPES = 210,
  PONG = 211,
  POPSQUARES = 212,
  POW2 = 213,
  PREFS = 214,
  PREFSW = 215,
  PROJECTIVEPLANE = 216,
  PROVIDENCE = 217,
  M6502 = 218,
  MAPSCROLLER = 219,
  MARBLING = 220,
  MARCHING = 221,
  MAZE3D = 222,
  MAZE = 223,
  MEMSCROLLER = 224,
  MENGER = 225,
  METABALLS = 226,
  MINIXPM = 227,
  MIRRORBLOB = 228,
  MOEBIUS = 229,
  MOEBIUSGEARS = 230,
  MOIRE2 = 231,
  MOIRE = 232,
  MORPH3D = 233,
  MUNCH = 234,
  NAKAGIN = 235,
  NERVEROT = 236,
  NOOF = 237,
  NORMALS = 238,
  NOSEGUY = 239,
  OVERLAY = 240,
  PACMAN_AI = 241,
  PACMAN = 242,
  PACMAN_LEVEL = 243,
  PAPERCUBE = 244,
  PASSWD = 245,
  PASSWD_KERBEROS = 246,
  PASSWD_PAM = 247,
  PASSWD_PWENT = 248,
  PEDAL = 249,
  PEEPERS = 250,
  PENETRATE = 251,
  PENROSE = 252,
  PULSAR = 253,
  PYRO = 254,
  QIX = 255,
  QUASICRYSTAL = 256,
  QUEENS = 257,
  QUICKHULL = 258,
  RAVERHOOP = 259,
  RAZZLEDAZZLE = 260,
  RDBOMB = 261,
  RECANIM = 262,
  REMOTE = 263,
  RESOURCES = 264,
  RIPPLES = 265,
  ROBOT = 266,
  ROBOT_WIREFRAME = 267,
  ROCKS = 268,
  ROMANBOY = 269,
  RORSCHACH = 270,
  ROTATOR = 271,
  SHIPS = 272,
  SIERPINSKI3D = 273,
  SIERPINSKI = 274,
  SKULL_MODEL = 275,
  SKULLOOP = 276,
  SKYTENTACLES = 277,
  SPHEREEVERSION_ANALYTIC = 278,
  SPHEREEVERSION = 279,
  SPHEREEVERSION_CORRUGATIONS = 280,
  SUBSTRATE = 281,
  SUPERQUADRICS = 282,
  SURFACES = 283,
  SWIM = 284,
  SWIRL = 285,
  T3D = 286,
  TANGRAM = 287,
  TEAPOT = 288,
  TESSELLIMAGE = 289,
  THORNBIRD = 290,
  TIMETUNNEL = 291,
  TIMEZONES = 292,
  TOASTER = 293,
  TOPBLOCK = 294,
  TRACKBALL = 295,
  TRIANGLE = 296,
  TRONBIT = 297,
  TRUCHET = 298,
  TUBE = 299,
  TWANG = 300,
  UNICRUD = 301,
  UNKNOWNPLEASURES = 302,
  VERMICULATE = 303,
  VFEEDBACK = 304,
  VIGILANCE = 305,
  VINES = 306,
  VORONOI = 307,
  WANDER = 308,
  WHALE = 309,
  WHIRLWINDWARP = 310,
  WHIRLYGIG = 311,
  WINDUPROBOT = 312,
  WORM = 313,
  WORMHOLE = 314,
  XANALOGTV = 315,
  XFLAME = 316,
  XJACK = 317,
  XLYAP = 318,
  XMATRIX = 319,
  XRAYSWARM = 320,
  XSPIROGRAPH = 321,
  XSUBLIM = 322,
  ZOOM = 323,
  ABSTRACTILE = 324,
  ANEMONE = 325,
  ANEMOTAXIS = 326,
  ANT = 327,
  APOLLONIAN = 328,
  APPLE2 = 329,
  ATLANTIS = 330,
  ATTRACTION = 331,
  ATUNNEL = 332,
  BARCODE = 333,
  BINARYHORIZON = 334,
  BINARYRING = 335,
  BLASTER = 336,
  BLITSPIN = 337,
  BLOCKTUBE = 338,
  BOUBOULE = 339,
  BOUNCINGCOW = 340,
  MODE_COUNT = 341
};

// Global variables
AnimationMode currentMode = BOXED;
float animationTime = 0;
bool bootPressed = false;
unsigned long lastPress = 0;
bool autoScroll = false;
unsigned long autoScrollTimer = 0;
unsigned long bootHoldStart = 0;
const unsigned long HOLD_TIME = 1000; // 1 second hold to toggle auto-scroll
const unsigned long AUTO_SCROLL_INTERVAL = 30000; // 30 seconds per effect

void setup() {
  Serial.begin(115200);
  Serial.println("Working 3-Effect Screensaver - Full Screen");
  
  // CRITICAL: Initialize backlight FIRST!
  pinMode(BL_PIN, OUTPUT);        
  digitalWrite(BL_PIN, HIGH);     // Turn on backlight!
  
  // Initialize display
  gfx->begin();
  gfx->setRotation(1); // Landscape mode
  gfx->fillScreen(BLACK);
  
  Serial.printf("Display size: %dx%d\n", gfx->width(), gfx->height());
  
  // Initialize touch
  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin();
  ts.setRotation(1);
  
  // Boot button
  pinMode(BOOT_PIN, INPUT_PULLUP);
  
  Serial.printf("Starting with mode %d\n", currentMode);
}

// Function declarations for new effects
void drawGltrackball();
void drawGlutStroke();
void drawGlutSwidth();
void drawGoop();
void drawGrabclient();
void drawGrabXimage();
void drawGrav();
void drawGravitywell();
void drawGreynetic();
void drawHalftone();
void drawHalo();
void drawHandsy();
void drawHandsyModel();
void drawHeadroom();
void drawHeadroomModel();
void drawHelix();
void drawHexadrop();
void drawHexstrut();
void drawHextrail();
void drawHighvoltage();
void drawHighvoltageModel();
void drawHilbert();
void drawHopalong();
void drawHopfanimations();
void drawHopffibration();
void drawHsv();
void drawHydrostat();
void drawHyperball();
void drawHypercube();
void drawHypertorus();
void drawHypnowheel();
void drawIdleProtocol();
void drawIfs();
void drawImsmap();
void drawInteraggregate();
void drawInterference();
void drawIntermomentary();
void drawInvolute();
void drawJigglypuff();
void drawJigsaw();
void drawJuggle();
void drawJuggler3d();
void drawRotor();
void drawRotzoomer();
void drawRubikblocks();
void drawRubik();
void drawSballs();
void drawScooter();
void drawSetuid();
void drawShadebobs();
void drawShark();
void drawSlidescreen();
void drawSlip();
void drawSonar();
void drawSpeedmine();
void drawSphere();
void drawSpheremonics();
void drawSpiral();
void drawSpotlight();
void drawSplitflap();
void drawSplodesic();
void drawSproingies();
void drawSquiral();
void drawSquirtorus();
void drawStairs();
void drawStarfish();
void drawStarwars();
void drawStonerview();
void drawStrange();
void drawSubstrate();
void drawSuperquadrics();
void drawSurfaces();
void drawSwim();
void drawSwirl();
void drawT3d();
void drawTangram();
void drawTeapot();
void drawTessellimage();
void drawThornbird();
void drawTimetunnel();
void drawTimezones();
void drawToaster();
void drawTopblock();
void drawTrackball();
void drawTriangle();
void drawTronbit();
void drawTruchet();
void drawTube();
void drawTwang();
void drawUnicrud();
void drawUnknownpleasures();
void drawVermiculate();
void drawVfeedback();
void drawVigilance();
void drawVines();
void drawVoronoi();
void drawWander();
void drawWhale();
void drawWhirlwindwarp();
void drawWhirlygig();
void drawWinduprobot();
void drawWorm();
void drawWormhole();
void drawXanalogtv();
void drawXflame();
void drawXjack();
void drawXlyap();
void drawXmatrix();
void drawXrayswarm();
void drawXspirograph();
void drawXsublim();
void drawZoom();
void drawAbstractile();
void drawAnemone();
void drawAnemotaxis();
void drawAnt();
void drawApollonian();
void drawApple2();
void drawAtlantis();
void drawAttraction();
void drawAtunnel();
void drawBarcode();
void drawBinaryhorizon();
void drawBinaryring();
void drawBlaster();
void drawBlitspin();
void drawBlocktube();
void drawBouboule();
void drawBouncingcow();
void drawPetri();
void drawPhosphor();
void drawPhotopile();
void drawPiecewise();
void drawPinion();
void drawPipeobjs();
void drawPipes();
void drawPlatonicfolding();
void drawPolyhedra();
void drawPolyhedraGl();
void drawPolyominoes();
void drawPolytopes();
void drawPong();
void drawPopsquares();
void drawPow2();
void drawPrefs();
void drawPrefsw();
void drawProjectiveplane();
void drawProvidence();
void drawM6502();
void drawMapscroller();
void drawMarbling();
void drawMarching();
void drawMaze3d();
void drawMaze();
void drawMemscroller();
void drawMenger();
void drawMetaballs();
void drawMinixpm();
void drawMirrorblob();
void drawMoebius();
void drawMoebiusgears();
void drawMoire2();
void drawMoire();
void drawMorph3d();
void drawMunch();
void drawNakagin();
void drawNerverot();
void drawNoof();
void drawNormals();
void drawNoseguy();
void drawOverlay();
void drawPacmanAI();
void drawPacman();
void drawPacmanLevel();
void drawPapercube();
void drawPasswd();
void drawPasswdKerberos();
void drawPasswdPam();
void drawPasswdPwent();
void drawPedal();
void drawPeepers();
void drawPenetrate();
void drawPenrose();
void drawPulsar();
void drawPyro();
void drawQix();
void drawQuasicrystal();
void drawQueens();
void drawQuickhull();
void drawRaverhoop();
void drawRazzledazzle();
void drawRdbomb();
void drawRecanim();
void drawRemote();
void drawResources();
void drawRipples();
void drawRobot();
void drawRobotWireframe();
void drawRocks();
void drawRomanboy();
void drawRorschach();
void drawRotator();
void drawShips();
void drawSierpinski3d();
void drawSierpinski();
void drawSkullModel();
void drawSkulloop();
void drawSkytentacles();
void drawSphereversionAnalytic();
void drawSphereversion();
void drawSphereversionCorrugations();

void drawBoxed() {
  // Boxed effect - rotating 3D-like boxes
  gfx->fillScreen(BLACK);
  for (int i = 0; i < 8; i++) {
    int x = (int)(sin(animationTime * 2 + i * 0.8) * 120 + gfx->width() / 2);
    int y = (int)(cos(animationTime * 1.5 + i * 0.6) * 100 + gfx->height() / 2);
    int size = (int)(sin(animationTime * 3 + i) * 8 + 15);
    
    float phase = animationTime + i * 0.7;
    int red = (int)(sin(phase) * 127 + 128);
    int green = (int)(cos(phase + 1) * 127 + 128);  
    int blue = (int)(sin(phase + 2) * 127 + 128);
    
    if (x > size && y > size && x < gfx->width()-size && y < gfx->height()-size) {
      gfx->fillRect(x-size, y-size, size*2, size*2, gfx->color565(red, green, blue));
      gfx->drawRect(x-size, y-size, size*2, size*2, WHITE);
    }
  }
}

void drawBoxfit() {
  // Boxfit effect - fitting rectangles
  gfx->fillScreen(BLACK);
  for (int i = 0; i < 29; i++) {
    int x = (int)(sin(animationTime + i * 0.5) * 100 + gfx->width() / 2);
    int y = (int)(cos(animationTime * 1.3 + i * 0.3) * 80 + gfx->height() / 2);
    int size = (int)(sin(animationTime * 2 + i) * 15 + 20);
    
    float phase = animationTime + i * 0.2;
    int r = (int)(sin(phase) * 127 + 128);
    int g = (int)(sin(phase + 2) * 127 + 128);
    int b = (int)(sin(phase + 4) * 127 + 128);
    
    if (x > 0 && y > 0 && x < gfx->width() && y < gfx->height() && size > 0) {
      gfx->fillRect(x, y, size, size, gfx->color565(r, g, b));
    }
  }
}

void drawBraid() {
  // Braid effect - interwoven lines
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 50; j++) {
      float t = animationTime + j * 0.1;
      float angle1 = t + i * PI / 3;
      float angle2 = t + i * PI / 3 + PI;
      
      int radius = 60 + sin(t + i) * 20;
      int x1 = centerX + cos(angle1) * radius;
      int y1 = centerY + sin(angle1) * radius * 0.5;
      int x2 = centerX + cos(angle2) * radius;
      int y2 = centerY + sin(angle2) * radius * 0.5;
      
      uint16_t color = gfx->color565(128 + sin(t + i) * 127, 
                                     128 + cos(t + i + 1) * 127,
                                     128 + sin(t + i + 2) * 127);
      gfx->drawLine(x1, y1, x2, y2, color);
    }
  }
}

void drawBSOD() {
  // Blue Screen of Death effect
  gfx->fillScreen(gfx->color565(0, 0, 170)); // Blue background
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  
  // Fake BSOD text
  gfx->setCursor(10, 20);
  gfx->printf("A problem has been detected");
  gfx->setCursor(10, 40);
  gfx->printf("SYSTEM_THREAD_EXCEPTION");
  gfx->setCursor(10, 60);
  gfx->printf("*** STOP: 0x000000");
  
  // Flickering effect
  if ((int)(animationTime * 10) % 20 < 10) {
    gfx->setCursor(10, 100);
    gfx->printf("Press any key to continue");
  }
  
  // Error bars
  for (int i = 0; i < 5; i++) {
    int y = 140 + i * 20;
    int width = (int)(sin(animationTime + i) * 100 + 150);
    gfx->drawFastHLine(10, y, width, WHITE);
  }
}

void drawBSphere() {
  // B_Sphere effect - bouncing spheres
  gfx->fillScreen(gfx->color565(0, 0, 50)); // Dark blue background
  
  for (int i = 0; i < 12; i++) {
    float t = animationTime + i * 0.3;
    int x = (int)(sin(t * 1.5) * (gfx->width()/2 - 30) + gfx->width()/2);
    int y = (int)(cos(t * 1.2) * (gfx->height()/2 - 30) + gfx->height()/2);
    int radius = (int)(sin(t * 2) * 8 + 12);
    
    // Color based on position and time
    int red = (int)(sin(t) * 127 + 128);
    int green = (int)(cos(t + 1) * 127 + 128);
    int blue = (int)(sin(t + 2) * 127 + 128);
    
    gfx->fillCircle(x, y, radius, gfx->color565(red, green, blue));
    gfx->drawCircle(x, y, radius + 2, WHITE);
  }
}

void drawBubble3D() {
  // Bubble3D effect - 3D floating bubbles
  static float bubbles[20][4]; // x, y, z, size for each bubble
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 20; i++) {
      bubbles[i][0] = random(-100, 100); // x
      bubbles[i][1] = random(-100, 100); // y  
      bubbles[i][2] = random(50, 200);   // z
      bubbles[i][3] = random(5, 15);     // size
    }
    initialized = true;
  }
  
  gfx->fillScreen(gfx->color565(0, 20, 60)); // Deep water background
  
  for (int i = 0; i < 20; i++) {
    // Animate bubbles
    bubbles[i][1] -= 0.5; // Rise up
    bubbles[i][0] += sin(animationTime + i) * 0.3; // Drift
    
    if (bubbles[i][1] < -100) {
      bubbles[i][1] = 100;
      bubbles[i][0] = random(-100, 100);
    }
    
    // Project 3D to 2D
    float perspective = 150.0 / bubbles[i][2];
    int screenX = (int)(bubbles[i][0] * perspective + gfx->width() / 2);
    int screenY = (int)(bubbles[i][1] * perspective + gfx->height() / 2);
    int screenSize = (int)(bubbles[i][3] * perspective);
    
    if (screenX > 0 && screenX < gfx->width() && screenY > 0 && screenY < gfx->height()) {
      // Draw bubble with highlight
      gfx->fillCircle(screenX, screenY, screenSize, gfx->color565(100, 150, 255));
      gfx->fillCircle(screenX - screenSize/3, screenY - screenSize/3, screenSize/3, WHITE);
    }
  }
}

void drawBuildlwo() {
  // Buildlwo effect - 3D object construction
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Draw wireframe-style 3D construction
  for (int i = 0; i < 8; i++) {
    float t = animationTime + i * PI / 4;
    int radius = 60 + sin(animationTime + i) * 20;
    
    // Draw rotating wireframe cube
    for (int j = 0; j < 4; j++) {
      float angle1 = t + j * PI / 2;
      float angle2 = t + (j + 1) * PI / 2;
      
      int x1 = centerX + cos(angle1) * radius;
      int y1 = centerY + sin(angle1) * radius;
      int x2 = centerX + cos(angle2) * radius;
      int y2 = centerY + sin(angle2) * radius;
      
      // Inner and outer connections
      uint16_t color = gfx->color565(128 + sin(t) * 127, 255, 128 + cos(t) * 127);
      gfx->drawLine(x1, y1, x2, y2, color);
      gfx->drawLine(x1, y1, centerX, centerY, color);
    }
  }
}

void drawBumps() {
  // Bumps effect - bump mapping simulation
  gfx->fillScreen(BLACK);
  
  // Create bump map effect with light source
  int lightX = (int)(sin(animationTime * 0.5) * 100 + gfx->width() / 2);
  int lightY = (int)(cos(animationTime * 0.3) * 80 + gfx->height() / 2);
  
  for (int y = 0; y < gfx->height(); y += 4) {
    for (int x = 0; x < gfx->width(); x += 4) {
      // Calculate distance from light source
      float dx = x - lightX;
      float dy = y - lightY;
      float distance = sqrt(dx*dx + dy*dy);
      
      // Height map based on sine waves
      float height = sin(x * 0.1 + animationTime) * sin(y * 0.1 + animationTime * 0.7);
      
      // Lighting calculation
      float intensity = 1.0 / (1.0 + distance * 0.01) * (height + 1.0) * 0.5;
      intensity = constrain(intensity, 0, 1);
      
      int brightness = (int)(intensity * 255);
      uint16_t color = gfx->color565(brightness, brightness/2, brightness);
      gfx->fillRect(x, y, 4, 4, color);
    }
  }
  
  // Draw light source
  gfx->fillCircle(lightX, lightY, 8, YELLOW);
}

void drawCage() {
  // Cage effect - Escher-style impossible cage
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Draw impossible cage structure
  for (int layer = 0; layer < 3; layer++) {
    float layerOffset = layer * PI / 3 + animationTime * 0.2;
    int radius = 40 + layer * 25;
    
    for (int i = 0; i < 6; i++) {
      float angle1 = layerOffset + i * PI / 3;
      float angle2 = layerOffset + (i + 1) * PI / 3;
      
      // Outer hexagon
      int x1 = centerX + cos(angle1) * radius;
      int y1 = centerY + sin(angle1) * radius;
      int x2 = centerX + cos(angle2) * radius;
      int y2 = centerY + sin(angle2) * radius;
      
      uint16_t color = gfx->color565(255 - layer * 50, 200 - layer * 30, 255);
      gfx->drawLine(x1, y1, x2, y2, color);
      
      // Connect to inner layers for impossible effect
      if (layer > 0) {
        int innerRadius = radius - 25;
        int ix = centerX + cos(angle1 + PI) * innerRadius;
        int iy = centerY + sin(angle1 + PI) * innerRadius;
        gfx->drawLine(x1, y1, ix, iy, color);
      }
    }
  }
}

void drawCarousel() {
  // Carousel effect - rotating objects in 3D
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int i = 0; i < 8; i++) {
    float angle = (animationTime + i * PI / 4) * 0.5;
    float z = sin(angle * 2) * 50 + 100; // Z depth
    float scale = 100.0 / z; // Perspective scaling
    
    int x = centerX + cos(angle) * 80 * scale;
    int y = centerY + sin(angle + animationTime) * 20 * scale;
    int size = (int)(15 * scale);
    
    if (size > 2) {
      uint16_t color = gfx->color565(
        (int)(sin(angle + animationTime) * 127 + 128),
        (int)(cos(angle + animationTime + 1) * 127 + 128),
        (int)(sin(angle + animationTime + 2) * 127 + 128)
      );
      gfx->fillRect(x - size/2, y - size/2, size, size, color);
    }
  }
}

void drawCCurve() {
  // C Curve fractal
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  float scale = 60 + sin(animationTime * 0.5) * 20;
  
  // Draw C-curve iterations
  for (int depth = 1; depth <= 6; depth++) {
    int segments = 1 << depth; // 2^depth segments
    
    for (int i = 0; i < segments; i++) {
      float t = (float)i / segments;
      float angle = 0;
      
      // Calculate C-curve angle
      int temp = i;
      for (int j = 0; j < depth; j++) {
        if (temp & 1) angle += PI / 4;
        else angle -= PI / 4;
        temp >>= 1;
      }
      
      angle += animationTime * 0.2;
      
      int x = centerX + cos(angle) * scale * t;
      int y = centerY + sin(angle) * scale * t;
      
      uint16_t color = gfx->color565(255 - depth * 30, 100 + depth * 20, 255);
      gfx->fillCircle(x, y, 2, color);
    }
  }
}

void drawCeltic() {
  // Celtic knot pattern
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Draw interwoven Celtic pattern
  for (int ring = 0; ring < 3; ring++) {
    float radius = 30 + ring * 25;
    int segments = 8 + ring * 4;
    
    for (int i = 0; i < segments; i++) {
      float angle1 = (float)i / segments * 2 * PI + animationTime * (0.1 + ring * 0.05);
      float angle2 = (float)(i + 1) / segments * 2 * PI + animationTime * (0.1 + ring * 0.05);
      
      // Create interwoven effect
      float wave = sin(angle1 * 4 + animationTime) * 10;
      
      int x1 = centerX + cos(angle1) * (radius + wave);
      int y1 = centerY + sin(angle1) * (radius + wave);
      int x2 = centerX + cos(angle2) * (radius + wave);
      int y2 = centerY + sin(angle2) * (radius + wave);
      
      uint16_t color = gfx->color565(
        100 + ring * 50,
        200 - ring * 30,
        150 + ring * 35
      );
      
      gfx->drawLine(x1, y1, x2, y2, color);
      
      // Draw crossing effect
      if (i % 2 == ring % 2) {
        gfx->fillCircle(x1, y1, 3, WHITE);
      }
    }
  }
}

void drawChessModels() {
  // Chess models effect - 3D chess pieces
  gfx->fillScreen(gfx->color565(40, 20, 0)); // Dark brown board
  
  // Draw chess board pattern
  for (int x = 0; x < 8; x++) {
    for (int y = 0; y < 8; y++) {
      int squareX = x * (gfx->width() / 8);
      int squareY = y * (gfx->height() / 8);
      int squareW = gfx->width() / 8;
      int squareH = gfx->height() / 8;
      
      if ((x + y) % 2 == 0) {
        gfx->fillRect(squareX, squareY, squareW, squareH, gfx->color565(200, 180, 120));
      }
    }
  }
  
  // Animate chess pieces
  for (int i = 0; i < 6; i++) {
    float angle = animationTime * 0.3 + i * PI / 3;
    int pieceX = (int)(sin(angle) * 120 + gfx->width() / 2);
    int pieceY = (int)(cos(angle) * 80 + gfx->height() / 2);
    
    // Draw different piece types
    uint16_t color = (i < 3) ? WHITE : BLACK;
    switch (i % 3) {
      case 0: // Pawn
        gfx->fillCircle(pieceX, pieceY, 8, color);
        gfx->fillRect(pieceX - 3, pieceY - 12, 6, 8, color);
        break;
      case 1: // Rook
        gfx->fillRect(pieceX - 8, pieceY - 8, 16, 16, color);
        gfx->fillRect(pieceX - 10, pieceY - 12, 4, 8, color);
        gfx->fillRect(pieceX + 6, pieceY - 12, 4, 8, color);
        break;
      case 2: // King
        gfx->fillCircle(pieceX, pieceY, 10, color);
        gfx->drawLine(pieceX - 6, pieceY - 15, pieceX + 6, pieceY - 15, color);
        gfx->drawLine(pieceX, pieceY - 20, pieceX, pieceY - 10, color);
        break;
    }
  }
}

void drawChompyTower() {
  // Chompy tower effect - stacking animated creatures
  gfx->fillScreen(gfx->color565(20, 40, 80)); // Night sky
  
  int baseY = gfx->height() - 20;
  int towerHeight = 8;
  
  for (int level = 0; level < towerHeight; level++) {
    int y = baseY - level * 25;
    int x = gfx->width() / 2 + sin(animationTime + level * 0.5) * (20 - level * 2);
    
    // Body
    int bodySize = 20 - level * 2;
    uint16_t bodyColor = gfx->color565(
      100 + level * 20, 
      150 - level * 10, 
      200 + level * 5
    );
    gfx->fillCircle(x, y, bodySize, bodyColor);
    
    // Eyes
    int eyeOffset = bodySize / 3;
    gfx->fillCircle(x - eyeOffset, y - eyeOffset/2, 3, WHITE);
    gfx->fillCircle(x + eyeOffset, y - eyeOffset/2, 3, WHITE);
    gfx->fillCircle(x - eyeOffset, y - eyeOffset/2, 1, BLACK);
    gfx->fillCircle(x + eyeOffset, y - eyeOffset/2, 1, BLACK);
    
    // Mouth (chompy!)
    float mouthOpen = sin(animationTime * 8 + level) * 0.5 + 0.5;
    int mouthHeight = (int)(mouthOpen * 8 + 2);
    gfx->fillRect(x - 8, y + 2, 16, mouthHeight, BLACK);
    
    // Teeth
    if (mouthHeight > 4) {
      for (int tooth = 0; tooth < 3; tooth++) {
        int toothX = x - 6 + tooth * 6;
        gfx->fillTriangle(toothX, y + 2, toothX + 2, y + 2, toothX + 1, y + 6, WHITE);
      }
    }
  }
}

void drawCircuit() {
  // Circuit board effect
  gfx->fillScreen(gfx->color565(0, 40, 0)); // Dark green PCB
  
  // Draw circuit traces
  for (int i = 0; i < 20; i++) {
    float traceTime = animationTime * 2 + i * 0.3;
    int startX = (int)(sin(traceTime) * 100 + gfx->width() / 2);
    int startY = (int)(cos(traceTime) * 80 + gfx->height() / 2);
    
    // Horizontal traces
    int traceLength = 40 + sin(traceTime) * 20;
    uint16_t traceColor = gfx->color565(0, 200, 100);
    gfx->drawFastHLine(startX, startY, traceLength, traceColor);
    
    // Vertical connections
    if (i % 3 == 0) {
      gfx->drawFastVLine(startX + traceLength, startY - 10, 20, traceColor);
    }
    
    // Components (resistors, capacitors)
    if (i % 4 == 0) {
      // Resistor
      gfx->fillRect(startX + 10, startY - 3, 15, 6, gfx->color565(139, 69, 19));
      // Color bands
      gfx->drawFastVLine(startX + 13, startY - 3, 6, RED);
      gfx->drawFastVLine(startX + 16, startY - 3, 6, YELLOW);
      gfx->drawFastVLine(startX + 19, startY - 3, 6, BLUE);
    } else if (i % 4 == 1) {
      // Capacitor
      gfx->fillRect(startX + 15, startY - 4, 3, 8, gfx->color565(100, 100, 100));
      gfx->fillRect(startX + 19, startY - 4, 3, 8, gfx->color565(100, 100, 100));
    }
    
    // IC chips
    if (i % 6 == 0) {
      gfx->fillRect(startX + 5, startY - 8, 20, 16, BLACK);
      // Pins
      for (int pin = 0; pin < 4; pin++) {
        gfx->drawFastVLine(startX + 7 + pin * 4, startY - 10, 4, gfx->color565(200, 200, 200));
        gfx->drawFastVLine(startX + 7 + pin * 4, startY + 8, 4, gfx->color565(200, 200, 200));
      }
    }
  }
  
  // Power flowing through traces
  for (int i = 0; i < 5; i++) {
    int pulseX = (int)(animationTime * 100 + i * 60) % gfx->width();
    int pulseY = 40 + i * 20;
    gfx->fillCircle(pulseX, pulseY, 3, CYAN);
  }
}

void drawCityFlow() {
  // City flow effect - traffic and data streams
  gfx->fillScreen(BLACK);
  
  // Draw city grid
  for (int i = 0; i < 8; i++) {
    int x = i * (gfx->width() / 7);
    gfx->drawFastVLine(x, 0, gfx->height(), gfx->color565(40, 40, 40));
  }
  for (int i = 0; i < 6; i++) {
    int y = i * (gfx->height() / 5);
    gfx->drawFastHLine(0, y, gfx->width(), gfx->color565(40, 40, 40));
  }
  
  // Moving traffic lights
  for (int i = 0; i < 15; i++) {
    float flowTime = animationTime * 3 + i * 0.4;
    int x = (int)(flowTime * 20) % gfx->width();
    int y = 20 + (i % 4) * 60;
    
    // Traffic stream
    uint16_t color = (i % 3 == 0) ? RED : (i % 3 == 1) ? GREEN : BLUE;
    gfx->fillRect(x, y, 8, 4, color);
    
    // Data packets
    if (i % 2 == 0) {
      int dataX = (int)(flowTime * 15 + 50) % gfx->width();
      gfx->fillCircle(dataX, y + 20, 2, YELLOW);
    }
  }
}

void drawClientMsg() {
  // Client message system - scrolling text and status
  gfx->fillScreen(gfx->color565(0, 0, 30)); // Dark blue terminal
  
  gfx->setTextColor(GREEN);
  gfx->setTextSize(1);
  
  // Scrolling message system
  int scroll = (int)(animationTime * 30) % 300;
  
  const char* messages[] = {
    "CLIENT> Connection established",
    "SERVER> Authentication required", 
    "CLIENT> Sending credentials...",
    "SERVER> Access granted",
    "CLIENT> Requesting data stream",
    "SERVER> Stream active",
    "CLIENT> Processing packets",
    "SERVER> Transfer complete"
  };
  
  for (int i = 0; i < 8; i++) {
    int y = 10 + i * 15 - scroll;
    if (y > -10 && y < gfx->height()) {
      gfx->setCursor(5, y);
      
      // Blinking cursor effect
      if (i == 7 && (int)(animationTime * 4) % 2 == 0) {
        gfx->printf("%s_", messages[i]);
      } else {
        gfx->printf("%s", messages[i]);
      }
      
      // Status indicators
      if (i % 2 == 0) {
        gfx->fillCircle(gfx->width() - 15, y + 4, 3, (i < 6) ? GREEN : RED);
      }
    }
  }
  
  // Network activity indicator
  int activity = (int)(sin(animationTime * 8) * 50 + 50);
  gfx->drawFastHLine(5, gfx->height() - 10, activity, CYAN);
}

void drawCloudLife() {
  // Cloud life - Conway's Game of Life in the clouds
  static bool grid[40][30];
  static bool newGrid[40][30];
  static bool initialized = false;
  static int generation = 0;
  
  if (!initialized || generation > 100) {
    // Initialize random pattern
    for (int x = 0; x < 40; x++) {
      for (int y = 0; y < 30; y++) {
        grid[x][y] = random(0, 4) == 0; // 25% alive
      }
    }
    initialized = true;
    generation = 0;
  }
  
  // Game of Life rules (every 10 frames)
  if ((int)(animationTime * 10) % 10 == 0) {
    for (int x = 0; x < 40; x++) {
      for (int y = 0; y < 30; y++) {
        int neighbors = 0;
        
        // Count neighbors
        for (int dx = -1; dx <= 1; dx++) {
          for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            int nx = (x + dx + 40) % 40;
            int ny = (y + dy + 30) % 30;
            if (grid[nx][ny]) neighbors++;
          }
        }
        
        // Apply rules
        if (grid[x][y]) {
          newGrid[x][y] = (neighbors == 2 || neighbors == 3);
        } else {
          newGrid[x][y] = (neighbors == 3);
        }
      }
    }
    
    // Copy back
    for (int x = 0; x < 40; x++) {
      for (int y = 0; y < 30; y++) {
        grid[x][y] = newGrid[x][y];
      }
    }
    generation++;
  }
  
  // Draw cloud background
  gfx->fillScreen(gfx->color565(135, 206, 235)); // Sky blue
  
  // Draw living cells as cloud particles
  for (int x = 0; x < 40; x++) {
    for (int y = 0; y < 30; y++) {
      if (grid[x][y]) {
        int screenX = x * (gfx->width() / 40);
        int screenY = y * (gfx->height() / 30);
        
        // Cloud-like appearance
        int size = 3 + sin(animationTime + x + y) * 2;
        uint16_t cloudColor = gfx->color565(255, 255, 255 - (x + y) % 50);
        gfx->fillCircle(screenX, screenY, size, cloudColor);
      }
    }
  }
}

void drawColorBars() {
  // Color bars effect - TV test pattern
  gfx->fillScreen(BLACK);
  
  int barWidth = gfx->width() / 8;
  uint16_t colors[] = {
    WHITE, YELLOW, CYAN, GREEN, MAGENTA, RED, BLUE, BLACK
  };
  
  // Main color bars
  for (int i = 0; i < 8; i++) {
    gfx->fillRect(i * barWidth, 0, barWidth, gfx->height() * 2 / 3, colors[i]);
  }
  
  // Bottom test pattern
  int bottomY = gfx->height() * 2 / 3;
  int bottomHeight = gfx->height() / 3;
  
  // Pluge pattern
  for (int i = 0; i < 4; i++) {
    int x = i * (gfx->width() / 4);
    int brightness = i * 85; // 0, 85, 170, 255
    gfx->fillRect(x, bottomY, gfx->width() / 4, bottomHeight, 
                  gfx->color565(brightness, brightness, brightness));
  }
  
  // Animated sweep
  int sweepX = (int)(sin(animationTime) * gfx->width() / 2 + gfx->width() / 2);
  gfx->drawFastVLine(sweepX, 0, gfx->height(), WHITE);
}

void drawColors() {
  // Colors effect - cycling through color space
  gfx->fillScreen(BLACK);
  
  // HSV color wheel effect
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  int radius = min(gfx->width(), gfx->height()) / 3;
  
  for (int angle = 0; angle < 360; angle += 5) {
    float rad = (angle + animationTime * 50) * PI / 180.0;
    int x1 = centerX;
    int y1 = centerY;
    int x2 = centerX + cos(rad) * radius;
    int y2 = centerY + sin(rad) * radius;
    
    // Convert HSV to RGB approximation
    float hue = (angle + animationTime * 100) / 360.0;
    while (hue > 1.0) hue -= 1.0;
    
    int r, g, b;
    if (hue < 0.167) {
      r = 255; g = hue * 6 * 255; b = 0;
    } else if (hue < 0.333) {
      r = (0.333 - hue) * 6 * 255; g = 255; b = 0;
    } else if (hue < 0.5) {
      r = 0; g = 255; b = (hue - 0.333) * 6 * 255;
    } else if (hue < 0.667) {
      r = 0; g = (0.667 - hue) * 6 * 255; b = 255;
    } else if (hue < 0.833) {
      r = (hue - 0.667) * 6 * 255; g = 0; b = 255;
    } else {
      r = 255; g = 0; b = (1.0 - hue) * 6 * 255;
    }
    
    gfx->drawLine(x1, y1, x2, y2, gfx->color565(r, g, b));
  }
  
  // Color gradient bands
  for (int y = 0; y < gfx->height(); y += 4) {
    int colorShift = (int)(animationTime * 50 + y) % 768;
    int r = 0, g = 0, b = 0;
    
    if (colorShift < 256) {
      r = colorShift; g = 255 - colorShift; b = 0;
    } else if (colorShift < 512) {
      r = 0; g = colorShift - 256; b = 255 - (colorShift - 256);
    } else {
      r = colorShift - 512; g = 0; b = 255 - (colorShift - 512);
    }
    
    gfx->drawFastHLine(0, y, gfx->width(), gfx->color565(r, g, b));
  }
}

void drawCompanion() {
  // Companion effect - following companion cube/creature
  gfx->fillScreen(gfx->color565(20, 20, 40)); // Dark space
  
  // Main companion
  int companionX = (int)(sin(animationTime * 0.8) * 100 + gfx->width() / 2);
  int companionY = (int)(cos(animationTime * 0.6) * 60 + gfx->height() / 2);
  
  // Draw companion body
  int bodySize = 20 + sin(animationTime * 3) * 3;
  gfx->fillRect(companionX - bodySize/2, companionY - bodySize/2, 
                bodySize, bodySize, gfx->color565(200, 200, 200));
  
  // Companion markings (heart symbol)
  uint16_t pink = gfx->color565(255, 192, 203);
  gfx->fillCircle(companionX - 4, companionY - 2, 3, pink);
  gfx->fillCircle(companionX + 4, companionY - 2, 3, pink);
  gfx->fillTriangle(companionX - 7, companionY + 1, 
                    companionX + 7, companionY + 1,
                    companionX, companionY + 8, pink);
  
  // Eyes
  gfx->fillCircle(companionX - 5, companionY - 5, 2, BLUE);
  gfx->fillCircle(companionX + 5, companionY - 5, 2, BLUE);
  
  // Following trail
  for (int i = 0; i < 10; i++) {
    float trailTime = animationTime - i * 0.1;
    int trailX = (int)(sin(trailTime * 0.8) * 100 + gfx->width() / 2);
    int trailY = (int)(cos(trailTime * 0.6) * 60 + gfx->height() / 2);
    
    int trailSize = (10 - i) * 2;
    uint16_t trailColor = gfx->color565(100 - i * 8, 100 - i * 8, 150);
    gfx->fillCircle(trailX, trailY, trailSize, trailColor);
  }
  
  // Floating particles around companion
  for (int i = 0; i < 8; i++) {
    float particleAngle = animationTime * 2 + i * PI / 4;
    int particleRadius = 35 + sin(animationTime * 4 + i) * 10;
    int particleX = companionX + cos(particleAngle) * particleRadius;
    int particleY = companionY + sin(particleAngle) * particleRadius;
    
    gfx->fillCircle(particleX, particleY, 2, YELLOW);
  }
}

void drawCompanionDisc() {
  // Companion disc - spinning disc with companion markings
  gfx->fillScreen(gfx->color565(30, 30, 50));
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  int discRadius = 60 + sin(animationTime * 2) * 10;
  
  // Main disc
  gfx->fillCircle(centerX, centerY, discRadius, gfx->color565(180, 180, 180));
  gfx->drawCircle(centerX, centerY, discRadius, WHITE);
  
  // Spinning segments
  for (int i = 0; i < 8; i++) {
    float angle = animationTime * 3 + i * PI / 4;
    int x1 = centerX + cos(angle) * discRadius;
    int y1 = centerY + sin(angle) * discRadius;
    int x2 = centerX + cos(angle) * (discRadius - 20);
    int y2 = centerY + sin(angle) * (discRadius - 20);
    
    uint16_t segmentColor = (i % 2 == 0) ? RED : BLUE;
    gfx->drawLine(centerX, centerY, x1, y1, segmentColor);
    gfx->fillCircle(x2, y2, 5, segmentColor);
  }
  
  // Center companion symbol
  uint16_t pink = gfx->color565(255, 192, 203);
  gfx->fillCircle(centerX - 8, centerY - 5, 6, pink);
  gfx->fillCircle(centerX + 8, centerY - 5, 6, pink);
  gfx->fillTriangle(centerX - 12, centerY + 2, 
                    centerX + 12, centerY + 2,
                    centerX, centerY + 15, pink);
  
  // Orbital particles
  for (int i = 0; i < 6; i++) {
    float orbitAngle = animationTime * 1.5 + i * PI / 3;
    int orbitRadius = discRadius + 20 + sin(animationTime * 4 + i) * 8;
    int orbX = centerX + cos(orbitAngle) * orbitRadius;
    int orbY = centerY + sin(orbitAngle) * orbitRadius;
    gfx->fillCircle(orbX, orbY, 3, YELLOW);
  }
}

void drawCompanionHeart() {
  // Companion heart - pulsing heart with companion features
  gfx->fillScreen(gfx->color565(40, 20, 40));
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Pulsing heart effect
  float pulse = sin(animationTime * 4) * 0.3 + 0.7;
  int heartSize = (int)(30 * pulse);
  uint16_t heartColor = gfx->color565((int)(255 * pulse), 100, 150);
  
  // Draw large heart shape
  gfx->fillCircle(centerX - heartSize/2, centerY - heartSize/4, heartSize/2, heartColor);
  gfx->fillCircle(centerX + heartSize/2, centerY - heartSize/4, heartSize/2, heartColor);
  gfx->fillTriangle(centerX - heartSize, centerY, 
                    centerX + heartSize, centerY,
                    centerX, centerY + heartSize, heartColor);
  
  // Companion eyes on the heart
  gfx->fillCircle(centerX - heartSize/3, centerY - heartSize/6, 4, BLUE);
  gfx->fillCircle(centerX + heartSize/3, centerY - heartSize/6, 4, BLUE);
  gfx->fillCircle(centerX - heartSize/3, centerY - heartSize/6, 2, WHITE);
  gfx->fillCircle(centerX + heartSize/3, centerY - heartSize/6, 2, WHITE);
  
  // Heart particles emanating outward
  for (int i = 0; i < 12; i++) {
    float particleAngle = i * PI / 6 + animationTime * 2;
    float distance = 80 + sin(animationTime * 3 + i) * 20;
    int partX = centerX + cos(particleAngle) * distance;
    int partY = centerY + sin(particleAngle) * distance;
    
    // Small hearts
    int smallSize = 4 + sin(animationTime * 5 + i) * 2;
    uint16_t pink = gfx->color565(255, 150 + i * 8, 200);
    gfx->fillCircle(partX - smallSize/2, partY, smallSize/2, pink);
    gfx->fillCircle(partX + smallSize/2, partY, smallSize/2, pink);
    gfx->fillTriangle(partX - smallSize, partY + smallSize/2,
                      partX + smallSize, partY + smallSize/2,
                      partX, partY + smallSize, pink);
  }
}

void drawCompanionQuad() {
  // Companion quad - four companion cubes in formation
  gfx->fillScreen(gfx->color565(25, 25, 35));
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Four companion positions in a square formation
  int positions[4][2] = {
    {centerX - 40, centerY - 40}, // Top left
    {centerX + 40, centerY - 40}, // Top right  
    {centerX - 40, centerY + 40}, // Bottom left
    {centerX + 40, centerY + 40}  // Bottom right
  };
  
  for (int i = 0; i < 4; i++) {
    float offset = animationTime * 2 + i * PI / 2;
    int compX = positions[i][0] + sin(offset) * 15;
    int compY = positions[i][1] + cos(offset * 0.7) * 10;
    
    // Companion body
    int size = 18 + sin(animationTime * 3 + i) * 3;
    gfx->fillRect(compX - size/2, compY - size/2, size, size, 
                  gfx->color565(200 - i * 20, 200 - i * 20, 200 - i * 20));
    
    // Different colored heart for each companion
    uint16_t heartColors[4] = {
      gfx->color565(255, 100, 150), // Pink
      gfx->color565(100, 255, 150), // Green
      gfx->color565(150, 100, 255), // Purple
      gfx->color565(255, 200, 100)  // Orange
    };
    
    // Heart symbol
    gfx->fillCircle(compX - 4, compY - 2, 3, heartColors[i]);
    gfx->fillCircle(compX + 4, compY - 2, 3, heartColors[i]);
    gfx->fillTriangle(compX - 6, compY + 1,
                      compX + 6, compY + 1,
                      compX, compY + 7, heartColors[i]);
    
    // Eyes
    gfx->fillCircle(compX - 5, compY - 6, 2, CYAN);
    gfx->fillCircle(compX + 5, compY - 6, 2, CYAN);
  }
  
  // Connection lines between companions
  for (int i = 0; i < 4; i++) {
    int next = (i + 1) % 4;
    float alpha = sin(animationTime * 4 + i) * 0.5 + 0.5;
    if (alpha > 0.3) {
      gfx->drawLine(positions[i][0], positions[i][1],
                    positions[next][0], positions[next][1], 
                    gfx->color565((int)(100 * alpha), (int)(200 * alpha), 255));
    }
  }
  
  // Center energy core
  int coreSize = (int)(8 + sin(animationTime * 6) * 4);
  gfx->fillCircle(centerX, centerY, coreSize, WHITE);
}

void drawCompass() {
  // Compass effect - navigational compass with moving needle
  gfx->fillScreen(gfx->color565(40, 60, 80)); // Dark blue background
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  int compassRadius = 70;
  
  // Compass outer ring
  gfx->drawCircle(centerX, centerY, compassRadius, WHITE);
  gfx->drawCircle(centerX, centerY, compassRadius - 2, WHITE);
  
  // Cardinal directions
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(centerX - 3, centerY - compassRadius + 15);
  gfx->printf("N");
  gfx->setCursor(centerX + compassRadius - 15, centerY - 3);
  gfx->printf("E");
  gfx->setCursor(centerX - 3, centerY + compassRadius - 25);
  gfx->printf("S");
  gfx->setCursor(centerX - compassRadius + 8, centerY - 3);
  gfx->printf("W");
  
  // Degree markings
  for (int i = 0; i < 36; i++) {
    float angle = i * PI / 18;
    int x1 = centerX + cos(angle) * (compassRadius - 5);
    int y1 = centerY + sin(angle) * (compassRadius - 5);
    int x2 = centerX + cos(angle) * (compassRadius - 10);
    int y2 = centerY + sin(angle) * (compassRadius - 10);
    
    if (i % 9 == 0) {
      gfx->drawLine(x1, y1, x2 - 5, y2 - 5, WHITE);
    } else {
      gfx->drawLine(x1, y1, x2, y2, gfx->color565(150, 150, 150));
    }
  }
  
  // Rotating needle
  float needleAngle = animationTime * 0.5 + sin(animationTime * 0.3) * 2;
  int needleLength = compassRadius - 20;
  
  // North needle (red)
  int nX = centerX + cos(needleAngle - PI/2) * needleLength;
  int nY = centerY + sin(needleAngle - PI/2) * needleLength;
  gfx->drawLine(centerX, centerY, nX, nY, RED);
  gfx->fillCircle(nX, nY, 3, RED);
  
  // South needle (white)
  int sX = centerX + cos(needleAngle + PI/2) * needleLength;
  int sY = centerY + sin(needleAngle + PI/2) * needleLength;
  gfx->drawLine(centerX, centerY, sX, sY, WHITE);
  gfx->fillCircle(sX, sY, 3, WHITE);
  
  // Center pivot
  gfx->fillCircle(centerX, centerY, 5, YELLOW);
}

void drawCoral() {
  // Coral effect - growing coral reef
  gfx->fillScreen(gfx->color565(0, 20, 60)); // Deep ocean blue
  
  static bool coralGrid[40][30];
  static bool initialized = false;
  static int generation = 0;
  
  if (!initialized || generation > 150) {
    // Initialize coral seeds
    for (int x = 0; x < 40; x++) {
      for (int y = 0; y < 30; y++) {
        coralGrid[x][y] = false;
      }
    }
    // Plant initial coral seeds at bottom
    for (int x = 5; x < 35; x++) {
      if (random(0, 3) == 0) {
        coralGrid[x][28] = true;
      }
    }
    initialized = true;
    generation = 0;
  }
  
  // Coral growth simulation (every 20 frames)
  if ((int)(animationTime * 10) % 20 == 0) {
    bool newGrid[40][30];
    for (int x = 0; x < 40; x++) {
      for (int y = 0; y < 30; y++) {
        newGrid[x][y] = coralGrid[x][y];
      }
    }
    
    for (int x = 1; x < 39; x++) {
      for (int y = 1; y < 29; y++) {
        if (!coralGrid[x][y]) {
          // Check for adjacent coral (growth)
          int neighbors = 0;
          for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
              if (dx == 0 && dy == 0) continue;
              if (coralGrid[x + dx][y + dy]) neighbors++;
            }
          }
          // Grow if 1-3 neighbors, prefer upward growth
          if (neighbors >= 1 && neighbors <= 3) {
            if (coralGrid[x][y + 1] && random(0, 4) == 0) {
              newGrid[x][y] = true;
            }
          }
        }
      }
    }
    
    for (int x = 0; x < 40; x++) {
      for (int y = 0; y < 30; y++) {
        coralGrid[x][y] = newGrid[x][y];
      }
    }
    generation++;
  }
  
  // Draw coral
  for (int x = 0; x < 40; x++) {
    for (int y = 0; y < 30; y++) {
      if (coralGrid[x][y]) {
        int screenX = x * (gfx->width() / 40);
        int screenY = y * (gfx->height() / 30);
        
        // Coral colors based on depth and position
        int depth = 30 - y;
        uint16_t coralColor = gfx->color565(
          255 - depth * 4,
          100 + depth * 2 + (x % 5) * 10,
          150 + sin(animationTime + x + y) * 30
        );
        
        gfx->fillRect(screenX, screenY, 
                      gfx->width() / 40 + 1, gfx->height() / 30 + 1, 
                      coralColor);
      }
    }
  }
  
  // Swimming fish
  for (int i = 0; i < 5; i++) {
    float fishTime = animationTime * 2 + i * 1.2;
    int fishX = (int)(sin(fishTime) * 120 + gfx->width() / 2);
    int fishY = 30 + i * 40 + sin(fishTime * 2) * 15;
    
    uint16_t fishColor = gfx->color565(255, 200 - i * 30, 100 + i * 20);
    gfx->fillCircle(fishX, fishY, 4, fishColor);
    gfx->fillTriangle(fishX - 6, fishY, fishX - 10, fishY - 3, fishX - 10, fishY + 3, fishColor);
  }
}

void drawCovid19() {
  // COVID-19 virus simulation
  gfx->fillScreen(gfx->color565(10, 10, 20)); // Dark background
  
  static int viruses[15][4]; // x, y, vx, vy
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 15; i++) {
      viruses[i][0] = random(20, gfx->width() - 20);
      viruses[i][1] = random(20, gfx->height() - 20);
      viruses[i][2] = random(-2, 3);
      viruses[i][3] = random(-2, 3);
    }
    initialized = true;
  }
  
  // Update virus positions
  for (int i = 0; i < 15; i++) {
    viruses[i][0] += viruses[i][2];
    viruses[i][1] += viruses[i][3];
    
    // Bounce off walls
    if (viruses[i][0] < 15 || viruses[i][0] > gfx->width() - 15) {
      viruses[i][2] = -viruses[i][2];
    }
    if (viruses[i][1] < 15 || viruses[i][1] > gfx->height() - 15) {
      viruses[i][3] = -viruses[i][3];
    }
    
    // Keep in bounds
    viruses[i][0] = constrain(viruses[i][0], 15, gfx->width() - 15);
    viruses[i][1] = constrain(viruses[i][1], 15, gfx->height() - 15);
  }
  
  // Draw viruses
  for (int i = 0; i < 15; i++) {
    int vX = viruses[i][0];
    int vY = viruses[i][1];
    
    // Virus core
    int coreSize = 8 + sin(animationTime * 3 + i) * 2;
    gfx->fillCircle(vX, vY, coreSize, gfx->color565(200, 50, 50));
    
    // Spike proteins
    for (int spike = 0; spike < 12; spike++) {
      float spikeAngle = spike * PI / 6 + animationTime + i * 0.5;
      int spikeLength = coreSize + 4 + sin(animationTime * 4 + spike + i) * 2;
      
      int sX = vX + cos(spikeAngle) * spikeLength;
      int sY = vY + sin(spikeAngle) * spikeLength;
      
      // Spike protein (red)
      gfx->fillCircle(sX, sY, 2, RED);
      gfx->drawLine(vX, vY, sX, sY, gfx->color565(150, 30, 30));
    }
    
    // RNA strands inside
    for (int rna = 0; rna < 3; rna++) {
      float rnaAngle = animationTime * 2 + rna * PI * 2 / 3;
      int rnaX = vX + cos(rnaAngle) * (coreSize - 3);
      int rnaY = vY + sin(rnaAngle) * (coreSize - 3);
      gfx->fillCircle(rnaX, rnaY, 1, YELLOW);
    }
  }
  
  // Transmission particles
  for (int i = 0; i < 8; i++) {
    float pTime = animationTime * 4 + i * 0.8;
    int pX = (int)(sin(pTime) * 80 + gfx->width() / 2);
    int pY = (int)(cos(pTime * 1.3) * 60 + gfx->height() / 2);
    
    uint16_t particleColor = gfx->color565(100, 100, 255 - i * 20);
    gfx->fillCircle(pX, pY, 1 + i % 3, particleColor);
  }
}

void drawCowFace() {
  // Cow face effect - animated cow face
  gfx->fillScreen(gfx->color565(135, 206, 235)); // Sky blue background
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Head
  int headSize = 60 + sin(animationTime * 2) * 5;
  gfx->fillCircle(centerX, centerY, headSize, WHITE);
  gfx->drawCircle(centerX, centerY, headSize, BLACK);
  
  // Cow spots on head
  for (int i = 0; i < 6; i++) {
    float spotAngle = i * PI / 3 + animationTime * 0.3;
    int spotX = centerX + cos(spotAngle) * (headSize - 20);
    int spotY = centerY + sin(spotAngle) * (headSize - 25);
    int spotSize = 8 + sin(animationTime * 4 + i) * 3;
    gfx->fillCircle(spotX, spotY, spotSize, BLACK);
  }
  
  // Eyes
  int eyeOffset = 18;
  int eyeY = centerY - 15;
  
  // Eye whites
  gfx->fillCircle(centerX - eyeOffset, eyeY, 12, WHITE);
  gfx->fillCircle(centerX + eyeOffset, eyeY, 12, WHITE);
  gfx->drawCircle(centerX - eyeOffset, eyeY, 12, BLACK);
  gfx->drawCircle(centerX + eyeOffset, eyeY, 12, BLACK);
  
  // Pupils (follow a pattern)
  int pupilOffsetX = (int)(sin(animationTime) * 4);
  int pupilOffsetY = (int)(cos(animationTime * 1.3) * 3);
  gfx->fillCircle(centerX - eyeOffset + pupilOffsetX, eyeY + pupilOffsetY, 6, BLACK);
  gfx->fillCircle(centerX + eyeOffset + pupilOffsetX, eyeY + pupilOffsetY, 6, BLACK);
  
  // Eye shine
  gfx->fillCircle(centerX - eyeOffset + pupilOffsetX - 2, eyeY + pupilOffsetY - 2, 2, WHITE);
  gfx->fillCircle(centerX + eyeOffset + pupilOffsetX - 2, eyeY + pupilOffsetY - 2, 2, WHITE);
  
  // Snout
  int snoutY = centerY + 10;
  gfx->fillRect(centerX - 20, snoutY, 40, 25, gfx->color565(255, 192, 203)); // Pink
  gfx->drawRect(centerX - 20, snoutY, 40, 25, BLACK);
  
  // Nostrils
  gfx->fillCircle(centerX - 8, snoutY + 12, 3, BLACK);
  gfx->fillCircle(centerX + 8, snoutY + 12, 3, BLACK);
  
  // Mouth
  gfx->drawLine(centerX, snoutY + 25, centerX - 15, snoutY + 35, BLACK);
  gfx->drawLine(centerX, snoutY + 25, centerX + 15, snoutY + 35, BLACK);
  
  // Ears
  gfx->fillCircle(centerX - 45, centerY - 35, 15, WHITE);
  gfx->fillCircle(centerX + 45, centerY - 35, 15, WHITE);
  gfx->drawCircle(centerX - 45, centerY - 35, 15, BLACK);
  gfx->drawCircle(centerX + 45, centerY - 35, 15, BLACK);
  
  // Inner ears
  gfx->fillCircle(centerX - 45, centerY - 35, 8, gfx->color565(255, 192, 203));
  gfx->fillCircle(centerX + 45, centerY - 35, 8, gfx->color565(255, 192, 203));
}

void drawCowHide() {
  // Cow hide pattern effect
  gfx->fillScreen(WHITE);
  
  static bool spots[50][4]; // x, y, size, initialized
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 50; i++) {
      spots[i][0] = random(0, gfx->width());
      spots[i][1] = random(0, gfx->height());
      spots[i][2] = random(10, 30);
      spots[i][3] = 1;
    }
    initialized = true;
  }
  
  // Draw cow spots
  for (int i = 0; i < 50; i++) {
    int x = spots[i][0];
    int y = spots[i][1];
    int baseSize = spots[i][2];
    
    // Animated size
    int size = baseSize + sin(animationTime * 2 + i * 0.3) * 5;
    
    // Main spot
    gfx->fillCircle(x, y, size, BLACK);
    
    // Irregular cow spot shape
    for (int j = 0; j < 6; j++) {
      float angle = j * PI / 3 + animationTime * 0.1;
      int blobX = x + cos(angle) * (size * 0.7);
      int blobY = y + sin(angle) * (size * 0.5);
      int blobSize = size / 3 + sin(animationTime * 3 + i + j) * 3;
      gfx->fillCircle(blobX, blobY, blobSize, BLACK);
    }
  }
  
  // Moving spots
  for (int i = 0; i < 10; i++) {
    float moveTime = animationTime * 0.5 + i * 0.8;
    int moveX = (int)(sin(moveTime) * 100 + gfx->width() / 2);
    int moveY = (int)(cos(moveTime * 1.3) * 80 + gfx->height() / 2);
    int moveSize = 15 + sin(animationTime * 4 + i) * 8;
    
    gfx->fillCircle(moveX, moveY, moveSize, gfx->color565(50, 50, 50));
  }
}

void drawCowHoofs() {
  // Cow hoofs walking effect
  gfx->fillScreen(gfx->color565(34, 139, 34)); // Forest green ground
  
  // Draw ground line
  int groundY = gfx->height() - 40;
  gfx->drawFastHLine(0, groundY, gfx->width(), gfx->color565(139, 69, 19));
  
  // Four cow legs walking
  for (int leg = 0; leg < 4; leg++) {
    float legPhase = animationTime * 3 + leg * PI / 2;
    
    // Leg position - walking cycle
    int legX = 60 + leg * 60;
    int legBaseY = groundY - 10;
    
    // Walking motion - lift legs alternately
    int legLift = 0;
    if (leg % 2 == 0) {
      legLift = (int)(sin(legPhase) * 15);
      if (legLift < 0) legLift = 0;
    } else {
      legLift = (int)(sin(legPhase + PI) * 15);
      if (legLift < 0) legLift = 0;
    }
    
    int legY = legBaseY - legLift;
    
    // Draw leg
    gfx->drawLine(legX, legY - 40, legX, legY, BLACK);
    gfx->drawLine(legX - 2, legY - 40, legX - 2, legY, BLACK);
    gfx->drawLine(legX + 2, legY - 40, legX + 2, legY, BLACK);
    
    // Draw hoof
    gfx->fillRect(legX - 8, legY, 16, 12, BLACK);
    gfx->drawRect(legX - 8, legY, 16, 12, gfx->color565(64, 64, 64));
    
    // Hoof split
    gfx->drawFastVLine(legX, legY, 12, gfx->color565(32, 32, 32));
    
    // Dust clouds when hoof hits ground
    if (legLift < 3) {
      for (int dust = 0; dust < 5; dust++) {
        int dustX = legX - 15 + dust * 6 + random(-3, 4);
        int dustY = legY + 10 + random(-2, 5);
        int dustSize = 2 + random(0, 3);
        gfx->fillCircle(dustX, dustY, dustSize, gfx->color565(160, 160, 120));
      }
    }
  }
  
  // Cow body silhouette above
  int bodyY = groundY - 80;
  gfx->fillRect(40, bodyY, 200, 40, WHITE);
  gfx->drawRect(40, bodyY, 200, 40, BLACK);
  
  // Cow spots on body
  for (int i = 0; i < 8; i++) {
    int spotX = 50 + i * 22 + sin(animationTime + i) * 5;
    int spotY = bodyY + 5 + sin(animationTime * 2 + i) * 8;
    int spotSize = 6 + sin(animationTime * 3 + i) * 3;
    gfx->fillCircle(spotX, spotY, spotSize, BLACK);
  }
  
  // Tail swishing
  float tailAngle = sin(animationTime * 4) * 0.5;
  int tailX = 240 + cos(tailAngle) * 20;
  int tailY = bodyY + 20 + sin(tailAngle) * 10;
  gfx->drawLine(240, bodyY + 20, tailX, tailY, BLACK);
  gfx->fillCircle(tailX, tailY, 4, BLACK); // Tail tuft
}

void drawCowHorns() {
  // Cow horns effect - bull with dramatic horns
  gfx->fillScreen(gfx->color565(160, 82, 45)); // Brown background
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Bull head
  int headSize = 55;
  gfx->fillCircle(centerX, centerY, headSize, gfx->color565(139, 69, 19));
  gfx->drawCircle(centerX, centerY, headSize, BLACK);
  
  // Horns - animated length and angle
  float hornAngle = sin(animationTime * 0.5) * 0.3;
  int hornLength = 40 + sin(animationTime * 2) * 8;
  
  // Left horn
  float leftAngle = -2.0 + hornAngle;
  int horn1X1 = centerX - 25;
  int horn1Y1 = centerY - 35;
  int horn1X2 = horn1X1 + cos(leftAngle) * hornLength;
  int horn1Y2 = horn1Y1 + sin(leftAngle) * hornLength;
  
  // Draw horn with thickness
  for (int thickness = 0; thickness < 5; thickness++) {
    gfx->drawLine(horn1X1 + thickness - 2, horn1Y1, 
                  horn1X2 + thickness - 2, horn1Y2, 
                  gfx->color565(240, 240, 200)); // Bone white
  }
  gfx->drawLine(horn1X1, horn1Y1, horn1X2, horn1Y2, BLACK); // Outline
  
  // Right horn
  float rightAngle = -1.14 - hornAngle;
  int horn2X1 = centerX + 25;
  int horn2Y1 = centerY - 35;
  int horn2X2 = horn2X1 + cos(rightAngle) * hornLength;
  int horn2Y2 = horn2Y1 + sin(rightAngle) * hornLength;
  
  // Draw horn with thickness
  for (int thickness = 0; thickness < 5; thickness++) {
    gfx->drawLine(horn2X1 + thickness - 2, horn2Y1, 
                  horn2X2 + thickness - 2, horn2Y2, 
                  gfx->color565(240, 240, 200));
  }
  gfx->drawLine(horn2X1, horn2Y1, horn2X2, horn2Y2, BLACK);
  
  // Horn tips (sharp points)
  gfx->fillCircle(horn1X2, horn1Y2, 3, BLACK);
  gfx->fillCircle(horn2X2, horn2Y2, 3, BLACK);
  
  // Angry eyes
  int eyeY = centerY - 10;
  gfx->fillCircle(centerX - 15, eyeY, 8, RED);
  gfx->fillCircle(centerX + 15, eyeY, 8, RED);
  gfx->fillCircle(centerX - 15, eyeY, 4, BLACK);
  gfx->fillCircle(centerX + 15, eyeY, 4, BLACK);
  
  // Angry eyebrows
  gfx->drawLine(centerX - 25, eyeY - 15, centerX - 8, eyeY - 8, BLACK);
  gfx->drawLine(centerX + 8, eyeY - 8, centerX + 25, eyeY - 15, BLACK);
  
  // Snorting nostrils
  gfx->fillRect(centerX - 20, centerY + 15, 40, 20, BLACK);
  gfx->fillCircle(centerX - 8, centerY + 25, 4, BLACK);
  gfx->fillCircle(centerX + 8, centerY + 25, 4, BLACK);
  
  // Steam from nostrils
  for (int i = 0; i < 6; i++) {
    float steamTime = animationTime * 4 + i * 0.5;
    int steamX = centerX + (i < 3 ? -8 : 8) + sin(steamTime) * 3;
    int steamY = centerY + 25 - i * 8;
    int steamSize = 3 - i / 2;
    if (steamSize > 0) {
      gfx->fillCircle(steamX, steamY, steamSize, gfx->color565(200, 200, 200));
    }
  }
  
  // Horn rings (age indicators)
  for (int ring = 0; ring < 3; ring++) {
    int ringPos = hornLength * (0.3 + ring * 0.2);
    int ring1X = horn1X1 + cos(leftAngle) * ringPos;
    int ring1Y = horn1Y1 + sin(leftAngle) * ringPos;
    int ring2X = horn2X1 + cos(rightAngle) * ringPos;
    int ring2Y = horn2Y1 + sin(rightAngle) * ringPos;
    
    gfx->drawCircle(ring1X, ring1Y, 2, gfx->color565(139, 69, 19));
    gfx->drawCircle(ring2X, ring2Y, 2, gfx->color565(139, 69, 19));
  }
}

void drawCowTail() {
  // Cow tail effect - swishing and swatting flies
  gfx->fillScreen(gfx->color565(34, 139, 34)); // Green pasture
  
  // Cow body (rear view)
  int bodyX = gfx->width() / 2;
  int bodyY = gfx->height() / 2;
  
  gfx->fillRect(bodyX - 60, bodyY - 40, 120, 80, WHITE);
  gfx->drawRect(bodyX - 60, bodyY - 40, 120, 80, BLACK);
  
  // Cow spots
  for (int i = 0; i < 10; i++) {
    int spotX = bodyX - 50 + (i % 4) * 25 + sin(animationTime + i) * 5;
    int spotY = bodyY - 30 + (i / 4) * 20;
    int spotSize = 8 + sin(animationTime * 2 + i) * 3;
    gfx->fillCircle(spotX, spotY, spotSize, BLACK);
  }
  
  // Tail base
  int tailBaseX = bodyX + 60;
  int tailBaseY = bodyY;
  
  // Animated tail swishing
  float tailSwing = sin(animationTime * 3) * 1.2;
  float tailCurve = sin(animationTime * 2) * 0.8;
  
  // Tail segments for curved motion
  int segments = 8;
  int prevX = tailBaseX;
  int prevY = tailBaseY;
  
  for (int seg = 1; seg <= segments; seg++) {
    float segmentRatio = (float)seg / segments;
    float angle = tailSwing * segmentRatio + tailCurve * sin(segmentRatio * PI);
    
    int segLength = 15;
    int segX = prevX + cos(angle) * segLength;
    int segY = prevY + sin(angle) * segLength;
    
    // Tail thickness decreases toward tip
    int thickness = 6 - seg;
    if (thickness < 1) thickness = 1;
    
    for (int t = 0; t < thickness; t++) {
      gfx->drawLine(prevX + t - thickness/2, prevY, 
                    segX + t - thickness/2, segY, 
                    gfx->color565(139, 69, 19));
    }
    
    prevX = segX;
    prevY = segY;
  }
  
  // Tail tuft at the end
  int tuftSize = 8 + sin(animationTime * 4) * 3;
  gfx->fillCircle(prevX, prevY, tuftSize, BLACK);
  
  // Flies being swatted
  for (int fly = 0; fly < 6; fly++) {
    float flyTime = animationTime * 6 + fly * 1.0;
    
    // Flies try to land on cow, tail swats them away
    float flyDistance = 80 + sin(flyTime) * 40;
    float flyAngle = fly * PI / 3 + sin(flyTime * 0.7) * 0.5;
    
    int flyX = bodyX + cos(flyAngle) * flyDistance;
    int flyY = bodyY + sin(flyAngle) * flyDistance;
    
    // Check if tail is near fly (swatting)
    float distToTail = sqrt((flyX - prevX) * (flyX - prevX) + (flyY - prevY) * (flyY - prevY));
    if (distToTail < 20) {
      // Fly gets swatted - show motion lines
      for (int line = 0; line < 3; line++) {
        int lineX = flyX + cos(tailSwing + line * 0.5) * (10 + line * 5);
        int lineY = flyY + sin(tailSwing + line * 0.5) * (10 + line * 5);
        gfx->drawLine(flyX, flyY, lineX, lineY, YELLOW);
      }
    } else {
      // Normal fly
      gfx->fillCircle(flyX, flyY, 2, BLACK);
      // Fly wings
      gfx->drawLine(flyX - 3, flyY - 1, flyX - 1, flyY - 1, gfx->color565(200, 200, 255));
      gfx->drawLine(flyX + 1, flyY - 1, flyX + 3, flyY - 1, gfx->color565(200, 200, 255));
    }
  }
}

void drawCowUdder() {
  // Cow udder effect - dairy cow with milk production
  gfx->fillScreen(gfx->color565(135, 206, 235)); // Sky blue
  
  // Draw barn in background
  gfx->fillRect(10, 50, gfx->width() - 20, gfx->height() - 100, gfx->color565(139, 69, 19));
  gfx->fillTriangle(10, 50, gfx->width()/2, 20, gfx->width() - 10, 50, RED);
  
  // Cow body (side view)
  int cowX = gfx->width() / 2;
  int cowY = gfx->height() - 80;
  
  // Body
  gfx->fillRect(cowX - 50, cowY - 40, 100, 40, WHITE);
  gfx->drawRect(cowX - 50, cowY - 40, 100, 40, BLACK);
  
  // Head
  gfx->fillCircle(cowX - 60, cowY - 25, 20, WHITE);
  gfx->drawCircle(cowX - 60, cowY - 25, 20, BLACK);
  
  // Eye
  gfx->fillCircle(cowX - 65, cowY - 30, 4, BLACK);
  gfx->fillCircle(cowX - 66, cowY - 32, 1, WHITE);
  
  // Legs
  for (int leg = 0; leg < 4; leg++) {
    int legX = cowX - 35 + leg * 20;
    gfx->drawLine(legX, cowY, legX, cowY + 25, BLACK);
    gfx->fillRect(legX - 3, cowY + 20, 6, 8, BLACK);
  }
  
  // Udder (main feature)
  int udderX = cowX - 10;
  int udderY = cowY - 5;
  
  // Main udder sac
  gfx->fillCircle(udderX, udderY, 25, gfx->color565(255, 192, 203)); // Pink
  gfx->drawCircle(udderX, udderY, 25, BLACK);
  
  // Four teats
  for (int teat = 0; teat < 4; teat++) {
    int teatX = udderX - 15 + teat * 10;
    int teatY = udderY + 20;
    
    // Teat
    gfx->fillRect(teatX - 2, teatY, 4, 12, gfx->color565(255, 182, 193));
    gfx->drawRect(teatX - 2, teatY, 4, 12, BLACK);
    
    // Milk drops (animated)
    if (sin(animationTime * 4 + teat) > 0.5) {
      for (int drop = 0; drop < 3; drop++) {
        int dropY = teatY + 15 + drop * 8 + (int)(animationTime * 30 + teat * 10) % 20;
        if (dropY < gfx->height() - 30) {
          gfx->fillCircle(teatX, dropY, 2, WHITE);
        }
      }
    }
  }
  
  // Milk bucket
  int bucketX = udderX;
  int bucketY = gfx->height() - 25;
  gfx->fillRect(bucketX - 15, bucketY, 30, 20, gfx->color565(150, 150, 150));
  gfx->drawRect(bucketX - 15, bucketY, 30, 20, BLACK);
  
  // Milk level in bucket (rises over time)
  int milkLevel = (int)(sin(animationTime * 0.5) * 8 + 10);
  gfx->fillRect(bucketX - 13, bucketY + 18 - milkLevel, 26, milkLevel, WHITE);
  
  // Bucket handle
  gfx->drawCircle(bucketX + 20, bucketY + 10, 8, BLACK);
  
  // Farmer's hand occasionally
  if (sin(animationTime * 0.3) > 0.7) {
    gfx->fillCircle(bucketX - 25, bucketY - 10, 8, gfx->color565(255, 219, 172));
    gfx->drawCircle(bucketX - 25, bucketY - 10, 8, BLACK);
    // Milking motion lines
    gfx->drawLine(bucketX - 25, bucketY - 10, udderX, udderY + 15, gfx->color565(200, 200, 200));
  }
  
  // Cow spots
  for (int spot = 0; spot < 6; spot++) {
    int spotX = cowX - 40 + spot * 15;
    int spotY = cowY - 35 + sin(animationTime + spot) * 8;
    gfx->fillCircle(spotX, spotY, 5 + spot % 3, BLACK);
  }
  
  // Tail
  float tailAngle = sin(animationTime * 2) * 0.3;
  int tailX = cowX + 50 + cos(tailAngle) * 15;
  int tailY = cowY - 20 + sin(tailAngle) * 8;
  gfx->drawLine(cowX + 50, cowY - 20, tailX, tailY, BLACK);
  gfx->fillCircle(tailX, tailY, 3, BLACK);
}

void drawCrackberg() {
  // Crackberg effect - cracked surface with growing fractures
  gfx->fillScreen(gfx->color565(80, 80, 80)); // Gray surface
  
  static bool cracks[60][40];
  static bool initialized = false;
  static int generation = 0;
  
  if (!initialized || generation > 200) {
    // Initialize with some initial cracks
    for (int x = 0; x < 60; x++) {
      for (int y = 0; y < 40; y++) {
        cracks[x][y] = false;
      }
    }
    // Create initial crack seeds
    for (int i = 0; i < 5; i++) {
      int x = random(10, 50);
      int y = random(10, 30);
      cracks[x][y] = true;
    }
    initialized = true;
    generation = 0;
  }
  
  // Crack propagation (every 30 frames)
  if ((int)(animationTime * 10) % 30 == 0) {
    bool newCracks[60][40];
    for (int x = 0; x < 60; x++) {
      for (int y = 0; y < 40; y++) {
        newCracks[x][y] = cracks[x][y];
      }
    }
    
    for (int x = 1; x < 59; x++) {
      for (int y = 1; y < 39; y++) {
        if (!cracks[x][y]) {
          // Check for adjacent cracks
          int adjacent = 0;
          if (cracks[x-1][y] || cracks[x+1][y] || cracks[x][y-1] || cracks[x][y+1]) adjacent++;
          
          // Propagate crack based on stress
          if (adjacent > 0 && random(0, 8) == 0) {
            newCracks[x][y] = true;
          }
        }
      }
    }
    
    for (int x = 0; x < 60; x++) {
      for (int y = 0; y < 40; y++) {
        cracks[x][y] = newCracks[x][y];
      }
    }
    generation++;
  }
  
  // Draw cracks
  for (int x = 0; x < 60; x++) {
    for (int y = 0; y < 40; y++) {
      if (cracks[x][y]) {
        int screenX = x * (gfx->width() / 60);
        int screenY = y * (gfx->height() / 40);
        
        // Crack depth based on age
        uint16_t crackColor = BLACK;
        gfx->fillRect(screenX, screenY, 
                      gfx->width() / 60 + 1, gfx->height() / 40 + 1, 
                      crackColor);
        
        // Add some red glow for deep cracks
        if (random(0, 4) == 0) {
          gfx->fillRect(screenX, screenY, 
                        gfx->width() / 60 + 1, gfx->height() / 40 + 1, 
                        gfx->color565(100, 0, 0));
        }
      }
    }
  }
  
  // Stress lines radiating from crack centers
  for (int i = 0; i < 8; i++) {
    float stressAngle = i * PI / 4 + animationTime * 0.5;
    int centerX = gfx->width() / 2;
    int centerY = gfx->height() / 2;
    
    int x1 = centerX + cos(stressAngle) * 20;
    int y1 = centerY + sin(stressAngle) * 20;
    int x2 = centerX + cos(stressAngle) * 80;
    int y2 = centerY + sin(stressAngle) * 80;
    
    gfx->drawLine(x1, y1, x2, y2, gfx->color565(60, 60, 60));
  }
}

void drawCritical() {
  // Critical system warning effect
  gfx->fillScreen(BLACK);
  
  // Flashing red background for urgency
  bool flash = (int)(animationTime * 8) % 2 == 0;
  if (flash) {
    gfx->fillScreen(gfx->color565(100, 0, 0));
  }
  
  // Warning text
  gfx->setTextColor(flash ? WHITE : RED);
  gfx->setTextSize(2);
  
  int textY = 20;
  gfx->setCursor(20, textY);
  gfx->printf("CRITICAL");
  
  textY += 25;
  gfx->setTextSize(1);
  gfx->setCursor(10, textY);
  gfx->printf("SYSTEM FAILURE IMMINENT");
  
  textY += 15;
  gfx->setCursor(10, textY);
  gfx->printf("Temperature: %d°C", 150 + (int)(sin(animationTime * 4) * 50));
  
  textY += 15;
  gfx->setCursor(10, textY);
  gfx->printf("Memory: %d%% FULL", 95 + (int)(sin(animationTime * 2) * 5));
  
  textY += 15;
  gfx->setCursor(10, textY);
  gfx->printf("CPU Load: %d%%", 98 + (int)(sin(animationTime * 6) * 2));
  
  // Warning symbols
  for (int i = 0; i < 6; i++) {
    float symbolTime = animationTime * 4 + i * 1.0;
    int symbolX = 20 + i * 40;
    int symbolY = gfx->height() - 40;
    
    if (sin(symbolTime) > 0) {
      // Warning triangle
      gfx->fillTriangle(symbolX, symbolY - 15, 
                        symbolX - 10, symbolY + 5,
                        symbolX + 10, symbolY + 5, 
                        YELLOW);
      gfx->drawTriangle(symbolX, symbolY - 15, 
                        symbolX - 10, symbolY + 5,
                        symbolX + 10, symbolY + 5, 
                        BLACK);
      
      // Exclamation mark
      gfx->drawFastVLine(symbolX, symbolY - 10, 8, BLACK);
      gfx->fillCircle(symbolX, symbolY + 2, 1, BLACK);
    }
  }
  
  // System graphs showing critical levels
  int graphX = gfx->width() - 80;
  int graphY = 80;
  int graphW = 60;
  int graphH = 40;
  
  gfx->drawRect(graphX, graphY, graphW, graphH, WHITE);
  
  // Critical level line
  int criticalY = graphY + graphH - 35;
  gfx->drawFastHLine(graphX, criticalY, graphW, RED);
  
  // Animated graph data showing critical values
  for (int x = 0; x < graphW - 5; x += 3) {
    float dataPoint = sin(animationTime * 2 + x * 0.1) * 0.3 + 0.9; // Mostly above critical
    int barHeight = (int)(dataPoint * graphH);
    int barY = graphY + graphH - barHeight;
    
    uint16_t barColor = (barHeight > 35) ? RED : YELLOW;
    gfx->drawFastVLine(graphX + x + 2, barY, barHeight, barColor);
  }
  
  // Error counter
  gfx->setCursor(10, gfx->height() - 15);
  gfx->setTextColor(RED);
  gfx->printf("Errors: %d", (int)(animationTime * 3) % 9999);
}

void drawCrumbler() {
  // Crumbler effect - structures crumbling and falling apart
  gfx->fillScreen(gfx->color565(135, 206, 235)); // Sky blue
  
  static int blocks[40][4]; // x, y, vy, type
  static bool initialized = false;
  
  if (!initialized) {
    // Initialize building blocks
    for (int i = 0; i < 40; i++) {
      blocks[i][0] = random(0, gfx->width());
      blocks[i][1] = random(50, gfx->height() - 100);
      blocks[i][2] = 0; // velocity
      blocks[i][3] = random(0, 3); // block type
    }
    initialized = true;
  }
  
  // Draw ground
  gfx->fillRect(0, gfx->height() - 20, gfx->width(), 20, gfx->color565(34, 139, 34));
  
  // Update and draw falling blocks
  for (int i = 0; i < 40; i++) {
    // Apply gravity to some blocks
    if (sin(animationTime * 0.5 + i * 0.1) > 0.3) {
      blocks[i][2] += 1; // gravity
      blocks[i][1] += blocks[i][2];
    }
    
    // Reset if block hits ground
    if (blocks[i][1] > gfx->height() - 30) {
      blocks[i][1] = random(20, 60);
      blocks[i][0] = random(0, gfx->width());
      blocks[i][2] = 0;
      
      // Dust cloud when hitting ground
      for (int dust = 0; dust < 5; dust++) {
        int dustX = blocks[i][0] + random(-10, 11);
        int dustY = gfx->height() - 25 + random(-5, 6);
        gfx->fillCircle(dustX, dustY, 2, gfx->color565(160, 160, 120));
      }
    }
    
    // Draw block based on type
    int blockX = blocks[i][0];
    int blockY = blocks[i][1];
    
    switch (blocks[i][3]) {
      case 0: // Brick
        gfx->fillRect(blockX, blockY, 20, 10, gfx->color565(139, 69, 19));
        gfx->drawRect(blockX, blockY, 20, 10, BLACK);
        break;
      case 1: // Concrete
        gfx->fillRect(blockX, blockY, 15, 15, gfx->color565(128, 128, 128));
        gfx->drawRect(blockX, blockY, 15, 15, BLACK);
        break;
      case 2: // Glass
        gfx->fillRect(blockX, blockY, 12, 18, gfx->color565(173, 216, 230));
        gfx->drawRect(blockX, blockY, 12, 18, BLACK);
        // Glass cracks
        gfx->drawLine(blockX + 2, blockY + 2, blockX + 10, blockY + 16, BLACK);
        gfx->drawLine(blockX + 8, blockY + 3, blockX + 4, blockY + 15, BLACK);
        break;
    }
    
    // Add motion blur for fast falling blocks
    if (blocks[i][2] > 5) {
      for (int trail = 1; trail < blocks[i][2] && trail < 8; trail++) {
        int trailY = blockY - trail * 3;
        if (trailY > 0) {
          uint16_t trailColor = gfx->color565(100 - trail * 10, 100 - trail * 10, 100 - trail * 10);
          gfx->fillRect(blockX, trailY, 
                        (blocks[i][3] == 0) ? 20 : (blocks[i][3] == 1) ? 15 : 12, 
                        2, trailColor);
        }
      }
    }
  }
  
  // Structural stress lines
  for (int line = 0; line < 6; line++) {
    float lineTime = animationTime * 2 + line * 0.8;
    int x1 = (int)(sin(lineTime) * 50 + gfx->width() / 2);
    int y1 = 60;
    int x2 = x1 + sin(lineTime * 1.3) * 30;
    int y2 = gfx->height() - 30;
    
    gfx->drawLine(x1, y1, x2, y2, gfx->color565(200, 100, 100));
  }
  
  // Explosion effects occasionally
  if (sin(animationTime * 0.7) > 0.8) {
    int explodeX = random(50, gfx->width() - 50);
    int explodeY = random(60, gfx->height() - 60);
    
    for (int spark = 0; spark < 8; spark++) {
      float sparkAngle = spark * PI / 4 + animationTime * 10;
      int sparkX = explodeX + cos(sparkAngle) * 15;
      int sparkY = explodeY + sin(sparkAngle) * 15;
      gfx->fillCircle(sparkX, sparkY, 3, YELLOW);
    }
    gfx->fillCircle(explodeX, explodeY, 8, RED);
  }
}

void drawCrystal() {
  // Crystal effect - growing crystalline structures
  gfx->fillScreen(gfx->color565(0, 0, 30)); // Dark background
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Main crystal formation
  for (int layer = 0; layer < 6; layer++) {
    float layerAngle = animationTime * 0.3 + layer * PI / 3;
    int layerRadius = 20 + layer * 15;
    
    for (int crystal = 0; crystal < 6; crystal++) {
      float angle = layerAngle + crystal * PI / 3;
      int x = centerX + cos(angle) * layerRadius;
      int y = centerY + sin(angle) * layerRadius;
      
      // Crystal size varies with time
      int crystalSize = 8 + sin(animationTime * 2 + layer + crystal) * 4;
      
      // Crystal color - prismatic effect
      float hue = (layer + crystal + animationTime * 0.5) / 6.0;
      int r = (int)(sin(hue * 2 * PI) * 127 + 128);
      int g = (int)(sin(hue * 2 * PI + 2 * PI / 3) * 127 + 128);
      int b = (int)(sin(hue * 2 * PI + 4 * PI / 3) * 127 + 128);
      
      uint16_t crystalColor = gfx->color565(r, g, b);
      
      // Draw hexagonal crystal
      for (int side = 0; side < 6; side++) {
        float sideAngle = angle + side * PI / 3;
        int x1 = x + cos(sideAngle) * crystalSize;
        int y1 = y + sin(sideAngle) * crystalSize;
        int x2 = x + cos(sideAngle + PI / 3) * crystalSize;
        int y2 = y + sin(sideAngle + PI / 3) * crystalSize;
        
        gfx->drawLine(x1, y1, x2, y2, crystalColor);
        gfx->drawLine(x, y, x1, y1, crystalColor);
      }
      
      // Crystal center
      gfx->fillCircle(x, y, 2, WHITE);
    }
  }
  
  // Floating crystal particles
  for (int i = 0; i < 15; i++) {
    float particleTime = animationTime * 1.5 + i * 0.4;
    int pX = centerX + sin(particleTime) * 80 + cos(particleTime * 0.7) * 40;
    int pY = centerY + cos(particleTime) * 60 + sin(particleTime * 1.3) * 30;
    
    int sparkle = (int)(sin(particleTime * 8) * 3 + 4);
    uint16_t sparkleColor = gfx->color565(200 + sparkle * 10, 150 + sparkle * 15, 255);
    gfx->fillCircle(pX, pY, sparkle, sparkleColor);
  }
}

void drawCube21() {
  // Cube21 effect - 3D rotating cubes in formation
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Draw 21 cubes in a pattern
  for (int cube = 0; cube < 21; cube++) {
    float cubeAngle = cube * 2 * PI / 21 + animationTime;
    float radius = 60 + sin(animationTime * 2 + cube * 0.3) * 20;
    
    int cubeX = centerX + cos(cubeAngle) * radius;
    int cubeY = centerY + sin(cubeAngle) * radius;
    
    // 3D cube rotation
    float rotX = animationTime * 1.5 + cube * 0.2;
    float rotY = animationTime * 1.2 + cube * 0.15;
    
    // Cube size varies
    int cubeSize = 8 + sin(animationTime * 3 + cube) * 4;
    
    // Cube color based on position
    uint16_t cubeColor = gfx->color565(
      128 + sin(cubeAngle) * 127,
      128 + cos(cubeAngle + 1) * 127,
      128 + sin(cubeAngle + 2) * 127
    );
    
    // Draw 3D cube wireframe
    // Front face
    gfx->drawRect(cubeX - cubeSize, cubeY - cubeSize, cubeSize * 2, cubeSize * 2, cubeColor);
    
    // Back face offset for 3D effect
    int offsetX = (int)(cos(rotX) * 6);
    int offsetY = (int)(sin(rotY) * 6);
    gfx->drawRect(cubeX - cubeSize + offsetX, cubeY - cubeSize + offsetY, 
                  cubeSize * 2, cubeSize * 2, cubeColor);
    
    // Connect front and back faces
    gfx->drawLine(cubeX - cubeSize, cubeY - cubeSize, 
                  cubeX - cubeSize + offsetX, cubeY - cubeSize + offsetY, cubeColor);
    gfx->drawLine(cubeX + cubeSize, cubeY - cubeSize, 
                  cubeX + cubeSize + offsetX, cubeY - cubeSize + offsetY, cubeColor);
    gfx->drawLine(cubeX - cubeSize, cubeY + cubeSize, 
                  cubeX - cubeSize + offsetX, cubeY + cubeSize + offsetY, cubeColor);
    gfx->drawLine(cubeX + cubeSize, cubeY + cubeSize, 
                  cubeX + cubeSize + offsetX, cubeY + cubeSize + offsetY, cubeColor);
  }
  
  // Central connecting lines
  for (int line = 0; line < 21; line++) {
    float lineAngle = line * 2 * PI / 21 + animationTime;
    float radius = 60 + sin(animationTime * 2 + line * 0.3) * 20;
    
    int lineX = centerX + cos(lineAngle) * radius;
    int lineY = centerY + sin(lineAngle) * radius;
    
    if (line % 3 == 0) {
      gfx->drawLine(centerX, centerY, lineX, lineY, 
                    gfx->color565(100, 100, 200));
    }
  }
}

void drawCubenetic() {
  // Cubenetic effect - genetic algorithm visualization with cubes
  gfx->fillScreen(gfx->color565(20, 40, 20)); // Dark green background
  
  static int population[16][4]; // x, y, fitness, generation
  static bool initialized = false;
  static int generation = 0;
  
  if (!initialized) {
    // Initialize random population
    for (int i = 0; i < 16; i++) {
      population[i][0] = random(20, gfx->width() - 20);
      population[i][1] = random(20, gfx->height() - 20);
      population[i][2] = random(0, 100); // fitness
      population[i][3] = 0; // generation
    }
    initialized = true;
  }
  
  // Evolution step every 60 frames
  if ((int)(animationTime * 10) % 60 == 0) {
    // Selection and mutation
    for (int i = 0; i < 16; i++) {
      // Calculate fitness based on position (closer to center = higher fitness)
      int dx = population[i][0] - gfx->width() / 2;
      int dy = population[i][1] - gfx->height() / 2;
      population[i][2] = 100 - (int)(sqrt(dx*dx + dy*dy) * 0.3);
      
      // Mutate position towards center for high fitness individuals
      if (population[i][2] > 50) {
        if (population[i][0] > gfx->width() / 2) population[i][0] -= random(1, 4);
        else population[i][0] += random(1, 4);
        
        if (population[i][1] > gfx->height() / 2) population[i][1] -= random(1, 4);
        else population[i][1] += random(1, 4);
      } else {
        // Random mutation for low fitness
        population[i][0] += random(-5, 6);
        population[i][1] += random(-5, 6);
        
        // Keep in bounds
        population[i][0] = constrain(population[i][0], 10, gfx->width() - 10);
        population[i][1] = constrain(population[i][1], 10, gfx->height() - 10);
      }
      
      population[i][3] = generation; // Update generation
    }
    generation++;
  }
  
  // Draw population as evolving cubes
  for (int i = 0; i < 16; i++) {
    int cubeX = population[i][0];
    int cubeY = population[i][1];
    int fitness = population[i][2];
    
    // Cube size based on fitness
    int cubeSize = 4 + fitness / 10;
    
    // Color based on fitness (green = good, red = bad)
    uint16_t cubeColor;
    if (fitness > 70) {
      cubeColor = gfx->color565(0, 255, 0); // Green - high fitness
    } else if (fitness > 40) {
      cubeColor = gfx->color565(255, 255, 0); // Yellow - medium fitness
    } else {
      cubeColor = gfx->color565(255, 0, 0); // Red - low fitness
    }
    
    // Draw cube
    gfx->fillRect(cubeX - cubeSize, cubeY - cubeSize, 
                  cubeSize * 2, cubeSize * 2, cubeColor);
    gfx->drawRect(cubeX - cubeSize, cubeY - cubeSize, 
                  cubeSize * 2, cubeSize * 2, WHITE);
    
    // Fitness indicator
    gfx->fillCircle(cubeX, cubeY, fitness / 20, cubeColor);
    
    // DNA connections to nearby individuals
    for (int j = i + 1; j < 16; j++) {
      int dx = abs(population[i][0] - population[j][0]);
      int dy = abs(population[i][1] - population[j][1]);
      int distance = dx + dy;
      
      if (distance < 50) {
        uint16_t connectionColor = gfx->color565(0, 100, 255);
        gfx->drawLine(population[i][0], population[i][1],
                      population[j][0], population[j][1], connectionColor);
      }
    }
  }
  
  // Target area (evolution goal)
  int targetX = gfx->width() / 2;
  int targetY = gfx->height() / 2;
  gfx->drawCircle(targetX, targetY, 30, WHITE);
  gfx->drawCircle(targetX, targetY, 25, WHITE);
  
  // Generation counter
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(5, 5);
  gfx->printf("Gen: %d", generation);
  
  // Best fitness
  int bestFitness = 0;
  for (int i = 0; i < 16; i++) {
    if (population[i][2] > bestFitness) bestFitness = population[i][2];
  }
  gfx->setCursor(5, 15);
  gfx->printf("Best: %d", bestFitness);
}

void drawCubeStack() {
  // Cubestack effect - stacking cubes building upward
  gfx->fillScreen(gfx->color565(40, 40, 80)); // Dark blue background
  
  static int stacks[8][10]; // 8 stacks, up to 10 cubes each
  static bool initialized = false;
  
  if (!initialized) {
    for (int stack = 0; stack < 8; stack++) {
      for (int cube = 0; cube < 10; cube++) {
        stacks[stack][cube] = 0; // 0 = empty, 1 = cube present
      }
    }
    initialized = true;
  }
  
  // Add cubes to stacks over time
  if ((int)(animationTime * 5) % 20 == 0) {
    int stackIndex = random(0, 8);
    // Find topmost empty position
    for (int cube = 0; cube < 10; cube++) {
      if (stacks[stackIndex][cube] == 0) {
        stacks[stackIndex][cube] = 1;
        break;
      }
    }
    
    // Reset if all stacks are full
    bool allFull = true;
    for (int stack = 0; stack < 8; stack++) {
      if (stacks[stack][9] == 0) allFull = false;
    }
    if (allFull) {
      for (int stack = 0; stack < 8; stack++) {
        for (int cube = 0; cube < 10; cube++) {
          stacks[stack][cube] = 0;
        }
      }
    }
  }
  
  // Draw the stacks
  int baseY = gfx->height() - 20;
  int stackWidth = gfx->width() / 8;
  
  for (int stack = 0; stack < 8; stack++) {
    int stackX = stack * stackWidth + stackWidth / 2;
    
    for (int cube = 0; cube < 10; cube++) {
      if (stacks[stack][cube] == 1) {
        int cubeY = baseY - cube * 18 - 10;
        int cubeSize = 15;
        
        // Cube color based on height and stack
        uint16_t cubeColor = gfx->color565(
          100 + cube * 15 + stack * 10,
          150 - cube * 8,
          200 + stack * 6
        );
        
        // Draw 3D cube
        gfx->fillRect(stackX - cubeSize/2, cubeY - cubeSize/2, 
                      cubeSize, cubeSize, cubeColor);
        gfx->drawRect(stackX - cubeSize/2, cubeY - cubeSize/2, 
                      cubeSize, cubeSize, WHITE);
        
        // 3D effect - top and right faces
        gfx->fillTriangle(stackX - cubeSize/2, cubeY - cubeSize/2,
                          stackX + cubeSize/2, cubeY - cubeSize/2,
                          stackX + cubeSize/2 - 4, cubeY - cubeSize/2 - 4, 
                          gfx->color565((100 + cube * 15 + stack * 10) + 30, 
                                        (150 - cube * 8) + 30, 
                                        (200 + stack * 6) + 30));
        
        gfx->fillTriangle(stackX + cubeSize/2, cubeY - cubeSize/2,
                          stackX + cubeSize/2, cubeY + cubeSize/2,
                          stackX + cubeSize/2 - 4, cubeY + cubeSize/2 - 4,
                          gfx->color565((100 + cube * 15 + stack * 10) - 30, 
                                        (150 - cube * 8) - 30, 
                                        (200 + stack * 6) - 30));
      }
    }
  }
  
  // Draw base platform
  gfx->fillRect(0, baseY, gfx->width(), 20, gfx->color565(100, 100, 100));
  gfx->drawRect(0, baseY, gfx->width(), 20, WHITE);
}

void drawCubeStorm() {
  // Cubestorm effect - cubes flying in storm pattern
  gfx->fillScreen(gfx->color565(20, 20, 40)); // Dark stormy background
  
  static float cubes[25][6]; // x, y, vx, vy, rotation, size
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 25; i++) {
      cubes[i][0] = random(0, gfx->width());   // x
      cubes[i][1] = random(0, gfx->height());  // y
      cubes[i][2] = random(-3, 4);             // vx
      cubes[i][3] = random(-3, 4);             // vy
      cubes[i][4] = 0;                         // rotation
      cubes[i][5] = random(8, 16);             // size
    }
    initialized = true;
  }
  
  // Storm wind force
  float windX = sin(animationTime * 0.5) * 2;
  float windY = cos(animationTime * 0.3) * 1.5;
  
  // Update cube positions and draw them
  for (int i = 0; i < 25; i++) {
    // Apply wind force
    cubes[i][2] += windX * 0.1;
    cubes[i][3] += windY * 0.1;
    
    // Apply turbulence
    cubes[i][2] += sin(animationTime * 4 + i) * 0.3;
    cubes[i][3] += cos(animationTime * 3 + i) * 0.3;
    
    // Update position
    cubes[i][0] += cubes[i][2];
    cubes[i][1] += cubes[i][3];
    
    // Update rotation
    cubes[i][4] += 0.1;
    
    // Wrap around screen
    if (cubes[i][0] < -20) cubes[i][0] = gfx->width() + 20;
    if (cubes[i][0] > gfx->width() + 20) cubes[i][0] = -20;
    if (cubes[i][1] < -20) cubes[i][1] = gfx->height() + 20;
    if (cubes[i][1] > gfx->height() + 20) cubes[i][1] = -20;
    
    // Draw rotating cube
    int cubeX = (int)cubes[i][0];
    int cubeY = (int)cubes[i][1];
    int cubeSize = (int)cubes[i][5];
    float rotation = cubes[i][4];
    
    // Cube color based on speed
    float speed = sqrt(cubes[i][2]*cubes[i][2] + cubes[i][3]*cubes[i][3]);
    uint16_t cubeColor = gfx->color565(
      (int)(100 + speed * 20),
      (int)(150 + sin(rotation) * 50),
      (int)(200 + cos(rotation) * 30)
    );
    
    // Draw rotated cube (simplified)
    int offset = (int)(sin(rotation) * 5);
    
    // Main cube
    gfx->fillRect(cubeX - cubeSize/2, cubeY - cubeSize/2, 
                  cubeSize, cubeSize, cubeColor);
    gfx->drawRect(cubeX - cubeSize/2, cubeY - cubeSize/2, 
                  cubeSize, cubeSize, WHITE);
    
    // Rotation effect - offset back face
    gfx->drawRect(cubeX - cubeSize/2 + offset, cubeY - cubeSize/2 + offset, 
                  cubeSize, cubeSize, gfx->color565(150, 150, 150));
    
    // Connect faces for 3D effect
    gfx->drawLine(cubeX - cubeSize/2, cubeY - cubeSize/2,
                  cubeX - cubeSize/2 + offset, cubeY - cubeSize/2 + offset, WHITE);
    gfx->drawLine(cubeX + cubeSize/2, cubeY - cubeSize/2,
                  cubeX + cubeSize/2 + offset, cubeY - cubeSize/2 + offset, WHITE);
    
    // Motion blur for fast moving cubes
    if (speed > 4) {
      for (int trail = 1; trail < 4; trail++) {
        int trailX = cubeX - (int)(cubes[i][2] * trail * 2);
        int trailY = cubeY - (int)(cubes[i][3] * trail * 2);
        uint16_t trailColor = gfx->color565(50 - trail * 10, 50 - trail * 10, 100 - trail * 20);
        gfx->fillRect(trailX - cubeSize/4, trailY - cubeSize/4, 
                      cubeSize/2, cubeSize/2, trailColor);
      }
    }
  }
  
  // Lightning effects occasionally
  if (random(0, 100) < 2) {
    int lightningX = random(0, gfx->width());
    for (int i = 0; i < gfx->height(); i += 8) {
      lightningX += random(-5, 6);
      lightningX = constrain(lightningX, 0, gfx->width()-1);
      gfx->drawFastVLine(lightningX, i, 6, WHITE);
    }
  }
}

void drawCubeTwist() {
  // Cubetwist effect - cubes twisting in spiral pattern
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Draw twisted cube formation
  for (int ring = 0; ring < 5; ring++) {
    float ringRadius = 20 + ring * 20;
    int cubesInRing = 6 + ring * 2;
    
    for (int cube = 0; cube < cubesInRing; cube++) {
      // Base angle for this cube
      float baseAngle = (cube * 2 * PI) / cubesInRing;
      
      // Twist effect - inner rings rotate faster
      float twistRate = (5 - ring) * 0.3;
      float twistAngle = animationTime * twistRate + baseAngle;
      
      // Spiral twist - each ring offset vertically
      float spiralOffset = sin(animationTime * 2 + ring * 0.5) * 15;
      
      // Calculate position
      int cubeX = centerX + cos(twistAngle) * ringRadius;
      int cubeY = centerY + sin(twistAngle) * ringRadius + spiralOffset;
      
      // Cube size decreases with distance from center
      int cubeSize = 12 - ring * 2;
      if (cubeSize < 4) cubeSize = 4;
      
      // Color based on twist angle and ring
      uint16_t cubeColor = gfx->color565(
        (int)(sin(twistAngle) * 127 + 128),
        (int)(cos(twistAngle + ring) * 127 + 128),
        255 - ring * 40
      );
      
      // 3D rotation based on twist
      float rotX = twistAngle * 2;
      float rotY = animationTime + ring * 0.3;
      
      // Draw 3D cube with perspective
      int depth = (int)(cos(rotY) * 8);
      
      // Front face
      gfx->fillRect(cubeX - cubeSize/2, cubeY - cubeSize/2, 
                    cubeSize, cubeSize, cubeColor);
      gfx->drawRect(cubeX - cubeSize/2, cubeY - cubeSize/2, 
                    cubeSize, cubeSize, WHITE);
      
      // Back face (offset for 3D effect)
      int backX = cubeX + depth;
      int backY = cubeY + depth;
      gfx->drawRect(backX - cubeSize/2, backY - cubeSize/2, 
                    cubeSize, cubeSize, 
                    gfx->color565(100, 100, 150));
      
      // Connect corners for 3D wireframe
      gfx->drawLine(cubeX - cubeSize/2, cubeY - cubeSize/2,
                    backX - cubeSize/2, backY - cubeSize/2, WHITE);
      gfx->drawLine(cubeX + cubeSize/2, cubeY - cubeSize/2,
                    backX + cubeSize/2, backY - cubeSize/2, WHITE);
      gfx->drawLine(cubeX - cubeSize/2, cubeY + cubeSize/2,
                    backX - cubeSize/2, backY + cubeSize/2, WHITE);
      gfx->drawLine(cubeX + cubeSize/2, cubeY + cubeSize/2,
                    backX + cubeSize/2, backY + cubeSize/2, WHITE);
    }
  }
  
  // Central twist vortex
  for (int i = 0; i < 8; i++) {
    float vortexAngle = animationTime * 4 + i * PI / 4;
    int vortexRadius = (int)(sin(animationTime * 3 + i) * 15 + 25);
    
    int vortexX = centerX + cos(vortexAngle) * vortexRadius;
    int vortexY = centerY + sin(vortexAngle) * vortexRadius;
    
    gfx->drawLine(centerX, centerY, vortexX, vortexY, 
                  gfx->color565(255, 200 - i * 20, 100 + i * 15));
  }
  
  // Center point
  gfx->fillCircle(centerX, centerY, 5, WHITE);
}

void drawCubicGrid() {
  // Cubic grid effect - 3D grid of rotating cubes
  gfx->fillScreen(gfx->color565(10, 10, 30)); // Dark background
  
  int gridSize = 6;
  int spacing = gfx->width() / (gridSize + 1);
  
  for (int x = 0; x < gridSize; x++) {
    for (int y = 0; y < gridSize; y++) {
      int cubeX = (x + 1) * spacing;
      int cubeY = (y + 1) * spacing;
      
      // Individual rotation for each cube
      float rotationOffset = (x + y) * 0.5;
      float rotation = animationTime * 2 + rotationOffset;
      
      // Cube size varies with rotation
      int cubeSize = 12 + sin(rotation) * 4;
      
      // Color based on position and rotation
      uint16_t cubeColor = gfx->color565(
        (int)(sin(rotation + x * 0.5) * 127 + 128),
        (int)(cos(rotation + y * 0.5) * 127 + 128),
        (int)(sin(rotation + x + y) * 127 + 128)
      );
      
      // 3D cube with rotation effect
      int depth = (int)(cos(rotation * 1.5) * 6);
      
      // Front face
      gfx->fillRect(cubeX - cubeSize/2, cubeY - cubeSize/2, 
                    cubeSize, cubeSize, cubeColor);
      gfx->drawRect(cubeX - cubeSize/2, cubeY - cubeSize/2, 
                    cubeSize, cubeSize, WHITE);
      
      // Back face
      int backX = cubeX + depth;
      int backY = cubeY + depth;
      gfx->drawRect(backX - cubeSize/2, backY - cubeSize/2, 
                    cubeSize, cubeSize, 
                    gfx->color565(100, 100, 150));
      
      // Connecting lines for 3D effect
      if (abs(depth) > 2) {
        gfx->drawLine(cubeX - cubeSize/2, cubeY - cubeSize/2,
                      backX - cubeSize/2, backY - cubeSize/2, WHITE);
        gfx->drawLine(cubeX + cubeSize/2, cubeY - cubeSize/2,
                      backX + cubeSize/2, backY - cubeSize/2, WHITE);
      }
      
      // Grid connections
      if (x < gridSize - 1) {
        int nextCubeX = (x + 2) * spacing;
        gfx->drawLine(cubeX + cubeSize/2, cubeY, 
                      nextCubeX - cubeSize/2, cubeY, 
                      gfx->color565(50, 50, 100));
      }
      if (y < gridSize - 1) {
        int nextCubeY = (y + 2) * spacing;
        gfx->drawLine(cubeX, cubeY + cubeSize/2, 
                      cubeX, nextCubeY - cubeSize/2, 
                      gfx->color565(50, 50, 100));
      }
    }
  }
}

void drawCuboctaversion() {
  // Cuboctaversion effect - morphing between cube and octahedron
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Morphing parameter between 0 and 1
  float morphParam = sin(animationTime * 0.8) * 0.5 + 0.5;
  
  // Draw multiple cuboctahedra at different scales
  for (int scale = 1; scale <= 3; scale++) {
    float radius = 30 * scale + sin(animationTime * 1.5 + scale) * 10;
    
    // 8 vertices of cuboctahedron
    float vertices[8][2];
    
    for (int i = 0; i < 8; i++) {
      float angle = i * PI / 4 + animationTime * (0.5 + scale * 0.2);
      
      // Interpolate between octagon (cube projection) and diamond (octahedron)
      float cubeRadius = radius;
      float octRadius = radius * 0.7;
      float currentRadius = cubeRadius * (1 - morphParam) + octRadius * morphParam;
      
      // Vertex distortion based on morph parameter
      float distortion = 1 + sin(angle * 4 + animationTime * 3) * morphParam * 0.3;
      currentRadius *= distortion;
      
      vertices[i][0] = centerX + cos(angle) * currentRadius;
      vertices[i][1] = centerY + sin(angle) * currentRadius;
    }
    
    // Color based on scale and morph
    uint16_t shapeColor = gfx->color565(
      (int)(100 + morphParam * 155),
      (int)(150 - morphParam * 50 + scale * 20),
      (int)(200 - morphParam * 100 + scale * 15)
    );
    
    // Draw edges
    for (int i = 0; i < 8; i++) {
      int next = (i + 1) % 8;
      gfx->drawLine((int)vertices[i][0], (int)vertices[i][1],
                    (int)vertices[next][0], (int)vertices[next][1], 
                    shapeColor);
      
      // Cross connections for cuboctahedral structure
      if (i % 2 == 0) {
        int cross = (i + 4) % 8;
        gfx->drawLine((int)vertices[i][0], (int)vertices[i][1],
                      (int)vertices[cross][0], (int)vertices[cross][1], 
                      gfx->color565(50 + scale * 30, 50 + scale * 30, 100 + scale * 30));
      }
    }
    
    // Draw vertices as points
    for (int i = 0; i < 8; i++) {
      gfx->fillCircle((int)vertices[i][0], (int)vertices[i][1], 
                      2 + morphParam * 2, WHITE);
    }
    
    // Central morph indicator
    int centerSize = (int)(morphParam * 8 + 2);
    gfx->fillCircle(centerX, centerY, centerSize, 
                    gfx->color565((int)(morphParam * 255), 
                                  (int)((1 - morphParam) * 255), 100));
  }
  
  // Rotation trails
  for (int trail = 0; trail < 12; trail++) {
    float trailAngle = animationTime * 2 + trail * PI / 6;
    float trailRadius = 80 + sin(animationTime * 3 + trail) * 20;
    
    int trailX = centerX + cos(trailAngle) * trailRadius;
    int trailY = centerY + sin(trailAngle) * trailRadius;
    
    gfx->fillCircle(trailX, trailY, 1 + trail % 3, 
                    gfx->color565(trail * 20, 100, 200 - trail * 10));
  }
}

void drawCWaves() {
  // C-waves effect - sine waves forming C shapes
  gfx->fillScreen(gfx->color565(0, 0, 20)); // Dark blue background
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Draw multiple C-wave patterns
  for (int wave = 0; wave < 5; wave++) {
    float waveOffset = wave * PI / 2.5 + animationTime * 0.3;
    float waveAmplitude = 20 + wave * 8;
    float waveRadius = 40 + wave * 15;
    
    // C-wave color
    uint16_t waveColor = gfx->color565(
      (int)(sin(waveOffset) * 127 + 128),
      (int)(cos(waveOffset + 1) * 127 + 128),
      255 - wave * 30
    );
    
    // Draw C-shaped wave
    for (int angle = 30; angle <= 330; angle += 3) { // C shape - not full circle
      float radians = angle * PI / 180.0 + waveOffset;
      
      // Wave modulation along the C
      float waveHeight = sin(radians * 3 + animationTime * 4) * waveAmplitude * 0.3;
      float currentRadius = waveRadius + waveHeight;
      
      // Additional wave component
      currentRadius += sin(radians * 2 + animationTime * 2) * waveAmplitude * 0.2;
      
      int x = centerX + cos(radians) * currentRadius;
      int y = centerY + sin(radians) * currentRadius;
      
      // Draw wave point with size based on wave height
      int pointSize = 2 + abs((int)waveHeight) / 8;
      gfx->fillCircle(x, y, pointSize, waveColor);
      
      // Connect to previous point for smooth wave
      if (angle > 30) {
        float prevRadians = (angle - 3) * PI / 180.0 + waveOffset;
        float prevWaveHeight = sin(prevRadians * 3 + animationTime * 4) * waveAmplitude * 0.3;
        float prevRadius = waveRadius + prevWaveHeight + sin(prevRadians * 2 + animationTime * 2) * waveAmplitude * 0.2;
        
        int prevX = centerX + cos(prevRadians) * prevRadius;
        int prevY = centerY + sin(prevRadians) * prevRadius;
        
        gfx->drawLine(prevX, prevY, x, y, waveColor);
      }
    }
    
    // C-wave endpoints (open ends of C)
    float startAngle = 30 * PI / 180.0 + waveOffset;
    float endAngle = 330 * PI / 180.0 + waveOffset;
    
    int startX = centerX + cos(startAngle) * waveRadius;
    int startY = centerY + sin(startAngle) * waveRadius;
    int endX = centerX + cos(endAngle) * waveRadius;
    int endY = centerY + sin(endAngle) * waveRadius;
    
    gfx->fillCircle(startX, startY, 4, WHITE);
    gfx->fillCircle(endX, endY, 4, WHITE);
  }
  
  // Interference patterns between C-waves
  for (int x = 0; x < gfx->width(); x += 8) {
    for (int y = 0; y < gfx->height(); y += 8) {
      float dx = x - centerX;
      float dy = y - centerY;
      float distance = sqrt(dx*dx + dy*dy);
      
      // Multiple wave interference
      float interference = 0;
      for (int w = 0; w < 3; w++) {
        interference += sin(distance * 0.1 + animationTime * (2 + w) + w * PI / 3);
      }
      
      if (interference > 1.5) {
        int intensity = (int)((interference - 1.5) * 100);
        intensity = constrain(intensity, 0, 255);
        gfx->fillCircle(x, y, 1, gfx->color565(0, intensity, intensity));
      }
    }
  }
  
  // Central wave source
  int pulseSize = (int)(sin(animationTime * 6) * 5 + 8);
  gfx->fillCircle(centerX, centerY, pulseSize, CYAN);
  gfx->drawCircle(centerX, centerY, pulseSize + 3, WHITE);
}

void drawCynosure() {
  // Cynosure effect - center of attraction with orbiting elements
  gfx->fillScreen(gfx->color565(5, 5, 20)); // Very dark background
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Central cynosure - bright attracting center
  int centerPulse = (int)(sin(animationTime * 4) * 8 + 15);
  gfx->fillCircle(centerX, centerY, centerPulse, WHITE);
  gfx->fillCircle(centerX, centerY, centerPulse - 3, YELLOW);
  gfx->fillCircle(centerX, centerY, centerPulse - 6, gfx->color565(255, 150, 0));
  
  // Attraction rings
  for (int ring = 1; ring <= 4; ring++) {
    int ringRadius = centerPulse + ring * 25;
    uint16_t ringColor = gfx->color565(50 - ring * 8, 50 - ring * 8, 100 - ring * 15);
    
    // Pulsing ring
    if ((int)(animationTime * 8 + ring) % 3 == 0) {
      gfx->drawCircle(centerX, centerY, ringRadius, ringColor);
    }
  }
  
  // Orbiting objects attracted to center
  for (int orbit = 0; orbit < 12; orbit++) {
    float orbitAngle = animationTime * (1.0 + orbit * 0.1) + orbit * PI / 6;
    float orbitRadius = 80 + sin(animationTime * 2 + orbit) * 30;
    
    // Spiral inward attraction
    orbitRadius -= sin(animationTime * 0.5 + orbit * 0.3) * 20;
    
    int objX = centerX + cos(orbitAngle) * orbitRadius;
    int objY = centerY + sin(orbitAngle) * orbitRadius;
    
    // Object size decreases as it gets closer to center
    int objSize = (int)(orbitRadius * 0.08 + 3);
    
    // Color based on distance from center
    float distanceRatio = orbitRadius / 120.0;
    uint16_t objColor = gfx->color565(
      (int)(255 * (1 - distanceRatio)),
      (int)(150 * distanceRatio),
      (int)(200 * distanceRatio)
    );
    
    gfx->fillCircle(objX, objY, objSize, objColor);
    
    // Attraction trail
    gfx->drawLine(objX, objY, 
                  centerX + cos(orbitAngle) * (orbitRadius * 0.7), 
                  centerY + sin(orbitAngle) * (orbitRadius * 0.7), 
                  gfx->color565(30, 30, 60));
  }
  
  // Magnetic field lines
  for (int field = 0; field < 8; field++) {
    float fieldAngle = field * PI / 4 + animationTime * 0.3;
    
    for (int point = 1; point <= 6; point++) {
      float fieldRadius = point * 20;
      int fieldX = centerX + cos(fieldAngle) * fieldRadius;
      int fieldY = centerY + sin(fieldAngle) * fieldRadius;
      
      // Curved field lines
      fieldX += (int)(sin(animationTime * 3 + point) * 5);
      fieldY += (int)(cos(animationTime * 2 + point) * 5);
      
      gfx->fillCircle(fieldX, fieldY, 1, gfx->color565(20, 100, 200));
    }
  }
}

void drawDangerball() {
  // Dangerball effect - hazardous bouncing ball with warnings
  gfx->fillScreen(BLACK);
  
  static float ballX = 120;
  static float ballY = 120;
  static float ballVX = 3;
  static float ballVY = 2.5;
  static bool initialized = false;
  
  if (!initialized) {
    ballX = gfx->width() / 2;
    ballY = gfx->height() / 2;
    ballVX = random(-4, 5);
    ballVY = random(-4, 5);
    initialized = true;
  }
  
  // Update ball position
  ballX += ballVX;
  ballY += ballVY;
  
  int ballRadius = 12 + sin(animationTime * 6) * 3;
  
  // Bounce off walls with danger effects
  bool bounced = false;
  if (ballX - ballRadius < 0 || ballX + ballRadius > gfx->width()) {
    ballVX = -ballVX;
    bounced = true;
  }
  if (ballY - ballRadius < 0 || ballY + ballRadius > gfx->height()) {
    ballVY = -ballVY;
    bounced = true;
  }
  
  // Keep ball in bounds
  ballX = constrain(ballX, ballRadius, gfx->width() - ballRadius);
  ballY = constrain(ballY, ballRadius, gfx->height() - ballRadius);
  
  // Draw danger ball
  uint16_t dangerColor = gfx->color565(255, (int)(sin(animationTime * 8) * 127 + 128), 0);
  gfx->fillCircle((int)ballX, (int)ballY, ballRadius, dangerColor);
  
  // Hazard symbols on ball
  gfx->fillTriangle((int)ballX, (int)ballY - 6,
                    (int)ballX - 4, (int)ballY + 2,
                    (int)ballX + 4, (int)ballY + 2, BLACK);
  gfx->fillCircle((int)ballX, (int)ballY + 4, 1, BLACK);
  gfx->drawFastVLine((int)ballX, (int)ballY - 3, 4, BLACK);
  
  // Danger aura
  for (int aura = 1; aura <= 3; aura++) {
    int auraRadius = ballRadius + aura * 8;
    uint16_t auraColor = gfx->color565(200 - aura * 50, 50 - aura * 15, 0);
    
    if ((int)(animationTime * 10 + aura) % 4 == 0) {
      gfx->drawCircle((int)ballX, (int)ballY, auraRadius, auraColor);
    }
  }
  
  // Bounce explosion effect
  if (bounced) {
    for (int spark = 0; spark < 8; spark++) {
      float sparkAngle = spark * PI / 4;
      int sparkX = (int)ballX + cos(sparkAngle) * 20;
      int sparkY = (int)ballY + sin(sparkAngle) * 20;
      gfx->fillCircle(sparkX, sparkY, 3, YELLOW);
    }
  }
  
  // Warning indicators at screen edges
  if (ballX < 60) {
    gfx->fillTriangle(5, (int)ballY, 15, (int)ballY - 8, 15, (int)ballY + 8, RED);
  }
  if (ballX > gfx->width() - 60) {
    gfx->fillTriangle(gfx->width() - 5, (int)ballY, 
                      gfx->width() - 15, (int)ballY - 8, 
                      gfx->width() - 15, (int)ballY + 8, RED);
  }
  if (ballY < 60) {
    gfx->fillTriangle((int)ballX, 5, (int)ballX - 8, 15, (int)ballX + 8, 15, RED);
  }
  if (ballY > gfx->height() - 60) {
    gfx->fillTriangle((int)ballX, gfx->height() - 5, 
                      (int)ballX - 8, gfx->height() - 15, 
                      (int)ballX + 8, gfx->height() - 15, RED);
  }
  
  // Speed indicator
  float speed = sqrt(ballVX*ballVX + ballVY*ballVY);
  int speedBars = (int)(speed);
  
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(5, 5);
  gfx->printf("DANGER");
  
  for (int bar = 0; bar < speedBars && bar < 8; bar++) {
    uint16_t barColor = (bar < 4) ? YELLOW : RED;
    gfx->fillRect(5 + bar * 8, 15, 6, 8, barColor);
  }
}

void drawDecayScreen() {
  // Decay screen effect - screen deterioration and static
  static uint16_t pixels[320*240/16]; // Reduced resolution for performance
  static bool initialized = false;
  static int decayLevel = 0;
  
  if (!initialized) {
    // Initialize with some pattern
    for (int i = 0; i < 320*240/16; i++) {
      pixels[i] = gfx->color565(random(100, 255), random(100, 255), random(100, 255));
    }
    initialized = true;
  }
  
  // Progressive decay
  if ((int)(animationTime * 20) % 10 == 0) {
    decayLevel++;
    
    // Add noise and corruption
    for (int i = 0; i < 50; i++) {
      int index = random(0, 320*240/16);
      
      if (decayLevel < 50) {
        // Early decay - color corruption
        uint8_t r = (pixels[index] >> 11) & 0x1F;
        uint8_t g = (pixels[index] >> 5) & 0x3F;
        uint8_t b = pixels[index] & 0x1F;
        
        r = max(0, (int)(r - random(0, 3)));
        g = max(0, (int)(g - random(0, 5)));
        b = max(0, (int)(b - random(0, 3)));
        
        pixels[index] = (r << 11) | (g << 5) | b;
      } else if (decayLevel < 100) {
        // Mid decay - dead pixels
        if (random(0, 3) == 0) {
          pixels[index] = BLACK;
        } else {
          pixels[index] = gfx->color565(random(0, 50), random(0, 50), random(0, 50));
        }
      } else {
        // Severe decay - mostly static
        pixels[index] = gfx->color565(random(0, 255), random(0, 255), random(0, 255));
      }
    }
  }
  
  // Draw decayed pixels
  int pixelSize = 4;
  for (int y = 0; y < gfx->height(); y += pixelSize) {
    for (int x = 0; x < gfx->width(); x += pixelSize) {
      int index = (y / pixelSize) * (gfx->width() / pixelSize) + (x / pixelSize);
      if (index < 320*240/16) {
        gfx->fillRect(x, y, pixelSize, pixelSize, pixels[index]);
      }
    }
  }
  
  // Additional decay effects
  if (decayLevel > 30) {
    // Scan lines
    for (int y = 0; y < gfx->height(); y += 4) {
      if (random(0, 4) == 0) {
        gfx->drawFastHLine(0, y, gfx->width(), BLACK);
      }
    }
  }
  
  if (decayLevel > 60) {
    // Vertical noise bars
    for (int x = 0; x < gfx->width(); x += 8) {
      if (random(0, 8) == 0) {
        uint16_t noiseColor = gfx->color565(random(0, 100), random(0, 100), random(0, 100));
        gfx->drawFastVLine(x, 0, gfx->height(), noiseColor);
      }
    }
  }
  
  if (decayLevel > 90) {
    // Screen flicker
    if (random(0, 10) == 0) {
      gfx->fillScreen(gfx->color565(random(0, 50), random(0, 50), random(0, 50)));
    }
  }
  
  // Reset after complete decay
  if (decayLevel > 150) {
    decayLevel = 0;
    // Reinitialize with new pattern
    for (int i = 0; i < 320*240/16; i++) {
      pixels[i] = gfx->color565(random(100, 255), random(100, 255), random(100, 255));
    }
  }
  
  // Decay level indicator
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(5, 5);
  gfx->printf("Decay: %d%%", (decayLevel * 100) / 150);
}

void drawDeco() {
  // Art Deco effect - geometric patterns in 1920s style
  gfx->fillScreen(gfx->color565(20, 20, 40)); // Dark background
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Art Deco sunburst pattern
  for (int ray = 0; ray < 16; ray++) {
    float rayAngle = ray * PI / 8 + animationTime * 0.2;
    
    // Alternating ray lengths for deco style
    int rayLength = (ray % 2 == 0) ? 80 : 60;
    rayLength += sin(animationTime * 3 + ray) * 10;
    
    int rayEndX = centerX + cos(rayAngle) * rayLength;
    int rayEndY = centerY + sin(rayAngle) * rayLength;
    
    // Deco colors - gold and black theme
    uint16_t rayColor = (ray % 3 == 0) ? 
      gfx->color565(255, 215, 0) :  // Gold
      gfx->color565(200, 200, 200); // Silver
    
    gfx->drawLine(centerX, centerY, rayEndX, rayEndY, rayColor);
    
    // Decorative diamonds at ray ends
    if (ray % 4 == 0) {
      gfx->fillTriangle(rayEndX, rayEndY - 6,
                        rayEndX - 4, rayEndY,
                        rayEndX + 4, rayEndY, rayColor);
      gfx->fillTriangle(rayEndX, rayEndY + 6,
                        rayEndX - 4, rayEndY,
                        rayEndX + 4, rayEndY, rayColor);
    }
  }
  
  // Concentric geometric shapes
  for (int ring = 1; ring <= 4; ring++) {
    int ringRadius = ring * 20 + sin(animationTime * 2 + ring) * 5;
    
    // Deco stepped pattern
    if (ring % 2 == 0) {
      // Octagon
      for (int side = 0; side < 8; side++) {
        float angle1 = side * PI / 4;
        float angle2 = (side + 1) * PI / 4;
        
        int x1 = centerX + cos(angle1) * ringRadius;
        int y1 = centerY + sin(angle1) * ringRadius;
        int x2 = centerX + cos(angle2) * ringRadius;
        int y2 = centerY + sin(angle2) * ringRadius;
        
        gfx->drawLine(x1, y1, x2, y2, gfx->color565(255, 215 - ring * 30, 0));
      }
    } else {
      // Diamond/square
      gfx->drawRect(centerX - ringRadius, centerY - ringRadius, 
                    ringRadius * 2, ringRadius * 2, 
                    gfx->color565(150 + ring * 20, 150 + ring * 20, 150 + ring * 20));
    }
  }
  
  // Art Deco corner decorations
  for (int corner = 0; corner < 4; corner++) {
    int cornerX = (corner % 2 == 0) ? 20 : gfx->width() - 20;
    int cornerY = (corner < 2) ? 20 : gfx->height() - 20;
    
    // Fan pattern in corners
    for (int fan = 0; fan < 5; fan++) {
      float fanAngle = corner * PI / 2 + fan * PI / 16 - PI / 8;
      int fanLength = 15 + fan * 3 + sin(animationTime * 4 + corner) * 3;
      
      int fanX = cornerX + cos(fanAngle) * fanLength;
      int fanY = cornerY + sin(fanAngle) * fanLength;
      
      gfx->drawLine(cornerX, cornerY, fanX, fanY, 
                    gfx->color565(255 - fan * 30, 200 - fan * 20, fan * 40));
    }
  }
  
  // Central art deco motif
  int motifSize = 15 + sin(animationTime * 4) * 5;
  gfx->fillCircle(centerX, centerY, motifSize, gfx->color565(255, 215, 0));
  gfx->drawCircle(centerX, centerY, motifSize + 5, WHITE);
  gfx->drawCircle(centerX, centerY, motifSize + 10, gfx->color565(200, 200, 200));
}

void drawDeepStars() {
  // Deep stars effect - deep space starfield with parallax
  gfx->fillScreen(BLACK);
  
  static float stars[100][4]; // x, y, z, brightness
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 100; i++) {
      stars[i][0] = random(-200, 200); // x
      stars[i][1] = random(-150, 150); // y
      stars[i][2] = random(1, 100);    // z depth
      stars[i][3] = random(50, 255);   // brightness
    }
    initialized = true;
  }
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Move stars toward viewer
  for (int i = 0; i < 100; i++) {
    stars[i][2] -= 0.5 + sin(animationTime * 0.1) * 0.3; // Variable speed
    
    // Reset star when it gets too close
    if (stars[i][2] < 1) {
      stars[i][0] = random(-200, 200);
      stars[i][1] = random(-150, 150);
      stars[i][2] = random(80, 100);
      stars[i][3] = random(50, 255);
    }
    
    // 3D projection
    float projectedX = centerX + (stars[i][0] / stars[i][2]) * 100;
    float projectedY = centerY + (stars[i][1] / stars[i][2]) * 100;
    
    // Only draw if on screen
    if (projectedX >= 0 && projectedX < gfx->width() && 
        projectedY >= 0 && projectedY < gfx->height()) {
      
      // Star size based on depth and brightness
      int starSize = (int)((100.0 / stars[i][2]) * 2);
      starSize = constrain(starSize, 1, 8);
      
      // Star brightness decreases with distance
      int brightness = (int)(stars[i][3] / stars[i][2] * 20);
      brightness = constrain(brightness, 0, 255);
      
      // Different star colors based on temperature/type
      uint16_t starColor;
      if (brightness > 200) {
        starColor = gfx->color565(255, 255, brightness); // Blue-white (hot)
      } else if (brightness > 150) {
        starColor = gfx->color565(255, brightness, brightness); // White
      } else if (brightness > 100) {
        starColor = gfx->color565(255, brightness, brightness/2); // Yellow
      } else {
        starColor = gfx->color565(brightness, brightness/2, brightness/4); // Red (cool)
      }
      
      gfx->fillCircle((int)projectedX, (int)projectedY, starSize, starColor);
      
      // Twinkling effect for close stars
      if (stars[i][2] < 20 && (int)(animationTime * 10 + i) % 3 == 0) {
        gfx->drawCircle((int)projectedX, (int)projectedY, starSize + 2, 
                        gfx->color565(brightness, brightness, 255));
      }
    }
  }
  
  // Nebula background effect
  for (int x = 0; x < gfx->width(); x += 16) {
    for (int y = 0; y < gfx->height(); y += 16) {
      float nebulaDensity = sin((x + animationTime * 10) * 0.02) * 
                           cos((y + animationTime * 8) * 0.03);
      if (nebulaDensity > 0.7) {
        int nebulaIntensity = (int)((nebulaDensity - 0.7) * 400);
        nebulaIntensity = constrain(nebulaIntensity, 0, 100);
        
        uint16_t nebulaColor = gfx->color565(nebulaIntensity/4, 0, nebulaIntensity);
        gfx->fillRect(x, y, 16, 16, nebulaColor);
      }
    }
  }
  
  // Shooting stars occasionally
  if (random(0, 200) == 0) {
    int shootX = random(0, gfx->width());
    int shootY = random(0, gfx->height()/2);
    
    for (int trail = 0; trail < 20; trail++) {
      int trailX = shootX + trail * 3;
      int trailY = shootY + trail * 2;
      
      if (trailX < gfx->width() && trailY < gfx->height()) {
        uint16_t trailColor = gfx->color565(255 - trail * 10, 255 - trail * 8, 255 - trail * 12);
        gfx->fillCircle(trailX, trailY, 2 - trail/10, trailColor);
      }
    }
  }
}

void drawDelaunay() {
  // Delaunay triangulation effect - animated triangular mesh
  gfx->fillScreen(gfx->color565(10, 10, 30)); // Dark background
  
  static float points[20][2]; // x, y coordinates of points
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 20; i++) {
      points[i][0] = random(20, gfx->width() - 20);
      points[i][1] = random(20, gfx->height() - 20);
    }
    initialized = true;
  }
  
  // Animate points
  for (int i = 0; i < 20; i++) {
    points[i][0] += sin(animationTime * (1 + i * 0.1)) * 0.5;
    points[i][1] += cos(animationTime * (1.2 + i * 0.08)) * 0.5;
    
    // Keep points on screen with wrapping
    if (points[i][0] < 0) points[i][0] = gfx->width();
    if (points[i][0] > gfx->width()) points[i][0] = 0;
    if (points[i][1] < 0) points[i][1] = gfx->height();
    if (points[i][1] > gfx->height()) points[i][1] = 0;
  }
  
  // Simple Delaunay-like triangulation (connecting nearby points)
  for (int i = 0; i < 20; i++) {
    for (int j = i + 1; j < 20; j++) {
      for (int k = j + 1; k < 20; k++) {
        float dx1 = points[j][0] - points[i][0];
        float dy1 = points[j][1] - points[i][1];
        float dx2 = points[k][0] - points[i][0];
        float dy2 = points[k][1] - points[i][1];
        float dx3 = points[k][0] - points[j][0];
        float dy3 = points[k][1] - points[j][1];
        
        float dist1 = sqrt(dx1*dx1 + dy1*dy1);
        float dist2 = sqrt(dx2*dx2 + dy2*dy2);
        float dist3 = sqrt(dx3*dx3 + dy3*dy3);
        
        // Only draw triangles with reasonable side lengths
        if (dist1 < 80 && dist2 < 80 && dist3 < 80) {
          // Triangle color based on area and position
          float area = abs(dx1 * dy2 - dx2 * dy1) / 2.0;
          
          uint16_t triangleColor = gfx->color565(
            (int)(area * 0.5 + 50),
            (int)(100 + sin(animationTime + i + j + k) * 50),
            (int)(150 + area * 0.3)
          );
          
          // Draw triangle outline
          gfx->drawLine((int)points[i][0], (int)points[i][1],
                        (int)points[j][0], (int)points[j][1], triangleColor);
          gfx->drawLine((int)points[j][0], (int)points[j][1],
                        (int)points[k][0], (int)points[k][1], triangleColor);
          gfx->drawLine((int)points[k][0], (int)points[k][1],
                        (int)points[i][0], (int)points[i][1], triangleColor);
          
          // Fill some triangles based on timing
          if ((int)(animationTime * 2 + i + j + k) % 7 == 0) {
            uint16_t fillColor = gfx->color565(
              (int)(area * 0.2 + 20),
              (int)(30 + sin(animationTime + area) * 20),
              (int)(50 + area * 0.1)
            );
            
            gfx->fillTriangle((int)points[i][0], (int)points[i][1],
                             (int)points[j][0], (int)points[j][1],
                             (int)points[k][0], (int)points[k][1], fillColor);
          }
        }
      }
    }
  }
  
  // Draw the points themselves
  for (int i = 0; i < 20; i++) {
    int pointSize = 3 + sin(animationTime * 4 + i) * 2;
    uint16_t pointColor = gfx->color565(
      255,
      (int)(200 + sin(animationTime * 3 + i) * 55),
      (int)(150 + cos(animationTime * 2 + i) * 105)
    );
    
    gfx->fillCircle((int)points[i][0], (int)points[i][1], pointSize, pointColor);
    
    // Point label (optional, for debugging)
    if (pointSize > 4) {
      gfx->drawCircle((int)points[i][0], (int)points[i][1], pointSize + 2, WHITE);
    }
  }
  
  // Voronoi-like cell boundaries (simplified)
  for (int i = 0; i < 20; i++) {
    for (int neighbor = 0; neighbor < 20; neighbor++) {
      if (i != neighbor) {
        float dx = points[neighbor][0] - points[i][0];
        float dy = points[neighbor][1] - points[i][1];
        float distance = sqrt(dx*dx + dy*dy);
        
        if (distance < 50) {
          // Draw perpendicular bisector (simplified)
          float midX = (points[i][0] + points[neighbor][0]) / 2;
          float midY = (points[i][1] + points[neighbor][1]) / 2;
          
          float perpX = midX + dy * 0.3;
          float perpY = midY - dx * 0.3;
          float perpX2 = midX - dy * 0.3;
          float perpY2 = midY + dx * 0.3;
          
          uint16_t cellColor = gfx->color565(50, 100, 200);
          gfx->drawLine((int)perpX, (int)perpY, (int)perpX2, (int)perpY2, cellColor);
        }
      }
    }
  }
}

void drawDeluxe() {
  // Deluxe effect - premium animated luxury patterns
  gfx->fillScreen(gfx->color565(10, 5, 20)); // Rich dark background
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Luxury golden frame
  for (int frame = 0; frame < 4; frame++) {
    int frameOffset = frame * 3;
    uint16_t frameColor = gfx->color565(255 - frame * 30, 215 - frame * 25, frame * 20);
    
    gfx->drawRect(frameOffset, frameOffset, 
                  gfx->width() - frameOffset * 2, gfx->height() - frameOffset * 2, 
                  frameColor);
  }
  
  // Premium crystal chandelier effect
  for (int crystal = 0; crystal < 12; crystal++) {
    float crystalAngle = crystal * PI / 6 + animationTime * 0.5;
    float crystalRadius = 40 + sin(animationTime * 2 + crystal) * 15;
    
    int crystalX = centerX + cos(crystalAngle) * crystalRadius;
    int crystalY = centerY + sin(crystalAngle) * crystalRadius;
    
    // Multi-faceted crystal
    for (int facet = 0; facet < 6; facet++) {
      float facetAngle = crystalAngle + facet * PI / 3;
      int facetSize = 8 + sin(animationTime * 3 + crystal + facet) * 3;
      
      int facetX = crystalX + cos(facetAngle) * facetSize;
      int facetY = crystalY + sin(facetAngle) * facetSize;
      
      // Prismatic colors
      uint16_t crystalColor = gfx->color565(
        (int)(sin(facetAngle + animationTime) * 127 + 128),
        (int)(cos(facetAngle + animationTime + 1) * 127 + 128),
        255
      );
      
      gfx->drawLine(crystalX, crystalY, facetX, facetY, crystalColor);
      gfx->fillCircle(facetX, facetY, 2, crystalColor);
    }
    
    // Crystal center
    gfx->fillCircle(crystalX, crystalY, 4, gfx->color565(255, 255, 200));
    
    // Light rays from crystals
    if (crystal % 3 == 0) {
      for (int ray = 0; ray < 8; ray++) {
        float rayAngle = ray * PI / 4 + animationTime * 2;
        int rayLength = 20 + sin(animationTime * 4 + ray) * 8;
        
        int rayEndX = crystalX + cos(rayAngle) * rayLength;
        int rayEndY = crystalY + sin(rayAngle) * rayLength;
        
        gfx->drawLine(crystalX, crystalY, rayEndX, rayEndY, 
                      gfx->color565(255, 255, 150));
      }
    }
  }
  
  // Luxury velvet texture pattern
  for (int x = 0; x < gfx->width(); x += 8) {
    for (int y = 0; y < gfx->height(); y += 8) {
      float textureNoise = sin(x * 0.1 + animationTime) * cos(y * 0.1 + animationTime * 0.8);
      if (textureNoise > 0.6) {
        uint16_t velvetColor = gfx->color565(80, 20, 100);
        gfx->fillRect(x, y, 4, 4, velvetColor);
      }
    }
  }
  
  // Premium gold leaf accents
  for (int leaf = 0; leaf < 6; leaf++) {
    float leafAngle = leaf * PI / 3 + animationTime * 0.3;
    int leafRadius = 80 + sin(animationTime + leaf) * 10;
    
    int leafX = centerX + cos(leafAngle) * leafRadius;
    int leafY = centerY + sin(leafAngle) * leafRadius;
    
    // Ornate leaf shape
    for (int segment = 0; segment < 5; segment++) {
      float segmentAngle = leafAngle + (segment - 2) * 0.2;
      int segmentLength = 12 - abs(segment - 2) * 2;
      
      int segX = leafX + cos(segmentAngle) * segmentLength;
      int segY = leafY + sin(segmentAngle) * segmentLength;
      
      gfx->drawLine(leafX, leafY, segX, segY, gfx->color565(255, 215, 0));
    }
    
    // Leaf center
    gfx->fillCircle(leafX, leafY, 3, gfx->color565(255, 223, 0));
  }
  
  // Central luxury emblem
  int emblemSize = 20 + sin(animationTime * 3) * 5;
  
  // Diamond shape
  gfx->fillTriangle(centerX, centerY - emblemSize,
                    centerX - emblemSize, centerY,
                    centerX, centerY + emblemSize, gfx->color565(255, 255, 255));
  gfx->fillTriangle(centerX, centerY - emblemSize,
                    centerX + emblemSize, centerY,
                    centerX, centerY + emblemSize, gfx->color565(200, 200, 255));
  
  // Crown on top
  for (int crown = 0; crown < 5; crown++) {
    int crownX = centerX - 10 + crown * 5;
    int crownHeight = (crown == 2) ? 15 : 10;
    gfx->drawLine(crownX, centerY - emblemSize - 5, 
                  crownX, centerY - emblemSize - 5 - crownHeight, 
                  gfx->color565(255, 215, 0));
  }
}

void drawDemoGtk() {
  // Demo GTK effect - GUI demo with windows and widgets
  gfx->fillScreen(gfx->color565(200, 200, 200)); // Light gray desktop
  
  // Desktop wallpaper pattern
  for (int x = 0; x < gfx->width(); x += 32) {
    for (int y = 0; y < gfx->height(); y += 32) {
      if ((x/32 + y/32) % 2 == 0) {
        gfx->fillRect(x, y, 32, 32, gfx->color565(180, 180, 220));
      }
    }
  }
  
  // Window 1 - Main application window
  int win1X = 20 + sin(animationTime * 0.5) * 10;
  int win1Y = 20 + cos(animationTime * 0.3) * 5;
  int win1W = 140;
  int win1H = 100;
  
  // Window frame
  gfx->fillRect(win1X, win1Y, win1W, win1H, gfx->color565(240, 240, 240));
  gfx->drawRect(win1X, win1Y, win1W, win1H, BLACK);
  
  // Title bar
  gfx->fillRect(win1X + 1, win1Y + 1, win1W - 2, 18, gfx->color565(0, 120, 200));
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(win1X + 5, win1Y + 6);
  gfx->printf("Demo App");
  
  // Window controls
  gfx->fillRect(win1X + win1W - 15, win1Y + 4, 10, 10, RED); // Close
  gfx->fillRect(win1X + win1W - 28, win1Y + 4, 10, 10, YELLOW); // Minimize
  
  // Menu bar
  gfx->fillRect(win1X + 1, win1Y + 20, win1W - 2, 15, gfx->color565(220, 220, 220));
  gfx->setTextColor(BLACK);
  gfx->setCursor(win1X + 5, win1Y + 24);
  gfx->printf("File Edit View");
  
  // Buttons
  for (int btn = 0; btn < 3; btn++) {
    int btnX = win1X + 10 + btn * 35;
    int btnY = win1Y + 45;
    
    uint16_t btnColor = gfx->color565(200, 200, 200);
    if ((int)(animationTime * 2) % 3 == btn) {
      btnColor = gfx->color565(150, 150, 255); // Highlight
    }
    
    gfx->fillRect(btnX, btnY, 30, 20, btnColor);
    gfx->drawRect(btnX, btnY, 30, 20, BLACK);
    
    gfx->setTextColor(BLACK);
    gfx->setCursor(btnX + 5, btnY + 6);
    switch(btn) {
      case 0: gfx->printf("OK"); break;
      case 1: gfx->printf("APP"); break;
      case 2: gfx->printf("EXIT"); break;
    }
  }
  
  // Progress bar
  int progressX = win1X + 10;
  int progressY = win1Y + 75;
  int progressW = win1W - 20;
  int progressFill = (int)((sin(animationTime * 2) * 0.5 + 0.5) * progressW);
  
  gfx->drawRect(progressX, progressY, progressW, 10, BLACK);
  gfx->fillRect(progressX + 1, progressY + 1, progressFill - 2, 8, GREEN);
  
  // Window 2 - Dialog box
  int win2X = 80 + sin(animationTime * 0.8) * 8;
  int win2Y = 60 + cos(animationTime * 0.6) * 6;
  int win2W = 100;
  int win2H = 70;
  
  gfx->fillRect(win2X, win2Y, win2W, win2H, gfx->color565(250, 250, 250));
  gfx->drawRect(win2X, win2Y, win2W, win2H, BLACK);
  
  // Dialog title
  gfx->fillRect(win2X + 1, win2Y + 1, win2W - 2, 15, gfx->color565(100, 100, 200));
  gfx->setTextColor(WHITE);
  gfx->setCursor(win2X + 5, win2Y + 5);
  gfx->printf("Alert");
  
  // Dialog content
  gfx->setTextColor(BLACK);
  gfx->setCursor(win2X + 8, win2Y + 25);
  gfx->printf("Demo Mode");
  gfx->setCursor(win2X + 8, win2Y + 35);
  gfx->printf("Active!");
  
  // Dialog button
  gfx->fillRect(win2X + 35, win2Y + 50, 30, 15, gfx->color565(220, 220, 220));
  gfx->drawRect(win2X + 35, win2Y + 50, 30, 15, BLACK);
  gfx->setCursor(win2X + 45, win2Y + 55);
  gfx->printf("OK");
  
  // Taskbar
  gfx->fillRect(0, gfx->height() - 20, gfx->width(), 20, gfx->color565(100, 100, 150));
  gfx->drawLine(0, gfx->height() - 20, gfx->width(), gfx->height() - 20, BLACK);
  
  // Start button
  gfx->fillRect(5, gfx->height() - 18, 40, 16, gfx->color565(150, 150, 200));
  gfx->drawRect(5, gfx->height() - 18, 40, 16, BLACK);
  gfx->setTextColor(BLACK);
  gfx->setCursor(10, gfx->height() - 14);
  gfx->printf("START");
  
  // Clock in taskbar
  gfx->setTextColor(WHITE);
  gfx->setCursor(gfx->width() - 35, gfx->height() - 14);
  gfx->printf("%02d:%02d", (int)(animationTime/4) % 24, ((int)animationTime*10) % 60);
}

void drawDemoGtkConf() {
  // Demo GTK configuration panel
  gfx->fillScreen(gfx->color565(240, 240, 240)); // Light background
  
  // Configuration window
  int confX = 10;
  int confY = 10;
  int confW = gfx->width() - 20;
  int confH = gfx->height() - 20;
  
  // Window frame
  gfx->fillRect(confX, confY, confW, confH, WHITE);
  gfx->drawRect(confX, confY, confW, confH, BLACK);
  
  // Title bar
  gfx->fillRect(confX + 1, confY + 1, confW - 2, 20, gfx->color565(0, 100, 180));
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(confX + 8, confY + 8);
  gfx->printf("XScreenSaver Settings");
  
  // Close button
  gfx->fillRect(confX + confW - 18, confY + 4, 12, 12, RED);
  gfx->setTextColor(WHITE);
  gfx->setCursor(confX + confW - 15, confY + 7);
  gfx->printf("X");
  
  // Tabs
  for (int tab = 0; tab < 3; tab++) {
    int tabX = confX + 5 + tab * 60;
    int tabY = confY + 25;
    
    uint16_t tabColor = (tab == ((int)(animationTime) % 3)) ? 
      gfx->color565(200, 220, 255) : gfx->color565(220, 220, 220);
    
    gfx->fillRect(tabX, tabY, 55, 20, tabColor);
    gfx->drawRect(tabX, tabY, 55, 20, BLACK);
    
    gfx->setTextColor(BLACK);
    gfx->setCursor(tabX + 8, tabY + 6);
    switch(tab) {
      case 0: gfx->printf("Display"); break;
      case 1: gfx->printf("Savers"); break;
      case 2: gfx->printf("Advanced"); break;
    }
  }
  
  // Settings content area
  gfx->fillRect(confX + 5, confY + 50, confW - 10, confH - 85, 
                gfx->color565(250, 250, 250));
  gfx->drawRect(confX + 5, confY + 50, confW - 10, confH - 85, BLACK);
  
  int currentTab = (int)(animationTime) % 3;
  int contentX = confX + 15;
  int contentY = confY + 60;
  
  gfx->setTextColor(BLACK);
  
  switch(currentTab) {
    case 0: { // Display settings
      gfx->setCursor(contentX, contentY);
      gfx->printf("Screen Saver:");
      
      // Checkbox
      gfx->drawRect(contentX, contentY + 15, 12, 12, BLACK);
      if ((int)(animationTime * 2) % 2 == 0) {
        gfx->fillRect(contentX + 2, contentY + 17, 8, 8, GREEN);
      }
      gfx->setCursor(contentX + 20, contentY + 18);
      gfx->printf("Enabled");
      
      // Slider
      gfx->setCursor(contentX, contentY + 35);
      gfx->printf("Timeout:");
      gfx->drawLine(contentX, contentY + 50, contentX + 100, contentY + 50, BLACK);
      int sliderPos = contentX + (int)((sin(animationTime) * 0.5 + 0.5) * 90);
      gfx->fillCircle(sliderPos, contentY + 50, 4, BLUE);
      break;
    }
      
    case 1: // Screen savers list
      gfx->setCursor(contentX, contentY);
      gfx->printf("Available Savers:");
      
      for (int saver = 0; saver < 6; saver++) {
        int saverY = contentY + 15 + saver * 15;
        bool selected = (saver == ((int)(animationTime * 2) % 6));
        
        if (selected) {
          gfx->fillRect(contentX - 2, saverY - 2, 120, 14, 
                        gfx->color565(0, 120, 255));
          gfx->setTextColor(WHITE);
        } else {
          gfx->setTextColor(BLACK);
        }
        
        gfx->setCursor(contentX, saverY);
        switch(saver) {
          case 0: gfx->printf("Boxed"); break;
          case 1: gfx->printf("Crystal"); break;
          case 2: gfx->printf("Delaunay"); break;
          case 3: gfx->printf("DeepStars"); break;
          case 4: gfx->printf("Cubes"); break;
          case 5: gfx->printf("Deco"); break;
        }
      }
      break;
      
    case 2: // Advanced settings
      gfx->setCursor(contentX, contentY);
      gfx->printf("Advanced Options:");
      
      gfx->setCursor(contentX, contentY + 20);
      gfx->printf("Mode: %s", ((int)(animationTime * 0.5) % 2) ? "Random" : "Sequential");
      
      gfx->setCursor(contentX, contentY + 35);
      gfx->printf("Effects: %d", 54);
      
      gfx->setCursor(contentX, contentY + 50);
      gfx->printf("FPS: %d", (int)(20 + sin(animationTime * 4) * 10));
      break;
  }
  
  // Bottom buttons
  int btnY = confY + confH - 30;
  
  for (int btn = 0; btn < 3; btn++) {
    int btnX = confX + confW - 180 + btn * 55;
    
    gfx->fillRect(btnX, btnY, 50, 20, gfx->color565(220, 220, 220));
    gfx->drawRect(btnX, btnY, 50, 20, BLACK);
    
    gfx->setTextColor(BLACK);
    gfx->setCursor(btnX + 8, btnY + 6);
    switch(btn) {
      case 0: gfx->printf("OK"); break;
      case 1: gfx->printf("Cancel"); break;
      case 2: gfx->printf("Apply"); break;
    }
  }
}

void drawDemon() {
  // Demon effect - dark supernatural entity with glowing eyes
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Dark aura spreading from center
  for (int aura = 1; aura <= 8; aura++) {
    int auraRadius = aura * 15 + sin(animationTime * 3 + aura) * 8;
    uint16_t auraColor = gfx->color565(aura * 8, 0, aura * 4);
    
    if ((int)(animationTime * 6 + aura) % 4 == 0) {
      gfx->drawCircle(centerX, centerY, auraRadius, auraColor);
    }
  }
  
  // Demon's head silhouette
  int headRadius = 35 + sin(animationTime * 2) * 5;
  gfx->fillCircle(centerX, centerY - 20, headRadius, gfx->color565(20, 5, 5));
  
  // Horns
  for (int horn = 0; horn < 2; horn++) {
    int hornX = centerX + (horn == 0 ? -25 : 25);
    int hornY = centerY - 45;
    
    // Horn shape - triangular
    for (int segment = 0; segment < 6; segment++) {
      int segY = hornY - segment * 5;
      int segWidth = 8 - segment;
      
      gfx->fillRect(hornX - segWidth/2, segY, segWidth, 4, 
                    gfx->color565(30 + segment * 5, 0, 0));
    }
    
    // Horn tip glow
    if ((int)(animationTime * 10) % 3 == 0) {
      gfx->fillCircle(hornX, hornY - 30, 3, RED);
    }
  }
  
  // Glowing demon eyes
  for (int eye = 0; eye < 2; eye++) {
    int eyeX = centerX + (eye == 0 ? -15 : 15);
    int eyeY = centerY - 25;
    
    // Eye glow effect
    int glowIntensity = (int)(sin(animationTime * 8 + eye * PI) * 127 + 128);
    
    for (int glow = 3; glow >= 0; glow--) {
      uint16_t glowColor = gfx->color565(
        glowIntensity - glow * 30,
        (glowIntensity - glow * 40) / 4,
        0
      );
      gfx->fillCircle(eyeX, eyeY, 5 + glow * 2, glowColor);
    }
    
    // Pupil
    gfx->fillCircle(eyeX, eyeY, 2, WHITE);
  }
  
  // Demon mouth/sneer
  int mouthY = centerY - 5;
  for (int tooth = 0; tooth < 5; tooth++) {
    int toothX = centerX - 15 + tooth * 8;
    int toothHeight = (tooth == 1 || tooth == 3) ? 8 : 5;
    
    gfx->fillTriangle(toothX, mouthY, 
                      toothX - 2, mouthY + toothHeight,
                      toothX + 2, mouthY + toothHeight, WHITE);
  }
  
  // Flowing demonic energy
  for (int energy = 0; energy < 12; energy++) {
    float energyAngle = energy * PI / 6 + animationTime * 2;
    float energyRadius = 60 + sin(animationTime * 4 + energy) * 20;
    
    int energyX = centerX + cos(energyAngle) * energyRadius;
    int energyY = centerY + sin(energyAngle) * energyRadius;
    
    // Energy orb
    int orbSize = 3 + sin(animationTime * 6 + energy) * 2;
    uint16_t energyColor = gfx->color565(
      200 + sin(energyAngle + animationTime) * 55,
      50,
      100 + cos(energyAngle + animationTime) * 100
    );
    
    gfx->fillCircle(energyX, energyY, orbSize, energyColor);
    
    // Energy trail back to center
    if (energy % 3 == 0) {
      gfx->drawLine(centerX, centerY, energyX, energyY, 
                    gfx->color565(100, 0, 50));
    }
  }
  
  // Hell flames at bottom
  for (int flame = 0; flame < 8; flame++) {
    int flameX = flame * (gfx->width() / 8) + random(-5, 6);
    int flameHeight = 15 + sin(animationTime * 6 + flame) * 8;
    
    // Flame segments
    for (int segment = 0; segment < flameHeight; segment++) {
      int segY = gfx->height() - segment - 1;
      int segWidth = 6 - (segment / 3);
      
      uint16_t flameColor;
      if (segment < flameHeight / 3) {
        flameColor = WHITE; // Hot center
      } else if (segment < flameHeight * 2 / 3) {
        flameColor = YELLOW; // Middle
      } else {
        flameColor = RED; // Outer flame
      }
      
      gfx->fillRect(flameX - segWidth/2, segY, segWidth, 1, flameColor);
    }
  }
  
  // Floating evil symbols
  for (int symbol = 0; symbol < 6; symbol++) {
    float symAngle = symbol * PI / 3 + animationTime * 0.5;
    int symX = centerX + cos(symAngle) * 80;
    int symY = centerY + sin(symAngle) * 60;
    
    // Pentagram-like symbol
    for (int line = 0; line < 5; line++) {
      float lineAngle = line * 2 * PI / 5 + animationTime;
      int x1 = symX + cos(lineAngle) * 8;
      int y1 = symY + sin(lineAngle) * 8;
      int x2 = symX + cos(lineAngle + 2 * PI / 5) * 8;
      int y2 = symY + sin(lineAngle + 2 * PI / 5) * 8;
      
      gfx->drawLine(x1, y1, x2, y2, gfx->color565(150, 0, 100));
    }
  }
  
  // Demon name in fiery text
  gfx->setTextColor(gfx->color565(255, 100, 0));
  gfx->setTextSize(1);
  gfx->setCursor(centerX - 24, gfx->height() - 15);
  gfx->printf("DAEMON");
}

void drawDemoXm() {
  // Demo X Motif - classic X11/Motif interface demo
  gfx->fillScreen(gfx->color565(192, 192, 192)); // Classic X11 gray
  
  // Root window pattern (stippled)
  for (int x = 0; x < gfx->width(); x += 4) {
    for (int y = 0; y < gfx->height(); y += 4) {
      if ((x/4 + y/4) % 2 == 0) {
        gfx->fillRect(x, y, 2, 2, gfx->color565(160, 160, 160));
      }
    }
  }
  
  // Window manager frame - main application
  int mainWinX = 30 + sin(animationTime * 0.4) * 8;
  int mainWinY = 30 + cos(animationTime * 0.3) * 6;
  int mainWinW = 180;
  int mainWinH = 120;
  
  // 3D window frame (raised)
  gfx->fillRect(mainWinX, mainWinY, mainWinW, mainWinH, gfx->color565(192, 192, 192));
  
  // Top and left borders (light)
  gfx->drawLine(mainWinX, mainWinY, mainWinX + mainWinW, mainWinY, WHITE);
  gfx->drawLine(mainWinX, mainWinY, mainWinX, mainWinY + mainWinH, WHITE);
  gfx->drawLine(mainWinX + 1, mainWinY + 1, mainWinX + mainWinW - 1, mainWinY + 1, gfx->color565(223, 223, 223));
  gfx->drawLine(mainWinX + 1, mainWinY + 1, mainWinX + 1, mainWinY + mainWinH - 1, gfx->color565(223, 223, 223));
  
  // Bottom and right borders (dark)
  gfx->drawLine(mainWinX, mainWinY + mainWinH, mainWinX + mainWinW, mainWinY + mainWinH, gfx->color565(128, 128, 128));
  gfx->drawLine(mainWinX + mainWinW, mainWinY, mainWinX + mainWinW, mainWinY + mainWinH, gfx->color565(128, 128, 128));
  gfx->drawLine(mainWinX + 1, mainWinY + mainWinH - 1, mainWinX + mainWinW - 1, mainWinY + mainWinH - 1, gfx->color565(64, 64, 64));
  gfx->drawLine(mainWinX + mainWinW - 1, mainWinY + 1, mainWinX + mainWinW - 1, mainWinY + mainWinH - 1, gfx->color565(64, 64, 64));
  
  // Title bar
  gfx->fillRect(mainWinX + 3, mainWinY + 3, mainWinW - 6, 20, gfx->color565(0, 96, 128));
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(mainWinX + 8, mainWinY + 9);
  gfx->printf("Motif Demo Application");
  
  // System menu box
  gfx->fillRect(mainWinX + 5, mainWinY + 5, 16, 16, gfx->color565(192, 192, 192));
  gfx->drawRect(mainWinX + 5, mainWinY + 5, 16, 16, BLACK);
  gfx->drawLine(mainWinX + 8, mainWinY + 13, mainWinX + 18, mainWinY + 13, BLACK);
  gfx->drawLine(mainWinX + 13, mainWinY + 8, mainWinX + 13, mainWinY + 18, BLACK);
  
  // Menu bar
  gfx->fillRect(mainWinX + 3, mainWinY + 25, mainWinW - 6, 18, gfx->color565(192, 192, 192));
  
  // Draw 3D menu bar
  gfx->drawLine(mainWinX + 3, mainWinY + 25, mainWinX + mainWinW - 3, mainWinY + 25, WHITE);
  gfx->drawLine(mainWinX + 3, mainWinY + 25, mainWinX + 3, mainWinY + 43, WHITE);
  gfx->drawLine(mainWinX + 3, mainWinY + 42, mainWinX + mainWinW - 3, mainWinY + 42, gfx->color565(128, 128, 128));
  gfx->drawLine(mainWinX + mainWinW - 4, mainWinY + 25, mainWinX + mainWinW - 4, mainWinY + 42, gfx->color565(128, 128, 128));
  
  // Menu items
  gfx->setTextColor(BLACK);
  gfx->setCursor(mainWinX + 10, mainWinY + 31);
  gfx->printf("File  Edit  View  Help");
  
  // Motif pushbuttons
  for (int btn = 0; btn < 3; btn++) {
    int btnX = mainWinX + 15 + btn * 50;
    int btnY = mainWinY + 55;
    int btnW = 45;
    int btnH = 25;
    
    bool pressed = ((int)(animationTime * 2) % 3 == btn);
    
    // Button face
    gfx->fillRect(btnX, btnY, btnW, btnH, gfx->color565(192, 192, 192));
    
    if (pressed) {
      // Pressed (inset) appearance
      gfx->drawLine(btnX, btnY, btnX + btnW, btnY, gfx->color565(128, 128, 128));
      gfx->drawLine(btnX, btnY, btnX, btnY + btnH, gfx->color565(128, 128, 128));
      gfx->drawLine(btnX + 1, btnY + 1, btnX + btnW - 1, btnY + 1, gfx->color565(64, 64, 64));
      gfx->drawLine(btnX + 1, btnY + 1, btnX + 1, btnY + btnH - 1, gfx->color565(64, 64, 64));
    } else {
      // Raised appearance
      gfx->drawLine(btnX, btnY, btnX + btnW, btnY, WHITE);
      gfx->drawLine(btnX, btnY, btnX, btnY + btnH, WHITE);
      gfx->drawLine(btnX + 1, btnY + 1, btnX + btnW - 1, btnY + 1, gfx->color565(223, 223, 223));
      gfx->drawLine(btnX + 1, btnY + 1, btnX + 1, btnY + btnH - 1, gfx->color565(223, 223, 223));
    }
    
    gfx->drawLine(btnX, btnY + btnH, btnX + btnW, btnY + btnH, gfx->color565(128, 128, 128));
    gfx->drawLine(btnX + btnW, btnY, btnX + btnW, btnY + btnH, gfx->color565(128, 128, 128));
    gfx->drawLine(btnX + 1, btnY + btnH - 1, btnX + btnW - 1, btnY + btnH - 1, gfx->color565(64, 64, 64));
    gfx->drawLine(btnX + btnW - 1, btnY + 1, btnX + btnW - 1, btnY + btnH - 1, gfx->color565(64, 64, 64));
    
    // Button text
    gfx->setTextColor(BLACK);
    gfx->setCursor(btnX + 8, btnY + 9);
    switch(btn) {
      case 0: gfx->printf("Push Me"); break;
      case 1: gfx->printf("Cancel"); break;  
      case 2: gfx->printf("OK"); break;
    }
  }
  
  // Status area
  gfx->fillRect(mainWinX + 8, mainWinY + 90, mainWinW - 16, 20, WHITE);
  gfx->drawRect(mainWinX + 8, mainWinY + 90, mainWinW - 16, 20, gfx->color565(128, 128, 128));
  gfx->drawLine(mainWinX + 9, mainWinY + 91, mainWinX + mainWinW - 9, mainWinY + 91, gfx->color565(64, 64, 64));
  gfx->drawLine(mainWinX + 9, mainWinY + 91, mainWinX + 9, mainWinY + 109, gfx->color565(64, 64, 64));
  
  gfx->setTextColor(BLACK);
  gfx->setCursor(mainWinX + 12, mainWinY + 97);
  gfx->printf("Motif v1.2 Ready");
  
  // Second window - dialog
  int dlgX = 120 + sin(animationTime * 0.6) * 5;
  int dlgY = 80 + cos(animationTime * 0.8) * 4;
  int dlgW = 100;
  int dlgH = 80;
  
  // Dialog frame
  gfx->fillRect(dlgX, dlgY, dlgW, dlgH, gfx->color565(192, 192, 192));
  
  // 3D frame
  gfx->drawLine(dlgX, dlgY, dlgX + dlgW, dlgY, WHITE);
  gfx->drawLine(dlgX, dlgY, dlgX, dlgY + dlgH, WHITE);
  gfx->drawLine(dlgX, dlgY + dlgH, dlgX + dlgW, dlgY + dlgH, gfx->color565(64, 64, 64));
  gfx->drawLine(dlgX + dlgW, dlgY, dlgX + dlgW, dlgY + dlgH, gfx->color565(64, 64, 64));
  
  // Dialog title
  gfx->fillRect(dlgX + 3, dlgY + 3, dlgW - 6, 16, gfx->color565(0, 96, 128));
  gfx->setTextColor(WHITE);
  gfx->setCursor(dlgX + 8, dlgY + 7);
  gfx->printf("Alert");
  
  // Dialog message
  gfx->setTextColor(BLACK);
  gfx->setCursor(dlgX + 8, dlgY + 28);
  gfx->printf("X11/Motif");
  gfx->setCursor(dlgX + 8, dlgY + 38);
  gfx->printf("Demo Active");
  
  // Dialog OK button
  int okBtnX = dlgX + 35;
  int okBtnY = dlgY + 55;
  gfx->fillRect(okBtnX, okBtnY, 30, 18, gfx->color565(192, 192, 192));
  gfx->drawLine(okBtnX, okBtnY, okBtnX + 30, okBtnY, WHITE);
  gfx->drawLine(okBtnX, okBtnY, okBtnX, okBtnY + 18, WHITE);
  gfx->drawLine(okBtnX, okBtnY + 18, okBtnX + 30, okBtnY + 18, gfx->color565(64, 64, 64));
  gfx->drawLine(okBtnX + 30, okBtnY, okBtnX + 30, okBtnY + 18, gfx->color565(64, 64, 64));
  
  gfx->setTextColor(BLACK);
  gfx->setCursor(okBtnX + 9, okBtnY + 6);
  gfx->printf("OK");
}

void drawDemoXmWidgets() {
  // Demo X Motif Widgets - showcase of classic Motif widgets
  gfx->fillScreen(gfx->color565(192, 192, 192)); // Standard Motif gray
  
  // Main form container
  int formX = 10;
  int formY = 10;
  int formW = gfx->width() - 20;
  int formH = gfx->height() - 20;
  
  // Form background
  gfx->fillRect(formX, formY, formW, formH, gfx->color565(192, 192, 192));
  
  // Form border (inset)
  gfx->drawLine(formX, formY, formX + formW, formY, gfx->color565(128, 128, 128));
  gfx->drawLine(formX, formY, formX, formY + formH, gfx->color565(128, 128, 128));
  gfx->drawLine(formX, formY + formH - 1, formX + formW - 1, formY + formH - 1, WHITE);
  gfx->drawLine(formX + formW - 1, formY, formX + formW - 1, formY + formH - 1, WHITE);
  
  // Title label
  gfx->setTextColor(BLACK);
  gfx->setTextSize(1);
  gfx->setCursor(formX + 10, formY + 8);
  gfx->printf("Motif Widget Demo");
  
  // Toggle buttons (radio buttons)
  for (int radio = 0; radio < 3; radio++) {
    int radioX = formX + 15;
    int radioY = formY + 25 + radio * 20;
    
    // Radio button circle
    gfx->drawCircle(radioX, radioY, 6, BLACK);
    gfx->fillCircle(radioX, radioY, 5, WHITE);
    
    // Selected indicator
    if (radio == ((int)(animationTime) % 3)) {
      gfx->fillCircle(radioX, radioY, 3, BLACK);
    }
    
    // Label
    gfx->setCursor(radioX + 15, radioY - 3);
    switch(radio) {
      case 0: gfx->printf("Option A"); break;
      case 1: gfx->printf("Option B"); break;
      case 2: gfx->printf("Option C"); break;
    }
  }
  
  // Check boxes
  for (int check = 0; check < 2; check++) {
    int checkX = formX + 15;
    int checkY = formY + 95 + check * 20;
    
    // Checkbox square
    gfx->fillRect(checkX - 6, checkY - 6, 12, 12, WHITE);
    gfx->drawRect(checkX - 6, checkY - 6, 12, 12, BLACK);
    gfx->drawLine(checkX - 5, checkY - 5, checkX + 5, checkY - 5, gfx->color565(128, 128, 128));
    gfx->drawLine(checkX - 5, checkY - 5, checkX - 5, checkY + 5, gfx->color565(128, 128, 128));
    
    // Check mark (animated)
    if (((int)(animationTime * 2) % 4) >= check * 2) {
      gfx->drawLine(checkX - 3, checkY, checkX - 1, checkY + 2, BLACK);
      gfx->drawLine(checkX - 1, checkY + 2, checkX + 3, checkY - 2, BLACK);
    }
    
    // Label
    gfx->setCursor(checkX + 10, checkY - 3);
    switch(check) {
      case 0: gfx->printf("Enable Sound"); break;
      case 1: gfx->printf("Auto Save"); break;
    }
  }
  
  // Horizontal scale/slider
  int sliderX = formX + 120;
  int sliderY = formY + 30;
  int sliderW = 80;
  
  gfx->setCursor(sliderX, sliderY - 10);
  gfx->printf("Volume:");
  
  // Scale track
  gfx->fillRect(sliderX, sliderY + 5, sliderW, 8, gfx->color565(160, 160, 160));
  gfx->drawLine(sliderX, sliderY + 5, sliderX + sliderW, sliderY + 5, gfx->color565(64, 64, 64));
  gfx->drawLine(sliderX, sliderY + 5, sliderX, sliderY + 13, gfx->color565(64, 64, 64));
  gfx->drawLine(sliderX, sliderY + 12, sliderX + sliderW, sliderY + 12, WHITE);
  gfx->drawLine(sliderX + sliderW - 1, sliderY + 5, sliderX + sliderW - 1, sliderY + 12, WHITE);
  
  // Slider thumb
  int thumbPos = sliderX + (int)((sin(animationTime) * 0.5 + 0.5) * (sliderW - 12));
  gfx->fillRect(thumbPos, sliderY + 2, 12, 14, gfx->color565(192, 192, 192));
  gfx->drawLine(thumbPos, sliderY + 2, thumbPos + 12, sliderY + 2, WHITE);
  gfx->drawLine(thumbPos, sliderY + 2, thumbPos, sliderY + 16, WHITE);
  gfx->drawLine(thumbPos, sliderY + 15, thumbPos + 12, sliderY + 15, gfx->color565(64, 64, 64));
  gfx->drawLine(thumbPos + 11, sliderY + 2, thumbPos + 11, sliderY + 15, gfx->color565(64, 64, 64));
  
  // Text field
  int textX = sliderX;
  int textY = sliderY + 40;
  int textW = 100;
  int textH = 18;
  
  gfx->setCursor(textX, textY - 12);
  gfx->printf("Text Field:");
  
  gfx->fillRect(textX, textY, textW, textH, WHITE);
  gfx->drawLine(textX, textY, textX + textW, textY, gfx->color565(64, 64, 64));
  gfx->drawLine(textX, textY, textX, textY + textH, gfx->color565(64, 64, 64));
  gfx->drawLine(textX, textY + textH - 1, textX + textW - 1, textY + textH - 1, WHITE);
  gfx->drawLine(textX + textW - 1, textY, textX + textW - 1, textY + textH - 1, WHITE);
  
  // Text content with cursor
  gfx->setTextColor(BLACK);
  gfx->setCursor(textX + 4, textY + 5);
  gfx->printf("Hello World");
  
  // Blinking cursor
  if ((int)(animationTime * 4) % 2 == 0) {
    gfx->drawLine(textX + 70, textY + 3, textX + 70, textY + 15, BLACK);
  }
  
  // List box
  int listX = sliderX;
  int listY = sliderY + 80;
  int listW = 90;
  int listH = 60;
  
  gfx->setCursor(listX, listY - 12);
  gfx->printf("List Box:");
  
  // List background
  gfx->fillRect(listX, listY, listW, listH, WHITE);
  gfx->drawLine(listX, listY, listX + listW, listY, gfx->color565(64, 64, 64));
  gfx->drawLine(listX, listY, listX, listY + listH, gfx->color565(64, 64, 64));
  gfx->drawLine(listX + 1, listY + 1, listX + listW - 1, listY + 1, gfx->color565(128, 128, 128));
  gfx->drawLine(listX + 1, listY + 1, listX + 1, listY + listH - 1, gfx->color565(128, 128, 128));
  
  // List items
  for (int item = 0; item < 4; item++) {
    int itemY = listY + 5 + item * 12;
    bool selected = (item == ((int)(animationTime * 1.5) % 4));
    
    if (selected) {
      gfx->fillRect(listX + 3, itemY - 1, listW - 18, 10, gfx->color565(0, 0, 128));
      gfx->setTextColor(WHITE);
    } else {
      gfx->setTextColor(BLACK);
    }
    
    gfx->setCursor(listX + 5, itemY);
    switch(item) {
      case 0: gfx->printf("Item One"); break;
      case 1: gfx->printf("Item Two"); break;
      case 2: gfx->printf("Item Three"); break;
      case 3: gfx->printf("Item Four"); break;
    }
  }
  
  // Vertical scrollbar for list
  int scrollX = listX + listW - 15;
  gfx->fillRect(scrollX, listY, 15, listH, gfx->color565(192, 192, 192));
  
  // Scroll arrows
  gfx->fillTriangle(scrollX + 7, listY + 3, scrollX + 4, listY + 8, scrollX + 10, listY + 8, BLACK);
  gfx->fillTriangle(scrollX + 7, listY + listH - 3, scrollX + 4, listY + listH - 8, scrollX + 10, listY + listH - 8, BLACK);
  
  // Scroll thumb
  int thumbY = listY + 15 + (int)((sin(animationTime * 2) * 0.5 + 0.5) * 25);
  gfx->fillRect(scrollX + 2, thumbY, 11, 15, gfx->color565(160, 160, 160));
  gfx->drawLine(scrollX + 2, thumbY, scrollX + 13, thumbY, WHITE);
  gfx->drawLine(scrollX + 2, thumbY, scrollX + 2, thumbY + 15, WHITE);
}

void drawDialog() {
  // Dialog effect - animated conversation or system dialog boxes
  gfx->fillScreen(gfx->color565(200, 200, 220)); // Light desktop background
  
  // Desktop pattern
  for (int x = 0; x < gfx->width(); x += 16) {
    for (int y = 0; y < gfx->height(); y += 16) {
      if ((x/16 + y/16) % 2 == 0) {
        gfx->fillRect(x, y, 8, 8, gfx->color565(180, 180, 200));
      }
    }
  }
  
  // Main dialog box
  int dlgX = 40 + sin(animationTime * 0.3) * 10;
  int dlgY = 30 + cos(animationTime * 0.4) * 8;
  int dlgW = 160;
  int dlgH = 120;
  
  // Dialog shadow
  gfx->fillRect(dlgX + 4, dlgY + 4, dlgW, dlgH, gfx->color565(100, 100, 100));
  
  // Dialog background
  gfx->fillRect(dlgX, dlgY, dlgW, dlgH, gfx->color565(240, 240, 240));
  gfx->drawRect(dlgX, dlgY, dlgW, dlgH, BLACK);
  
  // Title bar
  gfx->fillRect(dlgX + 1, dlgY + 1, dlgW - 2, 20, gfx->color565(0, 100, 200));
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(dlgX + 6, dlgY + 7);
  gfx->printf("System Dialog");
  
  // Close button
  gfx->fillRect(dlgX + dlgW - 18, dlgY + 4, 14, 14, RED);
  gfx->setTextColor(WHITE);
  gfx->setCursor(dlgX + dlgW - 15, dlgY + 7);
  gfx->printf("X");
  
  // Dialog icon (animated)
  int iconX = dlgX + 15;
  int iconY = dlgY + 35;
  int iconType = ((int)(animationTime) % 4);
  
  switch(iconType) {
    case 0: // Information icon
      gfx->fillCircle(iconX, iconY, 12, BLUE);
      gfx->setTextColor(WHITE);
      gfx->setCursor(iconX - 3, iconY - 4);
      gfx->printf("i");
      break;
      
    case 1: // Warning icon
      gfx->fillTriangle(iconX, iconY - 12, iconX - 10, iconY + 8, iconX + 10, iconY + 8, YELLOW);
      gfx->drawTriangle(iconX, iconY - 12, iconX - 10, iconY + 8, iconX + 10, iconY + 8, BLACK);
      gfx->setTextColor(BLACK);
      gfx->setCursor(iconX - 2, iconY - 2);
      gfx->printf("!");
      break;
      
    case 2: // Error icon
      gfx->fillCircle(iconX, iconY, 12, RED);
      gfx->setTextColor(WHITE);
      gfx->setCursor(iconX - 3, iconY - 4);
      gfx->printf("X");
      break;
      
    case 3: // Question icon
      gfx->fillCircle(iconX, iconY, 12, GREEN);
      gfx->setTextColor(WHITE);
      gfx->setCursor(iconX - 3, iconY - 4);
      gfx->printf("?");
      break;
  }
  
  // Dialog message (changes with icon type)
  gfx->setTextColor(BLACK);
  gfx->setCursor(dlgX + 40, dlgY + 30);
  
  switch(iconType) {
    case 0:
      gfx->printf("Information:");
      gfx->setCursor(dlgX + 40, dlgY + 42);
      gfx->printf("Process completed");
      gfx->setCursor(dlgX + 40, dlgY + 54);
      gfx->printf("successfully.");
      break;
      
    case 1:
      gfx->printf("Warning:");
      gfx->setCursor(dlgX + 40, dlgY + 42);
      gfx->printf("Low disk space");
      gfx->setCursor(dlgX + 40, dlgY + 54);
      gfx->printf("detected.");
      break;
      
    case 2:
      gfx->printf("Error:");
      gfx->setCursor(dlgX + 40, dlgY + 42);
      gfx->printf("Connection failed.");
      gfx->setCursor(dlgX + 40, dlgY + 54);
      gfx->printf("Retry?");
      break;
      
    case 3:
      gfx->printf("Confirm:");
      gfx->setCursor(dlgX + 40, dlgY + 42);
      gfx->printf("Save changes?");
      break;
  }
  
  // Dialog buttons
  int btn1X = dlgX + 30;
  int btn2X = dlgX + 90;
  int btnY = dlgY + 85;
  int btnW = 45;
  int btnH = 25;
  
  // Button 1
  bool btn1Pressed = ((int)(animationTime * 3) % 6) < 2;
  gfx->fillRect(btn1X, btnY, btnW, btnH, gfx->color565(220, 220, 220));
  
  if (btn1Pressed) {
    gfx->drawLine(btn1X, btnY, btn1X + btnW, btnY, gfx->color565(100, 100, 100));
    gfx->drawLine(btn1X, btnY, btn1X, btnY + btnH, gfx->color565(100, 100, 100));
  } else {
    gfx->drawLine(btn1X, btnY, btn1X + btnW, btnY, WHITE);
    gfx->drawLine(btn1X, btnY, btn1X, btnY + btnH, WHITE);
  }
  
  gfx->drawLine(btn1X, btnY + btnH, btn1X + btnW, btnY + btnH, gfx->color565(100, 100, 100));
  gfx->drawLine(btn1X + btnW, btnY, btn1X + btnW, btnY + btnH, gfx->color565(100, 100, 100));
  gfx->drawRect(btn1X, btnY, btnW, btnH, BLACK);
  
  // Button 2
  bool btn2Pressed = ((int)(animationTime * 3) % 6) >= 4;
  gfx->fillRect(btn2X, btnY, btnW, btnH, gfx->color565(220, 220, 220));
  
  if (btn2Pressed) {
    gfx->drawLine(btn2X, btnY, btn2X + btnW, btnY, gfx->color565(100, 100, 100));
    gfx->drawLine(btn2X, btnY, btn2X, btnY + btnH, gfx->color565(100, 100, 100));
  } else {
    gfx->drawLine(btn2X, btnY, btn2X + btnW, btnY, WHITE);
    gfx->drawLine(btn2X, btnY, btn2X, btnY + btnH, WHITE);
  }
  
  gfx->drawLine(btn2X, btnY + btnH, btn2X + btnW, btnY + btnH, gfx->color565(100, 100, 100));
  gfx->drawLine(btn2X + btnW, btnY, btn2X + btnW, btnY + btnH, gfx->color565(100, 100, 100));
  gfx->drawRect(btn2X, btnY, btnW, btnH, BLACK);
  
  // Button text
  gfx->setTextColor(BLACK);
  
  if (iconType == 3) { // Question dialog
    gfx->setCursor(btn1X + 12, btnY + 9);
    gfx->printf("Yes");
    gfx->setCursor(btn2X + 15, btnY + 9);
    gfx->printf("No");
  } else {
    gfx->setCursor(btn1X + 15, btnY + 9);
    gfx->printf("OK");
    gfx->setCursor(btn2X + 8, btnY + 9);
    gfx->printf("Cancel");
  }
  
  // Progress dialog (occasionally)
  if (((int)(animationTime * 0.5) % 10) > 7) {
    int progX = dlgX + 20;
    int progY = dlgY + 160;
    int progW = 120;
    
    gfx->fillRect(progX - 5, progY - 25, progW + 10, 50, gfx->color565(240, 240, 240));
    gfx->drawRect(progX - 5, progY - 25, progW + 10, 50, BLACK);
    
    gfx->setTextColor(BLACK);
    gfx->setCursor(progX + 10, progY - 18);
    gfx->printf("Processing...");
    
    // Progress bar
    gfx->drawRect(progX, progY, progW, 12, BLACK);
    gfx->fillRect(progX + 1, progY + 1, progW - 2, 10, WHITE);
    
    int progress = (int)((sin(animationTime * 4) * 0.5 + 0.5) * (progW - 4));
    gfx->fillRect(progX + 2, progY + 2, progress, 8, GREEN);
    
    // Percentage
    int percent = (progress * 100) / (progW - 4);
    gfx->setCursor(progX + 45, progY + 18);
    gfx->printf("%d%%", percent);
  }
}

void drawDiscoball() {
  // Discoball effect - spinning mirror ball with light reflections
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Disco ball main sphere
  int ballRadius = 40 + sin(animationTime * 2) * 5;
  
  // Ball background (darker)
  gfx->fillCircle(centerX, centerY, ballRadius, gfx->color565(30, 30, 30));
  
  // Mirror squares on the ball (simulated 3D)
  float rotation = animationTime * 0.5;
  
  for (int lat = -3; lat <= 3; lat++) {
    for (int lon = -6; lon <= 6; lon++) {
      float latAngle = lat * PI / 8;
      float lonAngle = lon * PI / 8 + rotation;
      
      // 3D to 2D projection
      float x3d = cos(latAngle) * cos(lonAngle);
      float y3d = sin(latAngle);
      float z3d = cos(latAngle) * sin(lonAngle);
      
      // Only draw front-facing mirrors
      if (z3d > 0) {
        int mirrorX = centerX + x3d * ballRadius * 0.8;
        int mirrorY = centerY - y3d * ballRadius * 0.8;
        
        // Mirror square size based on distance (3D effect)
        int mirrorSize = 4 + z3d * 3;
        
        // Mirror brightness based on angle to light sources
        float brightness = z3d * 0.8 + 0.2;
        
        // Color cycling for disco effect
        float colorPhase = animationTime * 3 + lon + lat;
        uint16_t mirrorColor = gfx->color565(
          (int)(sin(colorPhase) * 127 + 128) * brightness,
          (int)(sin(colorPhase + 2) * 127 + 128) * brightness,
          (int)(sin(colorPhase + 4) * 127 + 128) * brightness
        );
        
        gfx->fillRect(mirrorX - mirrorSize/2, mirrorY - mirrorSize/2, 
                      mirrorSize, mirrorSize, mirrorColor);
        
        // Add white highlight for extra sparkle
        if (((int)(animationTime * 10 + lon * 3 + lat * 5) % 20) < 2) {
          gfx->fillRect(mirrorX - mirrorSize/2 + 1, mirrorY - mirrorSize/2 + 1, 
                        mirrorSize - 2, mirrorSize - 2, WHITE);
        }
      }
    }
  }
  
  // Light beams from disco ball
  for (int beam = 0; beam < 8; beam++) {
    float beamAngle = beam * PI / 4 + animationTime * 2;
    
    // Beam length varies
    int beamLength = 60 + sin(animationTime * 4 + beam) * 30;
    
    int beamEndX = centerX + cos(beamAngle) * beamLength;
    int beamEndY = centerY + sin(beamAngle) * beamLength;
    
    // Beam color
    uint16_t beamColor = gfx->color565(
      (int)(sin(animationTime * 3 + beam) * 127 + 128),
      (int)(cos(animationTime * 2 + beam) * 127 + 128),
      255
    );
    
    // Draw beam as multiple lines for thickness
    for (int thickness = 0; thickness < 3; thickness++) {
      float offsetAngle = beamAngle + (thickness - 1) * 0.1;
      int thickEndX = centerX + cos(offsetAngle) * beamLength;
      int thickEndY = centerY + sin(offsetAngle) * beamLength;
      
      gfx->drawLine(centerX, centerY, thickEndX, thickEndY, beamColor);
    }
    
    // Spot lights at beam ends
    if (beamEndX >= 0 && beamEndX < gfx->width() && 
        beamEndY >= 0 && beamEndY < gfx->height()) {
      int spotSize = 8 + sin(animationTime * 6 + beam) * 4;
      
      for (int spot = spotSize; spot > 0; spot--) {
        uint16_t spotColor = gfx->color565(
          (beamColor >> 11) * spot / spotSize,
          ((beamColor >> 5) & 0x3F) * spot / spotSize,
          (beamColor & 0x1F) * spot / spotSize
        );
        gfx->drawCircle(beamEndX, beamEndY, spot, spotColor);
      }
    }
  }
  
  // Floor reflections (simplified)
  int floorY = gfx->height() - 30;
  gfx->drawLine(0, floorY, gfx->width(), floorY, gfx->color565(50, 50, 100));
  
  for (int reflect = 0; reflect < 6; reflect++) {
    float reflectAngle = reflect * PI / 3 + animationTime;
    int reflectX = centerX + cos(reflectAngle) * 40;
    int reflectSize = 8 + sin(animationTime * 4 + reflect) * 4;
    
    uint16_t reflectColor = gfx->color565(
      (int)(sin(reflectAngle + animationTime) * 100 + 50),
      (int)(cos(reflectAngle + animationTime + 1) * 100 + 50),
      150
    );
    
    gfx->fillCircle(reflectX, floorY + 10, reflectSize, reflectColor);
  }
  
  // Disco ball chain/mount
  gfx->drawLine(centerX, 0, centerX, centerY - ballRadius - 5, gfx->color565(150, 150, 150));
  for (int chain = 0; chain < 5; chain++) {
    int chainY = chain * 8;
    gfx->fillCircle(centerX, chainY, 2, gfx->color565(200, 200, 200));
  }
  
  // Party atmosphere - random sparkles
  for (int sparkle = 0; sparkle < 15; sparkle++) {
    if (random(0, 100) < 30) {
      int sparkleX = random(0, gfx->width());
      int sparkleY = random(0, gfx->height());
      
      uint16_t sparkleColor = gfx->color565(
        random(150, 255),
        random(150, 255),
        random(150, 255)
      );
      
      gfx->fillCircle(sparkleX, sparkleY, 1, sparkleColor);
    }
  }
  
  // "DISCO" text at bottom
  gfx->setTextColor(gfx->color565(255, 200, 0));
  gfx->setTextSize(1);
  gfx->setCursor(centerX - 15, gfx->height() - 15);
  gfx->printf("DISCO");
}

void drawDiscrete() {
  // Discrete effect - digital/pixel art style with discrete elements
  gfx->fillScreen(gfx->color565(20, 20, 40)); // Dark digital background
  
  // Grid pattern
  for (int x = 0; x < gfx->width(); x += 16) {
    gfx->drawLine(x, 0, x, gfx->height(), gfx->color565(40, 40, 60));
  }
  for (int y = 0; y < gfx->height(); y += 16) {
    gfx->drawLine(0, y, gfx->width(), y, gfx->color565(40, 40, 60));
  }
  
  // Digital blocks - cellular automata style
  static uint8_t cells[20][15]; // Grid cells
  static bool initialized = false;
  
  if (!initialized) {
    for (int x = 0; x < 20; x++) {
      for (int y = 0; y < 15; y++) {
        cells[x][y] = random(0, 100) < 30 ? 1 : 0;
      }
    }
    initialized = true;
  }
  
  // Update cells based on simple rules (every few frames)
  if ((int)(animationTime * 2) % 3 == 0) {
    static uint8_t newCells[20][15];
    
    for (int x = 1; x < 19; x++) {
      for (int y = 1; y < 14; y++) {
        int neighbors = 0;
        
        // Count neighbors
        for (int dx = -1; dx <= 1; dx++) {
          for (int dy = -1; dy <= 1; dy++) {
            if (dx != 0 || dy != 0) {
              neighbors += cells[x + dx][y + dy];
            }
          }
        }
        
        // Simple cellular automata rules
        if (cells[x][y] == 1) {
          newCells[x][y] = (neighbors >= 2 && neighbors <= 3) ? 1 : 0;
        } else {
          newCells[x][y] = (neighbors == 3) ? 1 : 0;
        }
        
        // Add some randomness for more interesting patterns
        if (random(0, 1000) < 5) {
          newCells[x][y] = 1 - newCells[x][y];
        }
      }
    }
    
    // Copy back
    for (int x = 0; x < 20; x++) {
      for (int y = 0; y < 15; y++) {
        cells[x][y] = newCells[x][y];
      }
    }
  }
  
  // Draw cells
  for (int x = 0; x < 20; x++) {
    for (int y = 0; y < 15; y++) {
      if (cells[x][y] == 1) {
        int pixelX = x * 16;
        int pixelY = y * 16;
        
        // Cell age affects color
        int age = (int)(animationTime * 4 + x + y) % 8;
        
        uint16_t cellColor = gfx->color565(
          50 + age * 20,
          100 + age * 15,
          200 - age * 10
        );
        
        gfx->fillRect(pixelX + 2, pixelY + 2, 12, 12, cellColor);
        
        // Digital highlight
        gfx->drawRect(pixelX + 1, pixelY + 1, 14, 14, 
                      gfx->color565(150, 200, 255));
      }
    }
  }
  
  // Discrete mathematical functions visualization
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Plot discrete sine wave
  for (int x = 0; x < gfx->width(); x += 8) {
    float t = (x - centerX) * 0.05 + animationTime;
    int y = centerY + sin(t) * 20;
    
    // Discrete points
    gfx->fillRect(x - 2, y - 2, 4, 4, GREEN);
    gfx->drawRect(x - 3, y - 3, 6, 6, WHITE);
  }
  
  // Digital counter display
  int counterX = 10;
  int counterY = 10;
  
  // Counter background
  gfx->fillRect(counterX, counterY, 80, 20, BLACK);
  gfx->drawRect(counterX, counterY, 80, 20, gfx->color565(0, 255, 0));
  
  // 7-segment style numbers
  int counter = (int)(animationTime * 10) % 10000;
  gfx->setTextColor(gfx->color565(0, 255, 0));
  gfx->setTextSize(1);
  gfx->setCursor(counterX + 5, counterY + 6);
  gfx->printf("%04d", counter);
  
  // Binary representation
  gfx->setCursor(counterX + 50, counterY + 6);
  for (int bit = 7; bit >= 0; bit--) {
    gfx->printf("%d", (counter >> bit) & 1);
  }
  
  // Discrete function graph
  int graphX = gfx->width() - 60;
  int graphY = 20;
  int graphW = 50;
  int graphH = 40;
  
  gfx->drawRect(graphX, graphY, graphW, graphH, WHITE);
  
  // Plot discrete function points
  for (int i = 0; i < graphW; i += 4) {
    float input = i * 0.2 + animationTime;
    int output = graphY + graphH - (int)(sin(input) * graphH/2 + graphH/2);
    
    gfx->fillCircle(graphX + i, output, 2, YELLOW);
    
    // Connect with lines for discrete visualization
    if (i > 0) {
      float prevInput = (i - 4) * 0.2 + animationTime;
      int prevOutput = graphY + graphH - (int)(sin(prevInput) * graphH/2 + graphH/2);
      gfx->drawLine(graphX + i - 4, prevOutput, graphX + i, output, 
                    gfx->color565(100, 100, 200));
    }
  }
  
  // Digital noise pattern
  for (int noise = 0; noise < 30; noise++) {
    int noiseX = random(0, gfx->width() / 4) * 4;
    int noiseY = random(0, gfx->height() / 4) * 4;
    
    if (random(0, 100) < 20) {
      uint16_t noiseColor = gfx->color565(
        random(0, 100),
        random(100, 200),
        random(150, 255)
      );
      gfx->fillRect(noiseX, noiseY, 4, 4, noiseColor);
    }
  }
  
  // Status indicators
  for (int led = 0; led < 8; led++) {
    int ledX = 10 + led * 12;
    int ledY = gfx->height() - 20;
    
    bool ledOn = ((int)(animationTime * 8 + led * 2) % 16) < 8;
    
    uint16_t ledColor = ledOn ? 
      gfx->color565(0, 255, 0) : gfx->color565(0, 50, 0);
    
    gfx->fillCircle(ledX, ledY, 4, ledColor);
    gfx->drawCircle(ledX, ledY, 5, WHITE);
  }
}

void drawDistort() {
  // Distort effect - image distortion and warping patterns
  gfx->fillScreen(gfx->color565(30, 30, 60)); // Dark blue background
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Create a distortion field based on sine waves
  for (int y = 0; y < gfx->height(); y += 2) {
    for (int x = 0; x < gfx->width(); x += 2) {
      // Multiple distortion sources
      float dist1 = sqrt((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY));
      float dist2 = sqrt((x - centerX/2) * (x - centerX/2) + (y - centerY/2) * (y - centerY/2));
      
      // Ripple effects
      float ripple1 = sin(dist1 * 0.2 - animationTime * 3) * 10;
      float ripple2 = cos(dist2 * 0.15 + animationTime * 2) * 8;
      
      // Distorted coordinates
      int distortX = x + ripple1 + sin(y * 0.1 + animationTime) * 5;
      int distortY = y + ripple2 + cos(x * 0.08 - animationTime * 0.8) * 6;
      
      // Bounds checking
      if (distortX >= 0 && distortX < gfx->width() && 
          distortY >= 0 && distortY < gfx->height()) {
        
        // Color based on distortion amount
        float distortion = abs(ripple1) + abs(ripple2);
        
        uint16_t color = gfx->color565(
          (int)(sin(distortion * 0.3 + animationTime) * 127 + 128),
          (int)(cos(distortion * 0.2 - animationTime * 0.7) * 100 + 100),
          (int)(sin(distortion * 0.1 + animationTime * 1.2) * 127 + 128)
        );
        
        gfx->drawPixel(distortX, distortY, color);
      }
    }
  }
  
  // Lens distortion effect in center
  int lensRadius = 40 + sin(animationTime * 2) * 10;
  
  for (int angle = 0; angle < 360; angle += 5) {
    float rad = angle * PI / 180;
    
    for (int r = 0; r < lensRadius; r += 3) {
      // Barrel distortion
      float distortedR = r + sin(r * 0.2 + animationTime * 3) * 3;
      
      int x1 = centerX + cos(rad) * r;
      int y1 = centerY + sin(rad) * r;
      int x2 = centerX + cos(rad) * distortedR;
      int y2 = centerY + sin(rad) * distortedR;
      
      if (x2 >= 0 && x2 < gfx->width() && y2 >= 0 && y2 < gfx->height()) {
        uint16_t lensColor = gfx->color565(
          255 - r * 3,
          150 + r * 2,
          200 + sin(angle + animationTime * 10) * 55
        );
        
        gfx->drawLine(x1, y1, x2, y2, lensColor);
      }
    }
  }
  
  // Wave distortion along edges
  for (int edge = 0; edge < 4; edge++) {
    for (int i = 0; i < 50; i++) {
      float t = i * 0.1 + animationTime * 2;
      int wave = sin(t) * 15;
      
      switch(edge) {
        case 0: // Top
          gfx->drawPixel(i * 6, wave + 10, gfx->color565(255, 150, 0));
          break;
        case 1: // Right
          gfx->drawPixel(gfx->width() - 10 - wave, i * 5, gfx->color565(0, 255, 150));
          break;
        case 2: // Bottom
          gfx->drawPixel(i * 6, gfx->height() - 10 - wave, gfx->color565(150, 0, 255));
          break;
        case 3: // Left
          gfx->drawPixel(10 + wave, i * 5, gfx->color565(255, 255, 0));
          break;
      }
    }
  }
}

void drawDnalogo() {
  // DNA Logo effect - double helix structure with scientific styling
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // DNA helix parameters
  float helixHeight = gfx->height() * 0.8;
  float helixRadius = 30;
  float helixTwist = animationTime * 2;
  
  // Draw double helix
  for (float y = -helixHeight/2; y < helixHeight/2; y += 2) {
    float normalizedY = y / (helixHeight/2);
    
    // Left helix strand
    float angle1 = normalizedY * 4 * PI + helixTwist;
    int x1 = centerX + cos(angle1) * helixRadius;
    int screenY = centerY + y;
    
    // Right helix strand (180 degrees out of phase)
    float angle2 = angle1 + PI;
    int x2 = centerX + cos(angle2) * helixRadius;
    
    // Color gradient along helix
    uint16_t helixColor1 = gfx->color565(
      (int)(sin(normalizedY * 2 + animationTime) * 127 + 128),
      100,
      255
    );
    
    uint16_t helixColor2 = gfx->color565(
      255,
      (int)(cos(normalizedY * 2 + animationTime) * 127 + 128),
      100
    );
    
    // Draw helix strands
    if (screenY >= 0 && screenY < gfx->height()) {
      gfx->fillCircle(x1, screenY, 3, helixColor1);
      gfx->fillCircle(x2, screenY, 3, helixColor2);
      
      // Connect base pairs when strands are close
      if (abs(x1 - x2) < helixRadius) {
        gfx->drawLine(x1, screenY, x2, screenY, 
                      gfx->color565(200, 200, 100));
        
        // Base pair nucleotides
        int midX = (x1 + x2) / 2;
        
        // Different base pairs (A-T, G-C)
        char basePair = ((int)(normalizedY * 10) % 4);
        uint16_t baseColor;
        
        switch(basePair) {
          case 0: baseColor = RED; break;    // A
          case 1: baseColor = BLUE; break;   // T
          case 2: baseColor = GREEN; break;  // G
          case 3: baseColor = YELLOW; break; // C
        }
        
        gfx->fillCircle(midX - 8, screenY, 2, baseColor);
        gfx->fillCircle(midX + 8, screenY, 2, baseColor);
      }
    }
  }
  
  // DNA title and scientific info
  gfx->setTextColor(gfx->color565(0, 255, 100));
  gfx->setTextSize(1);
  gfx->setCursor(centerX - 12, 10);
  gfx->printf("DNA");
  
  gfx->setTextColor(WHITE);
  gfx->setCursor(10, 30);
  gfx->printf("Double Helix");
  
  // Molecular structure indicators
  gfx->setCursor(10, gfx->height() - 40);
  gfx->printf("A-T G-C");
  
  gfx->setCursor(10, gfx->height() - 30);
  gfx->printf("Base Pairs");
  
  // Rotating DNA molecule symbol
  int logoX = gfx->width() - 40;
  int logoY = 40;
  
  for (int i = 0; i < 6; i++) {
    float logoAngle = i * PI / 3 + animationTime * 3;
    int logoRadius = 15;
    
    int atomX = logoX + cos(logoAngle) * logoRadius;
    int atomY = logoY + sin(logoAngle) * logoRadius * 0.3; // Flattened for perspective
    
    uint16_t atomColor = (i % 2 == 0) ? 
      gfx->color565(255, 100, 100) : gfx->color565(100, 100, 255);
    
    gfx->fillCircle(atomX, atomY, 4, atomColor);
    
    // Bonds between atoms
    if (i < 5) {
      float nextAngle = (i + 1) * PI / 3 + animationTime * 3;
      int nextX = logoX + cos(nextAngle) * logoRadius;
      int nextY = logoY + sin(nextAngle) * logoRadius * 0.3;
      
      gfx->drawLine(atomX, atomY, nextX, nextY, WHITE);
    }
  }
  
  // Genetic code scrolling
  int codeY = centerY + 80;
  
  gfx->setTextColor(gfx->color565(100, 255, 100));
  
  char geneticCode[] = "ATCGATCGTAGCTAGCTACGATCG";
  int codeLength = strlen(geneticCode);
  int scrollOffset = (int)(animationTime * 20) % (codeLength * 6);
  
  for (int i = 0; i < 20; i++) {
    int charIndex = (i + scrollOffset / 6) % codeLength;
    char nucleotide = geneticCode[charIndex];
    
    // Color code nucleotides
    uint16_t nucleotideColor;
    switch(nucleotide) {
      case 'A': nucleotideColor = RED; break;
      case 'T': nucleotideColor = BLUE; break;
      case 'G': nucleotideColor = GREEN; break;
      case 'C': nucleotideColor = YELLOW; break;
      default: nucleotideColor = WHITE; break;
    }
    
    gfx->setTextColor(nucleotideColor);
    gfx->setCursor(i * 12 - scrollOffset % 6, codeY);
    gfx->printf("%c", nucleotide);
  }
  
  // Scientific data display
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("3.4A pitch");
  
  gfx->setCursor(80, gfx->height() - 20);
  gfx->printf("36deg/bp");
}

void drawDolphin() {
  // Dolphin effect - animated dolphins swimming with ocean background
  gfx->fillScreen(gfx->color565(0, 50, 100)); // Deep ocean blue
  
  // Ocean depth gradient
  for (int y = 0; y < gfx->height(); y++) {
    int blueIntensity = 50 + (y * 150) / gfx->height();
    uint16_t oceanColor = gfx->color565(0, 20, blueIntensity);
    
    // Add some variation for water texture
    if ((y + (int)(animationTime * 30)) % 8 < 2) {
      oceanColor = gfx->color565(0, 30, blueIntensity + 20);
    }
    
    gfx->drawLine(0, y, gfx->width(), y, oceanColor);
  }
  
  // Water surface waves at top
  for (int x = 0; x < gfx->width(); x += 2) {
    float wave = sin(x * 0.1 + animationTime * 4) * 3 + 
                 cos(x * 0.05 - animationTime * 2) * 2;
    int waveY = 5 + wave;
    
    // Surface light effects
    uint16_t surfaceColor = gfx->color565(100, 150, 255);
    gfx->drawPixel(x, waveY, surfaceColor);
    gfx->drawPixel(x, waveY + 1, gfx->color565(50, 100, 200));
  }
  
  // Main dolphin
  int dolphinX = gfx->width()/2 + sin(animationTime * 0.8) * 60;
  int dolphinY = gfx->height()/2 + cos(animationTime * 0.6) * 20;
  
  // Dolphin body (simplified side view)
  gfx->fillCircle(dolphinX, dolphinY, 20, gfx->color565(150, 150, 200));
  gfx->fillCircle(dolphinX - 15, dolphinY, 12, gfx->color565(150, 150, 200));
  gfx->fillCircle(dolphinX + 18, dolphinY, 8, gfx->color565(150, 150, 200));
  
  // Dolphin belly (lighter)
  gfx->fillCircle(dolphinX, dolphinY + 8, 15, gfx->color565(200, 200, 230));
  gfx->fillCircle(dolphinX - 10, dolphinY + 6, 8, gfx->color565(200, 200, 230));
  
  // Dolphin eye
  gfx->fillCircle(dolphinX + 12, dolphinY - 5, 3, BLACK);
  gfx->fillCircle(dolphinX + 13, dolphinY - 6, 1, WHITE);
  
  // Dolphin beak/rostrum
  gfx->fillTriangle(dolphinX + 25, dolphinY, 
                    dolphinX + 35, dolphinY - 2,
                    dolphinX + 35, dolphinY + 2, 
                    gfx->color565(130, 130, 180));
  
  // Dolphin tail
  int tailOffset = sin(animationTime * 6) * 3;
  gfx->fillTriangle(dolphinX - 35, dolphinY + tailOffset,
                    dolphinX - 25, dolphinY - 8 + tailOffset,
                    dolphinX - 25, dolphinY + 8 + tailOffset,
                    gfx->color565(120, 120, 170));
  
  // Dorsal fin
  gfx->fillTriangle(dolphinX - 5, dolphinY - 20,
                    dolphinX + 5, dolphinY - 25,
                    dolphinX + 10, dolphinY - 15,
                    gfx->color565(130, 130, 180));
  
  // Pectoral fins
  gfx->fillEllipse(dolphinX - 8, dolphinY + 15, 8, 4, gfx->color565(140, 140, 190));
  
  // Second dolphin (smaller, in background)
  int dolphin2X = gfx->width()/3 + cos(animationTime * 1.2) * 40;
  int dolphin2Y = gfx->height()/3 + sin(animationTime * 0.9) * 15;
  
  gfx->fillCircle(dolphin2X, dolphin2Y, 12, gfx->color565(100, 120, 160));
  gfx->fillCircle(dolphin2X - 8, dolphin2Y, 8, gfx->color565(100, 120, 160));
  gfx->fillCircle(dolphin2X + 10, dolphin2Y, 5, gfx->color565(100, 120, 160));
  
  // Bubble trails from dolphins
  for (int bubble = 0; bubble < 8; bubble++) {
    float bubbleAge = animationTime * 3 + bubble;
    int bubbleX = dolphinX - 20 - bubble * 8 + sin(bubbleAge) * 3;
    int bubbleY = dolphinY - 10 + cos(bubbleAge * 1.5) * 5;
    
    if (bubbleX >= 0 && bubbleX < gfx->width() && 
        bubbleY >= 0 && bubbleY < gfx->height()) {
      
      int bubbleSize = 2 + sin(bubbleAge) * 1;
      gfx->fillCircle(bubbleX, bubbleY, bubbleSize, 
                      gfx->color565(150, 200, 255));
      gfx->drawCircle(bubbleX, bubbleY, bubbleSize + 1, 
                      gfx->color565(100, 150, 200));
    }
  }
  
  // Ocean life - fish
  for (int fish = 0; fish < 4; fish++) {
    float fishAngle = fish * PI/2 + animationTime;
    int fishX = gfx->width()/2 + cos(fishAngle) * 80;
    int fishY = gfx->height() * 2/3 + sin(fishAngle * 1.3) * 30;
    
    if (fishX >= 10 && fishX < gfx->width() - 10) {
      // Small fish
      gfx->fillCircle(fishX, fishY, 4, gfx->color565(255, 200, 0));
      
      // Fish tail
      int tailDir = (cos(fishAngle) > 0) ? -1 : 1;
      gfx->fillTriangle(fishX + tailDir * 6, fishY,
                        fishX + tailDir * 10, fishY - 3,
                        fishX + tailDir * 10, fishY + 3,
                        gfx->color565(200, 150, 0));
      
      // Fish eye
      gfx->fillCircle(fishX - tailDir * 2, fishY - 1, 1, BLACK);
    }
  }
  
  // Seaweed/kelp
  for (int kelp = 0; kelp < 3; kelp++) {
    int kelpX = kelp * (gfx->width() / 3) + 50;
    
    for (int segment = 0; segment < 8; segment++) {
      float kelpSway = sin(animationTime * 2 + kelp + segment * 0.3) * 5;
      int segmentX = kelpX + kelpSway;
      int segmentY = gfx->height() - segment * 15;
      
      if (segmentY >= 0) {
        gfx->fillRect(segmentX - 2, segmentY, 4, 12, 
                      gfx->color565(0, 100 + segment * 10, 0));
        
        // Kelp fronds
        if (segment % 2 == 0) {
          gfx->fillCircle(segmentX + 4, segmentY + 6, 2, 
                          gfx->color565(0, 120, 20));
          gfx->fillCircle(segmentX - 4, segmentY + 6, 2, 
                          gfx->color565(0, 120, 20));
        }
      }
    }
  }
  
  // Light rays from surface
  for (int ray = 0; ray < 5; ray++) {
    int rayX = ray * 60 + sin(animationTime + ray) * 10;
    
    for (int depth = 0; depth < gfx->height()/2; depth += 3) {
      int rayIntensity = 255 - (depth * 255) / (gfx->height()/2);
      uint16_t rayColor = gfx->color565(rayIntensity/4, rayIntensity/3, rayIntensity);
      
      gfx->drawPixel(rayX, depth, rayColor);
    }
  }
  
  // "DOLPHIN" text
  gfx->setTextColor(gfx->color565(150, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, gfx->height() - 15);
  gfx->printf("DOLPHIN");
}

void drawDoubletime() {
  // Doubletime effect - dual timepieces showing 12-hour vs 24-hour formats
  gfx->fillScreen(BLACK);
  
  // Left clock - 12-hour format
  int leftCenterX = gfx->width() / 4;
  int leftCenterY = gfx->height() / 2;
  int clockRadius = 50;
  
  // Clock face border
  gfx->drawCircle(leftCenterX, leftCenterY, clockRadius, gfx->color565(200, 200, 200));
  gfx->drawCircle(leftCenterX, leftCenterY, clockRadius - 2, gfx->color565(150, 150, 150));
  
  // 12-hour markers
  for (int h = 0; h < 12; h++) {
    float angle = h * PI / 6 - PI / 2;
    int x1 = leftCenterX + cos(angle) * (clockRadius - 8);
    int y1 = leftCenterY + sin(angle) * (clockRadius - 8);
    int x2 = leftCenterX + cos(angle) * (clockRadius - 15);
    int y2 = leftCenterY + sin(angle) * (clockRadius - 15);
    gfx->drawLine(x1, y1, x2, y2, WHITE);
  }
  
  // Right clock - 24-hour format
  int rightCenterX = 3 * gfx->width() / 4;
  int rightCenterY = gfx->height() / 2;
  
  gfx->drawCircle(rightCenterX, rightCenterY, clockRadius, gfx->color565(200, 100, 100));
  gfx->drawCircle(rightCenterX, rightCenterY, clockRadius - 2, gfx->color565(150, 75, 75));
  
  // 24-hour markers
  for (int h = 0; h < 24; h++) {
    float angle = h * PI / 12 - PI / 2;
    int x1 = rightCenterX + cos(angle) * (clockRadius - 8);
    int y1 = rightCenterY + sin(angle) * (clockRadius - 8);
    int x2 = rightCenterX + cos(angle) * (clockRadius - 15);
    int y2 = rightCenterY + sin(angle) * (clockRadius - 15);
    
    uint16_t color = (h % 2 == 0) ? WHITE : gfx->color565(200, 200, 200);
    gfx->drawLine(x1, y1, x2, y2, color);
  }
  
  // Animated hands based on animationTime
  float timeOffset = animationTime * 2; // Speed up for demo
  
  // Left clock hands (12-hour)
  float hour12 = fmod(timeOffset / 12.0, 1.0);
  float min12 = fmod(timeOffset, 1.0);
  
  float hourAngle12 = hour12 * 2 * PI - PI / 2;
  float minAngle12 = min12 * 2 * PI - PI / 2;
  
  // Hour hand
  int hx12 = leftCenterX + cos(hourAngle12) * 25;
  int hy12 = leftCenterY + sin(hourAngle12) * 25;
  gfx->drawLine(leftCenterX, leftCenterY, hx12, hy12, gfx->color565(255, 200, 0));
  
  // Minute hand
  int mx12 = leftCenterX + cos(minAngle12) * 35;
  int my12 = leftCenterY + sin(minAngle12) * 35;
  gfx->drawLine(leftCenterX, leftCenterY, mx12, my12, gfx->color565(0, 255, 200));
  
  // Right clock hands (24-hour)
  float hour24 = fmod(timeOffset / 24.0, 1.0);
  float min24 = fmod(timeOffset, 1.0);
  
  float hourAngle24 = hour24 * 2 * PI - PI / 2;
  float minAngle24 = min24 * 2 * PI - PI / 2;
  
  // Hour hand
  int hx24 = rightCenterX + cos(hourAngle24) * 25;
  int hy24 = rightCenterY + sin(hourAngle24) * 25;
  gfx->drawLine(rightCenterX, rightCenterY, hx24, hy24, gfx->color565(255, 100, 100));
  
  // Minute hand
  int mx24 = rightCenterX + cos(minAngle24) * 35;
  int my24 = rightCenterY + sin(minAngle24) * 35;
  gfx->drawLine(rightCenterX, rightCenterY, mx24, my24, gfx->color565(100, 255, 100));
  
  // Center dots
  gfx->fillCircle(leftCenterX, leftCenterY, 3, WHITE);
  gfx->fillCircle(rightCenterX, rightCenterY, 3, WHITE);
  
  // Labels
  gfx->setTextColor(gfx->color565(200, 200, 200));
  gfx->setTextSize(1);
  gfx->setCursor(leftCenterX - 15, leftCenterY + 60);
  gfx->printf("12-HR");
  gfx->setCursor(rightCenterX - 15, rightCenterY + 60);
  gfx->printf("24-HR");
}


void drawDpms() {
  // DPMS effect - Display Power Management visualization
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Cycle through power states
  int state = (int)(animationTime * 0.5) % 4;
  const char* states[] = {"ON", "STANDBY", "SUSPEND", "OFF"};
  uint16_t stateColors[] = {
    gfx->color565(0, 255, 0),     // Green - ON
    gfx->color565(255, 255, 0),   // Yellow - STANDBY
    gfx->color565(255, 128, 0),   // Orange - SUSPEND
    gfx->color565(255, 0, 0)      // Red - OFF
  };
  
  // Monitor outline
  gfx->drawRect(centerX - 60, centerY - 40, 120, 80, WHITE);
  gfx->drawRect(centerX - 58, centerY - 38, 116, 76, WHITE);
  
  // Monitor stand
  gfx->drawRect(centerX - 20, centerY + 42, 40, 8, WHITE);
  gfx->drawRect(centerX - 5, centerY + 50, 10, 15, WHITE);
  
  // Screen content based on state
  if (state == 0) { // ON
    // Active screen with content
    gfx->fillRect(centerX - 55, centerY - 35, 110, 70, gfx->color565(0, 50, 100));
    
    // Simulated content
    for (int i = 0; i < 8; i++) {
      int y = centerY - 25 + i * 8;
      int width = 80 + sin(animationTime + i) * 20;
      gfx->drawFastHLine(centerX - width/2, y, width, gfx->color565(100, 150, 255));
    }
  } else if (state == 1) { // STANDBY
    // Dim screen
    gfx->fillRect(centerX - 55, centerY - 35, 110, 70, gfx->color565(20, 20, 20));
    // Blinking cursor
    if ((int)(animationTime * 4) % 2) {
      gfx->fillRect(centerX - 2, centerY - 2, 4, 8, gfx->color565(100, 100, 100));
    }
  } else if (state == 2) { // SUSPEND
    // Very dim screen
    gfx->fillRect(centerX - 55, centerY - 35, 110, 70, gfx->color565(5, 5, 5));
  } else { // OFF
    // Black screen
    gfx->fillRect(centerX - 55, centerY - 35, 110, 70, BLACK);
  }
  
  // Power LED indicator
  int ledX = centerX + 50;
  int ledY = centerY + 35;
  gfx->fillCircle(ledX, ledY, 4, stateColors[state]);
  if (state == 1 && (int)(animationTime * 3) % 2) { // Blinking for standby
    gfx->fillCircle(ledX, ledY, 4, BLACK);
  }
  
  // Power consumption bar
  int consumptionLevels[] = {100, 30, 10, 0}; // Percentage for each state
  int barWidth = consumptionLevels[state] * 80 / 100;
  
  gfx->drawRect(centerX - 40, centerY + 60, 80, 10, WHITE);
  if (barWidth > 0) {
    gfx->fillRect(centerX - 40 + 1, centerY + 61, barWidth - 1, 8, stateColors[state]);
  }
  
  // State label
  gfx->setTextColor(stateColors[state]);
  gfx->setTextSize(2);
  gfx->setCursor(centerX - strlen(states[state]) * 6, centerY - 60);
  gfx->printf("%s", states[state]);
  
  // Power consumption text
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(centerX - 30, centerY + 75);
  gfx->printf("Power: %d%%", consumptionLevels[state]);
}

void drawDpmsProtocol() {
  // DPMS Protocol effect - technical visualization of DPMS communication
  gfx->fillScreen(BLACK);
  
  // Protocol state machine visualization
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Draw state boxes
  const char* states[] = {"ON", "STBY", "SUSP", "OFF"};
  int statePositions[][2] = {{-60, -30}, {60, -30}, {60, 30}, {-60, 30}};
  uint16_t stateColors[] = {
    gfx->color565(0, 255, 0),
    gfx->color565(255, 255, 0),
    gfx->color565(255, 128, 0),
    gfx->color565(255, 0, 0)
  };
  
  int currentState = (int)(animationTime * 0.8) % 4;
  
  for (int i = 0; i < 4; i++) {
    int x = centerX + statePositions[i][0];
    int y = centerY + statePositions[i][1];
    
    uint16_t color = (i == currentState) ? stateColors[i] : gfx->color565(50, 50, 50);
    gfx->drawRect(x - 20, y - 10, 40, 20, color);
    
    if (i == currentState) {
      gfx->fillRect(x - 18, y - 8, 36, 16, gfx->color565(20, 20, 20));
    }
    
    gfx->setTextColor(color);
    gfx->setTextSize(1);
    gfx->setCursor(x - strlen(states[i]) * 3, y - 4);
    gfx->printf("%s", states[i]);
  }
  
  // Draw transition arrows
  uint16_t arrowColor = gfx->color565(100, 100, 255);
  
  // Horizontal arrows
  gfx->drawLine(centerX - 40, centerY - 30, centerX + 40, centerY - 30, arrowColor);
  gfx->drawLine(centerX + 40, centerY + 30, centerX - 40, centerY + 30, arrowColor);
  
  // Vertical arrows
  gfx->drawLine(centerX + 60, centerY - 10, centerX + 60, centerY + 10, arrowColor);
  gfx->drawLine(centerX - 60, centerY + 10, centerX - 60, centerY - 10, arrowColor);
  
  // Signal timing diagram at bottom
  int timelineY = gfx->height() - 30;
  gfx->drawFastHLine(10, timelineY, gfx->width() - 20, WHITE);
  
  // HSYNC and VSYNC signals
  for (int x = 10; x < gfx->width() - 10; x += 4) {
    float phase = (x - 10) * 0.1 + animationTime * 3;
    
    // HSYNC signal
    int hsyncY = timelineY - 15;
    if ((int)phase % 8 < 4) {
      gfx->drawPixel(x, hsyncY, gfx->color565(255, 100, 100));
    }
    
    // VSYNC signal
    int vsyncY = timelineY - 25;
    if ((int)(phase * 0.1) % 2 == 0) {
      gfx->drawPixel(x, vsyncY, gfx->color565(100, 255, 100));
    }
  }
  
  // Labels
  gfx->setTextColor(gfx->color565(255, 100, 100));
  gfx->setTextSize(1);
  gfx->setCursor(10, timelineY - 10);
  gfx->printf("HSYNC");
  
  gfx->setTextColor(gfx->color565(100, 255, 100));
  gfx->setCursor(10, timelineY - 20);
  gfx->printf("VSYNC");
  
  // Protocol header
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("DPMS Protocol State Machine");
}

void drawDrift() {
  // Drift effect - slowly drifting particles and colors
  gfx->fillScreen(BLACK);
  
  static float particles[30][4]; // x, y, vx, vy for each particle
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 30; i++) {
      particles[i][0] = random(0, gfx->width());   // x
      particles[i][1] = random(0, gfx->height());  // y
      particles[i][2] = (random(-50, 51) / 100.0); // vx (slow drift)
      particles[i][3] = (random(-50, 51) / 100.0); // vy (slow drift)
    }
    initialized = true;
  }
  
  // Update and draw particles
  for (int i = 0; i < 30; i++) {
    // Add drift forces
    particles[i][2] += sin(animationTime * 0.3 + i) * 0.01;
    particles[i][3] += cos(animationTime * 0.4 + i * 0.7) * 0.01;
    
    // Apply velocity damping
    particles[i][2] *= 0.99;
    particles[i][3] *= 0.99;
    
    // Update positions
    particles[i][0] += particles[i][2];
    particles[i][1] += particles[i][3];
    
    // Wrap around edges with smooth transition
    if (particles[i][0] < 0) particles[i][0] = gfx->width();
    if (particles[i][0] > gfx->width()) particles[i][0] = 0;
    if (particles[i][1] < 0) particles[i][1] = gfx->height();
    if (particles[i][1] > gfx->height()) particles[i][1] = 0;
    
    // Draw particle with trail
    int x = (int)particles[i][0];
    int y = (int)particles[i][1];
    
    // Color based on position and time
    int red = (int)(sin(animationTime * 0.2 + i * 0.3) * 127 + 128);
    int green = (int)(cos(animationTime * 0.15 + i * 0.5) * 127 + 128);
    int blue = (int)(sin(animationTime * 0.25 + i * 0.8) * 127 + 128);
    
    uint16_t particleColor = gfx->color565(red, green, blue);
    
    // Draw particle with size variation
    int size = 2 + sin(animationTime + i) * 2;
    gfx->fillCircle(x, y, size, particleColor);
    
    // Draw fading trail
    for (int trail = 1; trail <= 3; trail++) {
      int trailX = x - particles[i][2] * trail * 10;
      int trailY = y - particles[i][3] * trail * 10;
      
      if (trailX >= 0 && trailX < gfx->width() && trailY >= 0 && trailY < gfx->height()) {
        uint16_t trailColor = gfx->color565(red/(trail*2), green/(trail*2), blue/(trail*2));
        gfx->fillCircle(trailX, trailY, 1, trailColor);
      }
    }
  }
  
  // Background drift effect - slowly changing gradient
  int gradientOffset = (int)(animationTime * 5) % (gfx->height() * 2);
  
  for (int y = 0; y < gfx->height(); y++) {
    float phase = (y + gradientOffset) * 0.01;
    int bgRed = (int)(sin(phase) * 20 + 25);
    int bgGreen = (int)(cos(phase * 1.3) * 15 + 20);
    int bgBlue = (int)(sin(phase * 0.7) * 25 + 30);
    
    uint16_t bgColor = gfx->color565(bgRed, bgGreen, bgBlue);
    
    // Draw subtle background lines
    if (y % 4 == 0) {
      gfx->drawFastHLine(0, y, gfx->width(), bgColor);
    }
  }
  
  // Floating text that drifts
  gfx->setTextColor(gfx->color565(150, 150, 200));
  gfx->setTextSize(1);
  int textX = gfx->width()/2 + sin(animationTime * 0.1) * 50;
  int textY = gfx->height()/2 + cos(animationTime * 0.08) * 20;
  gfx->setCursor(textX - 15, textY);
  gfx->printf("DRIFT");
}

void drawDropshadow() {
  // Dropshadow effect - objects with realistic drop shadows
  gfx->fillScreen(gfx->color565(220, 220, 240)); // Light background
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Light source position (animated)
  int lightX = centerX + sin(animationTime * 0.5) * 80;
  int lightY = centerY/2 + cos(animationTime * 0.3) * 30;
  
  // Draw light source
  gfx->fillCircle(lightX, lightY, 5, gfx->color565(255, 255, 0));
  gfx->drawCircle(lightX, lightY, 8, gfx->color565(255, 200, 0));
  
  // Object positions and properties
  struct Object {
    int x, y, size;
    uint16_t color;
    float height; // Z position for shadow calculation
  };
  
  Object objects[] = {
    {centerX - 60, centerY - 20, 25, gfx->color565(255, 100, 100), 20},
    {centerX + 40, centerY + 10, 30, gfx->color565(100, 255, 100), 15},
    {centerX - 20, centerY + 40, 20, gfx->color565(100, 100, 255), 25},
    {centerX + 10, centerY - 50, 35, gfx->color565(255, 255, 100), 10}
  };
  
  // Draw shadows first (behind objects)
  for (int i = 0; i < 4; i++) {
    Object &obj = objects[i];
    
    // Calculate shadow offset based on light position and object height
    float shadowOffsetX = (obj.x - lightX) * obj.height / 100.0;
    float shadowOffsetY = (obj.y - lightY) * obj.height / 100.0;
    
    int shadowX = obj.x + shadowOffsetX;
    int shadowY = obj.y + shadowOffsetY;
    
    // Shadow gets larger and more diffuse with height
    int shadowSize = obj.size + obj.height * 0.3;
    
    // Draw shadow with gradient (multiple circles for smooth shadow)
    for (int blur = 0; blur < 8; blur++) {
      int blurSize = shadowSize + blur * 2;
      int alpha = 50 - blur * 6; // Fade out at edges
      if (alpha > 0) {
        uint16_t shadowColor = gfx->color565(alpha, alpha, alpha);
        gfx->drawCircle(shadowX, shadowY, blurSize, shadowColor);
      }
    }
    
    // Core shadow (darkest part)
    uint16_t coreShadowColor = gfx->color565(80, 80, 80);
    gfx->fillCircle(shadowX, shadowY, shadowSize - 5, coreShadowColor);
  }
  
  // Draw objects on top of shadows
  for (int i = 0; i < 4; i++) {
    Object &obj = objects[i];
    
    // Animate object positions slightly
    int animatedX = obj.x + sin(animationTime + i) * 5;
    int animatedY = obj.y + cos(animationTime * 1.2 + i) * 3;
    
    // Calculate lighting on object (brighter side facing light)
    float lightDistance = sqrt((animatedX - lightX) * (animatedX - lightX) + 
                              (animatedY - lightY) * (animatedY - lightY));
    float lightIntensity = 1.0 - (lightDistance / 200.0);
    lightIntensity = constrain(lightIntensity, 0.3, 1.0);
    
    // Extract RGB components and apply lighting
    int r = (obj.color >> 11) & 0x1F;
    int g = (obj.color >> 5) & 0x3F;
    int b = obj.color & 0x1F;
    
    r = (int)(r * lightIntensity);
    g = (int)(g * lightIntensity);
    b = (int)(b * lightIntensity);
    
    uint16_t litColor = gfx->color565(r * 8, g * 4, b * 8);
    
    // Draw main object
    gfx->fillCircle(animatedX, animatedY, obj.size, litColor);
    
    // Highlight (reflection of light source)
    int highlightX = animatedX - (animatedX - lightX) * 0.3;
    int highlightY = animatedY - (animatedY - lightY) * 0.3;
    gfx->fillCircle(highlightX, highlightY, obj.size / 4, WHITE);
    
    // Object outline for definition
    gfx->drawCircle(animatedX, animatedY, obj.size, gfx->color565(50, 50, 50));
  }
  
  // Ground plane indicators
  for (int x = 0; x < gfx->width(); x += 20) {
    gfx->drawFastVLine(x, gfx->height() - 10, 10, gfx->color565(180, 180, 180));
  }
  
  // Title
  gfx->setTextColor(gfx->color565(100, 100, 100));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("DROPSHADOW");
}

void drawDroste() {
  // Droste effect - recursive/infinite loop visual effect
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Recursive rectangles creating infinite zoom effect
  float scale = 1.0;
  float rotation = animationTime * 0.5;
  
  for (int level = 0; level < 8; level++) {
    // Calculate current transformation
    float currentRotation = rotation * (level + 1);
    float currentScale = scale * pow(0.7, level); // Each level is 70% the size
    
    // Rectangle dimensions
    int rectWidth = (int)(80 * currentScale);
    int rectHeight = (int)(60 * currentScale);
    
    // Color shifts through levels
    int red = (int)(sin(currentRotation + level) * 127 + 128);
    int green = (int)(cos(currentRotation + level + 2) * 127 + 128);
    int blue = (int)(sin(currentRotation + level + 4) * 127 + 128);
    
    uint16_t levelColor = gfx->color565(red, green, blue);
    
    // Draw rotated rectangle (approximated with lines)
    float cos_r = cos(currentRotation);
    float sin_r = sin(currentRotation);
    
    // Rectangle corners before rotation
    int corners[4][2] = {
      {-rectWidth/2, -rectHeight/2},
      {rectWidth/2, -rectHeight/2},
      {rectWidth/2, rectHeight/2},
      {-rectWidth/2, rectHeight/2}
    };
    
    // Rotate and translate corners
    int rotatedCorners[4][2];
    for (int i = 0; i < 4; i++) {
      rotatedCorners[i][0] = centerX + corners[i][0] * cos_r - corners[i][1] * sin_r;
      rotatedCorners[i][1] = centerY + corners[i][0] * sin_r + corners[i][1] * cos_r;
    }
    
    // Draw rectangle edges
    for (int i = 0; i < 4; i++) {
      int next = (i + 1) % 4;
      gfx->drawLine(rotatedCorners[i][0], rotatedCorners[i][1],
                    rotatedCorners[next][0], rotatedCorners[next][1], levelColor);
    }
    
    // Draw some fill pattern for inner rectangles
    if (level < 5) {
      // Draw cross pattern inside
      gfx->drawLine(rotatedCorners[0][0], rotatedCorners[0][1],
                    rotatedCorners[2][0], rotatedCorners[2][1], levelColor);
      gfx->drawLine(rotatedCorners[1][0], rotatedCorners[1][1],
                    rotatedCorners[3][0], rotatedCorners[3][1], levelColor);
    }
  }
  
  // Spiral elements connecting the rectangles
  for (int spiral = 0; spiral < 3; spiral++) {
    float spiralPhase = animationTime * (spiral + 1) * 0.3;
    
    for (int point = 0; point < 50; point++) {
      float t = point * 0.1 + spiralPhase;
      float spiralRadius = t * 3;
      
      if (spiralRadius > 120) continue; // Don't go beyond screen
      
      int spiralX = centerX + cos(t) * spiralRadius;
      int spiralY = centerY + sin(t) * spiralRadius;
      
      // Color fades as spiral expands
      int intensity = (int)(255 * (1.0 - spiralRadius / 120.0));
      uint16_t spiralColor = gfx->color565(intensity, intensity/2, intensity/2);
      
      gfx->fillCircle(spiralX, spiralY, 2, spiralColor);
    }
  }
  
  // Central focal point that pulsates
  int pulseRadius = 5 + sin(animationTime * 4) * 3;
  gfx->fillCircle(centerX, centerY, pulseRadius, WHITE);
  gfx->drawCircle(centerX, centerY, pulseRadius + 2, gfx->color565(255, 200, 0));
  
  // Infinity symbol overlay
  float infinityScale = 0.5;
  for (int t = 0; t < 360; t += 3) {
    float rad = t * PI / 180.0;
    float x = cos(rad) / (1 + sin(rad) * sin(rad)) * 40 * infinityScale;
    float y = sin(rad) * cos(rad) / (1 + sin(rad) * sin(rad)) * 40 * infinityScale;
    
    int plotX = centerX + x;
    int plotY = centerY + y;
    
    if (plotX >= 0 && plotX < gfx->width() && plotY >= 0 && plotY < gfx->height()) {
      gfx->fillCircle(plotX, plotY, 1, gfx->color565(255, 255, 100));
    }
  }
  
  // Title with recursive text effect
  gfx->setTextColor(gfx->color565(200, 200, 200));
  gfx->setTextSize(1);
  for (int textLevel = 0; textLevel < 3; textLevel++) {
    int textScale = textLevel + 1;
    int textOffset = textLevel * 2;
    gfx->setCursor(10 + textOffset, 10 + textOffset);
    uint16_t textColor = gfx->color565(200/(textScale), 200/(textScale), 200/(textScale));
    gfx->setTextColor(textColor);
    gfx->printf("DROSTE");
  }
}

void drawDymaxionmapCoords() {
  // Dymaxion map coordinate system effect
  gfx->fillScreen(BLACK);
  
  static float rotation = 0;
  rotation += 0.02;
  
  // Draw geodesic grid pattern
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int i = 0; i < 12; i++) {
    float angle = i * PI / 6 + rotation;
    float radius = 80 + sin(animationTime * 2 + i) * 20;
    
    int x1 = centerX + cos(angle) * radius;
    int y1 = centerY + sin(angle) * radius;
    
    // Connect to center
    gfx->drawLine(centerX, centerY, x1, y1, gfx->color565(0, 150, 255));
    
    // Connect adjacent points
    float nextAngle = (i + 1) * PI / 6 + rotation;
    int x2 = centerX + cos(nextAngle) * radius;
    int y2 = centerY + sin(nextAngle) * radius;
    gfx->drawLine(x1, y1, x2, y2, gfx->color565(100, 255, 100));
    
    // Draw coordinate markers
    gfx->fillCircle(x1, y1, 3, gfx->color565(255, 200, 0));
  }
  
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("DYMAXION COORDS");
}

void drawEasing() {
  // Easing function demonstration effect
  gfx->fillScreen(BLACK);
  
  static float phase = 0;
  phase += 0.03;
  
  // Show different easing curves
  for (int curve = 0; curve < 5; curve++) {
    float t = fmod(phase + curve * 0.5, 2.0);
    if (t > 1.0) t = 2.0 - t; // Bounce back
    
    float easedValue;
    switch(curve) {
      case 0: easedValue = t; break; // Linear
      case 1: easedValue = t * t; break; // Quad
      case 2: easedValue = t * t * t; break; // Cubic
      case 3: easedValue = sin(t * PI / 2); break; // Sine
      case 4: easedValue = 1 - cos(t * PI / 2); break; // Cosine
    }
    
    int x = 20 + curve * 50;
    int y = gfx->height() - 20 - easedValue * (gfx->height() - 40);
    
    // Draw easing curve path
    for (int i = 0; i < 100; i++) {
      float plotT = i / 100.0;
      float plotEased;
      switch(curve) {
        case 0: plotEased = plotT; break;
        case 1: plotEased = plotT * plotT; break; 
        case 2: plotEased = plotT * plotT * plotT; break;
        case 3: plotEased = sin(plotT * PI / 2); break;
        case 4: plotEased = 1 - cos(plotT * PI / 2); break;
      }
      int plotX = x;
      int plotY = gfx->height() - 20 - plotEased * (gfx->height() - 40);
      gfx->fillCircle(plotX + i/5, plotY, 1, gfx->color565(100, 100, 255));
    }
    
    // Moving dot
    gfx->fillCircle(x, y, 4, gfx->color565(255, 100 + curve * 30, 100));
  }
  
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("EASING");
}

void drawEndgame() {
  // Endgame effect - final battle visualization
  gfx->fillScreen(BLACK);
  
  static float battlePhase = 0;
  battlePhase += 0.05;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Central explosion pattern
  for (int ray = 0; ray < 16; ray++) {
    float angle = ray * PI / 8 + battlePhase;
    float intensity = sin(battlePhase * 3 + ray) * 0.5 + 0.5;
    int rayLength = 60 + intensity * 40;
    
    int x1 = centerX + cos(angle) * 20;
    int y1 = centerY + sin(angle) * 20;
    int x2 = centerX + cos(angle) * rayLength;
    int y2 = centerY + sin(angle) * rayLength;
    
    uint16_t rayColor = gfx->color565(255 * intensity, 100, 100 + intensity * 155);
    gfx->drawLine(x1, y1, x2, y2, rayColor);
    
    // Particles at ray ends
    if (random(100) < 30) {
      gfx->fillCircle(x2 + random(10) - 5, y2 + random(10) - 5, 2, rayColor);
    }
  }
  
  // Debris field
  for (int debris = 0; debris < 20; debris++) {
    float debrisAngle = debris * PI / 10 + battlePhase * 2;
    float debrisRadius = 80 + sin(battlePhase + debris) * 30;
    
    int debrisX = centerX + cos(debrisAngle) * debrisRadius;
    int debrisY = centerY + sin(debrisAngle) * debrisRadius;
    
    gfx->fillRect(debrisX - 2, debrisY - 2, 4, 4, gfx->color565(150, 150, 150));
  }
  
  // Central core pulsing
  int coreRadius = 8 + sin(battlePhase * 6) * 4;
  gfx->fillCircle(centerX, centerY, coreRadius, WHITE);
  gfx->drawCircle(centerX, centerY, coreRadius + 3, gfx->color565(255, 0, 0));
  
  gfx->setTextColor(gfx->color565(255, 200, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("ENDGAME");
}

void drawEnergystream() {
  // Energy stream effect - flowing energy particles
  gfx->fillScreen(BLACK);
  
  static float streamPhase = 0;
  streamPhase += 0.08;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple energy streams flowing from center
  for (int stream = 0; stream < 6; stream++) {
    float baseAngle = stream * PI / 3;
    
    // Stream particles
    for (int particle = 0; particle < 20; particle++) {
      float particlePhase = streamPhase + particle * 0.3;
      float distance = fmod(particlePhase * 30, 120);
      
      if (distance > 100) continue; // Fade out at edge
      
      float angle = baseAngle + sin(particlePhase) * 0.2; // Slight wobble
      
      int x = centerX + cos(angle) * distance;
      int y = centerY + sin(angle) * distance;
      
      // Energy color - bright core with glow
      float intensity = 1.0 - distance / 100.0;
      int r = 255 * intensity;
      int g = 200 * intensity;
      int b = 100 + 155 * intensity;
      
      gfx->fillCircle(x, y, 2 + intensity * 2, gfx->color565(r, g, b));
      
      // Glow effect
      if (intensity > 0.5) {
        gfx->drawCircle(x, y, 4, gfx->color565(r/3, g/3, b/3));
      }
    }
  }
  
  // Central energy core
  int coreSize = 6 + sin(streamPhase * 4) * 2;
  gfx->fillCircle(centerX, centerY, coreSize, WHITE);
  gfx->drawCircle(centerX, centerY, coreSize + 2, gfx->color565(255, 255, 0));
  
  gfx->setTextColor(gfx->color565(200, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("ENERGYSTREAM");
}

void drawEngine() {
  // Engine effect - mechanical parts in motion
  gfx->fillScreen(BLACK);
  
  static float engineTime = 0;
  engineTime += 0.1;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Main engine cylinder
  int cylinderRadius = 40;
  gfx->drawCircle(centerX, centerY, cylinderRadius, gfx->color565(150, 150, 150));
  gfx->drawCircle(centerX, centerY, cylinderRadius - 5, gfx->color565(100, 100, 100));
  
  // Piston movement
  float pistonOffset = sin(engineTime * 2) * 15;
  
  // Piston rod
  gfx->drawLine(centerX, centerY - cylinderRadius, 
                centerX, centerY - cylinderRadius - 30 + pistonOffset, 
                gfx->color565(180, 180, 180));
  
  // Piston head
  gfx->fillRect(centerX - 8, centerY - cylinderRadius - 35 + pistonOffset, 16, 8, 
                gfx->color565(200, 200, 200));
  
  // Connecting rods (4 directions)
  for (int i = 0; i < 4; i++) {
    float angle = i * PI / 2 + engineTime;
    float rodLength = 25 + sin(engineTime * 3 + i) * 5;
    
    int x1 = centerX + cos(angle) * cylinderRadius;
    int y1 = centerY + sin(angle) * cylinderRadius;
    int x2 = centerX + cos(angle) * (cylinderRadius + rodLength);
    int y2 = centerY + sin(angle) * (cylinderRadius + rodLength);
    
    gfx->drawLine(x1, y1, x2, y2, gfx->color565(120, 120, 120));
    gfx->fillCircle(x2, y2, 3, gfx->color565(160, 160, 160));
  }
  
  // Flywheel
  int flywheelRadius = 20;
  gfx->drawCircle(centerX - 60, centerY, flywheelRadius, gfx->color565(100, 100, 100));
  
  // Flywheel spokes (rotating)
  for (int spoke = 0; spoke < 6; spoke++) {
    float spokeAngle = spoke * PI / 3 + engineTime * 4;
    int sx1 = centerX - 60 + cos(spokeAngle) * 5;
    int sy1 = centerY + sin(spokeAngle) * 5;
    int sx2 = centerX - 60 + cos(spokeAngle) * 18;
    int sy2 = centerY + sin(spokeAngle) * 18;
    
    gfx->drawLine(sx1, sy1, sx2, sy2, gfx->color565(140, 140, 140));
  }
  
  // Steam/exhaust particles
  for (int steam = 0; steam < 8; steam++) {
    float steamY = centerY - cylinderRadius - 40 + pistonOffset - steam * 8;
    if (steamY < 0) continue;
    
    int steamX = centerX + random(20) - 10;
    int steamAlpha = 255 - steam * 30;
    if (steamAlpha > 0) {
      gfx->fillCircle(steamX, steamY, 1 + random(2), 
                      gfx->color565(steamAlpha, steamAlpha, steamAlpha));
    }
  }
  
  gfx->setTextColor(gfx->color565(180, 180, 180));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("ENGINE");
}

void drawEpicycle() {
  // Epicycle effect - circles rolling around circles
  gfx->fillScreen(BLACK);
  
  static float epicycleTime = 0;
  epicycleTime += 0.03;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Main circle (deferent)
  int mainRadius = 60;
  gfx->drawCircle(centerX, centerY, mainRadius, gfx->color565(100, 100, 100));
  
  // Multiple epicycles
  for (int epic = 0; epic < 3; epic++) {
    float baseAngle = epicycleTime * (epic + 1) * 0.7;
    int epicycleRadius = 20 + epic * 10;
    
    // Center of epicycle on main circle
    int epicX = centerX + cos(baseAngle) * mainRadius;
    int epicY = centerY + sin(baseAngle) * mainRadius;
    
    // Draw epicycle circle
    gfx->drawCircle(epicX, epicY, epicycleRadius, 
                    gfx->color565(150 - epic * 30, 150 + epic * 30, 255 - epic * 50));
    
    // Point on epicycle
    float epicAngle = epicycleTime * (5 - epic) * 2; // Different speeds
    int pointX = epicX + cos(epicAngle) * epicycleRadius;
    int pointY = epicY + sin(epicAngle) * epicycleRadius;
    
    // Draw the traced path (simplified - just current points)
    for (int trail = 0; trail < 50; trail++) {
      float trailTime = epicycleTime - trail * 0.02;
      float trailBaseAngle = trailTime * (epic + 1) * 0.7;
      float trailEpicAngle = trailTime * (5 - epic) * 2;
      
      int trailEpicX = centerX + cos(trailBaseAngle) * mainRadius;
      int trailEpicY = centerY + sin(trailBaseAngle) * mainRadius;
      int trailPointX = trailEpicX + cos(trailEpicAngle) * epicycleRadius;
      int trailPointY = trailEpicY + sin(trailEpicAngle) * epicycleRadius;
      
      int alpha = 255 - trail * 5;
      if (alpha > 0) {
        uint16_t trailColor = gfx->color565(alpha/4 + epic * 20, alpha/4, alpha/2);
        gfx->fillCircle(trailPointX, trailPointY, 1, trailColor);
      }
    }
    
    // Current point (brighter)
    gfx->fillCircle(pointX, pointY, 3, 
                    gfx->color565(255, 200 - epic * 50, 100 + epic * 50));
  }
  
  // Center point
  gfx->fillCircle(centerX, centerY, 2, WHITE);
  
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("EPICYCLE");
}

void drawEruption() {
  // Volcanic eruption effect
  gfx->fillScreen(BLACK);
  
  static float eruptionTime = 0;
  eruptionTime += 0.1;
  
  int groundLevel = gfx->height() - 30;
  int volcanoX = gfx->width() / 2;
  
  // Volcano shape
  for (int i = 0; i < 60; i++) {
    int x1 = volcanoX - 30 + i;
    int y1 = groundLevel - (30 - abs(30 - i)) / 2;
    gfx->drawLine(x1, y1, x1, groundLevel, gfx->color565(100, 50, 30));
  }
  
  // Lava particles erupting
  for (int particle = 0; particle < 20; particle++) {
    float particlePhase = eruptionTime + particle * 0.3;
    float height = sin(particlePhase * 2) * 80 + 40;
    float spread = cos(particlePhase) * 50;
    
    if (height > 0) {
      int x = volcanoX + spread;
      int y = groundLevel - 15 - height;
      
      // Lava color - hot to cooling
      float temp = height / 120.0;
      int r = 255;
      int g = 100 + temp * 155;
      int b = temp * 100;
      
      gfx->fillCircle(x, y, 2 + temp * 2, gfx->color565(r, g, b));
      
      // Sparks
      if (random(100) < 30) {
        gfx->fillCircle(x + random(10) - 5, y + random(10) - 5, 1, 
                        gfx->color565(255, 200, 0));
      }
    }
  }
  
  // Smoke
  for (int smoke = 0; smoke < 10; smoke++) {
    float smokeY = groundLevel - 15 - smoke * 12 - sin(eruptionTime + smoke) * 10;
    if (smokeY < 0) continue;
    
    int smokeX = volcanoX + random(40) - 20;
    int alpha = 200 - smoke * 20;
    if (alpha > 0) {
      gfx->fillCircle(smokeX, smokeY, 3 + smoke/3, gfx->color565(alpha/3, alpha/3, alpha/3));
    }
  }
  
  gfx->setTextColor(gfx->color565(255, 100, 100));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("ERUPTION");
}

void drawEuler2d() {
  // 2D Euler fluid dynamics simulation
  gfx->fillScreen(BLACK);
  
  static float fluidTime = 0;
  fluidTime += 0.05;
  
  // Fluid grid simulation (simplified)
  for (int x = 0; x < gfx->width(); x += 8) {
    for (int y = 0; y < gfx->height(); y += 8) {
      // Velocity field
      float vx = sin(x * 0.02 + fluidTime) * cos(y * 0.03);
      float vy = cos(x * 0.03 - fluidTime) * sin(y * 0.02);
      
      // Pressure field
      float pressure = sin(x * 0.05 + fluidTime * 2) * cos(y * 0.05 + fluidTime);
      
      // Color based on fluid properties
      int r = (int)(abs(vx) * 255);
      int g = (int)(abs(vy) * 255);
      int b = (int)((pressure + 1) * 127);
      
      gfx->fillRect(x, y, 6, 6, gfx->color565(r, g, b));
      
      // Velocity vectors
      if (abs(vx) > 0.3 || abs(vy) > 0.3) {
        int endX = x + vx * 10;
        int endY = y + vy * 10;
        gfx->drawLine(x + 3, y + 3, endX, endY, WHITE);
      }
    }
  }
  
  // Fluid streamlines
  for (int stream = 0; stream < 5; stream++) {
    float startX = 10 + stream * 60;
    float currentX = startX;
    float currentY = 50;
    
    for (int step = 0; step < 30; step++) {
      float vx = sin(currentX * 0.02 + fluidTime) * cos(currentY * 0.03);
      float vy = cos(currentX * 0.03 - fluidTime) * sin(currentY * 0.02);
      
      float nextX = currentX + vx * 3;
      float nextY = currentY + vy * 3;
      
      if (nextX > 0 && nextX < gfx->width() && nextY > 0 && nextY < gfx->height()) {
        gfx->drawLine(currentX, currentY, nextX, nextY, 
                      gfx->color565(255 - step * 8, 200, 255 - step * 5));
        currentX = nextX;
        currentY = nextY;
      } else break;
    }
  }
  
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("EULER2D");
}

void drawFadeplot() {
  // Fading plot visualization
  gfx->fillScreen(BLACK);
  
  static float plotTime = 0;
  static int plotHistory[320]; // Store previous values
  plotTime += 0.1;
  
  // Generate new plot value
  float newValue = sin(plotTime) * cos(plotTime * 0.7) * sin(plotTime * 0.3);
  int plotY = gfx->height() / 2 + newValue * (gfx->height() / 3);
  
  // Shift history and add new point
  for (int i = 0; i < gfx->width() - 1; i++) {
    plotHistory[i] = plotHistory[i + 1];
  }
  plotHistory[gfx->width() - 1] = plotY;
  
  // Draw fading plot lines
  for (int x = 1; x < gfx->width(); x++) {
    if (plotHistory[x] != 0 && plotHistory[x-1] != 0) {
      // Fade based on age
      float fade = (float)x / gfx->width();
      int alpha = fade * 255;
      
      // Multiple colored plots
      gfx->drawLine(x-1, plotHistory[x-1], x, plotHistory[x], 
                    gfx->color565(alpha, alpha/2, 255 - alpha/2));
      
      // Secondary plot (phase shifted)
      int plotY2 = gfx->height() / 2 + sin(plotTime - x * 0.01) * (gfx->height() / 4);
      if (x > 1) {
        gfx->drawLine(x-1, gfx->height() / 2 + sin(plotTime - (x-1) * 0.01) * (gfx->height() / 4), 
                      x, plotY2, gfx->color565(255 - alpha, alpha, alpha/2));
      }
    }
  }
  
  // Grid lines
  for (int y = 0; y < gfx->height(); y += 20) {
    gfx->drawFastHLine(0, y, gfx->width(), gfx->color565(30, 30, 30));
  }
  for (int x = 0; x < gfx->width(); x += 40) {
    gfx->drawFastVLine(x, 0, gfx->height(), gfx->color565(30, 30, 30));
  }
  
  // Current value indicator
  gfx->fillCircle(gfx->width() - 5, plotY, 3, gfx->color565(255, 255, 0));
  
  gfx->setTextColor(gfx->color565(200, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FADEPLOT");
}

void drawEsper() {
  // ESP-based psychic visualization
  gfx->fillScreen(BLACK);
  
  static float esperTime = 0;
  esperTime += 0.08;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Psychic energy rings
  for (int ring = 0; ring < 6; ring++) {
    float ringPhase = esperTime + ring * 0.5;
    int radius = 20 + ring * 15 + sin(ringPhase) * 10;
    
    // Pulsing energy rings
    for (int segment = 0; segment < 24; segment++) {
      float angle = segment * PI / 12 + ringPhase * 0.3;
      float intensity = sin(ringPhase + segment * 0.2) * 0.5 + 0.5;
      
      if (intensity > 0.3) {
        int x = centerX + cos(angle) * radius;
        int y = centerY + sin(angle) * radius;
        
        int r = 100 + intensity * 155;
        int g = 50 + intensity * 100;
        int b = 200 + intensity * 55;
        
        gfx->fillCircle(x, y, 2 + intensity * 2, gfx->color565(r, g, b));
      }
    }
  }
  
  // Mind waves
  for (int wave = 0; wave < 8; wave++) {
    float wavePhase = esperTime * 2 + wave;
    int waveY = centerY + sin(wavePhase) * 60;
    
    for (int x = 0; x < gfx->width(); x += 4) {
      float waveValue = sin(x * 0.05 + wavePhase) * sin(wavePhase + wave);
      if (abs(waveValue) > 0.5) {
        int alpha = abs(waveValue) * 255;
        gfx->fillRect(x, waveY - 1, 2, 3, gfx->color565(alpha, alpha/3, alpha));
      }
    }
  }
  
  // Central consciousness
  int coreSize = 8 + sin(esperTime * 3) * 4;
  gfx->fillCircle(centerX, centerY, coreSize, gfx->color565(255, 255, 255));
  gfx->drawCircle(centerX, centerY, coreSize + 3, gfx->color565(150, 100, 255));
  
  gfx->setTextColor(gfx->color565(200, 150, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("ESPER");
}

void drawEtruscanvenus() {
  // Etruscan Venus classical art effect
  gfx->fillScreen(BLACK);
  
  static float venusTime = 0;
  venusTime += 0.04;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Classical marble base
  gfx->fillRect(0, gfx->height() - 40, gfx->width(), 40, gfx->color565(200, 190, 180));
  
  // Venus silhouette (simplified)
  int venusH = 100;
  int venusW = 40;
  
  // Body outline
  for (int y = 0; y < venusH; y++) {
    float bodyWidth = sin((float)y / venusH * PI) * venusW;
    int x1 = centerX - bodyWidth / 2;
    int x2 = centerX + bodyWidth / 2;
    
    // Marble texture
    float marble = sin(venusTime + y * 0.1) * 0.2 + 0.8;
    int marbleColor = marble * 220;
    
    gfx->drawLine(x1, centerY - venusH/2 + y, x2, centerY - venusH/2 + y, 
                  gfx->color565(marbleColor, marbleColor - 20, marbleColor - 40));
  }
  
  // Etruscan decorative patterns
  for (int pattern = 0; pattern < 4; pattern++) {
    float patternAngle = pattern * PI / 2 + venusTime * 0.5;
    int patternRadius = 80 + sin(venusTime + pattern) * 20;
    
    for (int segment = 0; segment < 16; segment++) {
      float segmentAngle = patternAngle + segment * PI / 8;
      int x1 = centerX + cos(segmentAngle) * patternRadius;
      int y1 = centerY + sin(segmentAngle) * patternRadius;
      int x2 = centerX + cos(segmentAngle + 0.2) * (patternRadius + 10);
      int y2 = centerY + sin(segmentAngle + 0.2) * (patternRadius + 10);
      
      gfx->drawLine(x1, y1, x2, y2, gfx->color565(180, 140, 100));
    }
  }
  
  // Golden ratio spiral
  float goldenAngle = venusTime;
  for (int spiral = 0; spiral < 50; spiral++) {
    float radius = spiral * 2;
    float angle = goldenAngle + spiral * 0.618 * 2;
    
    int x = centerX + cos(angle) * radius;
    int y = centerY + sin(angle) * radius;
    
    if (x >= 0 && x < gfx->width() && y >= 0 && y < gfx->height()) {
      gfx->fillCircle(x, y, 1, gfx->color565(220, 180, 120));
    }
  }
  
  gfx->setTextColor(gfx->color565(200, 160, 120));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("ETRUSCANVENUS");
}

void drawExec() {
  // Process execution visualization
  gfx->fillScreen(BLACK);
  
  static float execTime = 0;
  execTime += 0.12;
  
  // Terminal window
  gfx->drawRect(10, 10, gfx->width() - 20, gfx->height() - 20, gfx->color565(100, 100, 100));
  gfx->fillRect(11, 11, gfx->width() - 22, gfx->height() - 22, gfx->color565(10, 10, 10));
  
  // Process bars
  for (int proc = 0; proc < 8; proc++) {
    int y = 25 + proc * 25;
    float activity = sin(execTime + proc * 0.5) * 0.5 + 0.5;
    int barWidth = activity * (gfx->width() - 40);
    
    // Process name
    gfx->setTextColor(gfx->color565(0, 255, 0));
    gfx->setTextSize(1);
    gfx->setCursor(15, y);
    gfx->printf("PID%d", 1000 + proc);
    
    // Activity bar
    int r = activity * 255;
    int g = 255 - activity * 100;
    int b = 50;
    
    gfx->fillRect(60, y, barWidth, 8, gfx->color565(r, g, b));
    gfx->drawRect(60, y, gfx->width() - 80, 8, gfx->color565(60, 60, 60));
    
    // CPU percentage
    gfx->setCursor(gfx->width() - 50, y);
    gfx->printf("%d%%", (int)(activity * 100));
  }
  
  // Command prompt
  int promptY = gfx->height() - 30;
  gfx->setTextColor(gfx->color565(0, 255, 0));
  gfx->setCursor(15, promptY);
  gfx->printf("$ exec ");
  
  // Blinking cursor
  if ((int)(execTime * 2) % 2) {
    gfx->fillRect(75, promptY, 8, 8, gfx->color565(0, 255, 0));
  }
  
  gfx->setTextColor(gfx->color565(0, 200, 0));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("EXEC");
}

void drawExtIdleNotify() {
  // External idle notification protocol visualization
  gfx->fillScreen(BLACK);
  
  static float idleTime = 0;
  idleTime += 0.06;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Idle state indicator
  float idleLevel = sin(idleTime * 0.5) * 0.5 + 0.5;
  
  // Central idle monitor
  int monitorSize = 40 + idleLevel * 20;
  gfx->drawCircle(centerX, centerY, monitorSize, gfx->color565(100, 200, 255));
  gfx->fillCircle(centerX, centerY, monitorSize - 5, gfx->color565(20, 50, 100));
  
  // Protocol messages
  for (int msg = 0; msg < 6; msg++) {
    float msgAngle = msg * PI / 3 + idleTime * 0.3;
    int msgRadius = 80 + sin(idleTime + msg) * 20;
    
    int x = centerX + cos(msgAngle) * msgRadius;
    int y = centerY + sin(msgAngle) * msgRadius;
    
    // Message packets
    if (sin(idleTime * 2 + msg) > 0) {
      gfx->fillRect(x - 8, y - 4, 16, 8, gfx->color565(255, 200, 0));
      gfx->setTextColor(BLACK);
      gfx->setTextSize(1);
      gfx->setCursor(x - 6, y - 3);
      gfx->printf("ID%d", msg);
    }
    
    // Connection lines
    gfx->drawLine(centerX, centerY, x, y, gfx->color565(80, 80, 255));
  }
  
  // Idle percentage display
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(centerX - 20, centerY - 8);
  gfx->printf("%d%%", (int)(idleLevel * 100));
  
  // Status text
  gfx->setTextColor(gfx->color565(100, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, gfx->height() - 20);
  if (idleLevel > 0.7) {
    gfx->printf("SYSTEM IDLE");
  } else if (idleLevel > 0.3) {
    gfx->printf("MODERATE LOAD");
  } else {
    gfx->printf("HIGH ACTIVITY");
  }
  
  gfx->setTextColor(gfx->color565(100, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("EXT-IDLE-NOTIFY");
}

void drawExtrusion() {
  // 3D extrusion effect
  gfx->fillScreen(BLACK);
  
  static float extrudeTime = 0;
  extrudeTime += 0.08;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Base shape (hexagon)
  int baseRadius = 40;
  int extrudeDepth = 30 + sin(extrudeTime) * 20;
  
  // Draw extruded hexagon
  for (int depth = 0; depth < extrudeDepth; depth++) {
    float depthRatio = (float)depth / extrudeDepth;
    int currentRadius = baseRadius + depth * 0.5;
    
    // Color fades with depth
    int r = 100 + depthRatio * 155;
    int g = 50 + depthRatio * 100;
    int b = 200 - depthRatio * 100;
    
    // Draw hexagon at this depth level
    for (int side = 0; side < 6; side++) {
      float angle1 = side * PI / 3 + extrudeTime * 0.2;
      float angle2 = (side + 1) * PI / 3 + extrudeTime * 0.2;
      
      int x1 = centerX + cos(angle1) * currentRadius - depth * 0.3;
      int y1 = centerY + sin(angle1) * currentRadius - depth * 0.2;
      int x2 = centerX + cos(angle2) * currentRadius - depth * 0.3;
      int y2 = centerY + sin(angle2) * currentRadius - depth * 0.2;
      
      gfx->drawLine(x1, y1, x2, y2, gfx->color565(r, g, b));
      
      // Connect to previous depth level
      if (depth > 0) {
        int prevRadius = baseRadius + (depth - 1) * 0.5;
        int px1 = centerX + cos(angle1) * prevRadius - (depth - 1) * 0.3;
        int py1 = centerY + sin(angle1) * prevRadius - (depth - 1) * 0.2;
        
        gfx->drawLine(px1, py1, x1, y1, gfx->color565(r - 30, g - 30, b));
      }
    }
  }
  
  // Surface normal vectors
  for (int normal = 0; normal < 6; normal++) {
    float normalAngle = normal * PI / 3 + extrudeTime * 0.2;
    int surfaceX = centerX + cos(normalAngle) * (baseRadius + extrudeDepth * 0.5);
    int surfaceY = centerY + sin(normalAngle) * (baseRadius + extrudeDepth * 0.5);
    
    int normalX = surfaceX + cos(normalAngle) * 15;
    int normalY = surfaceY + sin(normalAngle) * 15;
    
    gfx->drawLine(surfaceX, surfaceY, normalX, normalY, gfx->color565(255, 255, 100));
    gfx->fillCircle(normalX, normalY, 2, gfx->color565(255, 200, 0));
  }
  
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("EXTRUSION");
}

void drawExtrusionHelix2() {
  // Double helix extrusion
  gfx->fillScreen(BLACK);
  
  static float helix2Time = 0;
  helix2Time += 0.1;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Double helix parameters
  float helixHeight = gfx->height() - 40;
  float helixRadius = 30;
  
  // Draw two intertwining helixes
  for (int helix = 0; helix < 2; helix++) {
    float helixOffset = helix * PI;
    
    for (int segment = 0; segment < 60; segment++) {
      float t = segment / 60.0;
      float y = 20 + t * helixHeight;
      float angle = t * 4 * PI + helix2Time + helixOffset;
      
      int x = centerX + cos(angle) * helixRadius;
      int z = sin(angle) * helixRadius; // Depth simulation
      
      // Color based on depth and helix
      int r = helix == 0 ? 255 : 100;
      int g = 100 + abs(z) * 3;
      int b = helix == 1 ? 255 : 100;
      
      // Size based on depth
      int size = 2 + (z + helixRadius) / (helixRadius * 2) * 3;
      
      gfx->fillCircle(x, y, size, gfx->color565(r, g, b));
      
      // Connect segments
      if (segment > 0) {
        float prevT = (segment - 1) / 60.0;
        float prevY = 20 + prevT * helixHeight;
        float prevAngle = prevT * 4 * PI + helix2Time + helixOffset;
        int prevX = centerX + cos(prevAngle) * helixRadius;
        
        gfx->drawLine(prevX, prevY, x, y, gfx->color565(r/2, g/2, b/2));
      }
    }
  }
  
  gfx->setTextColor(gfx->color565(255, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("EXTRUSION-HELIX2");
}

void drawExtrusionHelix3() {
  // Triple helix extrusion
  gfx->fillScreen(BLACK);
  
  static float helix3Time = 0;
  helix3Time += 0.08;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Triple helix parameters
  float helixHeight = gfx->height() - 40;
  float helixRadius = 25;
  
  // Draw three intertwining helixes
  for (int helix = 0; helix < 3; helix++) {
    float helixOffset = helix * 2 * PI / 3;
    
    // Helix colors
    int colors[3][3] = {{255, 100, 100}, {100, 255, 100}, {100, 100, 255}};
    
    for (int segment = 0; segment < 50; segment++) {
      float t = segment / 50.0;
      float y = 20 + t * helixHeight;
      float angle = t * 3 * PI + helix3Time + helixOffset;
      
      int x = centerX + cos(angle) * helixRadius;
      int z = sin(angle) * helixRadius;
      
      // Size and color based on position and depth
      int size = 2 + (z + helixRadius) / (helixRadius * 2) * 2;
      int intensity = 150 + (z + helixRadius) / (helixRadius * 2) * 105;
      
      int r = colors[helix][0] * intensity / 255;
      int g = colors[helix][1] * intensity / 255;
      int b = colors[helix][2] * intensity / 255;
      
      gfx->fillCircle(x, y, size, gfx->color565(r, g, b));
      
      // Connecting strands
      if (segment > 0 && segment % 3 == 0) {
        // Connect to next helix
        int nextHelix = (helix + 1) % 3;
        float nextAngle = t * 3 * PI + helix3Time + nextHelix * 2 * PI / 3;
        int nextX = centerX + cos(nextAngle) * helixRadius;
        
        gfx->drawLine(x, y, nextX, y, gfx->color565(100, 100, 100));
      }
    }
  }
  
  gfx->setTextColor(gfx->color565(200, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("EXTRUSION-HELIX3");
}

void drawExtrusionHelix4() {
  // Quadruple helix extrusion
  gfx->fillScreen(BLACK);
  
  static float helix4Time = 0;
  helix4Time += 0.06;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Quadruple helix parameters
  float helixHeight = gfx->height() - 40;
  float helixRadius = 20;
  
  // Draw four intertwining helixes
  for (int helix = 0; helix < 4; helix++) {
    float helixOffset = helix * PI / 2;
    
    // Different radius for each helix to create complex pattern
    float currentRadius = helixRadius + helix * 3;
    
    for (int segment = 0; segment < 40; segment++) {
      float t = segment / 40.0;
      float y = 20 + t * helixHeight;
      float angle = t * 2.5 * PI + helix4Time + helixOffset;
      
      int x = centerX + cos(angle) * currentRadius;
      int z = sin(angle) * currentRadius;
      
      // Unique color for each helix
      int r = (helix == 0 || helix == 2) ? 255 : 80;
      int g = (helix == 1 || helix == 2) ? 255 : 80;
      int b = (helix == 0 || helix == 1) ? 255 : 80;
      
      // Intensity based on depth
      float intensity = (z + currentRadius) / (currentRadius * 2);
      r = r * (0.5 + intensity * 0.5);
      g = g * (0.5 + intensity * 0.5);
      b = b * (0.5 + intensity * 0.5);
      
      int size = 1 + intensity * 3;
      gfx->fillCircle(x, y, size, gfx->color565(r, g, b));
      
      // Cross-connections between helixes
      if (segment % 5 == 0) {
        for (int otherHelix = helix + 1; otherHelix < 4; otherHelix++) {
          float otherOffset = otherHelix * PI / 2;
          float otherRadius = helixRadius + otherHelix * 3;
          float otherAngle = t * 2.5 * PI + helix4Time + otherOffset;
          int otherX = centerX + cos(otherAngle) * otherRadius;
          
          // Only draw connection if helixes are close
          if (abs(otherX - x) < 15) {
            gfx->drawLine(x, y, otherX, y, gfx->color565(60, 60, 60));
          }
        }
      }
    }
  }
  
  gfx->setTextColor(gfx->color565(255, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("EXTRUSION-HELIX4");
}

void drawExtrusionJoinoffset() {
  // Extrusion with offset joins
  gfx->fillScreen(BLACK);
  
  static float joinTime = 0;
  joinTime += 0.09;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple extrusion paths that join with offsets
  for (int path = 0; path < 4; path++) {
    float pathAngle = path * PI / 2 + joinTime * 0.3;
    int pathLength = 80;
    
    // Main extrusion line
    int startX = centerX + cos(pathAngle) * 20;
    int startY = centerY + sin(pathAngle) * 20;
    int endX = centerX + cos(pathAngle) * pathLength;
    int endY = centerY + sin(pathAngle) * pathLength;
    
    // Draw main path segments
    for (int segment = 0; segment < 20; segment++) {
      float t = segment / 20.0;
      int x = startX + (endX - startX) * t;
      int y = startY + (endY - startY) * t;
      
      // Thickness varies along path
      int thickness = 3 + sin(joinTime + segment * 0.3) * 2;
      
      // Color changes along path
      int r = 150 + t * 105;
      int g = 100 + sin(joinTime + t * PI) * 100;
      int b = 200 - t * 100;
      
      gfx->fillCircle(x, y, thickness, gfx->color565(r, g, b));
      
      // Offset joins at specific points
      if (segment % 5 == 0) {
        float offsetAngle = pathAngle + PI / 2;
        int offsetDist = 15 + sin(joinTime + segment) * 10;
        int offsetX = x + cos(offsetAngle) * offsetDist;
        int offsetY = y + sin(offsetAngle) * offsetDist;
        
        // Draw join connector
        gfx->drawLine(x, y, offsetX, offsetY, gfx->color565(255, 200, 100));
        gfx->fillCircle(offsetX, offsetY, 2, gfx->color565(255, 255, 0));
      }
    }
  }
  
  // Central junction with animated offset
  int junctionOffset = sin(joinTime * 2) * 8;
  gfx->fillCircle(centerX + junctionOffset, centerY, 8, gfx->color565(255, 255, 255));
  gfx->drawCircle(centerX + junctionOffset, centerY, 12, gfx->color565(200, 200, 200));
  
  gfx->setTextColor(gfx->color565(255, 200, 100));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("EXTRUSION-JOINOFFSET");
}

void drawExtrusionScrew() {
  // Screw thread extrusion effect
  gfx->fillScreen(BLACK);
  
  static float screwTime = 0;
  screwTime += 0.12;
  
  int centerX = gfx->width() / 2;
  int screwHeight = gfx->height() - 40;
  
  // Screw shaft
  gfx->fillRect(centerX - 8, 20, 16, screwHeight, gfx->color565(150, 150, 150));
  
  // Screw threads (helical pattern)
  for (int thread = 0; thread < screwHeight; thread += 2) {
    float threadAngle = thread * 0.2 + screwTime;
    int threadRadius = 12 + sin(screwTime + thread * 0.1) * 3;
    
    // Left thread
    int leftX = centerX + cos(threadAngle) * threadRadius;
    int leftY = 20 + thread;
    
    // Right thread  
    int rightX = centerX + cos(threadAngle + PI) * threadRadius;
    int rightY = 20 + thread;
    
    // Thread depth shading
    float depth = cos(threadAngle) * 0.5 + 0.5;
    int threadBrightness = 100 + depth * 155;
    
    gfx->fillCircle(leftX, leftY, 2, gfx->color565(threadBrightness, threadBrightness, threadBrightness));
    gfx->fillCircle(rightX, rightY, 2, gfx->color565(threadBrightness, threadBrightness, threadBrightness));
    
    // Connect thread segments
    if (thread > 0) {
      float prevAngle = (thread - 2) * 0.2 + screwTime;
      int prevLeftX = centerX + cos(prevAngle) * threadRadius;
      int prevRightX = centerX + cos(prevAngle + PI) * threadRadius;
      
      gfx->drawLine(prevLeftX, leftY - 2, leftX, leftY, gfx->color565(180, 180, 180));
      gfx->drawLine(prevRightX, rightY - 2, rightX, rightY, gfx->color565(180, 180, 180));
    }
  }
  
  // Screw head
  gfx->fillCircle(centerX, 15, 15, gfx->color565(200, 200, 200));
  gfx->drawCircle(centerX, 15, 15, gfx->color565(100, 100, 100));
  
  // Phillips head pattern
  gfx->drawLine(centerX - 8, 15, centerX + 8, 15, gfx->color565(80, 80, 80));
  gfx->drawLine(centerX, 7, centerX, 23, gfx->color565(80, 80, 80));
  
  // Rotation indication
  float rotAngle = screwTime * 2;
  for (int mark = 0; mark < 8; mark++) {
    float markAngle = mark * PI / 4 + rotAngle;
    int markX = centerX + cos(markAngle) * 18;
    int markY = 15 + sin(markAngle) * 18;
    gfx->fillCircle(markX, markY, 1, gfx->color565(255, 200, 0));
  }
  
  gfx->setTextColor(gfx->color565(200, 200, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("EXTRUSION-SCREW");
}

void drawExtrusionTaper() {
  // Tapered extrusion effect
  gfx->fillScreen(BLACK);
  
  static float taperTime = 0;
  taperTime += 0.07;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple tapered extrusions radiating from center
  for (int taper = 0; taper < 6; taper++) {
    float taperAngle = taper * PI / 3 + taperTime * 0.2;
    int maxLength = 100;
    
    // Draw tapered extrusion segments
    for (int segment = 0; segment < 30; segment++) {
      float t = segment / 30.0;
      int distance = t * maxLength;
      
      int x = centerX + cos(taperAngle) * distance;
      int y = centerY + sin(taperAngle) * distance;
      
      // Taper: size decreases with distance, pulsates
      float baseTaper = (1.0 - t * 0.8);
      float pulseTaper = 1.0 + sin(taperTime * 3 + segment * 0.2) * 0.3;
      int size = (baseTaper * pulseTaper * 8) + 1;
      
      // Color shifts along taper
      int r = 255 * (1.0 - t * 0.5);
      int g = 100 + t * 155;
      int b = 150 + sin(taperTime + t * PI) * 105;
      
      gfx->fillCircle(x, y, size, gfx->color565(r, g, b));
      
      // Taper outline for depth
      if (size > 2) {
        gfx->drawCircle(x, y, size + 1, gfx->color565(r/3, g/3, b/3));
      }
    }
    
    // Branch tapers at midpoints
    if (taper % 2 == 0) {
      float branchAngle = taperAngle + PI / 6;
      for (int branch = 10; branch < 20; branch++) {
        float bt = (branch - 10) / 10.0;
        int branchDist = bt * 50;
        
        int startX = centerX + cos(taperAngle) * (branch * maxLength / 30);
        int startY = centerY + sin(taperAngle) * (branch * maxLength / 30);
        
        int branchX = startX + cos(branchAngle) * branchDist;
        int branchY = startY + sin(branchAngle) * branchDist;
        
        int branchSize = (1.0 - bt) * 4 + 1;
        gfx->fillCircle(branchX, branchY, branchSize, gfx->color565(255, 200 - bt * 100, 100));
      }
    }
  }
  
  // Central core
  int coreSize = 6 + sin(taperTime * 4) * 2;
  gfx->fillCircle(centerX, centerY, coreSize, gfx->color565(255, 255, 255));
  gfx->drawCircle(centerX, centerY, coreSize + 2, gfx->color565(200, 200, 0));
  
  gfx->setTextColor(gfx->color565(255, 200, 150));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("EXTRUSION-TAPER");
}

void drawExtrusionTwistoid() {
  // Twisting toroidal extrusion effect
  gfx->fillScreen(BLACK);
  
  static float twistTime = 0;
  twistTime += 0.08;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Twistoid parameters
  float majorRadius = 60;
  float minorRadius = 20;
  float twistRate = 3.0;
  
  // Draw twisted torus
  for (int u = 0; u < 36; u++) {
    for (int v = 0; v < 24; v++) {
      float uAngle = u * PI / 18 + twistTime * 0.5;
      float vAngle = v * PI / 12 + twistTime * twistRate;
      
      // Twist factor varies along major radius
      float twistFactor = sin(uAngle * 2 + twistTime) * 0.5;
      float actualVAngle = vAngle + twistFactor;
      
      // Parametric torus equations with twist
      float x = (majorRadius + minorRadius * cos(actualVAngle)) * cos(uAngle);
      float y = (majorRadius + minorRadius * cos(actualVAngle)) * sin(uAngle);
      float z = minorRadius * sin(actualVAngle);
      
      // Project to 2D
      int screenX = centerX + x * 0.8;
      int screenY = centerY + y * 0.6 + z * 0.3;
      
      // Color based on twist and depth
      float depthFactor = (z + minorRadius) / (2 * minorRadius);
      int r = 100 + depthFactor * 155 + twistFactor * 100;
      int g = 150 + sin(uAngle + twistTime) * 105;
      int b = 200 + cos(actualVAngle) * 55;
      
      // Size based on depth
      int size = 1 + depthFactor * 3;
      
      if (screenX >= 0 && screenX < gfx->width() && screenY >= 0 && screenY < gfx->height()) {
        gfx->fillCircle(screenX, screenY, size, gfx->color565(r, g, b));
      }
      
      // Connect adjacent points for wireframe effect
      if (u > 0 && v > 0) {
        float prevUAngle = (u-1) * PI / 18 + twistTime * 0.5;
        float prevVAngle = (v-1) * PI / 12 + twistTime * twistRate;
        float prevTwist = sin(prevUAngle * 2 + twistTime) * 0.5;
        float prevActualVAngle = prevVAngle + prevTwist;
        
        float prevX = (majorRadius + minorRadius * cos(prevActualVAngle)) * cos(prevUAngle);
        float prevY = (majorRadius + minorRadius * cos(prevActualVAngle)) * sin(prevUAngle);
        float prevZ = minorRadius * sin(prevActualVAngle);
        
        int prevScreenX = centerX + prevX * 0.8;
        int prevScreenY = centerY + prevY * 0.6 + prevZ * 0.3;
        
        if (abs(screenX - prevScreenX) < 20 && abs(screenY - prevScreenY) < 20) {
          gfx->drawLine(screenX, screenY, prevScreenX, prevScreenY, gfx->color565(r/4, g/4, b/4));
        }
      }
    }
  }
  
  gfx->setTextColor(gfx->color565(255, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("EXTRUSION-TWISTOID");
}

void drawExts() {
  // Extensions system visualization
  gfx->fillScreen(BLACK);
  
  static float extTime = 0;
  extTime += 0.1;
  
  // Extension modules arranged in grid
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 4; col++) {
      int moduleX = 20 + col * 70;
      int moduleY = 30 + row * 60;
      
      // Module activity
      float activity = sin(extTime + row + col) * 0.5 + 0.5;
      
      // Module box
      int moduleSize = 25 + activity * 10;
      gfx->drawRect(moduleX - moduleSize/2, moduleY - moduleSize/2, moduleSize, moduleSize, 
                    gfx->color565(100, 150, 200));
      
      // Active modules have colored fill
      if (activity > 0.6) {
        gfx->fillRect(moduleX - moduleSize/2 + 2, moduleY - moduleSize/2 + 2, 
                     moduleSize - 4, moduleSize - 4, 
                     gfx->color565(0, 255 * activity, 100));
      }
      
      // Extension connections
      if (col < 3) {
        // Horizontal connections
        gfx->drawLine(moduleX + moduleSize/2, moduleY, 
                     moduleX + 70 - moduleSize/2, moduleY,
                     gfx->color565(150, 150, 150));
        
        // Data flow indicators
        if (sin(extTime * 3 - col) > 0) {
          int flowX = moduleX + moduleSize/2 + (70 - moduleSize) * ((sin(extTime * 2) + 1) / 2);
          gfx->fillCircle(flowX, moduleY, 2, gfx->color565(255, 255, 0));
        }
      }
      
      if (row < 2) {
        // Vertical connections
        gfx->drawLine(moduleX, moduleY + moduleSize/2,
                     moduleX, moduleY + 60 - moduleSize/2,
                     gfx->color565(150, 150, 150));
      }
      
      // Extension ID
      gfx->setTextColor(WHITE);
      gfx->setTextSize(1);
      gfx->setCursor(moduleX - 6, moduleY - 3);
      gfx->printf("E%d", row * 4 + col);
    }
  }
  
  // Status bar
  gfx->drawRect(10, gfx->height() - 25, gfx->width() - 20, 20, gfx->color565(100, 100, 100));
  
  // Active extension count
  int activeCount = (sin(extTime) * 6 + 6);
  gfx->setTextColor(gfx->color565(0, 255, 0));
  gfx->setCursor(15, gfx->height() - 20);
  gfx->printf("ACTIVE: %d/12", activeCount);
  
  gfx->setTextColor(gfx->color565(200, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("EXTS");
}

void drawExtSessionLock() {
  // External session lock protocol
  gfx->fillScreen(BLACK);
  
  static float lockTime = 0;
  lockTime += 0.06;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Lock state (cycles between locked/unlocked)
  bool isLocked = sin(lockTime * 0.8) > 0;
  
  // Lock mechanism visualization
  if (isLocked) {
    // Locked state - closed padlock
    gfx->drawRect(centerX - 20, centerY - 5, 40, 25, gfx->color565(200, 100, 100));
    gfx->fillRect(centerX - 18, centerY - 3, 36, 21, gfx->color565(150, 50, 50));
    
    // Shackle (closed)
    gfx->drawCircle(centerX, centerY - 15, 12, gfx->color565(180, 180, 180));
    gfx->fillCircle(centerX, centerY - 15, 8, BLACK);
    gfx->fillRect(centerX - 8, centerY - 15, 16, 10, BLACK);
    
    // Lock status
    gfx->setTextColor(gfx->color565(255, 100, 100));
    gfx->setCursor(centerX - 30, centerY + 40);
    gfx->printf("LOCKED");
  } else {
    // Unlocked state - open padlock
    gfx->drawRect(centerX - 20, centerY - 5, 40, 25, gfx->color565(100, 200, 100));
    gfx->fillRect(centerX - 18, centerY - 3, 36, 21, gfx->color565(50, 150, 50));
    
    // Shackle (open)
    gfx->drawCircle(centerX - 5, centerY - 15, 12, gfx->color565(180, 180, 180));
    gfx->fillCircle(centerX - 5, centerY - 15, 8, BLACK);
    gfx->fillRect(centerX - 13, centerY - 15, 16, 8, BLACK);
    
    // Lock status
    gfx->setTextColor(gfx->color565(100, 255, 100));
    gfx->setCursor(centerX - 35, centerY + 40);
    gfx->printf("UNLOCKED");
  }
  
  // Session indicators around the lock
  for (int session = 0; session < 8; session++) {
    float sessionAngle = session * PI / 4 + lockTime;
    int sessionRadius = 50;
    
    int sessionX = centerX + cos(sessionAngle) * sessionRadius;
    int sessionY = centerY + sin(sessionAngle) * sessionRadius;
    
    // Session activity
    bool sessionActive = sin(lockTime * 2 + session) > 0;
    
    if (sessionActive) {
      gfx->fillCircle(sessionX, sessionY, 4, gfx->color565(255, 255, 0));
      gfx->drawLine(centerX, centerY, sessionX, sessionY, gfx->color565(100, 100, 100));
    } else {
      gfx->drawCircle(sessionX, sessionY, 4, gfx->color565(100, 100, 100));
    }
    
    // Session ID
    gfx->setTextColor(WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(sessionX - 3, sessionY - 3);
    gfx->printf("%d", session);
  }
  
  // Protocol status
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("EXT-SESSION-LOCK-V1");
  
  gfx->setTextColor(gfx->color565(150, 150, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("EXT-SESSION-LOCK");
}

void drawFade() {
  // Fade in/out effect with multiple layers
  gfx->fillScreen(BLACK);
  
  static float fadeTime = 0;
  fadeTime += 0.05;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple fading layers with different rates
  for (int layer = 0; layer < 5; layer++) {
    float layerPhase = fadeTime + layer * 0.8;
    float fadeLevel = (sin(layerPhase) + 1) / 2; // 0 to 1
    
    // Layer properties
    int layerRadius = 20 + layer * 15;
    int layerSize = fadeLevel * layerRadius;
    
    // Fade color based on layer and time
    int r = fadeLevel * (200 + layer * 10);
    int g = fadeLevel * (100 + sin(layerPhase * 2) * 100);
    int b = fadeLevel * (255 - layer * 30);
    
    // Draw fading circles
    for (int ring = 0; ring < 8; ring++) {
      float ringAngle = ring * PI / 4 + fadeTime * 0.3;
      int ringX = centerX + cos(ringAngle) * layerSize;
      int ringY = centerY + sin(ringAngle) * layerSize;
      
      int ringSize = fadeLevel * (8 - ring);
      if (ringSize > 0) {
        gfx->fillCircle(ringX, ringY, ringSize, gfx->color565(r, g, b));
      }
    }
    
    // Fading lines radiating outward
    for (int line = 0; line < 12; line++) {
      float lineAngle = line * PI / 6 + fadeTime * 0.2;
      int lineLength = fadeLevel * 60;
      
      int x1 = centerX + cos(lineAngle) * 10;
      int y1 = centerY + sin(lineAngle) * 10;
      int x2 = centerX + cos(lineAngle) * lineLength;
      int y2 = centerY + sin(lineAngle) * lineLength;
      
      gfx->drawLine(x1, y1, x2, y2, gfx->color565(r/2, g/2, b/2));
    }
  }
  
  // Central fade indicator
  float centralFade = (sin(fadeTime * 2) + 1) / 2;
  int centralSize = centralFade * 12;
  gfx->fillCircle(centerX, centerY, centralSize, gfx->color565(255, 255, 255));
  
  // Fade percentage text
  gfx->setTextColor(gfx->color565(255, 255, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("FADE: %d%%", (int)(centralFade * 100));
  
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FADE");
}

void drawFfmpegOut() {
  // FFmpeg output stream visualization
  gfx->fillScreen(BLACK);
  
  static float streamTime = 0;
  streamTime += 0.1;
  
  // Video stream header
  gfx->drawRect(5, 5, gfx->width() - 10, 20, gfx->color565(100, 100, 100));
  gfx->fillRect(6, 6, gfx->width() - 12, 18, gfx->color565(20, 20, 20));
  
  gfx->setTextColor(gfx->color565(0, 255, 0));
  gfx->setTextSize(1);
  gfx->setCursor(8, 10);
  gfx->printf("FFmpeg Output Stream");
  
  // Encoding progress bar
  float progress = (sin(streamTime * 0.5) + 1) / 2;
  int progressWidth = progress * (gfx->width() - 20);
  
  gfx->drawRect(10, 30, gfx->width() - 20, 8, gfx->color565(100, 100, 100));
  gfx->fillRect(11, 31, progressWidth, 6, gfx->color565(0, 200, 0));
  
  // Frame data visualization
  int frameHeight = 100;
  int frameY = 45;
  
  // Video frames as colored rectangles
  for (int frame = 0; frame < 20; frame++) {
    int frameX = 10 + frame * 14;
    float frameIntensity = sin(streamTime * 2 + frame * 0.5) * 0.5 + 0.5;
    
    // Frame compression indicator
    int frameSize = frameHeight * (0.3 + frameIntensity * 0.7);
    
    // Color based on frame type (I, P, B frames)
    int frameType = frame % 3;
    int r, g, b;
    switch (frameType) {
      case 0: // I-frame (keyframe)
        r = 255 * frameIntensity;
        g = 100;
        b = 100;
        break;
      case 1: // P-frame 
        r = 100;
        g = 255 * frameIntensity;
        b = 100;
        break;
      case 2: // B-frame
        r = 100;
        g = 100;
        b = 255 * frameIntensity;
        break;
    }
    
    gfx->fillRect(frameX, frameY + frameHeight - frameSize, 12, frameSize, 
                  gfx->color565(r, g, b));
    gfx->drawRect(frameX, frameY, 12, frameHeight, gfx->color565(80, 80, 80));
  }
  
  // Codec information
  gfx->setTextColor(gfx->color565(200, 200, 200));
  gfx->setCursor(10, frameY + frameHeight + 10);
  gfx->printf("Codec: H.264");
  
  gfx->setCursor(10, frameY + frameHeight + 20);
  gfx->printf("Bitrate: %d kbps", (int)(1000 + progress * 2000));
  
  gfx->setCursor(10, frameY + frameHeight + 30);
  gfx->printf("FPS: %.1f", 30.0 + sin(streamTime) * 5);
  
  // Output stream indicators
  for (int stream = 0; stream < 3; stream++) {
    int streamY = frameY + frameHeight + 50 + stream * 15;
    bool streamActive = sin(streamTime * 3 + stream) > 0;
    
    uint16_t streamColor = streamActive ? gfx->color565(0, 255, 0) : gfx->color565(100, 100, 100);
    gfx->fillCircle(15, streamY, 3, streamColor);
    
    gfx->setTextColor(streamColor);
    gfx->setCursor(25, streamY - 3);
    switch (stream) {
      case 0: gfx->printf("Video Out"); break;
      case 1: gfx->printf("Audio Out"); break;
      case 2: gfx->printf("Meta Out"); break;
    }
  }
  
  gfx->setTextColor(gfx->color565(255, 200, 100));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FFMPEG-OUT");
}

void drawFiberlamp() {
  // Fiber optic lamp effect
  gfx->fillScreen(BLACK);
  
  static float fiberTime = 0;
  fiberTime += 0.12;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Fiber base lamp
  gfx->fillCircle(centerX, centerY + 50, 15, gfx->color565(60, 60, 60));
  gfx->drawCircle(centerX, centerY + 50, 15, gfx->color565(120, 120, 120));
  
  // Light source core
  int coreIntensity = 200 + sin(fiberTime * 4) * 55;
  gfx->fillCircle(centerX, centerY + 50, 8, gfx->color565(coreIntensity, coreIntensity, 255));
  
  // Fiber strands radiating outward
  for (int fiber = 0; fiber < 32; fiber++) {
    float fiberAngle = fiber * PI / 16 + fiberTime * 0.1;
    float fiberLength = 80 + sin(fiberTime + fiber * 0.2) * 20;
    
    // Fiber path with slight curve
    for (int segment = 0; segment < 20; segment++) {
      float t = segment / 20.0;
      float curveOffset = sin(fiberTime * 2 + fiber + t * PI) * 8;
      
      int x = centerX + cos(fiberAngle) * (20 + t * fiberLength) + curveOffset * cos(fiberAngle + PI/2);
      int y = centerY + 50 + sin(fiberAngle) * (20 + t * fiberLength) + curveOffset * sin(fiberAngle + PI/2);
      
      // Light travels along fiber
      float lightPos = fmod(fiberTime * 3 + fiber * 0.1, 1.0);
      float lightIntensity = 0;
      
      if (abs(t - lightPos) < 0.15) {
        lightIntensity = 1.0 - abs(t - lightPos) / 0.15;
      }
      
      // Fiber color with light pulse
      int r = 50 + lightIntensity * 205;
      int g = 100 + lightIntensity * 155;
      int b = 255;
      
      // Fiber thickness decreases toward tip
      int thickness = 2 + (1.0 - t) * 2;
      
      if (x >= 0 && x < gfx->width() && y >= 0 && y < gfx->height()) {
        gfx->fillCircle(x, y, thickness, gfx->color565(r, g, b));
        
        // Add glow for bright segments
        if (lightIntensity > 0.5) {
          gfx->drawCircle(x, y, thickness + 2, gfx->color565(r/3, g/3, b/3));
        }
      }
    }
  }
  
  // Sparkle effects at fiber tips
  for (int sparkle = 0; sparkle < 8; sparkle++) {
    float sparkleAngle = sparkle * PI / 4 + fiberTime * 0.3;
    float sparkleRadius = 100 + sin(fiberTime + sparkle) * 15;
    
    int sparkleX = centerX + cos(sparkleAngle) * sparkleRadius;
    int sparkleY = centerY + 50 + sin(sparkleAngle) * sparkleRadius;
    
    if (sin(fiberTime * 6 + sparkle) > 0.7) {
      gfx->fillCircle(sparkleX, sparkleY, 2, gfx->color565(255, 255, 255));
      gfx->drawCircle(sparkleX, sparkleY, 4, gfx->color565(100, 200, 255));
    }
  }
  
  gfx->setTextColor(gfx->color565(100, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FIBERLAMP");
}

void drawFilmleader() {
  // Film leader countdown effect
  gfx->fillScreen(BLACK);
  
  static float filmTime = 0;
  filmTime += 0.08;
  
  // Film strip background
  gfx->fillRect(0, 0, gfx->width(), gfx->height(), BLACK);
  
  // Film perforations on sides
  for (int perf = 0; perf < gfx->height(); perf += 12) {
    // Left perforations
    gfx->fillRect(2, perf + 2, 6, 8, gfx->color565(40, 40, 40));
    // Right perforations  
    gfx->fillRect(gfx->width() - 8, perf + 2, 6, 8, gfx->color565(40, 40, 40));
  }
  
  // Film edges
  gfx->drawFastVLine(12, 0, gfx->height(), gfx->color565(100, 100, 100));
  gfx->drawFastVLine(gfx->width() - 13, 0, gfx->height(), gfx->color565(100, 100, 100));
  
  // Countdown circle
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  int circleRadius = 60;
  
  gfx->drawCircle(centerX, centerY, circleRadius, gfx->color565(200, 200, 200));
  gfx->drawCircle(centerX, centerY, circleRadius - 5, gfx->color565(150, 150, 150));
  
  // Countdown number
  int countdown = (int)(8 - fmod(filmTime * 0.8, 8));
  if (countdown <= 0) countdown = 8;
  
  gfx->setTextColor(gfx->color565(255, 255, 255));
  gfx->setTextSize(4);
  gfx->setCursor(centerX - 12, centerY - 16);
  gfx->printf("%d", countdown);
  
  // Rotating sweep hand
  float sweepAngle = fmod(filmTime * 2, 2 * PI);
  int sweepX = centerX + cos(sweepAngle - PI/2) * (circleRadius - 10);
  int sweepY = centerY + sin(sweepAngle - PI/2) * (circleRadius - 10);
  gfx->drawLine(centerX, centerY, sweepX, sweepY, gfx->color565(255, 200, 0));
  
  // Film scratches and artifacts
  for (int scratch = 0; scratch < 5; scratch++) {
    int scratchX = 20 + scratch * 50;
    int scratchY = (int)(sin(filmTime + scratch) * 30) + gfx->height() / 2;
    int scratchLen = 10 + random(20);
    
    gfx->drawFastVLine(scratchX, scratchY, scratchLen, gfx->color565(80, 80, 80));
  }
  
  // Dust spots
  for (int dust = 0; dust < 8; dust++) {
    int dustX = 15 + random(gfx->width() - 30);
    int dustY = random(gfx->height());
    
    if (sin(filmTime * 3 + dust) > 0.5) {
      gfx->fillCircle(dustX, dustY, 1, gfx->color565(120, 120, 120));
    }
  }
  
  // Academy leader markings
  for (int mark = 0; mark < 4; mark++) {
    float markAngle = mark * PI / 2;
    int markX = centerX + cos(markAngle) * (circleRadius + 15);
    int markY = centerY + sin(markAngle) * (circleRadius + 15);
    gfx->drawLine(centerX + cos(markAngle) * circleRadius, 
                  centerY + sin(markAngle) * circleRadius,
                  markX, markY, gfx->color565(180, 180, 180));
  }
  
  gfx->setTextColor(gfx->color565(200, 200, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FILMLEADER");
}

void drawFireworkx() {
  // Enhanced fireworks display
  gfx->fillScreen(BLACK);
  
  static float fireworkTime = 0;
  fireworkTime += 0.1;
  
  // Multiple fireworks at different stages
  for (int firework = 0; firework < 4; firework++) {
    float fwPhase = fireworkTime + firework * 1.5;
    float fwAge = fmod(fwPhase, 4.0);
    
    // Firework center position
    int fwX = 50 + firework * 60;
    int fwY = gfx->height() - 40 + sin(fwPhase * 0.5) * 20;
    
    if (fwAge < 0.5) {
      // Launch phase - rocket trail
      int trailHeight = fwAge * 2 * gfx->height();
      for (int trail = 0; trail < trailHeight; trail += 3) {
        int trailY = gfx->height() - trail;
        int sparkIntensity = 200 - trail / 2;
        if (sparkIntensity > 0) {
          gfx->fillCircle(fwX + random(4) - 2, trailY, 1, 
                          gfx->color565(sparkIntensity, sparkIntensity/2, 0));
        }
      }
    } else if (fwAge < 3.5) {
      // Explosion phase
      float explodeAge = fwAge - 0.5;
      int explodeRadius = explodeAge * 40;
      
      // Main burst particles
      for (int particle = 0; particle < 24; particle++) {
        float particleAngle = particle * PI / 12 + firework * 0.3;
        float particleSpeed = 1.0 + sin(firework + particle) * 0.3;
        
        int pX = fwX + cos(particleAngle) * explodeRadius * particleSpeed;
        int pY = fwY + sin(particleAngle) * explodeRadius * particleSpeed;
        
        // Particle fade over time
        float fade = 1.0 - explodeAge / 3.0;
        if (fade > 0) {
          // Firework colors
          int r, g, b;
          switch (firework % 4) {
            case 0: r = 255 * fade; g = 100 * fade; b = 100 * fade; break; // Red
            case 1: r = 100 * fade; g = 255 * fade; b = 100 * fade; break; // Green
            case 2: r = 100 * fade; g = 100 * fade; b = 255 * fade; break; // Blue
            case 3: r = 255 * fade; g = 255 * fade; b = 100 * fade; break; // Yellow
          }
          
          // Particle with tail
          gfx->fillCircle(pX, pY, 2, gfx->color565(r, g, b));
          
          // Gravity effect - particles fall
          int fallOffset = (explodeAge * explodeAge * 5);
          gfx->fillCircle(pX, pY + fallOffset, 1, gfx->color565(r/2, g/2, b/2));
        }
      }
      
      // Secondary burst for some fireworks
      if (firework % 2 == 0 && explodeAge > 1.0) {
        int secondRadius = (explodeAge - 1.0) * 25;
        for (int spark = 0; spark < 12; spark++) {
          float sparkAngle = spark * PI / 6 + firework;
          int sX = fwX + cos(sparkAngle) * secondRadius;
          int sY = fwY + sin(sparkAngle) * secondRadius;
          
          float sparkFade = 1.0 - (explodeAge - 1.0) / 2.0;
          if (sparkFade > 0) {
            gfx->fillCircle(sX, sY, 1, gfx->color565(255 * sparkFade, 255 * sparkFade, 255 * sparkFade));
          }
        }
      }
    }
  }
  
  // Ground reflections
  for (int refl = 0; refl < gfx->width(); refl += 10) {
    if (sin(fireworkTime + refl * 0.1) > 0.8) {
      gfx->fillRect(refl, gfx->height() - 5, 8, 3, gfx->color565(50, 50, 100));
    }
  }
  
  gfx->setTextColor(gfx->color565(255, 200, 100));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FIREWORKX");
}

void drawFlag() {
  // Waving flag effect
  gfx->fillScreen(BLACK);
  
  static float flagTime = 0;
  flagTime += 0.1;
  
  int flagWidth = gfx->width() - 40;
  int flagHeight = 80;
  int flagX = 20;
  int flagY = 40;
  
  // Flag pole
  gfx->drawFastVLine(flagX - 5, 0, gfx->height(), gfx->color565(139, 69, 19));
  gfx->fillRect(flagX - 7, 0, 4, gfx->height(), gfx->color565(101, 67, 33));
  
  // Waving flag mesh
  for (int y = 0; y < flagHeight; y += 4) {
    for (int x = 0; x < flagWidth; x += 4) {
      // Wave calculation
      float waveX = x / 20.0;
      float waveY = y / 40.0;
      float wave = sin(waveX + flagTime * 2) * cos(waveY + flagTime) * 8;
      
      int pixelX = flagX + x + wave;
      int pixelY = flagY + y;
      
      // Flag colors - simple tricolor
      int colorBand = (y * 3) / flagHeight;
      uint16_t bandColor;
      switch (colorBand) {
        case 0: bandColor = gfx->color565(255, 0, 0); break; // Red
        case 1: bandColor = gfx->color565(255, 255, 255); break; // White  
        case 2: bandColor = gfx->color565(0, 0, 255); break; // Blue
        default: bandColor = gfx->color565(255, 255, 255); break;
      }
      
      // Shading based on wave position for 3D effect
      float shading = 1.0 + sin(waveX + flagTime * 2) * 0.3;
      int r = ((bandColor >> 11) & 0x1F) * shading;
      int g = ((bandColor >> 5) & 0x3F) * shading;
      int b = (bandColor & 0x1F) * shading;
      
      // Clamp values
      r = r > 31 ? 31 : r;
      g = g > 63 ? 63 : g;
      b = b > 31 ? 31 : b;
      
      gfx->fillRect(pixelX, pixelY, 3, 3, (r << 11) | (g << 5) | b);
    }
  }
  
  // Wind lines
  for (int wind = 0; wind < 5; wind++) {
    int windY = 20 + wind * 30;
    int windX = flagX + flagWidth + 10 + sin(flagTime * 3 + wind) * 15;
    gfx->drawLine(windX, windY, windX + 20, windY, gfx->color565(150, 150, 150));
  }
  
  gfx->setTextColor(gfx->color565(255, 255, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FLAG");
}

void drawFlame() {
  // Realistic flame effect
  gfx->fillScreen(BLACK);
  
  static float flameTime = 0;
  flameTime += 0.15;
  
  int baseY = gfx->height() - 20;
  int centerX = gfx->width() / 2;
  
  // Flame base (candle)
  gfx->fillRect(centerX - 6, baseY, 12, 20, gfx->color565(139, 69, 19));
  
  // Flame particles rising
  for (int particle = 0; particle < 50; particle++) {
    float particleAge = fmod(flameTime * 2 + particle * 0.1, 3.0);
    float particleHeight = particleAge * 40;
    
    // Flame sway
    float sway = sin(flameTime * 4 + particle * 0.3) * 15;
    
    int x = centerX + sway + random(10) - 5;
    int y = baseY - particleHeight;
    
    if (y < 0 || particleAge > 2.5) continue;
    
    // Flame color transition: white -> yellow -> orange -> red
    float intensity = 1.0 - particleAge / 2.5;
    int r, g, b;
    
    if (particleAge < 0.5) {
      // Hot core - white to yellow
      float t = particleAge / 0.5;
      r = 255;
      g = 255;
      b = (1.0 - t) * 255;
    } else if (particleAge < 1.5) {
      // Yellow to orange
      float t = (particleAge - 0.5) / 1.0;
      r = 255;
      g = 255 - t * 100;
      b = 0;
    } else {
      // Orange to red
      float t = (particleAge - 1.5) / 1.0;
      r = 255;
      g = 155 - t * 155;
      b = 0;
    }
    
    // Apply intensity
    r *= intensity;
    g *= intensity;
    b *= intensity;
    
    // Particle size decreases with age
    int size = (1.0 - particleAge / 2.5) * 4 + 1;
    
    gfx->fillCircle(x, y, size, gfx->color565(r, g, b));
  }
  
  // Flame core
  for (int core = 0; core < 15; core++) {
    float coreHeight = core * 8;
    float coreWidth = (15 - core) * 2;
    
    int coreX = centerX + sin(flameTime * 5 + core * 0.5) * 5;
    int coreY = baseY - coreHeight;
    
    // Core color
    int coreIntensity = 255 - core * 15;
    gfx->fillCircle(coreX, coreY, coreWidth/2, gfx->color565(coreIntensity, coreIntensity, coreIntensity/3));
  }
  
  // Heat distortion effect
  for (int distort = 0; distort < 8; distort++) {
    int distortX = centerX - 40 + distort * 10;
    int distortY = baseY - 60 + sin(flameTime * 3 + distort) * 20;
    
    gfx->drawLine(distortX, distortY, distortX + 2, distortY + 4, gfx->color565(50, 50, 50));
  }
  
  gfx->setTextColor(gfx->color565(255, 200, 100));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FLAME");
}

void drawFlipflop() {
  // Digital flip-flop circuit visualization
  gfx->fillScreen(BLACK);
  
  static float flipTime = 0;
  flipTime += 0.08;
  
  static bool flipState = false;
  static float lastFlip = 0;
  
  // Flip the state periodically
  if (flipTime - lastFlip > 1.0) {
    flipState = !flipState;
    lastFlip = flipTime;
  }
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Main flip-flop box
  gfx->drawRect(centerX - 40, centerY - 25, 80, 50, gfx->color565(100, 100, 100));
  gfx->fillRect(centerX - 38, centerY - 23, 76, 46, gfx->color565(20, 20, 20));
  
  // FF label
  gfx->setTextColor(gfx->color565(200, 200, 200));
  gfx->setTextSize(2);
  gfx->setCursor(centerX - 12, centerY - 8);
  gfx->printf("FF");
  
  // Input pins
  gfx->drawLine(centerX - 60, centerY - 10, centerX - 40, centerY - 10, gfx->color565(150, 150, 150));
  gfx->drawLine(centerX - 60, centerY + 10, centerX - 40, centerY + 10, gfx->color565(150, 150, 150));
  
  // Clock input with pulse
  bool clockPulse = sin(flipTime * 6) > 0;
  uint16_t clockColor = clockPulse ? gfx->color565(0, 255, 0) : gfx->color565(100, 100, 100);
  gfx->drawLine(centerX - 60, centerY, centerX - 40, centerY, clockColor);
  gfx->fillTriangle(centerX - 45, centerY - 3, centerX - 45, centerY + 3, centerX - 40, centerY, clockColor);
  
  // Output pins
  uint16_t qColor = flipState ? gfx->color565(255, 0, 0) : gfx->color565(100, 100, 100);
  uint16_t qBarColor = !flipState ? gfx->color565(255, 0, 0) : gfx->color565(100, 100, 100);
  
  gfx->drawLine(centerX + 40, centerY - 10, centerX + 60, centerY - 10, qColor);
  gfx->drawLine(centerX + 40, centerY + 10, centerX + 60, centerY + 10, qBarColor);
  
  // Pin labels
  gfx->setTextSize(1);
  gfx->setTextColor(gfx->color565(200, 200, 200));
  
  gfx->setCursor(centerX - 75, centerY - 15);
  gfx->printf("D");
  gfx->setCursor(centerX - 75, centerY - 4);
  gfx->printf("CLK");
  gfx->setCursor(centerX - 75, centerY + 6);
  gfx->printf("R");
  
  gfx->setCursor(centerX + 65, centerY - 15);
  gfx->printf("Q");
  gfx->setCursor(centerX + 65, centerY + 6);
  gfx->printf("/Q");
  
  // Logic gates feeding the flip-flop
  for (int gate = 0; gate < 2; gate++) {
    int gateX = centerX - 120;
    int gateY = centerY - 30 + gate * 60;
    
    // NAND gate symbol
    gfx->drawRect(gateX, gateY, 30, 20, gfx->color565(150, 150, 150));
    gfx->drawCircle(gateX + 32, gateY + 10, 2, gfx->color565(150, 150, 150));
    
    gfx->setTextSize(1);
    gfx->setCursor(gateX + 8, gateY + 7);
    gfx->printf("&");
    
    // Gate inputs
    bool input1 = sin(flipTime * 4 + gate) > 0;
    bool input2 = sin(flipTime * 3 - gate) > 0.5;
    
    uint16_t in1Color = input1 ? gfx->color565(0, 255, 0) : gfx->color565(100, 100, 100);
    uint16_t in2Color = input2 ? gfx->color565(0, 255, 0) : gfx->color565(100, 100, 100);
    
    gfx->drawLine(gateX - 15, gateY + 5, gateX, gateY + 5, in1Color);
    gfx->drawLine(gateX - 15, gateY + 15, gateX, gateY + 15, in2Color);
    
    // Gate output
    bool gateOut = !(input1 && input2);
    uint16_t outColor = gateOut ? gfx->color565(255, 0, 0) : gfx->color565(100, 100, 100);
    gfx->drawLine(gateX + 34, gateY + 10, gateX + 50, gateY + 10, outColor);
  }
  
  // Truth table display
  gfx->setTextSize(1);
  gfx->setTextColor(gfx->color565(200, 200, 200));
  gfx->setCursor(10, gfx->height() - 40);
  gfx->printf("CLK D | Q /Q");
  gfx->setCursor(10, gfx->height() - 30);
  gfx->printf(" %c  %c | %c  %c", 
              clockPulse ? '1' : '0',
              '1',
              flipState ? '1' : '0', 
              flipState ? '0' : '1');
  
  gfx->setTextColor(gfx->color565(100, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FLIPFLOP");
}

void drawFlipscreen3d() {
  // 3D screen flip transition effect
  gfx->fillScreen(BLACK);
  
  static float flipTime = 0;
  flipTime += 0.1;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Screen flip angle
  float flipAngle = sin(flipTime) * PI;
  
  // Two screens - front and back
  for (int screen = 0; screen < 2; screen++) {
    float screenAngle = flipAngle + screen * PI;
    float cosAngle = cos(screenAngle);
    
    // Only draw if screen is facing forward
    if (cosAngle > 0) {
      // Screen dimensions affected by rotation
      int screenWidth = abs(cosAngle * 120);
      int screenHeight = 80;
      
      // Screen position
      int screenX = centerX - screenWidth / 2;
      int screenY = centerY - screenHeight / 2;
      
      // Screen colors
      uint16_t screenColor, contentColor;
      if (screen == 0) {
        screenColor = gfx->color565(100, 150, 255);
        contentColor = gfx->color565(255, 255, 255);
      } else {
        screenColor = gfx->color565(255, 150, 100);
        contentColor = gfx->color565(0, 0, 0);
      }
      
      // Draw screen frame
      gfx->fillRect(screenX - 5, screenY - 5, screenWidth + 10, screenHeight + 10, 
                   gfx->color565(60, 60, 60));
      gfx->fillRect(screenX, screenY, screenWidth, screenHeight, screenColor);
      
      // Screen content based on 3D depth
      float depth = cosAngle;
      
      // Content grid
      for (int row = 0; row < 6; row++) {
        for (int col = 0; col < 8; col++) {
          if (screenWidth > 20 && screenHeight > 20) {
            int cellW = screenWidth / 8;
            int cellH = screenHeight / 6;
            int cellX = screenX + col * cellW + cellW / 4;
            int cellY = screenY + row * cellH + cellH / 4;
            
            // Content varies by screen
            if (screen == 0) {
              // Screen 1: Grid pattern
              if ((row + col) % 2 == 0) {
                gfx->fillRect(cellX, cellY, cellW/2, cellH/2, contentColor);
              }
            } else {
              // Screen 2: Dot pattern  
              gfx->fillCircle(cellX + cellW/4, cellY + cellH/4, 
                            max(1, (int)(cellW/4 * depth)), contentColor);
            }
          }
        }
      }
      
      // 3D perspective lines
      if (screenWidth > 10) {
        // Top perspective
        gfx->drawLine(screenX, screenY, screenX + screenWidth/4, screenY - 10, 
                     gfx->color565(80, 80, 80));
        gfx->drawLine(screenX + screenWidth, screenY, 
                     screenX + screenWidth - screenWidth/4, screenY - 10,
                     gfx->color565(80, 80, 80));
        
        // Side perspective
        gfx->drawLine(screenX + screenWidth, screenY, 
                     screenX + screenWidth + 8, screenY + screenHeight/4,
                     gfx->color565(40, 40, 40));
        gfx->drawLine(screenX + screenWidth, screenY + screenHeight,
                     screenX + screenWidth + 8, screenY + screenHeight - screenHeight/4,
                     gfx->color565(40, 40, 40));
      }
    }
  }
  
  // Flip axis indicator
  gfx->drawFastVLine(centerX, 20, gfx->height() - 40, gfx->color565(100, 100, 100));
  
  // Motion blur effect
  for (int blur = 0; blur < 3; blur++) {
    float blurAngle = flipAngle + blur * 0.2;
    if (abs(sin(blurAngle)) > 0.8) {
      int blurWidth = abs(cos(blurAngle) * 40);
      gfx->drawRect(centerX - blurWidth/2, centerY - 30, blurWidth, 60, 
                   gfx->color565(50, 50, 50));
    }
  }
  
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FLIPSCREEN3D");
}

void drawFliptext() {
  // Text flipping and morphing effect
  gfx->fillScreen(BLACK);
  
  static float textTime = 0;
  textTime += 0.08;
  
  // Text strings to cycle through
  const char* texts[] = {"FLIP", "TEXT", "MORPH", "CYCLE", "MAGIC"};
  int numTexts = 5;
  
  int currentText = (int)(textTime * 0.3) % numTexts;
  int nextText = (currentText + 1) % numTexts;
  
  // Flip progress
  float flipProgress = fmod(textTime * 0.3, 1.0);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Text flip effect
  for (int phase = 0; phase < 2; phase++) {
    const char* displayText;
    float textPhase;
    
    if (phase == 0) {
      displayText = texts[currentText];
      textPhase = flipProgress;
    } else {
      displayText = texts[nextText];
      textPhase = flipProgress - 0.5;
    }
    
    // Only draw if in correct phase
    if ((phase == 0 && flipProgress < 0.5) || (phase == 1 && flipProgress >= 0.5)) {
      
      // Calculate text flip transformation
      float scaleY = abs(cos(textPhase * PI));
      float skew = sin(textPhase * PI) * 0.3;
      
      // Text color changes during flip
      int intensity = 150 + scaleY * 105;
      uint16_t textColor = gfx->color565(intensity, intensity, intensity);
      
      // Character-by-character flip
      int textLen = strlen(displayText);
      int totalWidth = textLen * 12; // Approximate character width
      int startX = centerX - totalWidth / 2;
      
      for (int i = 0; i < textLen; i++) {
        char c = displayText[i];
        int charX = startX + i * 12;
        int charY = centerY - 4;
        
        // Individual character flip timing
        float charDelay = i * 0.1;
        float charFlip = textPhase - charDelay;
        
        if (charFlip > 0) {
          float charScale = abs(cos(charFlip * PI * 2));
          int adjustedY = charY + (1.0 - charScale) * 10;
          
          // Character flip colors
          int r = intensity;
          int g = intensity - abs(sin(charFlip * PI)) * 100;
          int b = intensity + abs(sin(charFlip * PI)) * 50;
          
          gfx->setTextColor(gfx->color565(r, g, b));
          gfx->setTextSize(2);
          gfx->setCursor(charX, adjustedY);
          gfx->printf("%c", c);
          
          // Character shadow/reflection
          if (charScale > 0.3) {
            gfx->setTextColor(gfx->color565(r/4, g/4, b/4));
            gfx->setCursor(charX + 1, adjustedY + 20);
            gfx->printf("%c", c);
          }
        }
      }
    }
  }
  
  // Flip transition particles
  if (flipProgress > 0.4 && flipProgress < 0.6) {
    for (int particle = 0; particle < 10; particle++) {
      int pX = centerX + random(80) - 40;
      int pY = centerY + random(40) - 20;
      
      gfx->fillCircle(pX, pY, 1, gfx->color565(255, 255, 0));
    }
  }
  
  // Text flip indicator
  gfx->drawRect(10, gfx->height() - 25, gfx->width() - 20, 20, gfx->color565(100, 100, 100));
  int progressWidth = flipProgress * (gfx->width() - 24);
  gfx->fillRect(12, gfx->height() - 23, progressWidth, 16, gfx->color565(0, 255, 100));
  
  gfx->setTextColor(gfx->color565(255, 200, 100));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FLIPTEXT");
}

void drawFlow() {
  // Fluid flow field visualization
  gfx->fillScreen(BLACK);
  
  static float flowTime = 0;
  flowTime += 0.06;
  
  // Flow field parameters
  float fieldScale = 0.02;
  
  // Flow particles
  static int particleX[50];
  static int particleY[50];
  static float particleAge[50];
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 50; i++) {
      particleX[i] = random(gfx->width());
      particleY[i] = random(gfx->height());
      particleAge[i] = random(100) / 100.0;
    }
    initialized = true;
  }
  
  // Update and draw particles
  for (int i = 0; i < 50; i++) {
    // Calculate flow field at particle position
    float fieldX = particleX[i] * fieldScale;
    float fieldY = particleY[i] * fieldScale;
    
    // Flow field equations (Perlin-like noise)
    float flowAngle = sin(fieldX + flowTime) * cos(fieldY + flowTime) * PI;
    float flowStrength = (sin(fieldX * 2 + flowTime * 0.5) + 1) * 0.5;
    
    // Move particle
    float vx = cos(flowAngle) * flowStrength * 2;
    float vy = sin(flowAngle) * flowStrength * 2;
    
    particleX[i] += vx;
    particleY[i] += vy;
    particleAge[i] += 0.01;
    
    // Wrap or reset particles
    if (particleX[i] < 0 || particleX[i] >= gfx->width() || 
        particleY[i] < 0 || particleY[i] >= gfx->height() || 
        particleAge[i] > 2.0) {
      particleX[i] = random(gfx->width());
      particleY[i] = random(gfx->height());
      particleAge[i] = 0;
    }
    
    // Draw particle with trail
    float alpha = 1.0 - particleAge[i] / 2.0;
    if (alpha > 0) {
      int r = alpha * 255;
      int g = alpha * 150;
      int b = alpha * 100;
      
      gfx->fillCircle(particleX[i], particleY[i], 2, gfx->color565(r, g, b));
      
      // Particle trail
      int trailX = particleX[i] - vx * 3;
      int trailY = particleY[i] - vy * 3;
      if (trailX >= 0 && trailX < gfx->width() && trailY >= 0 && trailY < gfx->height()) {
        gfx->fillCircle(trailX, trailY, 1, gfx->color565(r/2, g/2, b/2));
      }
    }
  }
  
  // Flow field visualization (grid)
  for (int x = 0; x < gfx->width(); x += 20) {
    for (int y = 0; y < gfx->height(); y += 20) {
      float fieldX = x * fieldScale;
      float fieldY = y * fieldScale;
      
      float flowAngle = sin(fieldX + flowTime) * cos(fieldY + flowTime) * PI;
      float flowStrength = (sin(fieldX * 2 + flowTime * 0.5) + 1) * 0.5;
      
      if (flowStrength > 0.3) {
        int endX = x + cos(flowAngle) * 10;
        int endY = y + sin(flowAngle) * 10;
        
        int intensity = flowStrength * 150;
        gfx->drawLine(x, y, endX, endY, gfx->color565(0, intensity, intensity));
        
        // Arrow head
        gfx->fillCircle(endX, endY, 1, gfx->color565(0, intensity + 50, intensity + 50));
      }
    }
  }
  
  // Flow source indicators
  for (int source = 0; source < 3; source++) {
    float sourceAngle = source * 2 * PI / 3 + flowTime * 0.2;
    int sourceX = gfx->width() / 2 + cos(sourceAngle) * 60;
    int sourceY = gfx->height() / 2 + sin(sourceAngle) * 40;
    
    int sourceSize = 4 + sin(flowTime * 3 + source) * 2;
    gfx->fillCircle(sourceX, sourceY, sourceSize, gfx->color565(255, 255, 255));
    gfx->drawCircle(sourceX, sourceY, sourceSize + 3, gfx->color565(100, 200, 255));
  }
  
  gfx->setTextColor(gfx->color565(100, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FLOW");
}

void drawFluidballs() {
  // Fluid dynamics with bouncing balls
  gfx->fillScreen(BLACK);
  
  static float fluidTime = 0;
  fluidTime += 0.1;
  
  // Fluid ball parameters
  static float ballX[8] = {40, 80, 120, 160, 200, 240, 60, 140};
  static float ballY[8] = {60, 100, 40, 80, 120, 60, 140, 180};
  static float ballVX[8] = {1.5, -1.2, 0.8, -2.0, 1.3, -0.9, 1.7, -1.6};
  static float ballVY[8] = {-1.8, 1.1, -1.5, 0.7, -1.0, 1.9, -1.3, 0.9};
  static float ballRadius[8] = {8, 12, 6, 10, 14, 7, 9, 11};
  
  // Update ball physics
  for (int i = 0; i < 8; i++) {
    // Apply gravity
    ballVY[i] += 0.1;
    
    // Update position
    ballX[i] += ballVX[i];
    ballY[i] += ballVY[i];
    
    // Bounce off walls
    if (ballX[i] <= ballRadius[i] || ballX[i] >= gfx->width() - ballRadius[i]) {
      ballVX[i] *= -0.8; // Energy loss
      ballX[i] = constrain(ballX[i], ballRadius[i], gfx->width() - ballRadius[i]);
    }
    
    if (ballY[i] <= ballRadius[i] || ballY[i] >= gfx->height() - ballRadius[i]) {
      ballVY[i] *= -0.8;
      ballY[i] = constrain(ballY[i], ballRadius[i], gfx->height() - ballRadius[i]);
    }
    
    // Ball-to-ball interactions
    for (int j = i + 1; j < 8; j++) {
      float dx = ballX[j] - ballX[i];
      float dy = ballY[j] - ballY[i];
      float dist = sqrt(dx*dx + dy*dy);
      float minDist = ballRadius[i] + ballRadius[j];
      
      if (dist < minDist && dist > 0) {
        // Collision response
        float overlap = minDist - dist;
        float nx = dx / dist;
        float ny = dy / dist;
        
        // Separate balls
        ballX[i] -= nx * overlap * 0.5;
        ballY[i] -= ny * overlap * 0.5;
        ballX[j] += nx * overlap * 0.5;
        ballY[j] += ny * overlap * 0.5;
        
        // Exchange velocities (simplified)
        float tempVX = ballVX[i];
        float tempVY = ballVY[i];
        ballVX[i] = ballVX[j] * 0.9;
        ballVY[i] = ballVY[j] * 0.9;
        ballVX[j] = tempVX * 0.9;
        ballVY[j] = tempVY * 0.9;
      }
    }
  }
  
  // Draw fluid connections
  for (int i = 0; i < 8; i++) {
    for (int j = i + 1; j < 8; j++) {
      float dx = ballX[j] - ballX[i];
      float dy = ballY[j] - ballY[i];
      float dist = sqrt(dx*dx + dy*dy);
      
      if (dist < 80) {
        float connection = 1.0 - dist / 80.0;
        int alpha = connection * 150;
        gfx->drawLine(ballX[i], ballY[i], ballX[j], ballY[j], 
                     gfx->color565(0, alpha, alpha));
      }
    }
  }
  
  // Draw balls with fluid effect
  for (int i = 0; i < 8; i++) {
    // Ball color based on velocity
    float speed = sqrt(ballVX[i]*ballVX[i] + ballVY[i]*ballVY[i]);
    int r = constrain(speed * 50, 50, 255);
    int g = constrain(255 - speed * 30, 100, 255);
    int b = 200;
    
    // Main ball
    gfx->fillCircle(ballX[i], ballY[i], ballRadius[i], gfx->color565(r, g, b));
    
    // Highlight
    gfx->fillCircle(ballX[i] - 2, ballY[i] - 2, ballRadius[i]/3, 
                   gfx->color565(255, 255, 255));
    
    // Motion trail
    int trailX = ballX[i] - ballVX[i] * 5;
    int trailY = ballY[i] - ballVY[i] * 5;
    gfx->fillCircle(trailX, trailY, ballRadius[i]/2, gfx->color565(r/3, g/3, b/3));
  }
  
  gfx->setTextColor(gfx->color565(100, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FLUIDBALLS");
}

void drawFlurry() {
  // Snow flurry effect
  gfx->fillScreen(BLACK);
  
  static float flurryTime = 0;
  flurryTime += 0.08;
  
  // Wind effect
  float windX = sin(flurryTime * 0.5) * 2;
  float windY = cos(flurryTime * 0.3) * 0.5;
  
  // Snowflakes
  static float snowX[60];
  static float snowY[60];
  static float snowVX[60];
  static float snowVY[60];
  static float snowSize[60];
  static float snowSpin[60];
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 60; i++) {
      snowX[i] = random(gfx->width());
      snowY[i] = random(gfx->height());
      snowVX[i] = (random(200) - 100) / 100.0;
      snowVY[i] = random(100) / 50.0 + 0.5;
      snowSize[i] = random(30) / 10.0 + 1;
      snowSpin[i] = random(628) / 100.0;
    }
    initialized = true;
  }
  
  // Update and draw snowflakes
  for (int i = 0; i < 60; i++) {
    // Update position with wind
    snowVX[i] += (windX - snowVX[i]) * 0.02;
    snowX[i] += snowVX[i];
    snowY[i] += snowVY[i];
    snowSpin[i] += 0.1;
    
    // Wrap around screen
    if (snowX[i] < -10) snowX[i] = gfx->width() + 10;
    if (snowX[i] > gfx->width() + 10) snowX[i] = -10;
    if (snowY[i] > gfx->height() + 10) {
      snowY[i] = -10;
      snowX[i] = random(gfx->width());
    }
    
    // Draw snowflake
    int x = snowX[i];
    int y = snowY[i];
    float size = snowSize[i];
    
    // Snowflake brightness based on size and position
    float brightness = size / 4.0;
    int alpha = brightness * 255;
    uint16_t snowColor = gfx->color565(alpha, alpha, alpha);
    
    // Draw snowflake pattern
    if (size > 2) {
      // Complex snowflake
      for (int arm = 0; arm < 6; arm++) {
        float armAngle = arm * PI / 3 + snowSpin[i];
        int armLength = size * 2;
        
        int endX = x + cos(armAngle) * armLength;
        int endY = y + sin(armAngle) * armLength;
        
        gfx->drawLine(x, y, endX, endY, snowColor);
        
        // Branch details
        int branchX = x + cos(armAngle) * armLength * 0.6;
        int branchY = y + sin(armAngle) * armLength * 0.6;
        int branch1X = branchX + cos(armAngle + 0.5) * size;
        int branch1Y = branchY + sin(armAngle + 0.5) * size;
        int branch2X = branchX + cos(armAngle - 0.5) * size;
        int branch2Y = branchY + sin(armAngle - 0.5) * size;
        
        gfx->drawLine(branchX, branchY, branch1X, branch1Y, snowColor);
        gfx->drawLine(branchX, branchY, branch2X, branch2Y, snowColor);
      }
    } else {
      // Simple snowflake
      gfx->fillCircle(x, y, size, snowColor);
    }
  }
  
  // Ground accumulation
  for (int x = 0; x < gfx->width(); x += 4) {
    float accumulation = sin(x * 0.1 + flurryTime * 0.2) * 3 + 5;
    gfx->fillRect(x, gfx->height() - accumulation, 4, accumulation, 
                 gfx->color565(200, 200, 255));
  }
  
  // Wind indicator
  int windIndicatorX = gfx->width() - 30;
  int windIndicatorY = 20;
  gfx->drawLine(windIndicatorX, windIndicatorY, 
               windIndicatorX + windX * 10, windIndicatorY + windY * 10,
               gfx->color565(100, 100, 255));
  
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FLURRY");
}

void drawFlurrySmoke() {
  // Flurry combined with smoke effect
  gfx->fillScreen(BLACK);
  
  static float smokeTime = 0;
  smokeTime += 0.12;
  
  // Smoke sources
  int smokeSourceX = gfx->width() / 2;
  int smokeSourceY = gfx->height() - 10;
  
  // Smoke particles
  static float smokeX[40];
  static float smokeY[40];
  static float smokeVX[40];
  static float smokeVY[40];
  static float smokeAge[40];
  static float smokeSize[40];
  static bool smokeInit = false;
  
  if (!smokeInit) {
    for (int i = 0; i < 40; i++) {
      smokeX[i] = smokeSourceX + random(20) - 10;
      smokeY[i] = smokeSourceY;
      smokeVX[i] = (random(100) - 50) / 50.0;
      smokeVY[i] = -random(100) / 50.0 - 1;
      smokeAge[i] = random(100) / 100.0 * 3;
      smokeSize[i] = random(50) / 10.0 + 2;
    }
    smokeInit = true;
  }
  
  // Update smoke
  for (int i = 0; i < 40; i++) {
    smokeAge[i] += 0.02;
    
    // Smoke physics
    smokeVY[i] -= 0.01; // Rise
    smokeVX[i] += sin(smokeTime + i) * 0.02; // Turbulence
    
    smokeX[i] += smokeVX[i];
    smokeY[i] += smokeVY[i];
    smokeSize[i] += 0.1; // Expand
    
    // Reset old smoke
    if (smokeAge[i] > 3 || smokeY[i] < -20) {
      smokeX[i] = smokeSourceX + random(20) - 10;
      smokeY[i] = smokeSourceY;
      smokeVX[i] = (random(100) - 50) / 50.0;
      smokeVY[i] = -random(100) / 50.0 - 1;
      smokeAge[i] = 0;
      smokeSize[i] = random(50) / 10.0 + 2;
    }
    
    // Draw smoke
    float alpha = 1.0 - smokeAge[i] / 3.0;
    if (alpha > 0) {
      int smokeAlpha = alpha * 100;
      gfx->fillCircle(smokeX[i], smokeY[i], smokeSize[i], 
                     gfx->color565(smokeAlpha, smokeAlpha, smokeAlpha));
    }
  }
  
  // Snow flurry over smoke
  static float flurryX[30];
  static float flurryY[30];
  static bool flurryInit = false;
  
  if (!flurryInit) {
    for (int i = 0; i < 30; i++) {
      flurryX[i] = random(gfx->width());
      flurryY[i] = random(gfx->height());
    }
    flurryInit = true;
  }
  
  // Update and draw snow
  for (int i = 0; i < 30; i++) {
    flurryY[i] += 1 + sin(smokeTime + i) * 0.5;
    flurryX[i] += sin(smokeTime * 2 + i) * 0.3;
    
    if (flurryY[i] > gfx->height()) {
      flurryY[i] = -5;
      flurryX[i] = random(gfx->width());
    }
    
    // Snow interacting with smoke heat
    float distToSmoke = sqrt(pow(flurryX[i] - smokeSourceX, 2) + 
                           pow(flurryY[i] - smokeSourceY, 2));
    
    if (distToSmoke < 50) {
      // Melt effect near smoke
      float melt = 1.0 - distToSmoke / 50.0;
      int meltAlpha = (1.0 - melt) * 255;
      gfx->fillCircle(flurryX[i], flurryY[i], 2, 
                     gfx->color565(meltAlpha, meltAlpha, 255));
    } else {
      // Normal snow
      gfx->fillCircle(flurryX[i], flurryY[i], 2, gfx->color565(255, 255, 255));
      
      // Snowflake arms
      gfx->drawLine(flurryX[i]-2, flurryY[i], flurryX[i]+2, flurryY[i], 
                   gfx->color565(200, 200, 200));
      gfx->drawLine(flurryX[i], flurryY[i]-2, flurryX[i], flurryY[i]+2, 
                   gfx->color565(200, 200, 200));
    }
  }
  
  // Smoke source (fire/chimney)
  gfx->fillRect(smokeSourceX - 8, smokeSourceY - 5, 16, 10, 
               gfx->color565(139, 69, 19));
  int flameSize = 3 + sin(smokeTime * 6) * 2;
  gfx->fillCircle(smokeSourceX, smokeSourceY - 8, flameSize, 
                 gfx->color565(255, 100, 0));
  
  gfx->setTextColor(gfx->color565(255, 200, 150));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FLURRY-SMOKE");
}

void drawFlurrySpark() {
  // Snow flurry with electric sparks
  gfx->fillScreen(BLACK);
  
  static float sparkTime = 0;
  sparkTime += 0.15;
  
  // Snow particles
  static float snowX[40];
  static float snowY[40];
  static float snowVY[40];
  static bool snowInit = false;
  
  if (!snowInit) {
    for (int i = 0; i < 40; i++) {
      snowX[i] = random(gfx->width());
      snowY[i] = random(gfx->height());
      snowVY[i] = random(100) / 50.0 + 0.5;
    }
    snowInit = true;
  }
  
  // Update snow
  for (int i = 0; i < 40; i++) {
    snowY[i] += snowVY[i];
    
    if (snowY[i] > gfx->height()) {
      snowY[i] = -5;
      snowX[i] = random(gfx->width());
    }
    
    gfx->fillCircle(snowX[i], snowY[i], 2, gfx->color565(255, 255, 255));
  }
  
  // Electric spark sources
  static int sparkSourceX[3] = {60, 160, 260};
  static int sparkSourceY[3] = {50, 100, 150};
  
  for (int source = 0; source < 3; source++) {
    int sourceX = sparkSourceX[source];
    int sourceY = sparkSourceY[source];
    
    // Spark generator
    gfx->fillCircle(sourceX, sourceY, 4, gfx->color565(255, 255, 0));
    gfx->drawCircle(sourceX, sourceY, 8, gfx->color565(200, 200, 0));
    
    // Electric arcs
    for (int arc = 0; arc < 5; arc++) {
      float arcAngle = arc * 2 * PI / 5 + sparkTime * 3 + source;
      float arcLength = 20 + sin(sparkTime * 4 + arc + source) * 15;
      
      // Jagged spark path
      int segments = 8;
      int prevX = sourceX;
      int prevY = sourceY;
      
      for (int seg = 1; seg <= segments; seg++) {
        float t = seg / (float)segments;
        float baseX = sourceX + cos(arcAngle) * arcLength * t;
        float baseY = sourceY + sin(arcAngle) * arcLength * t;
        
        // Add random jitter for spark effect
        int jitterX = baseX + sin(sparkTime * 10 + seg + source) * 8;
        int jitterY = baseY + cos(sparkTime * 8 + seg + source) * 8;
        
        // Spark intensity
        float intensity = 1.0 - t;
        int sparkBright = intensity * 255;
        
        gfx->drawLine(prevX, prevY, jitterX, jitterY, 
                     gfx->color565(sparkBright, sparkBright, 255));
        
        prevX = jitterX;
        prevY = jitterY;
      }
    }
    
    // Snow interaction with sparks
    for (int i = 0; i < 40; i++) {
      float distToSpark = sqrt(pow(snowX[i] - sourceX, 2) + pow(snowY[i] - sourceY, 2));
      
      if (distToSpark < 30) {
        // Electrified snow
        int sparkSnow = (30 - distToSpark) / 30.0 * 255;
        gfx->fillCircle(snowX[i], snowY[i], 3, gfx->color565(255, sparkSnow, 255));
        
        // Small electric tendrils
        if (random(100) < 20) {
          int tendrilX = snowX[i] + random(10) - 5;
          int tendrilY = snowY[i] + random(10) - 5;
          gfx->drawLine(snowX[i], snowY[i], tendrilX, tendrilY, 
                       gfx->color565(200, 200, 255));
        }
      }
    }
  }
  
  // Lightning flash
  if (sin(sparkTime * 2) > 0.9) {
    // Random lightning bolt
    int boltX1 = random(gfx->width());
    int boltY1 = 0;
    int boltX2 = boltX1 + random(60) - 30;
    int boltY2 = gfx->height();
    
    // Jagged bolt
    int boltSegments = 10;
    int prevBoltX = boltX1;
    int prevBoltY = boltY1;
    
    for (int i = 1; i <= boltSegments; i++) {
      int segX = boltX1 + (boltX2 - boltX1) * i / boltSegments + random(20) - 10;
      int segY = boltY1 + (boltY2 - boltY1) * i / boltSegments;
      
      gfx->drawLine(prevBoltX, prevBoltY, segX, segY, gfx->color565(255, 255, 255));
      
      prevBoltX = segX;
      prevBoltY = segY;
    }
  }
  
  gfx->setTextColor(gfx->color565(255, 255, 100));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FLURRY-SPARK");
}

void drawFlurryStar() {
  // Snow flurry with star patterns
  gfx->fillScreen(BLACK);
  
  static float starTime = 0;
  starTime += 0.1;
  
  // Star-shaped snowflakes
  static float starX[30];
  static float starY[30];
  static float starVY[30];
  static float starSpin[30];
  static float starSize[30];
  static bool starInit = false;
  
  if (!starInit) {
    for (int i = 0; i < 30; i++) {
      starX[i] = random(gfx->width());
      starY[i] = random(gfx->height());
      starVY[i] = random(100) / 100.0 + 0.3;
      starSpin[i] = random(628) / 100.0;
      starSize[i] = random(40) / 10.0 + 2;
    }
    starInit = true;
  }
  
  // Update and draw star snowflakes
  for (int i = 0; i < 30; i++) {
    starY[i] += starVY[i];
    starSpin[i] += 0.05;
    
    if (starY[i] > gfx->height() + 10) {
      starY[i] = -10;
      starX[i] = random(gfx->width());
    }
    
    // Draw star snowflake
    int x = starX[i];
    int y = starY[i];
    float size = starSize[i];
    
    // Star brightness
    float twinkle = sin(starTime * 4 + i) * 0.3 + 0.7;
    int brightness = twinkle * 255;
    
    // Draw 5-pointed star
    for (int point = 0; point < 5; point++) {
      float outerAngle = point * 2 * PI / 5 + starSpin[i];
      float innerAngle = outerAngle + PI / 5;
      
      // Outer point
      int outerX = x + cos(outerAngle) * size;
      int outerY = y + sin(outerAngle) * size;
      
      // Inner point
      int innerX = x + cos(innerAngle) * size * 0.4;
      int innerY = y + sin(innerAngle) * size * 0.4;
      
      // Next outer point
      float nextOuterAngle = (point + 1) * 2 * PI / 5 + starSpin[i];
      int nextOuterX = x + cos(nextOuterAngle) * size;
      int nextOuterY = y + sin(nextOuterAngle) * size;
      
      // Draw star segments
      gfx->drawLine(x, y, outerX, outerY, gfx->color565(brightness, brightness, 255));
      gfx->drawLine(outerX, outerY, innerX, innerY, gfx->color565(brightness, brightness, 255));
      gfx->drawLine(innerX, innerY, nextOuterX, nextOuterY, gfx->color565(brightness, brightness, 255));
    }
    
    // Central star core
    gfx->fillCircle(x, y, size / 4, gfx->color565(255, 255, brightness));
  }
  
  // Background constellation stars
  for (int bg = 0; bg < 50; bg++) {
    float bgAngle = bg * 0.5 + starTime * 0.1;
    int bgX = 20 + bg * 6;
    int bgY = 30 + sin(bgAngle) * 20;
    
    if (bgX < gfx->width() && bgY > 0 && bgY < gfx->height()) {
      float bgTwinkle = sin(starTime * 3 + bg) * 0.5 + 0.5;
      int bgBright = bgTwinkle * 150 + 50;
      
      gfx->fillCircle(bgX, bgY, 1, gfx->color565(bgBright, bgBright, bgBright));
      
      // Occasional bright twinkle
      if (bgTwinkle > 0.8) {
        gfx->drawLine(bgX - 2, bgY, bgX + 2, bgY, gfx->color565(200, 200, 255));
        gfx->drawLine(bgX, bgY - 2, bgX, bgY + 2, gfx->color565(200, 200, 255));
      }
    }
  }
  
  // Shooting stars
  static float shootingStarX = 0;
  static float shootingStarY = 0;
  static float shootingTimer = 0;
  
  shootingTimer += 0.1;
  if (shootingTimer > 5) {
    shootingStarX = gfx->width() + 20;
    shootingStarY = random(gfx->height() / 2);
    shootingTimer = 0;
  }
  
  if (shootingStarX > -20) {
    shootingStarX -= 4;
    shootingStarY += 1;
    
    // Draw shooting star
    gfx->fillCircle(shootingStarX, shootingStarY, 2, gfx->color565(255, 255, 255));
    
    // Tail
    for (int tail = 1; tail < 8; tail++) {
      int tailX = shootingStarX + tail * 4;
      int tailY = shootingStarY - tail;
      float tailAlpha = 1.0 - tail / 8.0;
      int tailBright = tailAlpha * 200;
      
      gfx->fillCircle(tailX, tailY, 1, gfx->color565(tailBright, tailBright, 255));
    }
  }
  
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FLURRY-STAR");
}

void drawFlurryTexture() {
  // Textured snow flurry effect
  gfx->fillScreen(BLACK);
  
  static float textureTime = 0;
  textureTime += 0.06;
  
  // Textured snow particles
  static float texSnowX[50];
  static float texSnowY[50];
  static float texSnowVX[50];
  static float texSnowVY[50];
  static int texSnowType[50];
  static float texSnowSpin[50];
  static bool texInit = false;
  
  if (!texInit) {
    for (int i = 0; i < 50; i++) {
      texSnowX[i] = random(gfx->width());
      texSnowY[i] = random(gfx->height());
      texSnowVX[i] = (random(100) - 50) / 200.0;
      texSnowVY[i] = random(100) / 100.0 + 0.5;
      texSnowType[i] = random(4); // Different texture types
      texSnowSpin[i] = random(628) / 100.0;
    }
    texInit = true;
  }
  
  // Update textured snow
  for (int i = 0; i < 50; i++) {
    texSnowX[i] += texSnowVX[i] + sin(textureTime + i) * 0.3;
    texSnowY[i] += texSnowVY[i];
    texSnowSpin[i] += 0.08;
    
    if (texSnowY[i] > gfx->height() + 5) {
      texSnowY[i] = -5;
      texSnowX[i] = random(gfx->width());
    }
    
    int x = texSnowX[i];
    int y = texSnowY[i];
    
    // Draw different textured snowflakes
    switch (texSnowType[i]) {
      case 0: // Crystalline
        {
          uint16_t crystalColor = gfx->color565(200, 220, 255);
          // Hexagonal crystal
          for (int side = 0; side < 6; side++) {
            float angle = side * PI / 3 + texSnowSpin[i];
            int x1 = x + cos(angle) * 4;
            int y1 = y + sin(angle) * 4;
            int x2 = x + cos(angle + PI/3) * 4;
            int y2 = y + sin(angle + PI/3) * 4;
            gfx->drawLine(x1, y1, x2, y2, crystalColor);
          }
          gfx->fillCircle(x, y, 1, gfx->color565(255, 255, 255));
        }
        break;
        
      case 1: // Feathery
        {
          uint16_t featherColor = gfx->color565(180, 200, 255);
          // Feather-like branches
          for (int branch = 0; branch < 4; branch++) {
            float branchAngle = branch * PI / 2 + texSnowSpin[i];
            int branchLen = 5;
            
            for (int seg = 0; seg < branchLen; seg++) {
              int segX = x + cos(branchAngle) * seg;
              int segY = y + sin(branchAngle) * seg;
              
              // Side feathers
              int featherX1 = segX + cos(branchAngle + PI/4) * 2;
              int featherY1 = segY + sin(branchAngle + PI/4) * 2;
              int featherX2 = segX + cos(branchAngle - PI/4) * 2;
              int featherY2 = segY + sin(branchAngle - PI/4) * 2;
              
              gfx->drawLine(segX, segY, featherX1, featherY1, featherColor);
              gfx->drawLine(segX, segY, featherX2, featherY2, featherColor);
            }
          }
        }
        break;
        
      case 2: // Dendrite
        {
          uint16_t dendriteColor = gfx->color565(220, 230, 255);
          // Tree-like dendrite pattern
          float mainAngle = texSnowSpin[i];
          
          for (int main = 0; main < 3; main++) {
            float angle = mainAngle + main * 2 * PI / 3;
            int mainX = x + cos(angle) * 6;
            int mainY = y + sin(angle) * 6;
            
            gfx->drawLine(x, y, mainX, mainY, dendriteColor);
            
            // Secondary branches
            for (int sec = 1; sec < 4; sec++) {
              int secBaseX = x + cos(angle) * sec * 2;
              int secBaseY = y + sin(angle) * sec * 2;
              
              int sec1X = secBaseX + cos(angle + PI/3) * 2;
              int sec1Y = secBaseY + sin(angle + PI/3) * 2;
              int sec2X = secBaseX + cos(angle - PI/3) * 2;
              int sec2Y = secBaseY + sin(angle - PI/3) * 2;
              
              gfx->drawLine(secBaseX, secBaseY, sec1X, sec1Y, dendriteColor);
              gfx->drawLine(secBaseX, secBaseY, sec2X, sec2Y, dendriteColor);
            }
          }
        }
        break;
        
      case 3: // Plate crystal
        {
          uint16_t plateColor = gfx->color565(240, 240, 255);
          // Flat plate with internal structure
          
          // Outer hexagon
          for (int side = 0; side < 6; side++) {
            float angle1 = side * PI / 3 + texSnowSpin[i];
            float angle2 = (side + 1) * PI / 3 + texSnowSpin[i];
            
            int x1 = x + cos(angle1) * 5;
            int y1 = y + sin(angle1) * 5;
            int x2 = x + cos(angle2) * 5;
            int y2 = y + sin(angle2) * 5;
            
            gfx->drawLine(x1, y1, x2, y2, plateColor);
            
            // Internal lines
            gfx->drawLine(x, y, x1, y1, plateColor);
          }
          
          // Inner pattern
          gfx->drawCircle(x, y, 2, plateColor);
        }
        break;
    }
  }
  
  // Texture overlay effect
  for (int tx = 0; tx < gfx->width(); tx += 8) {
    for (int ty = 0; ty < gfx->height(); ty += 8) {
      float texNoise = sin(tx * 0.1 + textureTime) * cos(ty * 0.1 + textureTime);
      if (texNoise > 0.3) {
        int texAlpha = (texNoise - 0.3) * 100;
        gfx->fillRect(tx, ty, 2, 2, gfx->color565(texAlpha, texAlpha, texAlpha + 50));
      }
    }
  }
  
  gfx->setTextColor(gfx->color565(220, 230, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FLURRY-TEXTURE");
}

void drawFlyingtoasters() {
  // Classic flying toasters screensaver
  gfx->fillScreen(BLACK);
  
  static float toasterTime = 0;
  toasterTime += 0.08;
  
  // Flying toasters
  static float toasterX[6] = {320, 380, 440, 500, 360, 420};
  static float toasterY[6] = {40, 80, 120, 160, 200, 60};
  static float toasterSpeed[6] = {1.2, 0.8, 1.5, 1.0, 0.9, 1.3};
  static float toasterBob[6] = {0, 1, 2, 3, 4, 5};
  
  for (int i = 0; i < 6; i++) {
    // Move toaster
    toasterX[i] -= toasterSpeed[i];
    
    // Reset when off screen
    if (toasterX[i] < -40) {
      toasterX[i] = gfx->width() + 40;
      toasterY[i] = random(gfx->height() - 60) + 20;
    }
    
    // Bobbing motion
    float bobOffset = sin(toasterTime * 2 + toasterBob[i]) * 8;
    int drawY = toasterY[i] + bobOffset;
    
    // Draw toaster body
    gfx->fillRect(toasterX[i], drawY, 28, 18, gfx->color565(180, 180, 180));
    gfx->drawRect(toasterX[i], drawY, 28, 18, gfx->color565(100, 100, 100));
    
    // Toaster slots
    gfx->fillRect(toasterX[i] + 3, drawY + 2, 6, 14, gfx->color565(50, 50, 50));
    gfx->fillRect(toasterX[i] + 11, drawY + 2, 6, 14, gfx->color565(50, 50, 50));
    gfx->fillRect(toasterX[i] + 19, drawY + 2, 6, 14, gfx->color565(50, 50, 50));
    
    // Toaster knobs
    gfx->fillCircle(toasterX[i] + 26, drawY + 5, 2, gfx->color565(120, 120, 120));
    gfx->fillCircle(toasterX[i] + 26, drawY + 13, 2, gfx->color565(120, 120, 120));
    
    // Wings
    float wingFlap = sin(toasterTime * 8 + i) * 0.5 + 0.5;
    int wingSpread = 8 + wingFlap * 6;
    
    // Left wing
    gfx->drawLine(toasterX[i] - 2, drawY + 4, toasterX[i] - wingSpread, drawY - 2, 
                 gfx->color565(255, 255, 255));
    gfx->drawLine(toasterX[i] - 2, drawY + 8, toasterX[i] - wingSpread, drawY + 2, 
                 gfx->color565(255, 255, 255));
    gfx->drawLine(toasterX[i] - wingSpread, drawY - 2, toasterX[i] - wingSpread, drawY + 2, 
                 gfx->color565(255, 255, 255));
    
    // Right wing  
    gfx->drawLine(toasterX[i] + 30, drawY + 4, toasterX[i] + 30 + wingSpread, drawY - 2, 
                 gfx->color565(255, 255, 255));
    gfx->drawLine(toasterX[i] + 30, drawY + 8, toasterX[i] + 30 + wingSpread, drawY + 2, 
                 gfx->color565(255, 255, 255));
    gfx->drawLine(toasterX[i] + 30 + wingSpread, drawY - 2, toasterX[i] + 30 + wingSpread, drawY + 2, 
                 gfx->color565(255, 255, 255));
    
    // Toast pieces flying out
    if (sin(toasterTime * 4 + i) > 0.7) {
      int toastX = toasterX[i] + 14;
      int toastY = drawY - 8;
      
      // Flying toast
      gfx->fillRect(toastX - 3, toastY, 6, 4, gfx->color565(200, 150, 100));
      gfx->drawRect(toastX - 3, toastY, 6, 4, gfx->color565(139, 69, 19));
    }
  }
  
  // Background clouds
  for (int cloud = 0; cloud < 8; cloud++) {
    float cloudX = cloud * 50 + sin(toasterTime * 0.3 + cloud) * 20;
    float cloudY = 30 + cloud * 20 + cos(toasterTime * 0.2 + cloud) * 10;
    
    if (cloudX > -30 && cloudX < gfx->width() + 30) {
      // Cloud puffs
      gfx->fillCircle(cloudX, cloudY, 8, gfx->color565(80, 80, 100));
      gfx->fillCircle(cloudX + 6, cloudY, 6, gfx->color565(80, 80, 100));
      gfx->fillCircle(cloudX - 6, cloudY, 6, gfx->color565(80, 80, 100));
      gfx->fillCircle(cloudX, cloudY - 4, 5, gfx->color565(80, 80, 100));
    }
  }
  
  gfx->setTextColor(gfx->color565(255, 255, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FLYINGTOASTERS");
}

void drawFontglide() {
  // Font gliding and morphing effect
  gfx->fillScreen(BLACK);
  
  static float glideTime = 0;
  glideTime += 0.06;
  
  // Text to display
  const char* glideText = "GLIDE";
  int textLen = strlen(glideText);
  
  // Character gliding
  for (int i = 0; i < textLen; i++) {
    char c = glideText[i];
    
    // Character position with wave motion
    float charPhase = glideTime + i * 0.8;
    int baseX = 40 + i * 35;
    int baseY = gfx->height() / 2;
    
    // Gliding motion - sine wave path
    int glideX = baseX + sin(charPhase * 0.8) * 60;
    int glideY = baseY + cos(charPhase * 0.6) * 40;
    
    // Character size variation
    int charSize = 2 + sin(charPhase * 1.2) * 1;
    
    // Character color cycling
    float colorPhase = charPhase * 2;
    int r = 128 + sin(colorPhase) * 127;
    int g = 128 + sin(colorPhase + PI/3) * 127;
    int b = 128 + sin(colorPhase + 2*PI/3) * 127;
    
    // Draw character with trail effect
    for (int trail = 0; trail < 5; trail++) {
      float trailPhase = charPhase - trail * 0.1;
      int trailX = baseX + sin(trailPhase * 0.8) * 60;
      int trailY = baseY + cos(trailPhase * 0.6) * 40;
      
      float trailAlpha = 1.0 - trail / 5.0;
      int trailR = r * trailAlpha;
      int trailG = g * trailAlpha;
      int trailB = b * trailAlpha;
      
      gfx->setTextColor(gfx->color565(trailR, trailG, trailB));
      gfx->setTextSize(charSize);
      gfx->setCursor(trailX - 6, trailY - 8);
      gfx->printf("%c", c);
    }
    
    // Main character
    gfx->setTextColor(gfx->color565(r, g, b));
    gfx->setTextSize(charSize);
    gfx->setCursor(glideX - 6, glideY - 8);
    gfx->printf("%c", c);
    
    // Character glow effect
    gfx->setTextColor(gfx->color565(255, 255, 255));
    gfx->setTextSize(1);
    gfx->setCursor(glideX - 3, glideY - 4);
    gfx->printf("%c", c);
  }
  
  // Background particle system
  static float particleX[20];
  static float particleY[20];
  static float particleVX[20];
  static float particleVY[20];
  static bool particleInit = false;
  
  if (!particleInit) {
    for (int i = 0; i < 20; i++) {
      particleX[i] = random(gfx->width());
      particleY[i] = random(gfx->height());
      particleVX[i] = (random(100) - 50) / 100.0;
      particleVY[i] = (random(100) - 50) / 100.0;
    }
    particleInit = true;
  }
  
  // Update and draw particles
  for (int i = 0; i < 20; i++) {
    particleX[i] += particleVX[i];
    particleY[i] += particleVY[i];
    
    // Wrap particles
    if (particleX[i] < 0) particleX[i] = gfx->width();
    if (particleX[i] > gfx->width()) particleX[i] = 0;
    if (particleY[i] < 0) particleY[i] = gfx->height();
    if (particleY[i] > gfx->height()) particleY[i] = 0;
    
    // Particle attraction to text
    float attraction = sin(glideTime * 3 + i) * 0.5 + 0.5;
    int particleBright = attraction * 150 + 50;
    
    gfx->fillCircle(particleX[i], particleY[i], 1, gfx->color565(particleBright, particleBright, 255));
  }
  
  // Font style indicator
  gfx->setTextColor(gfx->color565(200, 200, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Style: %d", (int)(glideTime) % 4 + 1);
  
  gfx->setTextColor(gfx->color565(100, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FONTGLIDE");
}

void drawFontRetry() {
  // Font loading retry simulation
  gfx->fillScreen(BLACK);
  
  static float retryTime = 0;
  retryTime += 0.1;
  
  // Retry cycle
  int retryPhase = (int)(retryTime) % 8;
  float phaseProgress = retryTime - (int)(retryTime);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  switch (retryPhase) {
    case 0: // Loading
      {
        gfx->setTextColor(gfx->color565(200, 200, 200));
        gfx->setTextSize(2);
        gfx->setCursor(centerX - 42, centerY - 8);
        gfx->printf("Loading");
        
        // Loading dots
        int dots = (int)(phaseProgress * 12) % 4;
        gfx->setCursor(centerX + 30, centerY - 8);
        for (int i = 0; i < dots; i++) {
          gfx->printf(".");
        }
        
        // Progress bar
        int progressWidth = phaseProgress * (gfx->width() - 40);
        gfx->drawRect(20, centerY + 20, gfx->width() - 40, 8, gfx->color565(100, 100, 100));
        gfx->fillRect(21, centerY + 21, progressWidth, 6, gfx->color565(0, 200, 0));
      }
      break;
      
    case 1: // Error
      {
        gfx->setTextColor(gfx->color565(255, 100, 100));
        gfx->setTextSize(2);
        gfx->setCursor(centerX - 30, centerY - 16);
        gfx->printf("ERROR");
        
        gfx->setTextSize(1);
        gfx->setCursor(centerX - 45, centerY + 8);
        gfx->printf("Font load failed");
        
        // Error icon
        gfx->drawCircle(centerX, centerY - 40, 12, gfx->color565(255, 0, 0));
        gfx->drawLine(centerX - 6, centerY - 46, centerX + 6, centerY - 34, gfx->color565(255, 0, 0));
        gfx->drawLine(centerX + 6, centerY - 46, centerX - 6, centerY - 34, gfx->color565(255, 0, 0));
      }
      break;
      
    case 2: // Retry countdown
      {
        int countdown = 3 - (int)(phaseProgress * 3);
        gfx->setTextColor(gfx->color565(255, 255, 100));
        gfx->setTextSize(2);
        gfx->setCursor(centerX - 36, centerY - 16);
        gfx->printf("Retry");
        
        gfx->setTextSize(3);
        gfx->setCursor(centerX - 8, centerY + 8);
        gfx->printf("%d", countdown + 1);
        
        // Countdown circle
        float angle = phaseProgress * 2 * PI;
        for (int i = 0; i < 20; i++) {
          float circleAngle = i * 2 * PI / 20;
          if (circleAngle <= angle) {
            int circleX = centerX + cos(circleAngle - PI/2) * 25;
            int circleY = centerY + sin(circleAngle - PI/2) * 25;
            gfx->fillCircle(circleX, circleY, 2, gfx->color565(255, 255, 0));
          }
        }
      }
      break;
      
    case 3: // Connecting
      {
        gfx->setTextColor(gfx->color565(100, 200, 255));
        gfx->setTextSize(2);
        gfx->setCursor(centerX - 54, centerY - 8);
        gfx->printf("Connecting");
        
        // Spinning connector
        float spinAngle = phaseProgress * 4 * PI;
        for (int i = 0; i < 8; i++) {
          float dotAngle = i * PI / 4 + spinAngle;
          int dotX = centerX + cos(dotAngle) * 20;
          int dotY = centerY + 30 + sin(dotAngle) * 20;
          
          float dotAlpha = (sin(dotAngle + spinAngle) + 1) / 2;
          int dotBright = dotAlpha * 255;
          gfx->fillCircle(dotX, dotY, 3, gfx->color565(0, dotBright, 255));
        }
      }
      break;
      
    case 4: // Downloading
      {
        gfx->setTextColor(gfx->color565(150, 255, 150));
        gfx->setTextSize(2);
        gfx->setCursor(centerX - 66, centerY - 8);
        gfx->printf("Download");
        
        // Download arrow
        gfx->drawLine(centerX, centerY - 35, centerX, centerY - 15, gfx->color565(0, 255, 0));
        gfx->fillTriangle(centerX - 5, centerY - 20, centerX + 5, centerY - 20, centerX, centerY - 10, 
                         gfx->color565(0, 255, 0));
        
        // Data packets
        for (int packet = 0; packet < 6; packet++) {
          int packetY = centerY - 35 + (phaseProgress + packet * 0.2) * 30;
          if (packetY > centerY - 35 && packetY < centerY - 10) {
            gfx->fillRect(centerX - 2, packetY, 4, 3, gfx->color565(0, 200, 0));
          }
        }
      }
      break;
      
    case 5: // Installing
      {
        gfx->setTextColor(gfx->color565(255, 200, 100));
        gfx->setTextSize(2);
        gfx->setCursor(centerX - 54, centerY - 8);
        gfx->printf("Installing");
        
        // Installation progress wheel
        int segments = 12;
        for (int i = 0; i < segments; i++) {
          float segAngle = i * 2 * PI / segments;
          int segX = centerX + cos(segAngle) * 20;
          int segY = centerY + 25 + sin(segAngle) * 20;
          
          if (i <= phaseProgress * segments) {
            gfx->fillCircle(segX, segY, 2, gfx->color565(255, 150, 0));
          } else {
            gfx->drawCircle(segX, segY, 2, gfx->color565(100, 75, 0));
          }
        }
      }
      break;
      
    case 6: // Success
      {
        gfx->setTextColor(gfx->color565(100, 255, 100));
        gfx->setTextSize(2);
        gfx->setCursor(centerX - 42, centerY - 8);
        gfx->printf("Success!");
        
        // Success checkmark
        gfx->drawLine(centerX - 8, centerY - 35, centerX - 2, centerY - 28, gfx->color565(0, 255, 0));
        gfx->drawLine(centerX - 2, centerY - 28, centerX + 8, centerY - 42, gfx->color565(0, 255, 0));
        gfx->drawCircle(centerX, centerY - 35, 12, gfx->color565(0, 255, 0));
      }
      break;
      
    case 7: // Font ready
      {
        gfx->setTextColor(gfx->color565(255, 255, 255));
        gfx->setTextSize(2);
        gfx->setCursor(centerX - 30, centerY - 16);
        gfx->printf("READY");
        
        // Sample text with new font
        gfx->setTextSize(1);
        gfx->setCursor(centerX - 45, centerY + 8);
        gfx->printf("Font loaded!");
        
        // Celebration sparkles
        for (int spark = 0; spark < 8; spark++) {
          float sparkAngle = spark * PI / 4 + phaseProgress * 4;
          int sparkX = centerX + cos(sparkAngle) * (15 + phaseProgress * 10);
          int sparkY = centerY + sin(sparkAngle) * (15 + phaseProgress * 10);
          
          gfx->fillCircle(sparkX, sparkY, 1, gfx->color565(255, 255, 255));
        }
      }
      break;
  }
  
  // Retry counter
  gfx->setTextColor(gfx->color565(150, 150, 150));
  gfx->setTextSize(1);
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Attempt: %d", (int)(retryTime / 8) + 1);
  
  gfx->setTextColor(gfx->color565(255, 200, 100));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FONT-RETRY");
}

void drawForest() {
  // Animated forest scene
  gfx->fillScreen(gfx->color565(20, 40, 60)); // Night sky
  
  static float forestTime = 0;
  forestTime += 0.05;
  
  // Background mountains
  for (int mountain = 0; mountain < 3; mountain++) {
    int mountainHeight = 60 + mountain * 20;
    uint16_t mountainColor = gfx->color565(40 - mountain * 10, 60 - mountain * 15, 80 - mountain * 20);
    
    for (int x = 0; x < gfx->width(); x += 2) {
      int peakHeight = mountainHeight + sin(x * 0.02 + mountain) * 15;
      gfx->drawFastVLine(x, gfx->height() - peakHeight, peakHeight, mountainColor);
    }
  }
  
  // Forest trees
  static int treeX[15];
  static int treeHeight[15];
  static int treeType[15];
  static bool forestInit = false;
  
  if (!forestInit) {
    for (int i = 0; i < 15; i++) {
      treeX[i] = i * 22;
      treeHeight[i] = 40 + random(60);
      treeType[i] = random(3);
    }
    forestInit = true;
  }
  
  // Draw trees
  for (int i = 0; i < 15; i++) {
    int x = treeX[i];
    int height = treeHeight[i];
    int groundY = gfx->height() - 10;
    
    // Tree swaying
    float sway = sin(forestTime * 2 + i) * 3;
    int topX = x + sway;
    
    // Tree trunk
    uint16_t trunkColor = gfx->color565(101, 67, 33);
    gfx->drawLine(x, groundY, topX, groundY - height, trunkColor);
    gfx->drawLine(x + 1, groundY, topX + 1, groundY - height, trunkColor);
    
    // Tree foliage based on type
    switch (treeType[i]) {
      case 0: // Pine tree
        {
          uint16_t pineColor = gfx->color565(34, 139, 34);
          for (int level = 0; level < 4; level++) {
            int levelY = groundY - height + level * height / 4;
            int levelWidth = (4 - level) * 6;
            
            for (int branch = -levelWidth; branch <= levelWidth; branch += 2) {
              int branchY = levelY + abs(branch) / 3;
              gfx->fillCircle(topX + branch, branchY, 2, pineColor);
            }
          }
        }
        break;
        
      case 1: // Oak tree
        {
          uint16_t leafColor = gfx->color565(50, 150, 50);
          // Main canopy
          gfx->fillCircle(topX, groundY - height + 10, height / 3, leafColor);
          gfx->fillCircle(topX - 8, groundY - height + 15, height / 4, leafColor);
          gfx->fillCircle(topX + 8, groundY - height + 15, height / 4, leafColor);
          
          // Branches
          gfx->drawLine(topX, groundY - height / 2, topX - 12, groundY - height / 2 - 8, trunkColor);
          gfx->drawLine(topX, groundY - height / 2, topX + 12, groundY - height / 2 - 8, trunkColor);
        }
        break;
        
      case 2: // Willow tree
        {
          uint16_t willowColor = gfx->color565(60, 179, 113);
          // Drooping branches
          for (int droop = 0; droop < 8; droop++) {
            float droopAngle = (droop - 4) * 0.3;
            int droopLength = height / 2 + droop * 3;
            
            for (int seg = 0; seg < 10; seg++) {
              float segProgress = seg / 10.0;
              int segX = topX + sin(droopAngle) * segProgress * 15;
              int segY = groundY - height + segProgress * droopLength + sin(forestTime + droop) * 3;
              
              gfx->fillCircle(segX, segY, 1, willowColor);
            }
          }
        }
        break;
    }
  }
  
  // Forest creatures
  static float rabbitX = 50;
  static float rabbitY = gfx->height() - 15;
  static int rabbitDir = 1;
  
  // Moving rabbit
  rabbitX += rabbitDir * 0.5;
  if (rabbitX > gfx->width() - 20 || rabbitX < 20) {
    rabbitDir *= -1;
  }
  
  // Draw rabbit
  gfx->fillCircle(rabbitX, rabbitY, 3, gfx->color565(139, 121, 94)); // Body
  gfx->fillCircle(rabbitX + rabbitDir * 2, rabbitY - 2, 2, gfx->color565(139, 121, 94)); // Head
  gfx->fillCircle(rabbitX + rabbitDir * 3, rabbitY - 4, 1, gfx->color565(255, 255, 255)); // Ear
  gfx->fillCircle(rabbitX + rabbitDir * 2, rabbitY - 5, 1, gfx->color565(255, 255, 255)); // Ear
  
  // Fireflies
  for (int firefly = 0; firefly < 8; firefly++) {
    float fireflyAngle = forestTime * 3 + firefly;
    int fireflyX = 30 + firefly * 35 + sin(fireflyAngle) * 20;
    int fireflyY = 40 + cos(fireflyAngle * 0.7) * 30;
    
    float glow = sin(forestTime * 6 + firefly) * 0.5 + 0.5;
    int glowBright = glow * 255;
    
    gfx->fillCircle(fireflyX, fireflyY, 1, gfx->color565(255, 255, glowBright));
    if (glow > 0.7) {
      gfx->drawCircle(fireflyX, fireflyY, 3, gfx->color565(255, 255, 100));
    }
  }
  
  // Moon
  int moonX = gfx->width() - 40;
  int moonY = 30;
  gfx->fillCircle(moonX, moonY, 12, gfx->color565(255, 255, 200));
  gfx->drawCircle(moonX, moonY, 15, gfx->color565(200, 200, 150));
  
  // Moon craters
  gfx->fillCircle(moonX - 3, moonY - 2, 2, gfx->color565(200, 200, 150));
  gfx->fillCircle(moonX + 2, moonY + 3, 1, gfx->color565(200, 200, 150));
  
  gfx->setTextColor(gfx->color565(100, 255, 100));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FOREST");
}

void drawFps() {
  // FPS counter and performance monitor
  gfx->fillScreen(BLACK);
  
  static float fpsTime = 0;
  static int frameCount = 0;
  static float lastFpsUpdate = 0;
  static float currentFps = 60;
  static float fpsHistory[60];
  static int historyIndex = 0;
  static bool fpsInit = false;
  
  fpsTime += 0.016; // Simulate 60fps timing
  frameCount++;
  
  if (!fpsInit) {
    for (int i = 0; i < 60; i++) {
      fpsHistory[i] = 60;
    }
    fpsInit = true;
  }
  
  // Update FPS calculation
  if (fpsTime - lastFpsUpdate >= 1.0) {
    currentFps = frameCount / (fpsTime - lastFpsUpdate);
    fpsHistory[historyIndex] = currentFps;
    historyIndex = (historyIndex + 1) % 60;
    frameCount = 0;
    lastFpsUpdate = fpsTime;
  }
  
  // Simulate varying performance
  float performanceVariation = sin(fpsTime * 0.5) * 20 + sin(fpsTime * 2) * 10;
  currentFps = 60 + performanceVariation;
  currentFps = constrain(currentFps, 10, 120);
  
  // Main FPS display
  gfx->setTextColor(gfx->color565(255, 255, 255));
  gfx->setTextSize(3);
  gfx->setCursor(20, 40);
  gfx->printf("FPS");
  
  // Current FPS number
  uint16_t fpsColor;
  if (currentFps >= 50) fpsColor = gfx->color565(0, 255, 0);      // Green
  else if (currentFps >= 30) fpsColor = gfx->color565(255, 255, 0); // Yellow
  else fpsColor = gfx->color565(255, 0, 0);                        // Red
  
  gfx->setTextColor(fpsColor);
  gfx->setTextSize(4);
  gfx->setCursor(20, 80);
  gfx->printf("%d", (int)currentFps);
  
  // FPS history graph
  int graphX = 150;
  int graphY = 50;
  int graphWidth = gfx->width() - graphX - 10;
  int graphHeight = 60;
  
  // Graph background
  gfx->drawRect(graphX, graphY, graphWidth, graphHeight, gfx->color565(100, 100, 100));
  
  // Draw FPS history line
  for (int i = 0; i < graphWidth - 1; i++) {
    int histIdx1 = (historyIndex + i * 60 / graphWidth) % 60;
    int histIdx2 = (historyIndex + (i + 1) * 60 / graphWidth) % 60;
    
    int y1 = graphY + graphHeight - (fpsHistory[histIdx1] / 120.0) * graphHeight;
    int y2 = graphY + graphHeight - (fpsHistory[histIdx2] / 120.0) * graphHeight;
    
    uint16_t lineColor;
    if (fpsHistory[histIdx1] >= 50) lineColor = gfx->color565(0, 255, 0);
    else if (fpsHistory[histIdx1] >= 30) lineColor = gfx->color565(255, 255, 0);
    else lineColor = gfx->color565(255, 0, 0);
    
    gfx->drawLine(graphX + i, y1, graphX + i + 1, y2, lineColor);
  }
  
  // Graph labels
  gfx->setTextColor(gfx->color565(200, 200, 200));
  gfx->setTextSize(1);
  gfx->setCursor(graphX, graphY - 10);
  gfx->printf("120");
  gfx->setCursor(graphX, graphY + graphHeight / 2 - 4);
  gfx->printf("60");
  gfx->setCursor(graphX, graphY + graphHeight - 8);
  gfx->printf("0");
  
  // Performance metrics
  gfx->setTextColor(gfx->color565(150, 150, 255));
  gfx->setTextSize(1);
  gfx->setCursor(20, 140);
  gfx->printf("Frame Time: %.1fms", 1000.0 / currentFps);
  
  gfx->setCursor(20, 155);
  float avgFps = 0;
  for (int i = 0; i < 60; i++) avgFps += fpsHistory[i];
  avgFps /= 60;
  gfx->printf("Avg FPS: %.1f", avgFps);
  
  gfx->setCursor(20, 170);
  gfx->printf("Min FPS: %.0f", *std::min_element(fpsHistory, fpsHistory + 60));
  
  gfx->setCursor(20, 185);
  gfx->printf("Max FPS: %.0f", *std::max_element(fpsHistory, fpsHistory + 60));
  
  // Performance indicators
  for (int i = 0; i < 10; i++) {
    float intensity = (sin(fpsTime * 8 + i) + 1) / 2;
    int barHeight = intensity * 20;
    uint16_t barColor = gfx->color565(intensity * 255, (1 - intensity) * 255, 0);
    
    gfx->fillRect(20 + i * 12, 210 - barHeight, 8, barHeight, barColor);
  }
  
  gfx->setTextColor(gfx->color565(100, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FPS");
}

void drawFpsGl() {
  // OpenGL-style FPS monitor with 3D elements
  gfx->fillScreen(BLACK);
  
  static float glTime = 0;
  glTime += 0.02;
  
  // Simulated 3D performance data
  static float gpuLoad = 0;
  static float cpuLoad = 0;
  static float memoryUsage = 0;
  static float temperature = 0;
  
  // Update performance metrics
  gpuLoad = 50 + sin(glTime * 1.5) * 30 + sin(glTime * 4) * 10;
  cpuLoad = 40 + cos(glTime * 1.2) * 25 + cos(glTime * 6) * 8;
  memoryUsage = 60 + sin(glTime * 0.8) * 20;
  temperature = 65 + cos(glTime * 0.5) * 15;
  
  gpuLoad = constrain(gpuLoad, 0, 100);
  cpuLoad = constrain(cpuLoad, 0, 100);
  memoryUsage = constrain(memoryUsage, 0, 100);
  temperature = constrain(temperature, 40, 90);
  
  // 3D-style frame counter
  gfx->setTextColor(gfx->color565(0, 255, 255));
  gfx->setTextSize(2);
  gfx->setCursor(20, 20);
  gfx->printf("OpenGL FPS");
  
  // Rotating 3D cube indicator
  int cubeX = 200;
  int cubeY = 50;
  float rotation = glTime * 2;
  
  // Cube vertices (simplified 2D projection)
  for (int face = 0; face < 3; face++) {
    float faceRotation = rotation + face * PI / 3;
    int size = 15 + sin(glTime * 3 + face) * 5;
    
    // Draw cube face
    int x1 = cubeX + cos(faceRotation) * size;
    int y1 = cubeY + sin(faceRotation) * size;
    int x2 = cubeX + cos(faceRotation + PI/2) * size;
    int y2 = cubeY + sin(faceRotation + PI/2) * size;
    int x3 = cubeX + cos(faceRotation + PI) * size;
    int y3 = cubeY + sin(faceRotation + PI) * size;
    int x4 = cubeX + cos(faceRotation + 3*PI/2) * size;
    int y4 = cubeY + sin(faceRotation + 3*PI/2) * size;
    
    uint16_t faceColor = gfx->color565(100 + face * 50, 100, 255 - face * 50);
    gfx->drawLine(x1, y1, x2, y2, faceColor);
    gfx->drawLine(x2, y2, x3, y3, faceColor);
    gfx->drawLine(x3, y3, x4, y4, faceColor);
    gfx->drawLine(x4, y4, x1, y1, faceColor);
  }
  
  // GPU Load bar
  int barY = 80;
  gfx->setTextColor(gfx->color565(255, 100, 100));
  gfx->setTextSize(1);
  gfx->setCursor(20, barY);
  gfx->printf("GPU: %d%%", (int)gpuLoad);
  
  int barWidth = (gpuLoad / 100.0) * 120;
  gfx->fillRect(80, barY, barWidth, 8, gfx->color565(255, 100, 100));
  gfx->drawRect(80, barY, 120, 8, gfx->color565(150, 150, 150));
  
  // CPU Load bar
  barY += 20;
  gfx->setTextColor(gfx->color565(100, 255, 100));
  gfx->setCursor(20, barY);
  gfx->printf("CPU: %d%%", (int)cpuLoad);
  
  barWidth = (cpuLoad / 100.0) * 120;
  gfx->fillRect(80, barY, barWidth, 8, gfx->color565(100, 255, 100));
  gfx->drawRect(80, barY, 120, 8, gfx->color565(150, 150, 150));
  
  // Memory usage bar
  barY += 20;
  gfx->setTextColor(gfx->color565(100, 100, 255));
  gfx->setCursor(20, barY);
  gfx->printf("MEM: %d%%", (int)memoryUsage);
  
  barWidth = (memoryUsage / 100.0) * 120;
  gfx->fillRect(80, barY, barWidth, 8, gfx->color565(100, 100, 255));
  gfx->drawRect(80, barY, 120, 8, gfx->color565(150, 150, 150));
  
  // Temperature gauge
  barY += 20;
  gfx->setTextColor(gfx->color565(255, 255, 100));
  gfx->setCursor(20, barY);
  gfx->printf("TEMP: %d°C", (int)temperature);
  
  float tempRatio = (temperature - 40) / 50.0;
  barWidth = tempRatio * 120;
  uint16_t tempColor = gfx->color565(255 * tempRatio, 255 * (1 - tempRatio), 0);
  gfx->fillRect(80, barY, barWidth, 8, tempColor);
  gfx->drawRect(80, barY, 120, 8, gfx->color565(150, 150, 150));
  
  // Render pipeline visualization
  gfx->setTextColor(gfx->color565(200, 200, 200));
  gfx->setTextSize(1);
  gfx->setCursor(20, 170);
  gfx->printf("Render Pipeline:");
  
  // Pipeline stages
  const char* stages[] = {"VERT", "GEOM", "RAST", "FRAG", "BLEND"};
  for (int i = 0; i < 5; i++) {
    int stageX = 20 + i * 50;
    int stageY = 185;
    
    // Stage activity
    float activity = sin(glTime * 6 + i * 1.2) * 0.5 + 0.5;
    uint16_t stageColor = gfx->color565(activity * 255, activity * 100, 255);
    
    gfx->fillRect(stageX, stageY, 40, 12, stageColor);
    gfx->drawRect(stageX, stageY, 40, 12, gfx->color565(200, 200, 200));
    
    gfx->setTextColor(BLACK);
    gfx->setCursor(stageX + 5, stageY + 3);
    gfx->printf("%s", stages[i]);
    
    // Connection lines
    if (i < 4) {
      gfx->drawLine(stageX + 40, stageY + 6, stageX + 50, stageY + 6, 
                   gfx->color565(150, 150, 150));
    }
  }
  
  gfx->setTextColor(gfx->color565(100, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FPS-GL");
}

void drawFuzzyflakes() {
  // Fuzzy snowflake effect with soft edges
  gfx->fillScreen(BLACK);
  
  static float fuzzyTime = 0;
  fuzzyTime += 0.04;
  
  // Fuzzy snowflakes
  static float flakeX[40];
  static float flakeY[40];
  static float flakeVY[40];
  static float flakeSize[40];
  static float flakeFuzz[40];
  static float flakeSpin[40];
  static bool fuzzyInit = false;
  
  if (!fuzzyInit) {
    for (int i = 0; i < 40; i++) {
      flakeX[i] = random(gfx->width());
      flakeY[i] = random(gfx->height());
      flakeVY[i] = random(50) / 50.0 + 0.3;
      flakeSize[i] = random(60) / 10.0 + 2;
      flakeFuzz[i] = random(30) / 10.0 + 1;
      flakeSpin[i] = random(628) / 100.0;
    }
    fuzzyInit = true;
  }
  
  // Update and draw fuzzy flakes
  for (int i = 0; i < 40; i++) {
    flakeY[i] += flakeVY[i];
    flakeSpin[i] += 0.03;
    
    if (flakeY[i] > gfx->height() + 10) {
      flakeY[i] = -10;
      flakeX[i] = random(gfx->width());
    }
    
    int x = flakeX[i];
    int y = flakeY[i];
    float size = flakeSize[i];
    float fuzz = flakeFuzz[i];
    
    // Fuzzy glow layers
    for (int layer = 0; layer < 5; layer++) {
      float layerSize = size + layer * fuzz;
      float layerAlpha = 1.0 - layer / 5.0;
      int alpha = layerAlpha * 150;
      
      uint16_t layerColor = gfx->color565(alpha, alpha, alpha + 50);
      
      // Draw fuzzy circle
      for (int angle = 0; angle < 360; angle += 30) {
        float rad = angle * PI / 180 + flakeSpin[i];
        int fx = x + cos(rad) * layerSize;
        int fy = y + sin(rad) * layerSize;
        
        gfx->fillCircle(fx, fy, 1 + layer, layerColor);
      }
    }
    
    // Core snowflake structure
    uint16_t coreColor = gfx->color565(255, 255, 255);
    for (int arm = 0; arm < 6; arm++) {
      float armAngle = arm * PI / 3 + flakeSpin[i];
      
      // Main arm
      int armLength = size;
      int armEndX = x + cos(armAngle) * armLength;
      int armEndY = y + sin(armAngle) * armLength;
      
      // Fuzzy line drawing
      for (int seg = 0; seg <= armLength; seg++) {
        float segT = seg / (float)armLength;
        int segX = x + cos(armAngle) * seg;
        int segY = y + sin(armAngle) * seg;
        
        // Add fuzziness
        int fuzzX = segX + sin(fuzzyTime * 4 + i + arm) * fuzz;
        int fuzzY = segY + cos(fuzzyTime * 3 + i + arm) * fuzz;
        
        gfx->fillCircle(fuzzX, fuzzY, 1, coreColor);
        
        // Branch details
        if (seg > armLength / 3) {
          int branchX1 = segX + cos(armAngle + PI/4) * (armLength - seg) / 3;
          int branchY1 = segY + sin(armAngle + PI/4) * (armLength - seg) / 3;
          int branchX2 = segX + cos(armAngle - PI/4) * (armLength - seg) / 3;
          int branchY2 = segY + sin(armAngle - PI/4) * (armLength - seg) / 3;
          
          gfx->fillCircle(branchX1, branchY1, 1, coreColor);
          gfx->fillCircle(branchX2, branchY2, 1, coreColor);
        }
      }
    }
  }
  
  // Fuzzy wind effect
  for (int wind = 0; wind < gfx->width(); wind += 20) {
    float windStrength = sin(fuzzyTime * 2 + wind * 0.1) * 0.5 + 0.5;
    int windY = 50 + cos(fuzzyTime + wind * 0.05) * 30;
    
    if (windStrength > 0.3) {
      for (int particle = 0; particle < 5; particle++) {
        int px = wind + particle * 4;
        int py = windY + particle * 2;
        int alpha = windStrength * 100;
        
        gfx->fillCircle(px, py, 1, gfx->color565(alpha, alpha, alpha + 100));
      }
    }
  }
  
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("FUZZYFLAKES");
}

void drawGalaxy() {
  // Spiral galaxy visualization
  gfx->fillScreen(BLACK);
  
  static float galaxyTime = 0;
  galaxyTime += 0.02;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Galaxy spiral arms
  for (int arm = 0; arm < 4; arm++) {
    float armAngle = arm * PI / 2 + galaxyTime * 0.3;
    
    // Draw spiral arm
    for (int r = 10; r < 120; r += 2) {
      float spiralAngle = armAngle + r * 0.05;
      
      // Spiral position
      int spiralX = centerX + cos(spiralAngle) * r;
      int spiralY = centerY + sin(spiralAngle) * r * 0.7; // Elliptical
      
      if (spiralX >= 0 && spiralX < gfx->width() && spiralY >= 0 && spiralY < gfx->height()) {
        // Star density variation
        float density = sin(r * 0.1 + galaxyTime) * 0.5 + 0.5;
        
        if (random(100) < density * 30) {
          // Star color based on distance from center
          float distFromCenter = sqrt((spiralX - centerX) * (spiralX - centerX) + 
                                    (spiralY - centerY) * (spiralY - centerY));
          
          // Younger stars (blue) in outer regions, older (red) in center
          float colorRatio = distFromCenter / 120.0;
          int r_color = 100 + (1 - colorRatio) * 155;
          int g_color = 100 + colorRatio * 100;
          int b_color = 150 + colorRatio * 105;
          
          gfx->fillCircle(spiralX, spiralY, 1, gfx->color565(r_color, g_color, b_color));
          
          // Bright stars
          if (random(100) < 5) {
            gfx->fillCircle(spiralX, spiralY, 2, gfx->color565(255, 255, 255));
            // Star glow
            gfx->drawCircle(spiralX, spiralY, 3, gfx->color565(200, 200, 255));
          }
        }
      }
    }
  }
  
  // Galactic core
  for (int core = 0; core < 20; core++) {
    float coreAngle = core * PI / 10 + galaxyTime * 2;
    float coreRadius = 8 + sin(galaxyTime * 4 + core) * 4;
    
    int coreX = centerX + cos(coreAngle) * coreRadius;
    int coreY = centerY + sin(coreAngle) * coreRadius;
    
    float brightness = sin(galaxyTime * 3 + core) * 0.5 + 0.5;
    int bright = brightness * 255;
    
    gfx->fillCircle(coreX, coreY, 2, gfx->color565(255, bright, 100));
  }
  
  // Central black hole
  gfx->fillCircle(centerX, centerY, 4, BLACK);
  gfx->drawCircle(centerX, centerY, 6, gfx->color565(100, 50, 0));
  gfx->drawCircle(centerX, centerY, 8, gfx->color565(150, 100, 50));
  
  // Dust lanes
  for (int dust = 0; dust < 3; dust++) {
    float dustAngle = dust * 2 * PI / 3 + galaxyTime * 0.1;
    
    for (int d = 20; d < 80; d += 3) {
      int dustX = centerX + cos(dustAngle + d * 0.03) * d;
      int dustY = centerY + sin(dustAngle + d * 0.03) * d * 0.5;
      
      if (dustX >= 0 && dustX < gfx->width() && dustY >= 0 && dustY < gfx->height()) {
        float dustDensity = sin(d * 0.2 + galaxyTime * 2) * 0.3 + 0.3;
        int dustAlpha = dustDensity * 100;
        
        gfx->fillCircle(dustX, dustY, 1, gfx->color565(dustAlpha, dustAlpha / 2, 0));
      }
    }
  }
  
  // Background stars
  for (int bg = 0; bg < 50; bg++) {
    int bgX = (bg * 37) % gfx->width();
    int bgY = (bg * 73) % gfx->height();
    
    float twinkle = sin(galaxyTime * 5 + bg) * 0.5 + 0.5;
    int twinkleBright = twinkle * 150 + 50;
    
    gfx->fillCircle(bgX, bgY, 1, gfx->color565(twinkleBright, twinkleBright, twinkleBright));
  }
  
  // Galaxy info
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, gfx->height() - 30);
  gfx->printf("Type: Spiral");
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Arms: 4");
  
  gfx->setTextColor(gfx->color565(100, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("GALAXY");
}

void drawGallant12x22Bsd() {
  // BSD-style font rendering demonstration
  gfx->fillScreen(BLACK);
  
  static float fontTime = 0;
  fontTime += 0.05;
  
  // Font size demonstration
  const char* sampleText = "BSD FONT";
  const char* charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  
  // Title
  gfx->setTextColor(gfx->color565(255, 200, 100));
  gfx->setTextSize(2);
  gfx->setCursor(20, 20);
  gfx->printf("Gallant 12x22");
  
  // Scrolling text demo
  int scrollX = gfx->width() - (int)(fontTime * 60) % (gfx->width() + 150);
  gfx->setTextColor(gfx->color565(100, 255, 100));
  gfx->setTextSize(1);
  gfx->setCursor(scrollX, 50);
  gfx->printf("BSD Font Rendering System");
  
  // Character showcase
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(20, 80);
  gfx->printf("Character Set:");
  
  // Display charset in grid
  for (int i = 0; i < strlen(charset); i++) {
    int col = i % 13;
    int row = i / 13;
    
    int charX = 20 + col * 20;
    int charY = 100 + row * 25;
    
    // Character cycling color
    float colorPhase = fontTime * 2 + i * 0.1;
    int r = 128 + sin(colorPhase) * 127;
    int g = 128 + sin(colorPhase + PI/3) * 127;
    int b = 128 + sin(colorPhase + 2*PI/3) * 127;
    
    gfx->setTextColor(gfx->color565(r, g, b));
    gfx->setTextSize(1);
    gfx->setCursor(charX, charY);
    gfx->printf("%c", charset[i]);
    
    // Character highlight
    if (((int)(fontTime * 5)) % strlen(charset) == i) {
      gfx->drawRect(charX - 2, charY - 2, 16, 12, gfx->color565(255, 255, 255));
    }
  }
  
  // Font metrics display
  gfx->setTextColor(gfx->color565(255, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(20, 170);
  gfx->printf("Font: Fixed-width");
  gfx->setCursor(20, 185);
  gfx->printf("Size: 12x22 pixels");
  gfx->setCursor(20, 200);
  gfx->printf("License: BSD");
  
  // Animated text effects
  for (int effect = 0; effect < 8; effect++) {
    float effectPhase = fontTime * 3 + effect;
    int effectX = 200 + effect * 15;
    int effectY = 170 + sin(effectPhase) * 10;
    
    char effectChar = sampleText[effect % strlen(sampleText)];
    
    // Text shadow
    gfx->setTextColor(gfx->color565(50, 50, 50));
    gfx->setTextSize(2);
    gfx->setCursor(effectX + 2, effectY + 2);
    gfx->printf("%c", effectChar);
    
    // Main character
    float brightness = sin(effectPhase * 2) * 0.5 + 0.5;
    int bright = brightness * 255;
    gfx->setTextColor(gfx->color565(255, bright, 100));
    gfx->setTextSize(2);
    gfx->setCursor(effectX, effectY);
    gfx->printf("%c", effectChar);
  }
  
  // Font rendering stats
  gfx->setTextColor(gfx->color565(150, 150, 150));
  gfx->setTextSize(1);
  gfx->setCursor(200, 100);
  gfx->printf("Rendering Stats:");
  gfx->setCursor(200, 115);
  gfx->printf("Chars/sec: %d", (int)(fontTime * 10) % 1000 + 500);
  gfx->setCursor(200, 130);
  gfx->printf("Memory: %dKB", (int)(sin(fontTime) * 50) + 100);
  gfx->setCursor(200, 145);
  gfx->printf("Quality: High");
  
  // Copyright notice
  gfx->setTextColor(gfx->color565(100, 100, 100));
  gfx->setTextSize(1);
  gfx->setCursor(10, gfx->height() - 10);
  gfx->printf("(c) BSD Licensed Font System");
  
  gfx->setTextColor(gfx->color565(100, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("GALLANT12X22-BSD");
}

void drawGear(int x, int y, int radius, int teeth, float rotation, uint16_t color) {
  // Draw gear with specified parameters
  float toothAngle = 2 * PI / teeth;
  
  // Draw gear body
  gfx->fillCircle(x, y, radius - 5, color);
  gfx->drawCircle(x, y, radius - 5, gfx->color565(100, 100, 100));
  
  // Draw gear teeth
  for (int tooth = 0; tooth < teeth; tooth++) {
    float angle1 = tooth * toothAngle + rotation;
    float angle2 = (tooth + 0.3) * toothAngle + rotation;
    float angle3 = (tooth + 0.7) * toothAngle + rotation;
    float angle4 = (tooth + 1) * toothAngle + rotation;
    
    // Inner tooth base
    int x1 = x + cos(angle1) * (radius - 5);
    int y1 = y + sin(angle1) * (radius - 5);
    int x2 = x + cos(angle2) * (radius - 5);
    int y2 = y + sin(angle2) * (radius - 5);
    
    // Outer tooth tip
    int x3 = x + cos(angle2) * radius;
    int y3 = y + sin(angle2) * radius;
    int x4 = x + cos(angle3) * radius;
    int y4 = y + sin(angle3) * radius;
    
    // Inner tooth base (other side)
    int x5 = x + cos(angle3) * (radius - 5);
    int y5 = y + sin(angle3) * (radius - 5);
    int x6 = x + cos(angle4) * (radius - 5);
    int y6 = y + sin(angle4) * (radius - 5);
    
    // Draw tooth outline
    gfx->drawLine(x1, y1, x2, y2, gfx->color565(200, 200, 200));
    gfx->drawLine(x2, y2, x3, y3, gfx->color565(200, 200, 200));
    gfx->drawLine(x3, y3, x4, y4, gfx->color565(200, 200, 200));
    gfx->drawLine(x4, y4, x5, y5, gfx->color565(200, 200, 200));
    gfx->drawLine(x5, y5, x6, y6, gfx->color565(200, 200, 200));
  }
  
  // Central hub
  gfx->fillCircle(x, y, 6, gfx->color565(80, 80, 80));
  gfx->drawCircle(x, y, 6, gfx->color565(150, 150, 150));
  gfx->fillCircle(x, y, 3, BLACK);
}

void drawGears() {
  // Mechanical gears animation - simplified version
  gfx->fillScreen(BLACK);
  
  static float gearTime = 0;
  gearTime += 0.05;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Draw multiple rotating gear circles
  for (int gear = 0; gear < 8; gear++) {
    float angle = gear * PI / 4 + gearTime * (0.5 + gear * 0.1);
    float distance = 60 + sin(gearTime + gear) * 20;
    
    int gearX = centerX + cos(angle) * distance;
    int gearY = centerY + sin(angle) * distance;
    
    int gearRadius = 15 + gear * 2;
    float gearRotation = gearTime * (1 + gear * 0.2);
    
    // Gear color based on position
    int r = 100 + gear * 20;
    int g = 150 + sin(gearTime + gear) * 50;
    int b = 200 - gear * 15;
    uint16_t gearColor = gfx->color565(r, g, b);
    
    // Draw gear body
    gfx->fillCircle(gearX, gearY, gearRadius, gearColor);
    gfx->drawCircle(gearX, gearY, gearRadius, gfx->color565(200, 200, 200));
    
    // Draw gear teeth as small rectangles
    int numTeeth = 8 + gear;
    for (int tooth = 0; tooth < numTeeth; tooth++) {
      float toothAngle = tooth * 2 * PI / numTeeth + gearRotation;
      int toothX = gearX + cos(toothAngle) * gearRadius;
      int toothY = gearY + sin(toothAngle) * gearRadius;
      
      gfx->fillRect(toothX - 1, toothY - 1, 3, 3, gfx->color565(255, 255, 255));
    }
    
    // Center hub
    gfx->fillCircle(gearX, gearY, 5, gfx->color565(80, 80, 80));
  }
  
  // Central master gear
  gfx->fillCircle(centerX, centerY, 20, gfx->color565(255, 200, 100));
  gfx->drawCircle(centerX, centerY, 20, gfx->color565(200, 200, 200));
  
  // Master gear teeth
  for (int tooth = 0; tooth < 12; tooth++) {
    float toothAngle = tooth * PI / 6 + gearTime;
    int toothX = centerX + cos(toothAngle) * 20;
    int toothY = centerY + sin(toothAngle) * 20;
    
    gfx->fillRect(toothX - 2, toothY - 2, 4, 4, gfx->color565(255, 255, 255));
  }
  
  gfx->fillCircle(centerX, centerY, 8, gfx->color565(100, 100, 100));
  
  // Connection lines
  for (int connection = 0; connection < 8; connection++) {
    float connAngle = connection * PI / 4 + gearTime * 0.1;
    int connX = centerX + cos(connAngle) * 40;
    int connY = centerY + sin(connAngle) * 40;
    
    gfx->drawLine(centerX, centerY, connX, connY, gfx->color565(100, 100, 150));
  }
  
  // Stats
  gfx->setTextColor(gfx->color565(255, 255, 100));
  gfx->setTextSize(1);
  gfx->setCursor(10, gfx->height() - 30);
  gfx->printf("RPM: %.0f", fabs(sin(gearTime * 3)) * 100 + 50);
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Torque: %.1fNm", cos(gearTime * 2) * 25 + 30);
  
  gfx->setTextColor(gfx->color565(100, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("GEARS");
}

void drawGeodesic() {
  // Geodesic dome wireframe
  gfx->fillScreen(BLACK);
  
  static float geodesicTime = 0;
  geodesicTime += 0.03;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  int radius = 80;
  
  // Icosahedron vertices (simplified geodesic)
  float phi = (1.0 + sqrt(5.0)) / 2.0; // Golden ratio
  
  // 12 vertices of icosahedron
  float vertices[12][3] = {
    {-1,  phi,  0}, { 1,  phi,  0}, {-1, -phi,  0}, { 1, -phi,  0},
    { 0, -1,  phi}, { 0,  1,  phi}, { 0, -1, -phi}, { 0,  1, -phi},
    { phi,  0, -1}, { phi,  0,  1}, {-phi,  0, -1}, {-phi,  0,  1}
  };
  
  // Normalize vertices
  for (int i = 0; i < 12; i++) {
    float len = sqrt(vertices[i][0]*vertices[i][0] + 
                    vertices[i][1]*vertices[i][1] + 
                    vertices[i][2]*vertices[i][2]);
    vertices[i][0] /= len;
    vertices[i][1] /= len;
    vertices[i][2] /= len;
  }
  
  // Rotation matrices
  float rotY = geodesicTime * 0.5;
  float rotX = geodesicTime * 0.3;
  
  // Project and draw vertices
  int projectedX[12], projectedY[12];
  float projectedZ[12];
  
  for (int i = 0; i < 12; i++) {
    // Apply rotation
    float x = vertices[i][0];
    float y = vertices[i][1];
    float z = vertices[i][2];
    
    // Rotate around Y axis
    float newX = x * cos(rotY) + z * sin(rotY);
    float newZ = -x * sin(rotY) + z * cos(rotY);
    x = newX;
    z = newZ;
    
    // Rotate around X axis
    float newY = y * cos(rotX) - z * sin(rotX);
    newZ = y * sin(rotX) + z * cos(rotX);
    y = newY;
    z = newZ;
    
    // Project to 2D
    float perspective = 200.0 / (200.0 + z * 100);
    projectedX[i] = centerX + x * radius * perspective;
    projectedY[i] = centerY + y * radius * perspective;
    projectedZ[i] = z;
  }
  
  // Draw edges (simplified icosahedron connections)
  int edges[][2] = {
    {0,1}, {0,5}, {0,7}, {0,10}, {0,11},
    {1,5}, {1,7}, {1,8}, {1,9},
    {2,3}, {2,4}, {2,6}, {2,10}, {2,11},
    {3,4}, {3,6}, {3,8}, {3,9},
    {4,5}, {4,9}, {4,11},
    {5,9}, {5,11},
    {6,7}, {6,8}, {6,10},
    {7,8}, {7,10},
    {8,9}
  };
  
  for (int i = 0; i < 29; i++) {
    int v1 = edges[i][0];
    int v2 = edges[i][1];
    
    // Color based on depth
    float avgZ = (projectedZ[v1] + projectedZ[v2]) / 2;
    int brightness = 100 + (avgZ + 1) * 77;
    brightness = constrain(brightness, 50, 255);
    
    uint16_t edgeColor = gfx->color565(brightness, brightness/2, brightness);
    
    gfx->drawLine(projectedX[v1], projectedY[v1], 
                 projectedX[v2], projectedY[v2], edgeColor);
  }
  
  // Draw vertices
  for (int i = 0; i < 12; i++) {
    float brightness = (projectedZ[i] + 1) * 127 + 50;
    uint16_t vertexColor = gfx->color565(255, brightness, brightness);
    
    gfx->fillCircle(projectedX[i], projectedY[i], 3, vertexColor);
    gfx->drawCircle(projectedX[i], projectedY[i], 4, gfx->color565(255, 255, 255));
  }
  
  // Geodesic info
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, gfx->height() - 40);
  gfx->printf("Vertices: 12");
  gfx->setCursor(10, gfx->height() - 30);
  gfx->printf("Edges: 30");
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Faces: 20");
  
  gfx->setTextColor(gfx->color565(100, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("GEODESIC");
}

void drawGeodesicgears() {
  // Combination of geodesic structure with rotating gears
  gfx->fillScreen(BLACK);
  
  static float hybridTime = 0;
  hybridTime += 0.04;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Geodesic framework (simplified)
  float radius = 60;
  int numNodes = 8;
  
  // Draw geodesic nodes with gears
  for (int node = 0; node < numNodes; node++) {
    float angle = node * 2 * PI / numNodes + hybridTime * 0.2;
    float nodeRadius = radius + sin(hybridTime * 2 + node) * 15;
    
    int nodeX = centerX + cos(angle) * nodeRadius;
    int nodeY = centerY + sin(angle) * nodeRadius * 0.7;
    
    // Gear at each node
    float gearRotation = hybridTime * (1 + node * 0.1);
    int gearSize = 12 + sin(hybridTime + node) * 4;
    uint16_t gearColor = gfx->color565(100 + node * 20, 150, 200 - node * 15);
    
    // Simple node gear
    gfx->fillCircle(nodeX, nodeY, gearSize, gearColor);
    gfx->drawCircle(nodeX, nodeY, gearSize, gfx->color565(200, 200, 200));
    
    // Node gear teeth
    int nodeTeeth = 6 + node % 4;
    for (int tooth = 0; tooth < nodeTeeth; tooth++) {
      float toothAngle = tooth * 2 * PI / nodeTeeth + gearRotation;
      int toothX = nodeX + cos(toothAngle) * gearSize;
      int toothY = nodeY + sin(toothAngle) * gearSize;
      gfx->fillCircle(toothX, toothY, 1, gfx->color565(255, 255, 255));
    }
    gfx->fillCircle(nodeX, nodeY, 4, gfx->color565(80, 80, 80));
    
    // Connect to center
    uint16_t connectionColor = gfx->color565(80, 80, 120);
    gfx->drawLine(centerX, centerY, nodeX, nodeY, connectionColor);
    
    // Connect to adjacent nodes
    if (node < numNodes - 1) {
      float nextAngle = (node + 1) * 2 * PI / numNodes + hybridTime * 0.2;
      float nextNodeRadius = radius + sin(hybridTime * 2 + node + 1) * 15;
      int nextX = centerX + cos(nextAngle) * nextNodeRadius;
      int nextY = centerY + sin(nextAngle) * nextNodeRadius * 0.7;
      
      gfx->drawLine(nodeX, nodeY, nextX, nextY, connectionColor);
    } else {
      // Connect last to first
      float firstAngle = hybridTime * 0.2;
      float firstNodeRadius = radius + sin(hybridTime * 2) * 15;
      int firstX = centerX + cos(firstAngle) * firstNodeRadius;
      int firstY = centerY + sin(firstAngle) * firstNodeRadius * 0.7;
      
      gfx->drawLine(nodeX, nodeY, firstX, firstY, connectionColor);
    }
  }
  
  // Central master gear
  float centralRotation = -hybridTime * 0.8;
  gfx->fillCircle(centerX, centerY, 25, gfx->color565(255, 200, 100));
  gfx->drawCircle(centerX, centerY, 25, gfx->color565(200, 200, 200));
  
  // Central gear teeth
  for (int tooth = 0; tooth < 12; tooth++) {
    float toothAngle = tooth * PI / 6 + centralRotation;
    int toothX = centerX + cos(toothAngle) * 25;
    int toothY = centerY + sin(toothAngle) * 25;
    gfx->fillRect(toothX - 1, toothY - 1, 3, 3, gfx->color565(255, 255, 255));
  }
  gfx->fillCircle(centerX, centerY, 8, gfx->color565(100, 100, 100));
  
  // Orbital gears
  for (int orbit = 0; orbit < 3; orbit++) {
    float orbitAngle = hybridTime * (0.5 + orbit * 0.3) + orbit * 2;
    float orbitRadius = 90 + orbit * 20;
    
    int orbitX = centerX + cos(orbitAngle) * orbitRadius;
    int orbitY = centerY + sin(orbitAngle) * orbitRadius;
    
    float orbitGearRotation = hybridTime * (2 - orbit * 0.4);
    uint16_t orbitColor = gfx->color565(200 - orbit * 50, 100 + orbit * 50, 150);
    
    // Simple orbital gear (circle with teeth marks)
    int gearRadius = 8 + orbit * 2;
    gfx->fillCircle(orbitX, orbitY, gearRadius, orbitColor);
    gfx->drawCircle(orbitX, orbitY, gearRadius, gfx->color565(200, 200, 200));
    
    // Simple teeth marks
    for (int tooth = 0; tooth < 5; tooth++) {
      float toothAngle = tooth * 2 * PI / 5 + orbitGearRotation;
      int toothX = orbitX + cos(toothAngle) * gearRadius;
      int toothY = orbitY + sin(toothAngle) * gearRadius;
      gfx->fillCircle(toothX, toothY, 1, gfx->color565(255, 255, 255));
    }
  }
  
  // Energy flow visualization
  for (int flow = 0; flow < 12; flow++) {
    float flowAngle = flow * PI / 6 + hybridTime * 3;
    float flowDistance = 40 + sin(hybridTime * 4 + flow) * 20;
    
    int flowX = centerX + cos(flowAngle) * flowDistance;
    int flowY = centerY + sin(flowAngle) * flowDistance;
    
    float pulseIntensity = sin(hybridTime * 6 + flow) * 0.5 + 0.5;
    int pulseAlpha = pulseIntensity * 200 + 55;
    
    gfx->fillCircle(flowX, flowY, 2, gfx->color565(pulseAlpha, pulseAlpha/2, 255));
  }
  
  // System stats
  gfx->setTextColor(gfx->color565(255, 255, 100));
  gfx->setTextSize(1);
  gfx->setCursor(10, gfx->height() - 40);
  gfx->printf("Nodes: %d", numNodes);
  gfx->setCursor(10, gfx->height() - 30);
  gfx->printf("Gears: %d", numNodes + 4);
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Efficiency: %.0f%%", 85 + sin(hybridTime * 2) * 10);
  
  gfx->setTextColor(gfx->color565(100, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("GEODESICGEARS");
}

void drawGllist() {
  gfx->fillScreen(gfx->color565(10, 10, 20));
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // GL list visualization with 3D-like depth
  float rotationSpeed = animationTime * 0.5;
  
  // Create depth layers
  for (int depth = 0; depth < 8; depth++) {
    float depthFactor = 1.0 - (depth * 0.15);
    float depthOffset = depth * 5;
    
    // Layer color based on depth
    int intensity = 255 - depth * 25;
    uint16_t layerColor = gfx->color565(intensity/2, intensity, intensity/3);
    
    // Draw geometric primitives in this layer
    for (int prim = 0; prim < 6; prim++) {
      float angle = prim * PI / 3 + rotationSpeed + depth * 0.2;
      float radius = (60 + depth * 8) * depthFactor;
      
      int x = centerX + cos(angle) * radius;
      int y = centerY + sin(angle) * radius * 0.8;
      
      // Different primitive shapes
      switch (prim % 4) {
        case 0: // Triangle
          for (int i = 0; i < 3; i++) {
            float triAngle = angle + i * 2 * PI / 3;
            int tx = x + cos(triAngle) * (8 * depthFactor);
            int ty = y + sin(triAngle) * (8 * depthFactor);
            gfx->drawLine(x, y, tx, ty, layerColor);
          }
          break;
        case 1: // Square
          {
            int size = 6 * depthFactor;
            gfx->drawRect(x - size/2, y - size/2, size, size, layerColor);
          }
          break;
        case 2: // Circle
          gfx->drawCircle(x, y, 4 * depthFactor, layerColor);
          break;
        case 3: // Line
          {
            int lineLen = 12 * depthFactor;
            gfx->drawLine(x - lineLen/2, y, x + lineLen/2, y, layerColor);
          }
          break;
      }
    }
  }
  
  // Dynamic wire-frame overlay
  for (int wire = 0; wire < 20; wire++) {
    float wireAngle = wire * PI / 10 + animationTime * 0.3;
    float wireRadius = 80 + sin(animationTime * 2 + wire) * 20;
    
    int x1 = centerX + cos(wireAngle) * wireRadius;
    int y1 = centerY + sin(wireAngle) * wireRadius * 0.7;
    int x2 = centerX + cos(wireAngle + 0.2) * (wireRadius + 15);
    int y2 = centerY + sin(wireAngle + 0.2) * (wireRadius + 15) * 0.7;
    
    uint16_t wireColor = gfx->color565(100, 150, 255);
    gfx->drawLine(x1, y1, x2, y2, wireColor);
  }
  
  gfx->setTextColor(gfx->color565(150, 255, 150));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("GLLIST");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Primitives: %d", 48);
}

void drawGlmatrix() {
  static char matrix[40][20];
  static bool initialized = false;
  
  if (!initialized) {
    for (int y = 0; y < 20; y++) {
      for (int x = 0; x < 40; x++) {
        matrix[x][y] = random(33, 127); // ASCII printable
      }
    }
    initialized = true;
  }
  
  gfx->fillScreen(gfx->color565(0, 0, 0));
  
  // Matrix rain effect
  for (int col = 0; col < 40; col++) {
    // Column parameters
    float dropSpeed = 0.5 + (col % 3) * 0.3;
    int dropPos = (int)(animationTime * dropSpeed * 10) % 25;
    
    for (int row = 0; row < 20; row++) {
      // Calculate intensity based on distance from drop
      int distance = abs(row - dropPos);
      float intensity = 0;
      
      if (distance < 8) {
        intensity = 1.0 - (distance / 8.0);
        if (distance == 0) intensity = 1.0; // Bright head
      }
      
      if (intensity > 0) {
        // Occasionally change character
        if (random(100) < 5) {
          matrix[col][row] = random(33, 127);
        }
        
        // Color based on intensity
        int green = intensity * 255;
        int blue = intensity * 100;
        uint16_t charColor = gfx->color565(0, green, blue);
        
        // Draw character
        gfx->setTextColor(charColor);
        gfx->setTextSize(1);
        gfx->setCursor(col * 8, row * 12);
        gfx->write(matrix[col][row]);
      }
    }
  }
  
  // Digital rain streaks
  for (int streak = 0; streak < 15; streak++) {
    int streakXInt = (int)(streak * 25 + animationTime * 30) % gfx->width();
    float streakX = streakXInt;
    float streakSpeed = 2 + streak % 4;
    
    for (int particle = 0; particle < 8; particle++) {
      int particleYInt = (int)(animationTime * streakSpeed * 50 + particle * 20) % (gfx->height() + 100);
      float particleY = particleYInt - 50;
      
      if (particleY >= 0 && particleY < gfx->height()) {
        float alpha = (8 - particle) / 8.0;
        int intensity = alpha * 150;
        uint16_t streakColor = gfx->color565(0, intensity, intensity/2);
        
        gfx->fillCircle(streakX, particleY, 1, streakColor);
      }
    }
  }
  
  gfx->setTextColor(gfx->color565(0, 255, 100));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("GLMATRIX");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Streams: %d", 40);
}

void drawGlplanet() {
  gfx->fillScreen(gfx->color565(5, 5, 15));
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Planet rotation
  float planetRotation = animationTime * 0.3;
  
  // Draw planet surface with texture
  int planetRadius = 80;
  for (int angle = 0; angle < 360; angle += 3) {
    float rad = angle * PI / 180;
    float textureAngle = rad + planetRotation;
    
    // Surface height variation
    float height = sin(textureAngle * 4) * sin(textureAngle * 7) * 8 + 
                   sin(textureAngle * 12) * 3;
    float surfaceRadius = planetRadius + height;
    
    int x = centerX + cos(rad) * surfaceRadius;
    int y = centerY + sin(rad) * surfaceRadius * 0.7; // Flatten for 3D effect
    
    // Color based on height and position
    float lightAngle = rad - planetRotation;
    float lighting = (cos(lightAngle) + 1) / 2; // 0 to 1
    
    int baseRed = 150 + height * 3;
    int baseGreen = 100 + height * 2;
    int baseBlue = 50 + height;
    
    int red = baseRed * lighting;
    int green = baseGreen * lighting;
    int blue = baseBlue * lighting;
    
    red = constrain(red, 0, 255);
    green = constrain(green, 0, 255);
    blue = constrain(blue, 0, 255);
    
    uint16_t surfaceColor = gfx->color565(red, green, blue);
    gfx->fillCircle(x, y, 2, surfaceColor);
  }
  
  // Atmosphere glow
  for (int glow = 0; glow < 15; glow++) {
    int glowRadius = planetRadius + glow * 3;
    float glowAlpha = (15 - glow) / 15.0 * 0.3;
    int glowIntensity = glowAlpha * 100;
    
    uint16_t glowColor = gfx->color565(glowIntensity/2, glowIntensity/3, glowIntensity);
    
    // Draw partial glow circles
    for (int angle = 0; angle < 360; angle += 10) {
      float rad = angle * PI / 180;
      int x = centerX + cos(rad) * glowRadius;
      int y = centerY + sin(rad) * glowRadius * 0.7;
      
      if (random(100) < 30) { // Sparse glow effect
        gfx->fillCircle(x, y, 1, glowColor);
      }
    }
  }
  
  // Orbital objects (moons/debris)
  for (int orbit = 0; orbit < 3; orbit++) {
    float orbitAngle = animationTime * (0.5 + orbit * 0.3) + orbit * 2;
    float orbitRadius = 120 + orbit * 25;
    
    int orbitX = centerX + cos(orbitAngle) * orbitRadius;
    int orbitY = centerY + sin(orbitAngle) * orbitRadius * 0.7;
    
    // Moon color and size
    int moonSize = 3 + orbit;
    uint16_t moonColor = gfx->color565(200 - orbit * 30, 200 - orbit * 20, 180 + orbit * 20);
    
    gfx->fillCircle(orbitX, orbitY, moonSize, moonColor);
    gfx->drawCircle(orbitX, orbitY, moonSize, gfx->color565(255, 255, 255));
    
    // Orbital trail
    for (int trail = 1; trail <= 5; trail++) {
      float trailAngle = orbitAngle - trail * 0.1;
      int trailX = centerX + cos(trailAngle) * orbitRadius;
      int trailY = centerY + sin(trailAngle) * orbitRadius * 0.7;
      
      float trailAlpha = (6 - trail) / 5.0 * 0.5;
      int trailIntensity = trailAlpha * 100;
      uint16_t trailColor = gfx->color565(trailIntensity, trailIntensity, trailIntensity);
      
      gfx->fillCircle(trailX, trailY, 1, trailColor);
    }
  }
  
  // Space particles
  for (int star = 0; star < 30; star++) {
    float starAngle = star * 0.8 + animationTime * 0.1;
    float starDist = 150 + (star % 5) * 20;
    
    int starX = centerX + cos(starAngle) * starDist;
    int starY = centerY + sin(starAngle) * starDist;
    
    if (starX >= 0 && starX < gfx->width() && starY >= 0 && starY < gfx->height()) {
      float twinkle = sin(animationTime * 4 + star) * 0.5 + 0.5;
      int brightness = 100 + twinkle * 155;
      uint16_t starColor = gfx->color565(brightness, brightness, brightness);
      gfx->fillCircle(starX, starY, 1, starColor);
    }
  }
  
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("GLPLANET");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Orbits: %d", 3);
}

void drawGlpuzzle() {
  gfx->fillScreen(gfx->color565(20, 20, 30));
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Puzzle pieces - sliding puzzle simulation
  int pieceSize = 40;
  int gridSize = 4;
  int totalPieces = gridSize * gridSize - 1; // 15 pieces + 1 empty
  
  for (int row = 0; row < gridSize; row++) {
    for (int col = 0; col < gridSize; col++) {
      int pieceNum = row * gridSize + col;
      
      // Skip the empty space (moving around)
      float emptyX = sin(animationTime * 0.8) * 1.5;
      float emptyY = cos(animationTime * 0.6) * 1.5;
      int emptyRow = constrain((gridSize-1) + emptyY, 0, gridSize-1);
      int emptyCol = constrain((gridSize-1) + emptyX, 0, gridSize-1);
      
      if (row == emptyRow && col == emptyCol) continue;
      
      int x = centerX - (gridSize * pieceSize) / 2 + col * pieceSize;
      int y = centerY - (gridSize * pieceSize) / 2 + row * pieceSize;
      
      // Piece movement animation
      float moveOffset = sin(animationTime * 2 + pieceNum * 0.5) * 3;
      x += moveOffset;
      y += moveOffset * 0.5;
      
      // Color based on correct position vs current position
      int correctRow = pieceNum / gridSize;
      int correctCol = pieceNum % gridSize;
      bool isCorrect = (row == correctRow && col == correctCol);
      
      uint16_t pieceColor;
      if (isCorrect) {
        pieceColor = gfx->color565(0, 150, 0); // Green for correct
      } else {
        int distance = abs(row - correctRow) + abs(col - correctCol);
        int red = 150 + distance * 20;
        int green = 100 - distance * 15;
        pieceColor = gfx->color565(red, green, 50);
      }
      
      // Draw piece
      gfx->fillRect(x, y, pieceSize - 2, pieceSize - 2, pieceColor);
      gfx->drawRect(x, y, pieceSize - 2, pieceSize - 2, gfx->color565(255, 255, 255));
      
      // Number on piece
      gfx->setTextColor(gfx->color565(255, 255, 255));
      gfx->setTextSize(1);
      gfx->setCursor(x + 15, y + 15);
      gfx->printf("%d", pieceNum + 1);
      
      // Sliding trail effect
      for (int trail = 1; trail <= 3; trail++) {
        int trailX = x - trail * moveOffset * 0.3;
        int trailY = y - trail * moveOffset * 0.15;
        float trailAlpha = (4 - trail) / 4.0 * 0.3;
        
        int trailBrightness = trailAlpha * 100;
        uint16_t trailColor = gfx->color565(trailBrightness, trailBrightness/2, trailBrightness/4);
        gfx->drawRect(trailX, trailY, pieceSize - 2, pieceSize - 2, trailColor);
      }
    }
  }
  
  // Grid lines
  for (int i = 0; i <= gridSize; i++) {
    int lineX = centerX - (gridSize * pieceSize) / 2 + i * pieceSize;
    int lineY = centerY - (gridSize * pieceSize) / 2 + i * pieceSize;
    
    gfx->drawLine(lineX, centerY - (gridSize * pieceSize) / 2, 
                  lineX, centerY + (gridSize * pieceSize) / 2, 
                  gfx->color565(100, 100, 150));
    gfx->drawLine(centerX - (gridSize * pieceSize) / 2, lineY,
                  centerX + (gridSize * pieceSize) / 2, lineY, 
                  gfx->color565(100, 100, 150));
  }
  
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("GLPUZZLE");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Pieces: %d/15", totalPieces);
}

void drawGlschool() {
  gfx->fillScreen(gfx->color565(5, 20, 40));
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Swimming fish school simulation
  int numFish = 25;
  
  for (int fish = 0; fish < numFish; fish++) {
    // Each fish follows flocking behavior
    float baseAngle = animationTime * 0.8 + fish * 0.3;
    float schoolRadius = 80 + sin(animationTime * 0.4 + fish) * 30;
    
    // Flocking forces
    float separationForce = sin(fish * 2.1 + animationTime * 3) * 0.2;
    float alignmentForce = cos(fish * 1.7 + animationTime * 2) * 0.3;
    float cohesionForce = sin(fish * 0.9 + animationTime * 1.5) * 0.4;
    
    float fishAngle = baseAngle + separationForce + alignmentForce + cohesionForce;
    
    int fishX = centerX + cos(fishAngle) * schoolRadius;
    int fishY = centerY + sin(fishAngle) * schoolRadius * 0.7;
    
    // Fish direction
    float swimDirection = fishAngle + PI/2;
    
    // Fish body (ellipse)
    int bodyLength = 8 + (fish % 3);
    int bodyHeight = 4 + (fish % 2);
    
    // Fish color varies by position in school
    int depth = (fish % 5) * 30;
    int red = 100 + depth + sin(animationTime + fish) * 20;
    int green = 80 + depth/2;
    int blue = 200 - depth;
    
    uint16_t fishColor = gfx->color565(red, green, blue);
    
    // Draw fish body
    for (int segment = 0; segment < bodyLength; segment++) {
      float segmentX = fishX + cos(swimDirection) * (segment - bodyLength/2);
      float segmentY = fishY + sin(swimDirection) * (segment - bodyLength/2);
      
      int segmentWidth = bodyHeight * sin(PI * segment / bodyLength);
      gfx->fillCircle(segmentX, segmentY, segmentWidth/2, fishColor);
    }
    
    // Fish tail
    float tailX = fishX - cos(swimDirection) * bodyLength * 0.8;
    float tailY = fishY - sin(swimDirection) * bodyLength * 0.8;
    float tailWave = sin(animationTime * 6 + fish) * 0.3;
    
    float tail1X = tailX + cos(swimDirection + PI/4 + tailWave) * 6;
    float tail1Y = tailY + sin(swimDirection + PI/4 + tailWave) * 6;
    float tail2X = tailX + cos(swimDirection - PI/4 + tailWave) * 6;
    float tail2Y = tailY + sin(swimDirection - PI/4 + tailWave) * 6;
    
    gfx->drawLine(tailX, tailY, tail1X, tail1Y, fishColor);
    gfx->drawLine(tailX, tailY, tail2X, tail2Y, fishColor);
    
    // Fish eye
    float eyeX = fishX + cos(swimDirection) * bodyLength * 0.3;
    float eyeY = fishY + sin(swimDirection) * bodyLength * 0.3;
    gfx->fillCircle(eyeX, eyeY, 2, gfx->color565(255, 255, 255));
    gfx->fillCircle(eyeX + cos(swimDirection), eyeY + sin(swimDirection), 1, gfx->color565(0, 0, 0));
    
    // Swimming trail
    for (int trail = 1; trail <= 4; trail++) {
      float trailAngle = fishAngle - trail * 0.1;
      float trailRadius = schoolRadius - trail * 5;
      
      int trailX = centerX + cos(trailAngle) * trailRadius;
      int trailY = centerY + sin(trailAngle) * trailRadius * 0.7;
      
      float trailAlpha = (5 - trail) / 5.0 * 0.4;
      int trailBrightness = trailAlpha * (red + green + blue) / 3;
      uint16_t trailColor = gfx->color565(trailBrightness/2, trailBrightness/3, trailBrightness);
      
      gfx->fillCircle(trailX, trailY, 1, trailColor);
    }
  }
  
  // Water bubbles
  for (int bubble = 0; bubble < 15; bubble++) {
    float bubbleY = fmod(bubble * 37 + animationTime * 30, gfx->height());
    int bubbleX = (bubble * 29) % gfx->width();
    
    float bubbleSize = 1 + sin(animationTime * 2 + bubble) * 2;
    float bubbleAlpha = sin(bubbleY / gfx->height() * PI) * 0.6;
    
    int bubbleBrightness = bubbleAlpha * 150;
    uint16_t bubbleColor = gfx->color565(bubbleBrightness, bubbleBrightness, 200 + bubbleBrightness/2);
    
    gfx->drawCircle(bubbleX, bubbleY, bubbleSize, bubbleColor);
  }
  
  gfx->setTextColor(gfx->color565(200, 255, 255));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("GLSCHOOL");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Fish: %d", numFish);
}

void drawGlslideshow() {
  gfx->fillScreen(gfx->color565(15, 15, 20));
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Slideshow with multiple transition effects
  float slideTime = animationTime * 0.3;
  int currentSlide = ((int)slideTime) % 6;
  float slideProgress = slideTime - floor(slideTime);
  
  // Slide transition effects
  float transitionStart = 0.8;
  bool inTransition = slideProgress > transitionStart;
  float transitionProgress = inTransition ? (slideProgress - transitionStart) / (1 - transitionStart) : 0;
  
  // Draw current slide content
  uint16_t slideColors[6][3] = {
    {200, 100, 150}, // Slide 0: Pink
    {100, 200, 150}, // Slide 1: Green  
    {150, 150, 200}, // Slide 2: Blue
    {200, 200, 100}, // Slide 3: Yellow
    {200, 150, 100}, // Slide 4: Orange
    {150, 200, 200}  // Slide 5: Cyan
  };
  
  int currentR = slideColors[currentSlide][0];
  int currentG = slideColors[currentSlide][1]; 
  int currentB = slideColors[currentSlide][2];
  
  if (!inTransition) {
    // Normal slide display
    uint16_t slideColor = gfx->color565(currentR, currentG, currentB);
    
    // Slide background
    gfx->fillRect(30, 30, gfx->width() - 60, gfx->height() - 60, slideColor);
    gfx->drawRect(28, 28, gfx->width() - 56, gfx->height() - 56, gfx->color565(255, 255, 255));
    
    // Slide content based on slide number
    gfx->setTextColor(gfx->color565(255, 255, 255));
    gfx->setTextSize(2);
    gfx->setCursor(centerX - 30, centerY - 20);
    gfx->printf("Slide %d", currentSlide + 1);
    
    // Slide-specific graphics
    switch (currentSlide) {
      case 0: // Circles
        for (int i = 0; i < 8; i++) {
          int x = 50 + (i % 4) * 50;
          int y = 60 + (i / 4) * 40;
          gfx->drawCircle(x, y, 15, gfx->color565(255, 255, 255));
        }
        break;
      case 1: // Lines
        for (int i = 0; i < 10; i++) {
          int y = 50 + i * 15;
          gfx->drawLine(40, y, gfx->width() - 40, y, gfx->color565(255, 255, 255));
        }
        break;
      case 2: // Rectangles  
        for (int i = 0; i < 6; i++) {
          int x = 50 + (i % 3) * 60;
          int y = 60 + (i / 3) * 50;
          gfx->drawRect(x, y, 40, 30, gfx->color565(255, 255, 255));
        }
        break;
      case 3: // Triangles (diamonds)
        for (int i = 0; i < 9; i++) {
          int x = 60 + (i % 3) * 50;
          int y = 60 + (i / 3) * 40;
          gfx->drawLine(x, y-10, x-10, y+10, gfx->color565(255, 255, 255));
          gfx->drawLine(x, y-10, x+10, y+10, gfx->color565(255, 255, 255));
          gfx->drawLine(x-10, y+10, x+10, y+10, gfx->color565(255, 255, 255));
        }
        break;
      case 4: // Cross pattern
        gfx->drawLine(centerX, 40, centerX, gfx->height() - 40, gfx->color565(255, 255, 255));
        gfx->drawLine(40, centerY, gfx->width() - 40, centerY, gfx->color565(255, 255, 255));
        for (int i = 0; i < 4; i++) {
          int offset = 30 + i * 15;
          gfx->drawCircle(centerX, centerY, offset, gfx->color565(255, 255, 255));
        }
        break;
      case 5: // Grid pattern
        for (int x = 50; x < gfx->width() - 30; x += 20) {
          gfx->drawLine(x, 40, x, gfx->height() - 40, gfx->color565(255, 255, 255));
        }
        for (int y = 50; y < gfx->height() - 30; y += 20) {
          gfx->drawLine(40, y, gfx->width() - 40, y, gfx->color565(255, 255, 255));
        }
        break;
    }
  } else {
    // Transition effects
    int nextSlide = (currentSlide + 1) % 6;
    int nextR = slideColors[nextSlide][0];
    int nextG = slideColors[nextSlide][1];
    int nextB = slideColors[nextSlide][2];
    
    // Different transition types
    int transitionType = currentSlide % 4;
    
    switch (transitionType) {
      case 0: // Fade
        {
          int blendR = currentR + (nextR - currentR) * transitionProgress;
          int blendG = currentG + (nextG - currentG) * transitionProgress;
          int blendB = currentB + (nextB - currentB) * transitionProgress;
          
          uint16_t blendColor = gfx->color565(blendR, blendG, blendB);
          gfx->fillRect(30, 30, gfx->width() - 60, gfx->height() - 60, blendColor);
        }
        break;
      case 1: // Slide left
        {
          int slideOffset = transitionProgress * (gfx->width() - 60);
          
          uint16_t currentColor = gfx->color565(currentR, currentG, currentB);
          uint16_t nextColor = gfx->color565(nextR, nextG, nextB);
          
          gfx->fillRect(30 - slideOffset, 30, gfx->width() - 60, gfx->height() - 60, currentColor);
          gfx->fillRect(30 + (gfx->width() - 60) - slideOffset, 30, gfx->width() - 60, gfx->height() - 60, nextColor);
        }
        break;
      case 2: // Wipe down
        {
          int wipeHeight = transitionProgress * (gfx->height() - 60);
          
          uint16_t currentColor = gfx->color565(currentR, currentG, currentB);
          uint16_t nextColor = gfx->color565(nextR, nextG, nextB);
          
          gfx->fillRect(30, 30, gfx->width() - 60, gfx->height() - 60 - wipeHeight, currentColor);
          gfx->fillRect(30, 30 + (gfx->height() - 60 - wipeHeight), gfx->width() - 60, wipeHeight, nextColor);
        }
        break;
      case 3: // Zoom
        {
          int zoomSize = transitionProgress * min(gfx->width(), gfx->height());
          
          uint16_t currentColor = gfx->color565(currentR, currentG, currentB);
          uint16_t nextColor = gfx->color565(nextR, nextG, nextB);
          
          gfx->fillRect(30, 30, gfx->width() - 60, gfx->height() - 60, currentColor);
          gfx->fillRect(centerX - zoomSize/2, centerY - zoomSize/2, zoomSize, zoomSize, nextColor);
        }
        break;
    }
    
    gfx->drawRect(28, 28, gfx->width() - 56, gfx->height() - 56, gfx->color565(255, 255, 255));
  }
  
  // Progress indicator
  int progressBarY = gfx->height() - 15;
  int progressWidth = (gfx->width() - 40) * slideProgress;
  gfx->drawRect(20, progressBarY, gfx->width() - 40, 8, gfx->color565(255, 255, 255));
  gfx->fillRect(20, progressBarY, progressWidth, 8, gfx->color565(100, 200, 100));
  
  gfx->setTextColor(gfx->color565(200, 255, 200));
  gfx->setTextSize(1);
  gfx->setCursor(10, 10);
  gfx->printf("GLSLIDESHOW");
  
  gfx->setCursor(10, 25);
  gfx->printf("Slide %d/%d", currentSlide + 1, 6);
}

void drawGlslUtils() {
  gfx->fillScreen(BLACK);
  
  // GLSL utility shader effects - vertex and fragment processing
  int numParticles = 20;
  float shaderTime = animationTime * 2.0;
  
  for (int i = 0; i < numParticles; i++) {
    float x = sin(shaderTime + i * 0.3) * 80 + gfx->width() / 2;
    float y = cos(shaderTime * 0.7 + i * 0.5) * 60 + gfx->height() / 2;
    
    // Vertex shader simulation
    float vertexIntensity = (sin(shaderTime + i) + 1.0) / 2.0;
    
    // Fragment shader color processing
    int r = (int)(255 * vertexIntensity);
    int g = (int)(128 + 127 * sin(shaderTime * 2 + i));
    int b = (int)(64 + 191 * cos(shaderTime * 1.5 + i));
    
    gfx->fillCircle(x, y, 3 + vertexIntensity * 2, gfx->color565(r, g, b));
  }
  
  gfx->setTextColor(WHITE);
  gfx->setCursor(10, 10);
  gfx->printf("GLSLutils");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Shader: %.1f", shaderTime);
}

void drawGlsnake() {
  gfx->fillScreen(BLACK);
  
  // 3D snake game simulation
  static int snakeX[50], snakeY[50];
  static int snakeLength = 10;
  static float snakeAngle = 0;
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < snakeLength; i++) {
      snakeX[i] = gfx->width() / 2 - i * 8;
      snakeY[i] = gfx->height() / 2;
    }
    initialized = true;
  }
  
  // Update snake movement
  snakeAngle += 0.1;
  
  // Move snake head
  for (int i = snakeLength - 1; i > 0; i--) {
    snakeX[i] = snakeX[i-1];
    snakeY[i] = snakeY[i-1];
  }
  
  snakeX[0] += cos(snakeAngle) * 2;
  snakeY[0] += sin(snakeAngle * 0.7) * 2;
  
  // Wrap around screen
  if (snakeX[0] < 0) snakeX[0] = gfx->width();
  if (snakeX[0] >= gfx->width()) snakeX[0] = 0;
  if (snakeY[0] < 0) snakeY[0] = gfx->height();
  if (snakeY[0] >= gfx->height()) snakeY[0] = 0;
  
  // Draw snake segments with 3D effect
  for (int i = 0; i < snakeLength; i++) {
    float segmentDepth = 1.0 - (float)i / snakeLength;
    int brightness = (int)(255 * segmentDepth);
    int segmentSize = 3 + segmentDepth * 2;
    
    gfx->fillCircle(snakeX[i], snakeY[i], segmentSize, 
                   gfx->color565(brightness, brightness/2, 0));
  }
  
  // Draw food items
  for (int i = 0; i < 3; i++) {
    int foodX = 40 + i * 80;
    int foodY = 80 + sin(animationTime + i) * 20;
    gfx->fillRect(foodX, foodY, 6, 6, GREEN);
  }
  
  gfx->setTextColor(WHITE);
  gfx->setCursor(10, 10);
  gfx->printf("GLsnake");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Length: %d", snakeLength);
}

void drawGltext() {
  gfx->fillScreen(BLACK);
  
  // 3D text rendering with multiple effects
  float textTime = animationTime * 0.5;
  
  // Rotating text effect
  const char* message = "OpenGL TEXT";
  int msgLen = strlen(message);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int i = 0; i < msgLen; i++) {
    float charAngle = textTime + i * 0.3;
    float radius = 60 + sin(textTime * 2 + i) * 20;
    
    int x = centerX + cos(charAngle) * radius;
    int y = centerY + sin(charAngle) * radius * 0.6;
    
    // 3D depth effect
    float depth = (sin(charAngle) + 1.0) / 2.0;
    int charSize = 1 + depth * 2;
    
    // Color based on position and time
    int r = (int)(128 + 127 * sin(textTime + i * 0.2));
    int g = (int)(128 + 127 * cos(textTime + i * 0.3));
    int b = (int)(128 + 127 * sin(textTime * 1.5 + i * 0.1));
    
    gfx->setTextColor(gfx->color565(r, g, b));
    gfx->setCursor(x - 3, y - 4);
    gfx->setTextSize(charSize);
    gfx->printf("%c", message[i]);
  }
  
  // Text effects demonstration
  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  
  // Scrolling text
  int scrollX = (int)(sin(textTime * 3) * 40);
  gfx->setCursor(10 + scrollX, 30);
  gfx->printf("Scrolling");
  
  // Blinking text
  if ((int)(textTime * 4) % 2 == 0) {
    gfx->setCursor(10, 50);
    gfx->printf("Blinking");
  }
  
  gfx->setCursor(10, 10);
  gfx->printf("GLtext");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Effects: 3D+Color");
}

void loop() {
  // Check boot button for mode switch and auto-scroll toggle
  if (digitalRead(BOOT_PIN) == LOW) {
    if (!bootPressed) {
      bootHoldStart = millis();
    }
    
    // Check for hold (toggle auto-scroll)
    if (millis() - bootHoldStart >= HOLD_TIME && bootPressed) {
      autoScroll = !autoScroll;
      autoScrollTimer = millis();
      Serial.printf("Auto-scroll %s\n", autoScroll ? "ON" : "OFF");
      gfx->fillScreen(BLACK);
      // Flash screen to indicate toggle
      gfx->fillScreen(autoScroll ? GREEN : RED);
      delay(200);
      gfx->fillScreen(BLACK);
      bootHoldStart = millis() + 5000; // Prevent immediate retrigger
    }
    // Check for quick press (cycle modes)
    else if (!bootPressed && (millis() - bootHoldStart < HOLD_TIME) && (millis() - lastPress > 300)) {
      autoScroll = false; // Disable auto-scroll when manually cycling
      currentMode = (AnimationMode)((currentMode + 1) % MODE_COUNT);
      Serial.printf("Manual switch to mode %d\n", currentMode);
      lastPress = millis();
      gfx->fillScreen(BLACK);
    }
    bootPressed = true;
  } else {
    bootPressed = false;
  }
  
  // Auto-scroll functionality
  if (autoScroll && (millis() - autoScrollTimer >= AUTO_SCROLL_INTERVAL)) {
    currentMode = (AnimationMode)((currentMode + 1) % MODE_COUNT);
    Serial.printf("Auto-scroll to mode %d\n", currentMode);
    autoScrollTimer = millis();
    gfx->fillScreen(BLACK);
  }
  
  // Check touch
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    if (millis() - lastPress > 300) {
      autoScroll = false; // Disable auto-scroll when manually cycling
      currentMode = (AnimationMode)((currentMode + 1) % MODE_COUNT);
      Serial.printf("Touch: Switched to mode %d\n", currentMode);
      lastPress = millis();
      gfx->fillScreen(BLACK);
    }
  }
  
  // Draw current effect
  switch (currentMode) {
    case BOXED:
      drawBoxed();
      break;
    case BOXFIT:
      drawBoxfit();
      break;
    case BRAID:
      drawBraid();
      break;
    case BSOD:
      drawBSOD();
      break;
    case B_SPHERE:
      drawBSphere();
      break;
    case BUBBLE3D:
      drawBubble3D();
      break;
    case BUILDLWO:
      drawBuildlwo();
      break;
    case BUMPS:
      drawBumps();
      break;
    case CAGE:
      drawCage();
      break;
    case CAROUSEL:
      drawCarousel();
      break;
    case CCURVE:
      drawCCurve();
      break;
    case CELTIC:
      drawCeltic();
      break;
    case CHESSMODELS:
      drawChessModels();
      break;
    case CHOMPYTOWER:
      drawChompyTower();
      break;
    case CIRCUIT:
      drawCircuit();
      break;
    case CITYFLOW:
      drawCityFlow();
      break;
    case CLIENTMSG:
      drawClientMsg();
      break;
    case CLOUDLIFE:
      drawCloudLife();
      break;
    case COLORBARS:
      drawColorBars();
      break;
    case COLORS:
      drawColors();
      break;
    case COMPANION:
      drawCompanion();
      break;
    case COMPANION_DISC:
      drawCompanionDisc();
      break;
    case COMPANION_HEART:
      drawCompanionHeart();
      break;
    case COMPANION_QUAD:
      drawCompanionQuad();
      break;
    case COMPASS:
      drawCompass();
      break;
    case CORAL:
      drawCoral();
      break;
    case COVID19:
      drawCovid19();
      break;
    case COW_FACE:
      drawCowFace();
      break;
    case COW_HIDE:
      drawCowHide();
      break;
    case COW_HOOFS:
      drawCowHoofs();
      break;
    case COW_HORNS:
      drawCowHorns();
      break;
    case COW_TAIL:
      drawCowTail();
      break;
    case COW_UDDER:
      drawCowUdder();
      break;
    case CRACKBERG:
      drawCrackberg();
      break;
    case CRITICAL:
      drawCritical();
      break;
    case CRUMBLER:
      drawCrumbler();
      break;
    case CRYSTAL:
      drawCrystal();
      break;
    case CUBE21:
      drawCube21();
      break;
    case CUBENETIC:
      drawCubenetic();
      break;
    case CUBESTACK:
      drawCubeStack();
      break;
    case CUBESTORM:
      drawCubeStorm();
      break;
    case CUBETWIST:
      drawCubeTwist();
      break;
    case CUBICGRID:
      drawCubicGrid();
      break;
    case CUBOCTEVERSION:
      drawCuboctaversion();
      break;
    case CWAVES:
      drawCWaves();
      break;
    case CYNOSURE:
      drawCynosure();
      break;
    case DANGERBALL:
      drawDangerball();
      break;
    case DECAYSCREEN:
      drawDecayScreen();
      break;
    case DECO:
      drawDeco();
      break;
    case DEEPSTARS:
      drawDeepStars();
      break;
    case DELAUNAY:
      drawDelaunay();
      break;
    case DELUXE:
      drawDeluxe();
      break;
    case DEMO_GTK:
      drawDemoGtk();
      break;
    case DEMO_GTK_CONF:
      drawDemoGtkConf();
      break;
    case DEMON:
      drawDemon();
      break;
    case DEMO_XM:
      drawDemoXm();
      break;
    case DEMO_XM_WIDGETS:
      drawDemoXmWidgets();
      break;
    case DIALOG:
      drawDialog();
      break;
    case DISCOBALL:
      drawDiscoball();
      break;
    case DISCRETE:
      drawDiscrete();
      break;
    case DISTORT:
      drawDistort();
      break;
    case DNALOGO:
      drawDnalogo();
      break;
    case DOLPHIN:
      drawDolphin();
      break;
    case DOUBLETIME:
      drawDoubletime();
      break;
    case DRIFT:
      drawDrift();
      break;
    case DPMS:
      drawDpms();
      break;
    case DPMS_PROTOCOL:
      drawDpmsProtocol();
      break;
    case DROPSHADOW:
      drawDropshadow();
      break;
    case DROSTE:
      drawDroste();
      break;
    case DYMAXIONMAP_COORDS:
      drawDymaxionmapCoords();
      break;
    case EASING:
      drawEasing();
      break;
    case ENDGAME:
      drawEndgame();
      break;
    case ENERGYSTREAM:
      drawEnergystream();
      break;
    case ENGINE:
      drawEngine();
      break;
    case EPICYCLE:
      drawEpicycle();
      break;
    case ERUPTION:
      drawEruption();
      break;
    case EULER2D:
      drawEuler2d();
      break;
    case FADEPLOT:
      drawFadeplot();
      break;
    case ESPER:
      drawEsper();
      break;
    case ETRUSCANVENUS:
      drawEtruscanvenus();
      break;
    case EXEC:
      drawExec();
      break;
    case EXT_IDLE_NOTIFY:
      drawExtIdleNotify();
      break;
    case EXTRUSION:
      drawExtrusion();
      break;
    case EXTRUSION_HELIX2:
      drawExtrusionHelix2();
      break;
    case EXTRUSION_HELIX3:
      drawExtrusionHelix3();
      break;
    case EXTRUSION_HELIX4:
      drawExtrusionHelix4();
      break;
    case EXTRUSION_JOINOFFSET:
      drawExtrusionJoinoffset();
      break;
    case EXTRUSION_SCREW:
      drawExtrusionScrew();
      break;
    case EXTRUSION_TAPER:
      drawExtrusionTaper();
      break;
    case EXTRUSION_TWISTOID:
      drawExtrusionTwistoid();
      break;
    case EXTS:
      drawExts();
      break;
    case EXT_SESSION_LOCK:
      drawExtSessionLock();
      break;
    case FADE:
      drawFade();
      break;
    case FFMPEG_OUT:
      drawFfmpegOut();
      break;
    case FIBERLAMP:
      drawFiberlamp();
      break;
    case FILMLEADER:
      drawFilmleader();
      break;
    case FIREWORKX:
      drawFireworkx();
      break;
    case FLAG:
      drawFlag();
      break;
    case FLAME:
      drawFlame();
      break;
    case FLIPFLOP:
      drawFlipflop();
      break;
    case FLIPSCREEN3D:
      drawFlipscreen3d();
      break;
    case FLIPTEXT:
      drawFliptext();
      break;
    case FLOW:
      drawFlow();
      break;
    case FLUIDBALLS:
      drawFluidballs();
      break;
    case FLURRY:
      drawFlurry();
      break;
    case FLURRY_SMOKE:
      drawFlurrySmoke();
      break;
    case FLURRY_SPARK:
      drawFlurrySpark();
      break;
    case FLURRY_STAR:
      drawFlurryStar();
      break;
    case FLURRY_TEXTURE:
      drawFlurryTexture();
      break;
    case FLYINGTOASTERS:
      drawFlyingtoasters();
      break;
    case FONTGLIDE:
      drawFontglide();
      break;
    case FONT_RETRY:
      drawFontRetry();
      break;
    case FOREST:
      drawForest();
      break;
    case FPS:
      drawFps();
      break;
    case FPS_GL:
      drawFpsGl();
      break;
    case FUZZYFLAKES:
      drawFuzzyflakes();
      break;
    case GALAXY:
      drawGalaxy();
      break;
    case GALLANT12X22_BSD:
      drawGallant12x22Bsd();
      break;
    case GEARS:
      drawGears();
      break;
    case GEODESIC:
      drawGeodesic();
      break;
    case GEODESICGEARS:
      drawGeodesicgears();
      break;
    case GLLIST:
      drawGllist();
      break;
    case GLMATRIX:
      drawGlmatrix();
      break;
    case GLPLANET:
      drawGlplanet();
      break;
    case GLPUZZLE:
      drawGlpuzzle();
      break;
    case GLSCHOOL:
      drawGlschool();
      break;
    case GLSLIDESHOW:
      drawGlslideshow();
      break;
    case GLSL_UTILS:
      drawGlslUtils();
      break;
    case GLSNAKE:
      drawGlsnake();
      break;
    case GLTEXT:
      drawGltext();
      break;
    case GLTRACKBALL:
      drawGltrackball();
      break;
    case GLUT_STROKE:
      drawGlutStroke();
      break;
    case GLUT_SWIDTH:
      drawGlutSwidth();
      break;
    case GOOP:
      drawGoop();
      break;
    case GRABCLIENT:
      drawGrabclient();
      break;
    case GRAB_XIMAGE:
      drawGrabXimage();
      break;
    case GRAV:
      drawGrav();
      break;
    case GRAVITYWELL:
      drawGravitywell();
      break;
    case GREYNETIC:
      drawGreynetic();
      break;
    case HALFTONE:
      drawHalftone();
      break;
    case HALO:
      drawHalo();
      break;
    case HANDSY:
      drawHandsy();
      break;
    case HANDSY_MODEL:
      drawHandsyModel();
      break;
    case HEADROOM:
      drawHeadroom();
      break;
    case HEADROOM_MODEL:
      drawHeadroomModel();
      break;
    case HELIX:
      drawHelix();
      break;
    case HEXADROP:
      drawHexadrop();
      break;
    case HEXSTRUT:
      drawHexstrut();
      break;
    case HEXTRAIL:
      drawHextrail();
      break;
    case HIGHVOLTAGE:
      drawHighvoltage();
      break;
    case HIGHVOLTAGE_MODEL:
      drawHighvoltageModel();
      break;
    case HILBERT:
      drawHilbert();
      break;
    case HOPALONG:
      drawHopalong();
      break;
    case HOPFANIMATIONS:
      drawHopfanimations();
      break;
    case HOPFFIBRATION:
      drawHopffibration();
      break;
    case HSV:
      drawHsv();
      break;
    case HYDROSTAT:
      drawHydrostat();
      break;
    case HYPERBALL:
      drawHyperball();
      break;
    case HYPERCUBE:
      drawHypercube();
      break;
    case HYPERTORUS:
      drawHypertorus();
      break;
    case HYPNOWHEEL:
      drawHypnowheel();
      break;
    case IDLE_PROTOCOL:
      drawIdleProtocol();
      break;
    case IFS:
      drawIfs();
      break;
    case IMSMAP:
      drawImsmap();
      break;
    case INTERAGGREGATE:
      drawInteraggregate();
      break;
    case INTERFERENCE:
      drawInterference();
      break;
    case INTERMOMENTARY:
      drawIntermomentary();
      break;
    case INVOLUTE:
      drawInvolute();
      break;
    case JIGGLYPUFF:
      drawJigglypuff();
      break;
    case JIGSAW:
      drawJigsaw();
      break;
    case JUGGLE:
      drawJuggle();
      break;
    case JUGGLER3D:
      drawJuggler3d();
      break;
    case ROTOR:
      drawRotor();
      break;
    case ROTZOOMER:
      drawRotzoomer();
      break;
    case RUBIKBLOCKS:
      drawRubikblocks();
      break;
    case RUBIK:
      drawRubik();
      break;
    case SBALLS:
      drawSballs();
      break;
    case SCOOTER:
      drawScooter();
      break;
    case SETUID:
      drawSetuid();
      break;
    case SHADEBOBS:
      drawShadebobs();
      break;
    case SHARK:
      drawShark();
      break;
    case SLIDESCREEN:
      drawSlidescreen();
      break;
    case SLIP:
      drawSlip();
      break;
    case SONAR:
      drawSonar();
      break;
    case SPEEDMINE:
      drawSpeedmine();
      break;
    case SPHERE:
      drawSphere();
      break;
    case SPHEREMONICS:
      drawSpheremonics();
      break;
    case SPIRAL:
      drawSpiral();
      break;
    case SPOTLIGHT:
      drawSpotlight();
      break;
    case SPLITFLAP:
      drawSplitflap();
      break;
    case SPLODESIC:
      drawSplodesic();
      break;
    case SPROINGIES:
      drawSproingies();
      break;
    case SQUIRAL:
      drawSquiral();
      break;
    case SQUIRTORUS:
      drawSquirtorus();
      break;
    case STAIRS:
      drawStairs();
      break;
    case STARFISH:
      drawStarfish();
      break;
    case STARWARS:
      drawStarwars();
      break;
    case STONERVIEW:
      drawStonerview();
      break;
    case STRANGE:
      drawStrange();
      break;
    case PETRI:
      drawPetri();
      break;
    case PHOSPHOR:
      drawPhosphor();
      break;
    case PHOTOPILE:
      drawPhotopile();
      break;
    case PIECEWISE:
      drawPiecewise();
      break;
    case PINION:
      drawPinion();
      break;
    case PIPEOBJS:
      drawPipeobjs();
      break;
    case PIPES:
      drawPipes();
      break;
    case PLATONICFOLDING:
      drawPlatonicfolding();
      break;
    case POLYHEDRA:
      drawPolyhedra();
      break;
    case POLYHEDRA_GL:
      drawPolyhedraGl();
      break;
    case POLYOMINOES:
      drawPolyominoes();
      break;
    case POLYTOPES:
      drawPolytopes();
      break;
    case PONG:
      drawPong();
      break;
    case POPSQUARES:
      drawPopsquares();
      break;
    case POW2:
      drawPow2();
      break;
    case PREFS:
      drawPrefs();
      break;
    case PREFSW:
      drawPrefsw();
      break;
    case PROJECTIVEPLANE:
      drawProjectiveplane();
      break;
    case PROVIDENCE:
      drawProvidence();
      break;
    case M6502:
      drawM6502();
      break;
    case MAPSCROLLER:
      drawMapscroller();
      break;
    case MARBLING:
      drawMarbling();
      break;
    case MARCHING:
      drawMarching();
      break;
    case MAZE3D:
      drawMaze3d();
      break;
    case MAZE:
      drawMaze();
      break;
    case MEMSCROLLER:
      drawMemscroller();
      break;
    case MENGER:
      drawMenger();
      break;
    case METABALLS:
      drawMetaballs();
      break;
    case MINIXPM:
      drawMinixpm();
      break;
    case MIRRORBLOB:
      drawMirrorblob();
      break;
    case MOEBIUS:
      drawMoebius();
      break;
    case MOEBIUSGEARS:
      drawMoebiusgears();
      break;
    case MOIRE2:
      drawMoire2();
      break;
    case MOIRE:
      drawMoire();
      break;
    case MORPH3D:
      drawMorph3d();
      break;
    case MUNCH:
      drawMunch();
      break;
    case NAKAGIN:
      drawNakagin();
      break;
    case NERVEROT:
      drawNerverot();
      break;
    case NOOF:
      drawNoof();
      break;
    case NORMALS:
      drawNormals();
      break;
    case NOSEGUY:
      drawNoseguy();
      break;
    case OVERLAY:
      drawOverlay();
      break;
    case PACMAN_AI:
      drawPacmanAI();
      break;
    case PACMAN:
      drawPacman();
      break;
    case PACMAN_LEVEL:
      drawPacmanLevel();
      break;
    case PAPERCUBE:
      drawPapercube();
      break;
    case PASSWD:
      drawPasswd();
      break;
    case PASSWD_KERBEROS:
      drawPasswdKerberos();
      break;
    case PASSWD_PAM:
      drawPasswdPam();
      break;
    case PASSWD_PWENT:
      drawPasswdPwent();
      break;
    case PEDAL:
      drawPedal();
      break;
    case PEEPERS:
      drawPeepers();
      break;
    case PENETRATE:
      drawPenetrate();
      break;
    case PENROSE:
      drawPenrose();
      break;
    case PULSAR:
      drawPulsar();
      break;
    case PYRO:
      drawPyro();
      break;
    case QIX:
      drawQix();
      break;
    case QUASICRYSTAL:
      drawQuasicrystal();
      break;
    case QUEENS:
      drawQueens();
      break;
    case QUICKHULL:
      drawQuickhull();
      break;
    case RAVERHOOP:
      drawRaverhoop();
      break;
    case RAZZLEDAZZLE:
      drawRazzledazzle();
      break;
    case RDBOMB:
      drawRdbomb();
      break;
    case RECANIM:
      drawRecanim();
      break;
    case REMOTE:
      drawRemote();
      break;
    case RESOURCES:
      drawResources();
      break;
    case RIPPLES:
      drawRipples();
      break;
    case ROBOT:
      drawRobot();
      break;
    case ROBOT_WIREFRAME:
      drawRobotWireframe();
      break;
    case ROCKS:
      drawRocks();
      break;
    case ROMANBOY:
      drawRomanboy();
      break;
    case RORSCHACH:
      drawRorschach();
      break;
    case ROTATOR:
      drawRotator();
      break;
    case SHIPS:
      drawShips();
      break;
    case SIERPINSKI3D:
      drawSierpinski3d();
      break;
    case SIERPINSKI:
      drawSierpinski();
      break;
    case SKULL_MODEL:
      drawSkullModel();
      break;
    case SKULLOOP:
      drawSkulloop();
      break;
    case SKYTENTACLES:
      drawSkytentacles();
      break;
    case SPHEREEVERSION_ANALYTIC:
      drawSphereversionAnalytic();
      break;
    case SPHEREEVERSION:
      drawSphereversion();
      break;
    case SPHEREEVERSION_CORRUGATIONS:
      drawSphereversionCorrugations();
      break;
    case SUBSTRATE:
      drawSubstrate();
      break;
    case SUPERQUADRICS:
      drawSuperquadrics();
      break;
    case SURFACES:
      drawSurfaces();
      break;
    case SWIM:
      drawSwim();
      break;
    case SWIRL:
      drawSwirl();
      break;
    case T3D:
      drawT3d();
      break;
    case TANGRAM:
      drawTangram();
      break;
    case TEAPOT:
      drawTeapot();
      break;
    case TESSELLIMAGE:
      drawTessellimage();
      break;
    case THORNBIRD:
      drawThornbird();
      break;
    case TIMETUNNEL:
      drawTimetunnel();
      break;
    case TIMEZONES:
      drawTimezones();
      break;
    case TOASTER:
      drawToaster();
      break;
    case TOPBLOCK:
      drawTopblock();
      break;
    case TRACKBALL:
      drawTrackball();
      break;
    case TRIANGLE:
      drawTriangle();
      break;
    case TRONBIT:
      drawTronbit();
      break;
    case TRUCHET:
      drawTruchet();
      break;
    case TUBE:
      drawTube();
      break;
    case TWANG:
      drawTwang();
      break;
    case UNICRUD:
      drawUnicrud();
      break;
    case UNKNOWNPLEASURES:
      drawUnknownpleasures();
      break;
    case VERMICULATE:
      drawVermiculate();
      break;
    case VFEEDBACK:
      drawVfeedback();
      break;
    case VIGILANCE:
      drawVigilance();
      break;
    case VINES:
      drawVines();
      break;
    case VORONOI:
      drawVoronoi();
      break;
    case WANDER:
      drawWander();
      break;
    case WHALE:
      drawWhale();
      break;
    case WHIRLWINDWARP:
      drawWhirlwindwarp();
      break;
    case WHIRLYGIG:
      drawWhirlygig();
      break;
    case WINDUPROBOT:
      drawWinduprobot();
      break;
    case WORM:
      drawWorm();
      break;
    case WORMHOLE:
      drawWormhole();
      break;
    case XANALOGTV:
      drawXanalogtv();
      break;
    case XFLAME:
      drawXflame();
      break;
    case XJACK:
      drawXjack();
      break;
    case XLYAP:
      drawXlyap();
      break;
    case XMATRIX:
      drawXmatrix();
      break;
    case XRAYSWARM:
      drawXrayswarm();
      break;
    case XSPIROGRAPH:
      drawXspirograph();
      break;
    case XSUBLIM:
      drawXsublim();
      break;
    case ZOOM:
      drawZoom();
      break;
    case ABSTRACTILE:
      drawAbstractile();
      break;
    case ANEMONE:
      drawAnemone();
      break;
    case ANEMOTAXIS:
      drawAnemotaxis();
      break;
    case ANT:
      drawAnt();
      break;
    case APOLLONIAN:
      drawApollonian();
      break;
    case APPLE2:
      drawApple2();
      break;
    case ATLANTIS:
      drawAtlantis();
      break;
    case ATTRACTION:
      drawAttraction();
      break;
    case ATUNNEL:
      drawAtunnel();
      break;
    case BARCODE:
      drawBarcode();
      break;
    case BINARYHORIZON:
      drawBinaryhorizon();
      break;
    case BINARYRING:
      drawBinaryring();
      break;
    case BLASTER:
      drawBlaster();
      break;
    case BLITSPIN:
      drawBlitspin();
      break;
    case BLOCKTUBE:
      drawBlocktube();
      break;
    case BOUBOULE:
      drawBouboule();
      break;
    case BOUNCINGCOW:
      drawBouncingcow();
      break;
  }
  
  animationTime += 0.05;
  delay(50);
}
void drawGoop() {
  static float goopTime = 0;
  goopTime += 0.02;
  
  gfx->fillScreen(BLACK);
  gfx->setTextColor(GREEN);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple goop blobs with organic movement
  for (int blob = 0; blob < 8; blob++) {
    float angle = blob * PI / 4 + goopTime * 0.5;
    float radius = 60 + sin(goopTime * 2 + blob) * 20;
    
    int x = centerX + cos(angle) * radius;
    int y = centerY + sin(angle) * radius;
    
    // Goop blob size varies organically
    int blobSize = 15 + sin(goopTime * 3 + blob * 0.7) * 8;
    
    // Color shifts through spectrum
    uint16_t color = gfx->color565(
      128 + sin(goopTime + blob * 0.5) * 127,
      128 + sin(goopTime * 1.3 + blob * 0.7) * 127,
      128 + sin(goopTime * 1.7 + blob * 0.9) * 127
    );
    
    gfx->fillCircle(x, y, blobSize, color);
    
    // Connecting tentacles between nearby blobs
    if (blob < 7) {
      float nextAngle = (blob + 1) * PI / 4 + goopTime * 0.5;
      float nextRadius = 60 + sin(goopTime * 2 + (blob + 1)) * 20;
      int nextX = centerX + cos(nextAngle) * nextRadius;
      int nextY = centerY + sin(nextAngle) * nextRadius;
      
      gfx->drawLine(x, y, nextX, nextY, color);
    }
  }
  
  // Title
  gfx->setCursor(10, 10);
  gfx->setTextSize(2);
  gfx->printf("GOOP");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->setTextSize(1);
  gfx->printf("Organic blobs: %d", (int)(goopTime * 10) % 100);
}

void drawGltrackball() {
  static float trackTime = 0;
  trackTime += 0.03;
  
  gfx->fillScreen(BLACK);
  gfx->setTextColor(CYAN);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Rotating trackball sphere simulation
  for (int i = 0; i < 20; i++) {
    float lat = (i - 10) * PI / 20;
    for (int j = 0; j < 40; j++) {
      float lon = j * PI / 20 + trackTime;
      
      // 3D to 2D projection
      float x3d = cos(lat) * cos(lon);
      float y3d = cos(lat) * sin(lon);
      float z3d = sin(lat);
      
      // Simple rotation around Y axis
      float rotX = x3d * cos(trackTime * 0.7) - z3d * sin(trackTime * 0.7);
      float rotZ = x3d * sin(trackTime * 0.7) + z3d * cos(trackTime * 0.7);
      
      if (rotZ > 0) { // Only draw front-facing points
        int screenX = centerX + rotX * 80;
        int screenY = centerY - y3d * 80;
        
        // Color based on depth
        uint8_t intensity = (rotZ + 1) * 127;
        uint16_t color = gfx->color565(intensity, intensity, 255);
        
        gfx->drawPixel(screenX, screenY, color);
      }
    }
  }
  
  // Title
  gfx->setCursor(10, 10);
  gfx->setTextSize(2);
  gfx->printf("GLTrackball");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->setTextSize(1);
  gfx->printf("Rotation: %.0f°", trackTime * 180 / PI);
}

void drawGlutStroke() {
  static float strokeTime = 0;
  strokeTime += 0.04;
  
  gfx->fillScreen(BLACK);
  gfx->setTextColor(MAGENTA);
  
  // Simulated stroke font rendering with vector-like text
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Draw animated stroke letters
  for (int letter = 0; letter < 5; letter++) {
    float angle = letter * PI / 2.5 + strokeTime;
    int x = centerX + cos(angle) * 60;
    int y = centerY + sin(angle) * 40;
    
    // Different stroke patterns for each letter
    switch (letter) {
      case 0: // Letter A
        gfx->drawLine(x-8, y+10, x, y-10, WHITE);
        gfx->drawLine(x, y-10, x+8, y+10, WHITE);
        gfx->drawLine(x-4, y, x+4, y, WHITE);
        break;
      case 1: // Letter B
        gfx->drawLine(x-6, y-10, x-6, y+10, WHITE);
        gfx->drawLine(x-6, y-10, x+4, y-10, WHITE);
        gfx->drawLine(x-6, y, x+2, y, WHITE);
        gfx->drawLine(x-6, y+10, x+4, y+10, WHITE);
        break;
      case 2: // Letter C
        gfx->drawCircle(x, y, 8, WHITE);
        gfx->drawLine(x+6, y-6, x+8, y-8, BLACK);
        gfx->drawLine(x+6, y+6, x+8, y+8, BLACK);
        break;
      case 3: // Letter D
        gfx->drawLine(x-6, y-10, x-6, y+10, WHITE);
        gfx->drawCircle(x-2, y, 8, WHITE);
        gfx->drawLine(x-10, y-8, x-6, y-10, BLACK);
        gfx->drawLine(x-10, y+8, x-6, y+10, BLACK);
        break;
      case 4: // Letter E
        gfx->drawLine(x-6, y-10, x-6, y+10, WHITE);
        gfx->drawLine(x-6, y-10, x+6, y-10, WHITE);
        gfx->drawLine(x-6, y, x+2, y, WHITE);
        gfx->drawLine(x-6, y+10, x+6, y+10, WHITE);
        break;
    }
  }
  
  // Title
  gfx->setCursor(10, 10);
  gfx->setTextSize(2);
  gfx->printf("GLUT Stroke");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->setTextSize(1);
  gfx->printf("Vector fonts");
}

void drawGlutSwidth() {
  static float swidthTime = 0;
  swidthTime += 0.05;
  
  gfx->fillScreen(BLACK);
  gfx->setTextColor(YELLOW);
  
  // String width calculation visualization
  int centerY = gfx->height() / 2;
  
  // Animated text with varying widths
  const char* texts[] = {"A", "AB", "ABC", "ABCD", "ABCDE"};
  
  for (int i = 0; i < 5; i++) {
    float wave = sin(swidthTime + i * 0.5) * 30;
    int x = 20 + i * 50 + wave;
    int y = centerY + sin(swidthTime * 2 + i) * 20;
    
    // Calculate text width (simulated)
    int textWidth = strlen(texts[i]) * 12; // 12 pixels per character
    
    // Draw text with width indicator
    gfx->setCursor(x, y);
    gfx->setTextSize(2);
    gfx->printf("%s", texts[i]);
    
    // Draw width measurement
    gfx->drawLine(x, y + 25, x + textWidth, y + 25, RED);
    gfx->drawLine(x, y + 23, x, y + 27, RED);
    gfx->drawLine(x + textWidth, y + 23, x + textWidth, y + 27, RED);
    
    // Width number
    gfx->setCursor(x, y + 30);
    gfx->setTextSize(1);
    gfx->printf("%dpx", textWidth);
  }
  
  // Title
  gfx->setCursor(10, 10);
  gfx->setTextSize(2);
  gfx->printf("GLUT SWidth");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->setTextSize(1);
  gfx->printf("String width calc");
}

void drawGrabclient() {
  static float grabTime = 0;
  grabTime += 0.03;
  
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  
  // Screen capture simulation with scanning effect
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Scanning line effect
  int scanLine = (int)(grabTime * 50) % gfx->height();
  
  // Draw captured "screen" content (simulated)
  for (int y = 0; y < gfx->height(); y += 4) {
    for (int x = 0; x < gfx->width(); x += 4) {
      if (y < scanLine) {
        // "Captured" area - show some pattern
        uint16_t color = gfx->color565(
          (x + y + (int)(grabTime * 100)) % 256,
          (x * 2 + y + (int)(grabTime * 150)) % 256,
          (x + y * 2 + (int)(grabTime * 200)) % 256
        );
        gfx->fillRect(x, y, 3, 3, color);
      }
    }
  }
  
  // Scanning line
  gfx->drawLine(0, scanLine, gfx->width(), scanLine, RED);
  gfx->drawLine(0, scanLine+1, gfx->width(), scanLine+1, WHITE);
  
  // Corner indicators (capture frame)
  gfx->drawLine(10, 10, 30, 10, GREEN);
  gfx->drawLine(10, 10, 10, 30, GREEN);
  gfx->drawLine(gfx->width()-30, 10, gfx->width()-10, 10, GREEN);
  gfx->drawLine(gfx->width()-10, 10, gfx->width()-10, 30, GREEN);
  
  // Title
  gfx->setCursor(40, 10);
  gfx->setTextSize(2);
  gfx->printf("GrabClient");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->setTextSize(1);
  gfx->printf("Capturing: %d%%", (scanLine * 100) / gfx->height());
}

void drawGrabXimage() {
  static float ximageTime = 0;
  ximageTime += 0.04;
  
  gfx->fillScreen(BLACK);
  gfx->setTextColor(CYAN);
  
  // X11 image capture simulation with pixel-by-pixel effect
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Progressive image "capture" effect
  int captureRadius = (int)(sin(ximageTime) * 50 + 60);
  
  for (int angle = 0; angle < 360; angle += 10) {
    float rad = angle * PI / 180;
    int x = centerX + cos(rad) * captureRadius;
    int y = centerY + sin(rad) * captureRadius;
    
    if (x >= 0 && x < gfx->width() && y >= 0 && y < gfx->height()) {
      // Simulated captured pixel data
      uint16_t color = gfx->color565(
        128 + sin(ximageTime + angle * 0.01) * 127,
        128 + sin(ximageTime * 1.3 + angle * 0.02) * 127,
        128 + sin(ximageTime * 1.7 + angle * 0.03) * 127
      );
      
      // Draw captured "pixel" with size based on distance
      int pixelSize = 3 + sin(ximageTime + angle * 0.1) * 2;
      gfx->fillRect(x-pixelSize/2, y-pixelSize/2, pixelSize, pixelSize, color);
    }
  }
  
  // X11 window frame simulation
  gfx->drawRect(centerX-80, centerY-60, 160, 120, WHITE);
  gfx->drawRect(centerX-78, centerY-58, 156, 116, BLUE);
  
  // Title bar
  gfx->fillRect(centerX-76, centerY-56, 152, 20, BLUE);
  gfx->setCursor(centerX-70, centerY-52);
  gfx->setTextSize(1);
  gfx->printf("XImage Capture");
  
  // Main title
  gfx->setCursor(10, 10);
  gfx->setTextSize(2);
  gfx->printf("Grab XImage");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->setTextSize(1);
  gfx->printf("X11 capture: %d px", captureRadius);
}

void drawGrav() {
  gfx->fillScreen(BLACK);
  
  // Gravity simulation with particles
  static float particles[20][4]; // x, y, vx, vy for each particle
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 20; i++) {
      particles[i][0] = random(gfx->width());
      particles[i][1] = random(gfx->height());
      particles[i][2] = (random(200) - 100) / 100.0;
      particles[i][3] = (random(200) - 100) / 100.0;
    }
    initialized = true;
  }
  
  float centerX = gfx->width() / 2 + sin(animationTime) * 40;
  float centerY = gfx->height() / 2 + cos(animationTime * 0.7) * 30;
  
  // Draw gravitational center
  gfx->fillCircle(centerX, centerY, 8, YELLOW);
  
  for (int i = 0; i < 20; i++) {
    float dx = centerX - particles[i][0];
    float dy = centerY - particles[i][1];
    float dist = sqrt(dx*dx + dy*dy);
    
    if (dist > 5) {
      float force = 200.0 / (dist * dist);
      particles[i][2] += (dx / dist) * force * 0.01;
      particles[i][3] += (dy / dist) * force * 0.01;
    }
    
    particles[i][0] += particles[i][2];
    particles[i][1] += particles[i][3];
    
    // Wrap around screen
    if (particles[i][0] < 0) particles[i][0] = gfx->width();
    if (particles[i][0] >= gfx->width()) particles[i][0] = 0;
    if (particles[i][1] < 0) particles[i][1] = gfx->height();
    if (particles[i][1] >= gfx->height()) particles[i][1] = 0;
    
    uint16_t color = gfx->color565(255 - i*10, 100 + i*5, i*12);
    gfx->fillCircle(particles[i][0], particles[i][1], 2, color);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextSize(2);
  gfx->printf("GRAV");
}

void drawGravitywell() {
  gfx->fillScreen(BLACK);
  
  // Multiple gravity wells
  int wells = 3;
  float wellX[3], wellY[3];
  
  for (int w = 0; w < wells; w++) {
    wellX[w] = gfx->width()/2 + sin(animationTime + w*2) * 60;
    wellY[w] = gfx->height()/2 + cos(animationTime*0.8 + w*2) * 40;
  }
  
  // Draw field visualization
  for (int x = 0; x < gfx->width(); x += 8) {
    for (int y = 0; y < gfx->height(); y += 8) {
      float totalForceX = 0, totalForceY = 0;
      
      for (int w = 0; w < wells; w++) {
        float dx = wellX[w] - x;
        float dy = wellY[w] - y;
        float dist = sqrt(dx*dx + dy*dy) + 1;
        float force = 100.0 / dist;
        
        totalForceX += (dx / dist) * force;
        totalForceY += (dy / dist) * force;
      }
      
      float intensity = sqrt(totalForceX*totalForceX + totalForceY*totalForceY);
      uint8_t brightness = min(255, (int)(intensity * 20));
      
      if (brightness > 20) {
        uint16_t color = gfx->color565(brightness/2, brightness, brightness/3);
        gfx->fillRect(x, y, 4, 4, color);
      }
    }
  }
  
  // Draw gravity wells
  for (int w = 0; w < wells; w++) {
    uint16_t wellColor = gfx->color565(255, 150 + w*30, 100 + w*50);
    gfx->fillCircle(wellX[w], wellY[w], 10, wellColor);
    
    // Draw gravitational rings
    for (int r = 15; r < 50; r += 10) {
      uint8_t alpha = 100 - (r-15)*3;
      gfx->drawCircle(wellX[w], wellY[w], r, gfx->color565(alpha, alpha/2, alpha/4));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextSize(2);
  gfx->printf("GravityWELL");
}

void drawGreynetic() {
  gfx->fillScreen(BLACK);
  
  // Genetic algorithm visualization with grey patterns
  int cellSize = 4;
  int gridW = gfx->width() / cellSize;
  int gridH = gfx->height() / cellSize;
  
  static uint8_t generation = 0;
  generation = (generation + 1) % 60;
  
  for (int x = 0; x < gridW; x++) {
    for (int y = 0; y < gridH; y++) {
      // Pseudo-genetic pattern
      int gene1 = (x * 17 + y * 23 + generation * 3) % 256;
      int gene2 = (x * 31 + y * 13 + generation * 7) % 256;
      int gene3 = (x * 7 + y * 41 + generation * 11) % 256;
      
      // Fitness function (pattern complexity)
      int fitness = abs(gene1 - gene2) + abs(gene2 - gene3) + abs(gene3 - gene1);
      
      // Mutation
      if (random(1000) < 50) {
        fitness = random(256);
      }
      
      // Selection (only show "fit" patterns)
      if (fitness > 100) {
        uint8_t grey = fitness % 256;
        uint16_t color = gfx->color565(grey, grey, grey);
        gfx->fillRect(x * cellSize, y * cellSize, cellSize, cellSize, color);
        
        // Add some colored mutations
        if (fitness > 200) {
          uint16_t mutantColor = gfx->color565(grey, grey/2, 0);
          gfx->fillRect(x * cellSize + 1, y * cellSize + 1, cellSize-2, cellSize-2, mutantColor);
        }
      }
    }
  }
  
  // Evolution progress bar
  gfx->drawRect(10, gfx->height()-20, 100, 8, WHITE);
  gfx->fillRect(11, gfx->height()-19, (generation * 98) / 60, 6, GREEN);
  
  gfx->setCursor(10, 10);
  gfx->setTextSize(2);
  gfx->printf("GREYNETIC");
  
  gfx->setCursor(10, 30);
  gfx->setTextSize(1);
  gfx->printf("Gen: %d", generation);
}

void drawHalftone() {
  static float halftoneTime = 0;
  halftoneTime += 0.03;
  
  gfx->fillScreen(BLACK);
  
  int spacing = 12;
  int maxDotSize = 8;
  
  // Create gravity points
  int numPoints = 4;
  float gravityX[4], gravityY[4];
  
  for (int i = 0; i < numPoints; i++) {
    gravityX[i] = gfx->width()/2 + sin(halftoneTime * (0.7 + i * 0.3)) * (gfx->width() * 0.3);
    gravityY[i] = gfx->height()/2 + cos(halftoneTime * (0.5 + i * 0.2)) * (gfx->height() * 0.3);
  }
  
  // Draw halftone dots based on gravitational field
  for (int y = spacing/2; y < gfx->height(); y += spacing) {
    for (int x = spacing/2; x < gfx->width(); x += spacing) {
      
      float totalForce = 0;
      for (int i = 0; i < numPoints; i++) {
        float dx = x - gravityX[i];
        float dy = y - gravityY[i];
        float distance = sqrt(dx*dx + dy*dy) + 1;
        totalForce += 1000.0 / (distance * distance);
      }
      
      int dotSize = constrain(totalForce * 0.1, 0, maxDotSize);
      
      if (dotSize > 1) {
        uint8_t intensity = map(dotSize, 0, maxDotSize, 0, 255);
        uint16_t dotColor = gfx->color565(intensity/3, intensity/2, intensity);
        gfx->fillCircle(x, y, dotSize/2, dotColor);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextSize(2);
  gfx->printf("HALFTONE");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->setTextSize(1);
  gfx->printf("Gravity Field Dots");
}

void drawHalo() {
  static float haloTime = 0;
  static int haloPhase = 0;
  haloTime += 0.05;
  
  if (haloTime > 6.28) {
    haloTime = 0;
    haloPhase = (haloPhase + 1) % 3;
  }
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  int maxRadius = min(gfx->width(), gfx->height()) / 2;
  
  // Draw multiple concentric halos
  for (int h = 0; h < 4; h++) {
    float phaseOffset = h * 1.57; // π/2
    float radius = maxRadius * (0.3 + 0.2 * h) * (1 + sin(haloTime * 2 + phaseOffset) * 0.3);
    
    for (int i = 0; i < 36; i++) {
      float angle = i * 0.174533; // 10 degrees in radians
      float x = centerX + cos(angle + haloTime) * radius;
      float y = centerY + sin(angle + haloTime) * radius;
      
      uint8_t brightness = 128 + sin(haloTime * 3 + i * 0.2) * 127;
      uint16_t color;
      
      switch (haloPhase) {
        case 0: color = gfx->color565(brightness, brightness/2, 0); break;
        case 1: color = gfx->color565(0, brightness, brightness/2); break;
        case 2: color = gfx->color565(brightness/2, 0, brightness); break;
      }
      
      gfx->fillCircle(x, y, 3, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextSize(2);
  gfx->printf("HALO");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->setTextSize(1);
  gfx->printf("Phase: %d", haloPhase + 1);
}

void drawHandsy() {
  static float handsyTime = 0;
  handsyTime += 0.08;
  
  gfx->fillScreen(BLACK);
  
  // Draw animated hand silhouettes
  int numHands = 3;
  
  for (int h = 0; h < numHands; h++) {
    float handPhase = handsyTime + h * 2.094; // 120 degrees apart
    
    int handX = gfx->width() * (0.2 + h * 0.3);
    int handY = gfx->height() * (0.5 + sin(handPhase) * 0.2);
    
    // Hand palm
    uint16_t handColor = gfx->color565(180 + h * 20, 140 + h * 30, 100 + h * 40);
    gfx->fillRect(handX, handY, 20, 35, handColor);
    
    // Animated fingers
    for (int f = 0; f < 5; f++) {
      float fingerPhase = handsyTime * (1 + f * 0.2) + h;
      int fingerLength = 15 + sin(fingerPhase) * 8;
      int fingerX = handX + f * 4;
      int fingerY = handY - fingerLength;
      
      gfx->fillRect(fingerX, fingerY, 3, fingerLength, handColor);
      
      // Finger joints
      int joint1Y = fingerY + fingerLength * 0.3;
      int joint2Y = fingerY + fingerLength * 0.7;
      
      uint16_t jointColor = gfx->color565(200, 180, 160);
      gfx->fillCircle(fingerX + 1, joint1Y, 1, jointColor);
      gfx->fillCircle(fingerX + 1, joint2Y, 1, jointColor);
    }
    
    // Thumb
    float thumbPhase = handsyTime * 1.5 + h;
    int thumbX = handX - 5;
    int thumbY = handY + 10 + sin(thumbPhase) * 5;
    gfx->fillRect(thumbX, thumbY, 12, 4, handColor);
  }
  
  // Draw gesture trails
  for (int t = 0; t < 20; t++) {
    float trailPhase = handsyTime - t * 0.1;
    int trailX = gfx->width()/2 + sin(trailPhase * 2) * 80;
    int trailY = gfx->height()/2 + cos(trailPhase * 1.5) * 60;
    
    uint8_t alpha = 255 - t * 12;
    uint16_t trailColor = gfx->color565(alpha/4, alpha/6, alpha/3);
    gfx->drawPixel(trailX, trailY, trailColor);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextSize(2);
  gfx->printf("HANDSY");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->setTextSize(1);
  gfx->printf("Hand Gestures");
}

void drawHandsyModel() {
  static float modelTime = 0;
  modelTime += 0.06;
  
  gfx->fillScreen(BLACK);
  
  // Draw 3D hand model wireframe
  int centerX = gfx->width()/2;
  int centerY = gfx->height()/2;
  
  // Rotating 3D hand vertices
  float vertices[12][3] = {
    {-20, -30, 0}, {20, -30, 0}, {20, 30, 0}, {-20, 30, 0},  // Palm
    {-15, -45, 5}, {-7, -50, 8}, {2, -48, 6}, {10, -52, 4}, {18, -45, 2},  // Fingers
    {-25, -10, 3}, {-30, 0, 5}, {-25, 10, 3}  // Thumb
  };
  
  // Rotate and project vertices
  int projected[12][2];
  for (int i = 0; i < 12; i++) {
    float x = vertices[i][0];
    float y = vertices[i][1] * cos(modelTime) - vertices[i][2] * sin(modelTime);
    float z = vertices[i][1] * sin(modelTime) + vertices[i][2] * cos(modelTime);
    
    // Simple perspective projection
    float scale = 100.0 / (100 + z);
    projected[i][0] = centerX + x * scale;
    projected[i][1] = centerY + y * scale;
  }
  
  // Draw wireframe connections
  uint16_t wireColor = gfx->color565(150, 200, 255);
  
  // Palm connections
  for (int i = 0; i < 3; i++) {
    gfx->drawLine(projected[i][0], projected[i][1], 
                  projected[i+1][0], projected[i+1][1], wireColor);
  }
  gfx->drawLine(projected[3][0], projected[3][1], 
                projected[0][0], projected[0][1], wireColor);
  
  // Finger connections
  for (int i = 4; i < 9; i++) {
    gfx->drawLine(projected[0][0], projected[0][1], 
                  projected[i][0], projected[i][1], wireColor);
  }
  
  // Thumb connections  
  for (int i = 9; i < 12; i++) {
    gfx->drawLine(projected[0][0], projected[0][1], 
                  projected[i][0], projected[i][1], wireColor);
  }
  
  // Draw vertices as dots
  for (int i = 0; i < 12; i++) {
    uint16_t dotColor = gfx->color565(255, 180 + i * 5, 100);
    gfx->fillCircle(projected[i][0], projected[i][1], 2, dotColor);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextSize(2);
  gfx->printf("HANDSY 3D");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->setTextSize(1);
  gfx->printf("3D Hand Model");
}

void drawHeadroom() {
  static float headroomTime = 0;
  headroomTime += 0.05;
  
  // Cyberpunk digital head simulation
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width()/2;
  int centerY = gfx->height()/2;
  
  // Draw digital head outline
  float headScale = 1.0 + sin(headroomTime * 2) * 0.1;
  
  // Head circle
  int headRadius = 50 * headScale;
  uint16_t headColor = gfx->color565(0, 255, 255);
  
  // Draw scanning lines across the head
  for (int y = centerY - headRadius; y < centerY + headRadius; y += 3) {
    float scanPhase = headroomTime * 5 + y * 0.1;
    uint8_t intensity = 100 + sin(scanPhase) * 155;
    
    int scanX1 = centerX - sqrt(headRadius * headRadius - (y - centerY) * (y - centerY));
    int scanX2 = centerX + sqrt(headRadius * headRadius - (y - centerY) * (y - centerY));
    
    uint16_t scanColor = gfx->color565(0, intensity/4, intensity/2);
    gfx->drawLine(scanX1, y, scanX2, y, scanColor);
  }
  
  // Draw digital eyes
  int eyeY = centerY - 10;
  int eyeSize = 8;
  
  // Left eye
  int leftEyeX = centerX - 15;
  uint16_t eyeColor = gfx->color565(255, 100, 0);
  gfx->fillRect(leftEyeX - eyeSize/2, eyeY - eyeSize/2, eyeSize, eyeSize, eyeColor);
  
  // Right eye  
  int rightEyeX = centerX + 15;
  gfx->fillRect(rightEyeX - eyeSize/2, eyeY - eyeSize/2, eyeSize, eyeSize, eyeColor);
  
  // Blinking effect
  if ((int)(headroomTime * 8) % 20 < 2) {
    gfx->fillRect(leftEyeX - eyeSize/2, eyeY - 2, eyeSize, 4, BLACK);
    gfx->fillRect(rightEyeX - eyeSize/2, eyeY - 2, eyeSize, 4, BLACK);
  }
  
  // Draw digital mouth
  int mouthY = centerY + 20;
  int mouthWidth = 20 + sin(headroomTime * 3) * 10;
  uint16_t mouthColor = gfx->color565(255, 255, 0);
  
  for (int i = 0; i < mouthWidth; i++) {
    int pixelX = centerX - mouthWidth/2 + i;
    int pixelY = mouthY + sin(headroomTime * 4 + i * 0.5) * 3;
    gfx->drawPixel(pixelX, pixelY, mouthColor);
  }
  
  // Draw data streams
  for (int s = 0; s < 5; s++) {
    float streamPhase = headroomTime * 2 + s * 1.26;
    int streamX = 20 + s * 50;
    int streamLength = 40 + sin(streamPhase) * 20;
    
    for (int l = 0; l < streamLength; l++) {
      int streamY = gfx->height() - 50 + l;
      uint8_t alpha = 255 - l * 4;
      uint16_t streamColor = gfx->color565(0, alpha/3, alpha/2);
      
      if (streamY >= 0 && streamY < gfx->height()) {
        gfx->drawPixel(streamX, streamY, streamColor);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextSize(2);
  gfx->printf("HEADROOM");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->setTextSize(1);
  gfx->printf("Digital Avatar: %.1f%%", (sin(headroomTime) * 0.5 + 0.5) * 100);
}

void drawHeadroomModel() {
  static float modelTime = 0;
  modelTime += 0.04;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width()/2;
  int centerY = gfx->height()/2;
  
  // 3D head wireframe model
  float rotX = modelTime * 0.7;
  float rotY = modelTime * 0.5;
  
  // Define 3D head vertices (simplified)
  float vertices[16][3] = {
    // Head outline (8 points)
    {0, -40, 20}, {20, -30, 15}, {30, 0, 10}, {20, 30, 15},
    {0, 40, 20}, {-20, 30, 15}, {-30, 0, 10}, {-20, -30, 15},
    // Inner features (8 points)
    {-10, -15, 25}, {10, -15, 25}, // Eyes
    {0, 0, 30}, // Nose tip
    {-8, 15, 25}, {8, 15, 25}, // Mouth corners
    {0, 20, 25}, // Mouth center
    {0, -45, 15}, {0, 45, 15} // Top/bottom
  };
  
  // Rotate and project vertices
  int projected[16][2];
  for (int i = 0; i < 16; i++) {
    // Rotate around Y axis
    float x1 = vertices[i][0] * cos(rotY) + vertices[i][2] * sin(rotY);
    float z1 = -vertices[i][0] * sin(rotY) + vertices[i][2] * cos(rotY);
    
    // Rotate around X axis  
    float y2 = vertices[i][1] * cos(rotX) - z1 * sin(rotX);
    float z2 = vertices[i][1] * sin(rotX) + z1 * cos(rotX);
    
    // Project to 2D
    float scale = 200.0 / (200 + z2);
    projected[i][0] = centerX + x1 * scale;
    projected[i][1] = centerY + y2 * scale;
  }
  
  // Draw head outline connections
  uint16_t outlineColor = gfx->color565(100, 255, 200);
  for (int i = 0; i < 7; i++) {
    gfx->drawLine(projected[i][0], projected[i][1], 
                  projected[i+1][0], projected[i+1][1], outlineColor);
  }
  gfx->drawLine(projected[7][0], projected[7][1], 
                projected[0][0], projected[0][1], outlineColor);
  
  // Draw facial features
  uint16_t featureColor = gfx->color565(255, 200, 100);
  
  // Eyes
  gfx->drawLine(projected[8][0], projected[8][1], 
                projected[9][0], projected[9][1], featureColor);
  
  // Nose
  gfx->drawLine(projected[9][0], projected[9][1], 
                projected[10][0], projected[10][1], featureColor);
  gfx->drawLine(projected[8][0], projected[8][1], 
                projected[10][0], projected[10][1], featureColor);
  
  // Mouth
  gfx->drawLine(projected[11][0], projected[11][1], 
                projected[13][0], projected[13][1], featureColor);
  gfx->drawLine(projected[11][0], projected[11][1], 
                projected[12][0], projected[12][1], featureColor);
  gfx->drawLine(projected[12][0], projected[12][1], 
                projected[13][0], projected[13][1], featureColor);
  
  // Vertical guides
  gfx->drawLine(projected[14][0], projected[14][1], 
                projected[15][0], projected[15][1], 
                gfx->color565(80, 80, 255));
  
  // Draw all vertices as points
  for (int i = 0; i < 16; i++) {
    uint16_t pointColor = (i < 8) ? outlineColor : featureColor;
    gfx->fillCircle(projected[i][0], projected[i][1], 1, pointColor);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextSize(2);
  gfx->printf("HEAD 3D");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->setTextSize(1);
  gfx->printf("Rotation: %.0f°", rotY * 57.3);
}

void drawHelix() {
  gfx->fillScreen(gfx->color565(5, 10, 25));
  
  float centerX = gfx->width() / 2;
  float centerY = gfx->height() / 2;
  float radius = 60;
  
  // Draw helix strands
  for (int strand = 0; strand < 2; strand++) {
    uint16_t color = strand == 0 ? gfx->color565(255, 100, 100) : gfx->color565(100, 100, 255);
    
    for (int i = 0; i < 100; i++) {
      float t = i * 0.1 + animationTime * 2;
      float angle = t + strand * PI;
      
      float x = centerX + cos(angle) * radius;
      float y = centerY + sin(t * 0.3) * 80 - 40 + i * 2;
      
      if (y >= 0 && y < gfx->height()) {
        gfx->drawPixel(x, y, color);
        if (i > 0) {
          float prevT = (i-1) * 0.1 + animationTime * 2;
          float prevAngle = prevT + strand * PI;
          float prevX = centerX + cos(prevAngle) * radius;
          float prevY = centerY + sin(prevT * 0.3) * 80 - 40 + (i-1) * 2;
          
          if (prevY >= 0 && prevY < gfx->height()) {
            gfx->drawLine(prevX, prevY, x, y, color);
          }
        }
      }
    }
  }
  
  gfx->setTextColor(gfx->color565(255, 255, 255), gfx->color565(0, 0, 0));
  gfx->setCursor(10, 10);
  gfx->setTextSize(1);
  gfx->printf("HELIX");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("DNA Strand %.1f", animationTime);
}

void drawHexadrop() {
  gfx->fillScreen(gfx->color565(0, 5, 15));
  
  static float drops[20];
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 20; i++) {
      drops[i] = random(gfx->height());
    }
    initialized = true;
  }
  
  // Update and draw hexagonal drops
  for (int i = 0; i < 20; i++) {
    drops[i] += 2 + i * 0.1;
    if (drops[i] > gfx->height() + 20) {
      drops[i] = -20;
    }
    
    float x = (i * 15) % gfx->width();
    float y = drops[i];
    
    uint16_t color = gfx->color565(50 + i * 10, 100 + i * 5, 255 - i * 5);
    
    // Draw hexagon
    float radius = 8;
    for (int j = 0; j < 6; j++) {
      float angle1 = j * PI / 3;
      float angle2 = (j + 1) * PI / 3;
      
      float x1 = x + cos(angle1) * radius;
      float y1 = y + sin(angle1) * radius;
      float x2 = x + cos(angle2) * radius;
      float y2 = y + sin(angle2) * radius;
      
      if (y1 >= 0 && y1 < gfx->height() && y2 >= 0 && y2 < gfx->height()) {
        gfx->drawLine(x1, y1, x2, y2, color);
      }
    }
  }
  
  gfx->setTextColor(gfx->color565(255, 255, 255), gfx->color565(0, 0, 0));
  gfx->setCursor(10, 10);
  gfx->setTextSize(1);
  gfx->printf("HEXAdrop");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Drops: 20");
}

void drawHexstrut() {
  gfx->fillScreen(gfx->color565(10, 5, 20));
  
  float centerX = gfx->width() / 2;
  float centerY = gfx->height() / 2;
  
  // Draw hexagonal structure
  for (int layer = 1; layer < 5; layer++) {
    float radius = layer * 25;
    uint16_t color = gfx->color565(255 - layer * 30, 100 + layer * 30, 200 - layer * 20);
    
    // Rotate the structure
    float rotation = animationTime * 0.5 + layer * 0.3;
    
    for (int i = 0; i < 6; i++) {
      float angle1 = i * PI / 3 + rotation;
      float angle2 = (i + 1) * PI / 3 + rotation;
      
      float x1 = centerX + cos(angle1) * radius;
      float y1 = centerY + sin(angle1) * radius;
      float x2 = centerX + cos(angle2) * radius;
      float y2 = centerY + sin(angle2) * radius;
      
      gfx->drawLine(x1, y1, x2, y2, color);
      
      // Connect to inner layer
      if (layer > 1) {
        float innerRadius = (layer - 1) * 25;
        float innerX = centerX + cos(angle1) * innerRadius;
        float innerY = centerY + sin(angle1) * innerRadius;
        gfx->drawLine(x1, y1, innerX, innerY, color);
      }
    }
  }
  
  gfx->setTextColor(gfx->color565(255, 255, 255), gfx->color565(0, 0, 0));
  gfx->setCursor(10, 10);
  gfx->setTextSize(1);
  gfx->printf("HEXstrut");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Layers: 4");
}

void drawHextrail() {
  static float time = 0;
  time += 0.03;
  
  gfx->fillScreen(0x0000);
  
  // Hexagonal trail effect - flowing hex patterns
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int i = 0; i < 8; i++) {
    float angle = time + i * 0.8;
    int x = centerX + cos(angle) * (30 + i * 15);
    int y = centerY + sin(angle) * (20 + i * 10);
    
    // Draw hexagon
    uint16_t color = gfx->color565(
      (int)(128 + 127 * sin(time + i * 0.5)),
      (int)(128 + 127 * cos(time + i * 0.7)),
      (int)(128 + 127 * sin(time + i * 0.9))
    );
    
    // Simple hexagon approximation
    for (int j = 0; j < 6; j++) {
      float a1 = j * PI / 3;
      float a2 = (j + 1) * PI / 3;
      int x1 = x + cos(a1) * 8;
      int y1 = y + sin(a1) * 8;
      int x2 = x + cos(a2) * 8;
      int y2 = y + sin(a2) * 8;
      gfx->drawLine(x1, y1, x2, y2, color);
    }
  }
  
  gfx->setTextColor(0xFFFF);
  gfx->setCursor(10, 10);
  gfx->printf("HEXTRAIL");
}

void drawHighvoltage() {
  static float electricTime = 0;
  electricTime += 0.05;
  
  gfx->fillScreen(0x0000);
  
  // Electric arc simulation
  int numArcs = 6;
  
  for (int i = 0; i < numArcs; i++) {
    float phase = electricTime + i * 1.2;
    
    // Electric arc from left to right
    int startX = 20;
    int endX = gfx->width() - 20;
    int baseY = 50 + i * 30;
    
    // Draw jagged electric arc
    int segments = 15;
    int lastX = startX;
    int lastY = baseY;
    
    uint16_t arcColor = gfx->color565(
      255,
      (int)(200 + 55 * sin(phase * 2)),
      (int)(150 + 105 * cos(phase * 1.5))
    );
    
    for (int seg = 1; seg <= segments; seg++) {
      int x = startX + (endX - startX) * seg / segments;
      int y = baseY + (int)(sin(phase + seg * 0.5) * 15 * sin(electricTime * 3));
      
      // Add random jaggedness
      y += (int)(random(-8, 8) * sin(electricTime * 5 + i));
      
      gfx->drawLine(lastX, lastY, x, y, arcColor);
      lastX = x;
      lastY = y;
    }
  }
  
  gfx->setTextColor(0xFFFF);
  gfx->setCursor(10, 10);
  gfx->printf("HIGH VOLTAGE");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Danger: %.0fkV", 50 + 30 * sin(electricTime * 2));
}

void drawHighvoltageModel() {
  static float modelTime = 0;
  modelTime += 0.04;
  
  gfx->fillScreen(0x0011);
  
  // 3D electrical tower model simulation
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Draw power tower structure
  uint16_t towerColor = gfx->color565(100, 100, 150);
  
  // Main vertical supports
  gfx->drawLine(centerX - 30, centerY + 50, centerX - 20, centerY - 60, towerColor);
  gfx->drawLine(centerX + 30, centerY + 50, centerX + 20, centerY - 60, towerColor);
  gfx->drawLine(centerX - 20, centerY - 60, centerX + 20, centerY - 60, towerColor);
  
  // Cross beams
  for (int i = 0; i < 4; i++) {
    int y = centerY + 40 - i * 25;
    gfx->drawLine(centerX - 25 + i * 3, y, centerX + 25 - i * 3, y, towerColor);
  }
  
  // Electrical effects on power lines
  for (int line = 0; line < 3; line++) {
    int lineY = centerY - 40 + line * 15;
    
    // Power line
    gfx->drawLine(0, lineY, gfx->width(), lineY, 0x39E7);
    
    // Electrical sparks
    for (int spark = 0; spark < 5; spark++) {
      if (sin(modelTime * 4 + line + spark) > 0.7) {
        int sparkX = 30 + spark * 50;
        int sparkY = lineY + (int)(sin(modelTime * 6 + spark) * 8);
        
        uint16_t sparkColor = gfx->color565(255, 255, 100);
        gfx->drawCircle(sparkX, sparkY, 2, sparkColor);
        
        // Lightning bolt
        gfx->drawLine(sparkX, sparkY - 5, sparkX + 3, sparkY + 5, sparkColor);
        gfx->drawLine(sparkX + 3, sparkY + 5, sparkX - 2, sparkY + 10, sparkColor);
      }
    }
  }
  
  gfx->setTextColor(0xFFFF);
  gfx->setCursor(10, 10);
  gfx->printf("HV TOWER");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Output: %.1fMW", 500 + 200 * sin(modelTime));
}

void drawHilbert() {
  // Hilbert space-filling curve
  gfx->fillScreen(BLACK);
  
  static int order = 4;
  static float curveTime = 0;
  curveTime += 0.02;
  
  // Simple Hilbert curve approximation with animated colors
  int size = min(gfx->width(), gfx->height()) / (1 << order);
  int offsetX = (gfx->width() - (size << order)) / 2;
  int offsetY = (gfx->height() - (size << order)) / 2;
  
  for (int i = 0; i < (1 << (order * 2)); i++) {
    int x, y;
    // Simplified Hilbert coordinate calculation
    int t = i;
    int s = 1;
    x = y = 0;
    while (s < (1 << order)) {
      int rx = 1 & (t / 2);
      int ry = 1 & (t ^ rx);
      if (ry == 0) {
        if (rx == 1) {
          x = s - 1 - x;
          y = s - 1 - y;
        }
        int temp = x;
        x = y;
        y = temp;
      }
      x += s * rx;
      y += s * ry;
      t /= 4;
      s *= 2;
    }
    
    float phase = curveTime + i * 0.01;
    uint16_t color = gfx->color565(
      (int)(sin(phase) * 127 + 128),
      (int)(cos(phase + 1) * 127 + 128),
      (int)(sin(phase + 2) * 127 + 128)
    );
    
    gfx->fillRect(offsetX + x * size, offsetY + y * size, size, size, color);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("HILBERT");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Order: %d", order);
}

void drawHopalong() {
  // Hopalong attractor
  gfx->fillScreen(BLACK);
  
  static float x = 0, y = 0;
  static float a = 7.7, b = 0.4, c = 1.0;
  static float paramTime = 0;
  paramTime += 0.005;
  
  // Slowly vary parameters
  a = 7.7 + sin(paramTime) * 2.0;
  b = 0.4 + cos(paramTime * 1.3) * 0.2;
  
  for (int i = 0; i < 500; i++) {
    float newX = y - (x < 0 ? -1 : 1) * sqrt(abs(b * x - c));
    float newY = a - x;
    
    x = newX;
    y = newY;
    
    int px = (int)(x * 15 + gfx->width() / 2);
    int py = (int)(y * 15 + gfx->height() / 2);
    
    if (px >= 0 && px < gfx->width() && py >= 0 && py < gfx->height()) {
      float phase = i * 0.02 + paramTime * 5;
      uint16_t color = gfx->color565(
        (int)(sin(phase) * 127 + 128),
        (int)(cos(phase + 1) * 127 + 128),
        (int)(sin(phase + 2) * 127 + 128)
      );
      gfx->drawPixel(px, py, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("HOPALONG");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("a=%.1f b=%.1f", a, b);
}

void drawHopfanimations() {
  // Hopf fibration visualization
  gfx->fillScreen(BLACK);
  
  static float hopfTime = 0;
  hopfTime += 0.03;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  int radius = min(centerX, centerY) - 20;
  
  // Draw Hopf circles
  for (int fiber = 0; fiber < 12; fiber++) {
    float fiberAngle = fiber * PI * 2 / 12;
    float phase = hopfTime + fiberAngle;
    
    // Hopf fibration parameters
    float u = cos(phase);
    float v = sin(phase);
    
    for (int t = 0; t < 36; t++) {
      float theta = t * PI * 2 / 36;
      
      // Stereographic projection from 3-sphere to 2-sphere
      float x = 2 * u * cos(theta) / (1 + u*u + v*v);
      float y = 2 * v * sin(theta) / (1 + u*u + v*v);
      
      int px = centerX + (int)(x * radius);
      int py = centerY + (int)(y * radius);
      
      if (px >= 0 && px < gfx->width() && py >= 0 && py < gfx->height()) {
        uint16_t color = gfx->color565(
          (int)(sin(fiberAngle + hopfTime) * 127 + 128),
          (int)(cos(fiberAngle + hopfTime + 1) * 127 + 128),
          (int)(sin(fiberAngle + hopfTime + 2) * 127 + 128)
        );
        gfx->drawPixel(px, py, color);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("HOPF");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Fibration");
}

void drawHopffibration() {
  static float hopfTime = 0;
  hopfTime += 0.03;
  
  gfx->fillScreen(BLACK);
  
  // Hopf fibration visualization with mathematical mapping
  for (int i = 0; i < 120; i++) {
    float theta = (i / 120.0) * 2 * PI + hopfTime;
    float phi = sin(hopfTime * 0.7 + i * 0.1) * PI;
    
    // Stereographic projection from S3 to R3
    float x = sin(phi) * cos(theta);
    float y = sin(phi) * sin(theta);
    float z = cos(phi);
    
    // Project to screen coordinates
    int screenX = gfx->width()/2 + (int)(x * 80);
    int screenY = gfx->height()/2 + (int)(y * 80);
    
    uint16_t color = gfx->color565(
      (sin(theta + hopfTime) * 127 + 128),
      (cos(phi + hopfTime * 0.8) * 127 + 128),
      (sin(theta * 2 + phi) * 127 + 128)
    );
    
    if (screenX >= 0 && screenX < gfx->width() && screenY >= 0 && screenY < gfx->height()) {
      gfx->fillCircle(screenX, screenY, 2, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("HOPF Fibration");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("S3 -> S2 Projection");
}

void drawHsv() {
  static float hsvTime = 0;
  hsvTime += 0.02;
  
  gfx->fillScreen(BLACK);
  
  // HSV color space visualization
  for (int y = 0; y < gfx->height(); y += 4) {
    for (int x = 0; x < gfx->width(); x += 4) {
      // Map screen coordinates to HSV space
      float hue = (x / (float)gfx->width()) * 360 + hsvTime * 30;
      float sat = 1.0;
      float val = (y / (float)gfx->height()) * 0.8 + 0.2;
      
      // Add animation
      val += sin(hsvTime * 2 + x * 0.02 + y * 0.03) * 0.2;
      val = constrain(val, 0, 1);
      
      // Convert HSV to RGB
      float c = val * sat;
      float h_prime = fmod(hue / 60.0, 6);
      float x_comp = c * (1 - abs(fmod(h_prime, 2) - 1));
      float m = val - c;
      
      float r, g, b;
      if (h_prime < 1) { r = c; g = x_comp; b = 0; }
      else if (h_prime < 2) { r = x_comp; g = c; b = 0; }
      else if (h_prime < 3) { r = 0; g = c; b = x_comp; }
      else if (h_prime < 4) { r = 0; g = x_comp; b = c; }
      else if (h_prime < 5) { r = x_comp; g = 0; b = c; }
      else { r = c; g = 0; b = x_comp; }
      
      uint8_t red = (r + m) * 255;
      uint8_t green = (g + m) * 255;
      uint8_t blue = (b + m) * 255;
      
      gfx->fillRect(x, y, 4, 4, gfx->color565(red, green, blue));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("HSV Color Space");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Hue: %.0f°", fmod(hsvTime * 30, 360));
}

void drawHydrostat() {
  static float hydroTime = 0;
  hydroTime += 0.025;
  
  gfx->fillScreen(BLACK);
  
  // Hydrostatic pressure visualization with fluid dynamics
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int y = 0; y < gfx->height(); y += 2) {
    for (int x = 0; x < gfx->width(); x += 2) {
      float dx = x - centerX;
      float dy = y - centerY;
      float dist = sqrt(dx*dx + dy*dy);
      
      // Fluid pressure field
      float pressure = sin(hydroTime * 2) * cos(dist * 0.1 + hydroTime);
      pressure += sin(hydroTime * 1.5 + dx * 0.05) * cos(hydroTime * 0.8 + dy * 0.05);
      
      // Depth-based coloring (blue to green gradient)
      float depth = (y / (float)gfx->height()) + pressure * 0.3;
      depth = constrain(depth, 0, 1);
      
      uint8_t blue = 255 * (1 - depth * 0.7);
      uint8_t green = 180 * depth;
      uint8_t red = 50 * sin(hydroTime + dist * 0.1);
      
      // Add bubbles effect
      if (sin(hydroTime * 3 + x * 0.1 + y * 0.15) > 0.95) {
        red = green = blue = 255;
      }
      
      gfx->drawPixel(x, y, gfx->color565(red, green, blue));
    }
  }
  
  // Draw surface waves
  for (int x = 0; x < gfx->width(); x += 3) {
    int waveY = 20 + sin(hydroTime * 2 + x * 0.1) * 10;
    gfx->drawPixel(x, waveY, WHITE);
    gfx->drawPixel(x, waveY + 1, WHITE);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("HYDROSTAT");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Pressure: %.1f", sin(hydroTime * 2) * 50 + 50);
}

void drawHyperball() {
  static float hyperTime = 0;
  hyperTime += 0.05;
  
  gfx->fillScreen(BLACK);
  
  // 4D hypersphere projected to 3D then to 2D
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int i = 0; i < 60; i++) {
    float theta = i * PI / 30 + hyperTime;
    float phi = sin(hyperTime * 0.7 + i * 0.1) * PI;
    float psi = hyperTime * 0.5 + i * 0.15;
    
    // 4D coordinates
    float x4 = sin(phi) * cos(theta) * cos(psi);
    float y4 = sin(phi) * sin(theta) * cos(psi);
    float z4 = cos(phi) * cos(psi);
    float w4 = sin(psi);
    
    // Project from 4D to 3D (stereographic)
    float x3 = x4 / (1 - w4 + 0.1);
    float y3 = y4 / (1 - w4 + 0.1);
    float z3 = z4 / (1 - w4 + 0.1);
    
    // Project from 3D to 2D
    int screenX = centerX + (int)(x3 * 80);
    int screenY = centerY + (int)(y3 * 80);
    
    uint16_t color = gfx->color565(
      (sin(theta + hyperTime) * 127 + 128),
      (cos(phi + hyperTime * 0.8) * 127 + 128),
      (sin(psi + hyperTime * 1.2) * 127 + 128)
    );
    
    if (screenX >= 0 && screenX < gfx->width() && screenY >= 0 && screenY < gfx->height()) {
      gfx->fillCircle(screenX, screenY, 3 + sin(hyperTime + i) * 2, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("HYPERBALL");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("4D Hypersphere");
}

void drawHypercube() {
  static float cubeTime = 0;
  cubeTime += 0.03;
  
  gfx->fillScreen(BLACK);
  
  // 4D hypercube (tesseract) visualization
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // 16 vertices of tesseract
  float vertices[16][4] = {
    {-1,-1,-1,-1}, {1,-1,-1,-1}, {-1,1,-1,-1}, {1,1,-1,-1},
    {-1,-1,1,-1}, {1,-1,1,-1}, {-1,1,1,-1}, {1,1,1,-1},
    {-1,-1,-1,1}, {1,-1,-1,1}, {-1,1,-1,1}, {1,1,-1,1},
    {-1,-1,1,1}, {1,-1,1,1}, {-1,1,1,1}, {1,1,1,1}
  };
  
  int projected[16][2];
  
  // Rotate and project vertices
  for (int i = 0; i < 16; i++) {
    // 4D rotation in XW and YZ planes
    float x = vertices[i][0] * cos(cubeTime) - vertices[i][3] * sin(cubeTime);
    float y = vertices[i][1] * cos(cubeTime * 0.7) - vertices[i][2] * sin(cubeTime * 0.7);
    float z = vertices[i][1] * sin(cubeTime * 0.7) + vertices[i][2] * cos(cubeTime * 0.7);
    float w = vertices[i][0] * sin(cubeTime) + vertices[i][3] * cos(cubeTime);
    
    // Project 4D to 3D
    float scale = 1.0 / (2 - w);
    float x3 = x * scale;
    float y3 = y * scale;
    float z3 = z * scale;
    
    // Project 3D to 2D
    projected[i][0] = centerX + (int)(x3 * 60);
    projected[i][1] = centerY + (int)(y3 * 60);
  }
  
  // Draw tesseract edges
  uint16_t edgeColor = gfx->color565(100, 200, 255);
  
  // Connect vertices that differ by one coordinate
  for (int i = 0; i < 16; i++) {
    for (int j = i + 1; j < 16; j++) {
      int diff = 0;
      for (int k = 0; k < 4; k++) {
        if (vertices[i][k] != vertices[j][k]) diff++;
      }
      
      if (diff == 1) {
        gfx->drawLine(projected[i][0], projected[i][1], 
                      projected[j][0], projected[j][1], edgeColor);
      }
    }
  }
  
  // Draw vertices
  for (int i = 0; i < 16; i++) {
    uint16_t vertexColor = gfx->color565(255, 150 + i * 6, 100);
    gfx->fillCircle(projected[i][0], projected[i][1], 2, vertexColor);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("HYPERCUBE");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("4D Tesseract");
}

void drawHypertorus() {
  static float torusTime = 0;
  torusTime += 0.04;
  
  gfx->fillScreen(BLACK);
  
  // 4D hypertorus visualization
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  float R = 30; // Major radius
  float r = 15; // Minor radius
  
  for (int u = 0; u < 24; u++) {
    for (int v = 0; v < 16; v++) {
      float theta = u * 2 * PI / 24 + torusTime;
      float phi = v * 2 * PI / 16 + torusTime * 0.7;
      
      // 4D torus parametrization
      float x = (R + r * cos(phi)) * cos(theta);
      float y = (R + r * cos(phi)) * sin(theta);
      float z = r * sin(phi);
      float w = r * sin(phi + torusTime * 0.5);
      
      // 4D to 3D projection
      float scale = 1.0 / (2 - w * 0.1);
      float x3 = x * scale;
      float y3 = y * scale;
      float z3 = z * scale;
      
      // 3D rotation for better view
      float rotY = torusTime * 0.3;
      float xr = x3 * cos(rotY) + z3 * sin(rotY);
      float yr = y3;
      
      int screenX = centerX + (int)xr;
      int screenY = centerY + (int)yr;
      
      if (screenX >= 0 && screenX < gfx->width() && screenY >= 0 && screenY < gfx->height()) {
        uint16_t color = gfx->color565(
          (sin(theta + torusTime) * 127 + 128),
          (cos(phi + torusTime * 0.8) * 127 + 128),
          (sin(theta + phi + torusTime * 1.3) * 127 + 128)
        );
        gfx->drawPixel(screenX, screenY, color);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("HYPERTORUS");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("4D Torus");
}

void drawHypnowheel() {
  static float hypnoTime = 0;
  hypnoTime += 0.03;
  
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Hypnotic rotating wheel patterns
  for (int ring = 0; ring < 8; ring++) {
    int radius = 20 + ring * 15;
    int spokes = 8 + ring * 2;
    
    for (int i = 0; i < spokes; i++) {
      float angle = (float)i / spokes * 2 * PI + hypnoTime * (1 + ring * 0.2);
      
      int x1 = centerX + cos(angle) * (radius - 10);
      int y1 = centerY + sin(angle) * (radius - 10);
      int x2 = centerX + cos(angle) * radius;
      int y2 = centerY + sin(angle) * radius;
      
      uint16_t color = gfx->color565(
        (sin(hypnoTime * 2 + ring * 0.5) * 0.5 + 0.5) * 255,
        (cos(hypnoTime * 1.7 + i * 0.3) * 0.5 + 0.5) * 255,
        (sin(hypnoTime * 1.3 + ring * 0.7) * 0.5 + 0.5) * 255
      );
      
      gfx->drawLine(x1, y1, x2, y2, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(CYAN);
  gfx->printf("HYPNOWHEEL");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Hypnotic: %.1f", hypnoTime);
}

void drawIdleProtocol() {
  static float idleTime = 0;
  idleTime += 0.01;
  
  gfx->fillScreen(BLACK);
  gfx->setTextColor(GREEN);
  
  // Simulate system idle activity monitoring
  for (int i = 0; i < gfx->width(); i += 4) {
    float activity = sin(idleTime * 3 + i * 0.02) * 0.5 + 0.5;
    int barHeight = activity * (gfx->height() / 3);
    
    uint16_t color = gfx->color565(
      activity * 100,
      255 - activity * 155,
      activity * 200
    );
    
    gfx->fillRect(i, gfx->height() - barHeight, 3, barHeight, color);
  }
  
  // Overlay text
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("IDLE PROTOCOL");
  
  gfx->setCursor(10, 30);
  gfx->printf("System Monitor");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Activity: %.1f%%", (sin(idleTime * 2) * 0.5 + 0.5) * 100);
}

void drawIfs() {
  static float ifsTime = 0;
  ifsTime += 0.02;
  
  gfx->fillScreen(BLACK);
  gfx->setTextColor(MAGENTA);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Iterated Function System fractal
  float x = 0.5, y = 0.5;
  
  for (int i = 0; i < 1000; i++) {
    // Simple IFS transformation
    int choice = random(3);
    float newX, newY;
    
    if (choice == 0) {
      newX = 0.5 * x;
      newY = 0.5 * y;
    } else if (choice == 1) {
      newX = 0.5 * x + 0.5;
      newY = 0.5 * y + 0.5;
    } else {
      newX = 0.5 * x + 0.25;
      newY = 0.5 * y + sin(ifsTime) * 0.1;
    }
    
    x = newX;
    y = newY;
    
    int screenX = centerX + (x - 0.5) * gfx->width() * 0.8;
    int screenY = centerY + (y - 0.5) * gfx->height() * 0.8;
    
    if (screenX >= 0 && screenX < gfx->width() && screenY >= 0 && screenY < gfx->height()) {
      uint16_t color = gfx->color565(
        (sin(ifsTime + x * 5) * 0.5 + 0.5) * 255,
        (cos(ifsTime + y * 5) * 0.5 + 0.5) * 255,
        (sin(ifsTime * 1.3 + (x + y) * 3) * 0.5 + 0.5) * 255
      );
      gfx->drawPixel(screenX, screenY, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("IFS");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Fractal System");
}

void drawImsmap() {
  static float mapTime = 0;
  mapTime += 0.02;
  
  gfx->fillScreen(BLACK);
  gfx->setTextColor(YELLOW);
  
  // Image mapping simulation with distortion
  for (int y = 0; y < gfx->height(); y += 2) {
    for (int x = 0; x < gfx->width(); x += 2) {
      float u = (float)x / gfx->width();
      float v = (float)y / gfx->height();
      
      // Apply mapping distortion
      float distortX = u + sin(mapTime + v * 8) * 0.1;
      float distortY = v + cos(mapTime + u * 6) * 0.1;
      
      uint16_t color = gfx->color565(
        (sin(distortX * 10 + mapTime) * 0.5 + 0.5) * 255,
        (cos(distortY * 8 + mapTime * 1.2) * 0.5 + 0.5) * 255,
        (sin((distortX + distortY) * 6 + mapTime * 0.8) * 0.5 + 0.5) * 255
      );
      
      gfx->drawPixel(x, y, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("IMSMAP");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Image Mapping");
}

void drawInteraggregate() {
  static float aggTime = 0;
  aggTime += 0.03;
  
  gfx->fillScreen(BLACK);
  gfx->setTextColor(CYAN);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Particle aggregation simulation
  for (int cluster = 0; cluster < 5; cluster++) {
    float clusterX = centerX + cos(aggTime * 0.5 + cluster * 1.3) * 60;
    float clusterY = centerY + sin(aggTime * 0.7 + cluster * 1.1) * 40;
    
    for (int particle = 0; particle < 20; particle++) {
      float angle = (float)particle / 20 * 2 * PI + aggTime * (1 + cluster * 0.2);
      float radius = 5 + sin(aggTime * 2 + particle * 0.3) * 15;
      
      int x = clusterX + cos(angle) * radius;
      int y = clusterY + sin(angle) * radius;
      
      if (x >= 0 && x < gfx->width() && y >= 0 && y < gfx->height()) {
        uint16_t color = gfx->color565(
          (sin(aggTime + cluster * 0.8) * 0.5 + 0.5) * 255,
          (cos(aggTime * 1.2 + particle * 0.2) * 0.5 + 0.5) * 255,
          (sin(aggTime * 0.9 + cluster + particle * 0.1) * 0.5 + 0.5) * 255
        );
        gfx->fillCircle(x, y, 2, color);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("INTERAGGREGATE");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Particles: %.1f", aggTime);
}

void drawInterference() {
  static float intTime = 0;
  intTime += 0.04;
  
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Wave interference patterns
  for (int y = 0; y < gfx->height(); y += 2) {
    for (int x = 0; x < gfx->width(); x += 2) {
      float dist1 = sqrt(pow(x - centerX + 40, 2) + pow(y - centerY, 2));
      float dist2 = sqrt(pow(x - centerX - 40, 2) + pow(y - centerY, 2));
      
      float wave1 = sin(dist1 * 0.2 - intTime * 3);
      float wave2 = sin(dist2 * 0.2 - intTime * 3);
      float interference = (wave1 + wave2) * 0.5;
      
      uint16_t intensity = (interference * 0.5 + 0.5) * 255;
      uint16_t color = gfx->color565(
        intensity,
        intensity * 0.7,
        intensity * 0.9
      );
      
      gfx->drawPixel(x, y, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(YELLOW);
  gfx->printf("INTERFERENCE");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Wave Pattern");
}

void drawIntermomentary() {
  static float interTime = 0;
  interTime += 0.05;
  
  gfx->fillScreen(BLACK);
  
  // Momentary interference patterns
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int i = 0; i < 200; i++) {
    float phase = interTime + i * 0.1;
    float x = centerX + sin(phase) * (50 + cos(phase * 2) * 30);
    float y = centerY + cos(phase) * (40 + sin(phase * 1.5) * 25);
    
    if (x >= 0 && x < gfx->width() && y >= 0 && y < gfx->height()) {
      int intensity = 128 + sin(phase * 3) * 127;
      uint16_t color = gfx->color565(intensity, intensity / 2, intensity / 4);
      gfx->drawPixel(x, y, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(ORANGE);
  gfx->printf("INTERMOMENTARY");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Moment Flash");
}

void drawInvolute() {
  static float invTime = 0;
  invTime += 0.03;
  
  gfx->fillScreen(BLACK);
  
  // Mathematical involute curves
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (float t = 0; t < 6.28 * 4; t += 0.05) {
    float radius = t + invTime;
    float x = centerX + (cos(t) + t * sin(t)) * 3;
    float y = centerY + (sin(t) - t * cos(t)) * 3;
    
    if (x >= 0 && x < gfx->width() && y >= 0 && y < gfx->height()) {
      int hue = (int)(t * 50 + invTime * 100) % 360;
      uint16_t color = gfx->color565(
        128 + sin(hue * PI / 180) * 127,
        128 + sin((hue + 120) * PI / 180) * 127,
        128 + sin((hue + 240) * PI / 180) * 127
      );
      gfx->drawPixel(x, y, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(CYAN);
  gfx->printf("INVOLUTE");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Math Curves");
}

void drawJigglypuff() {
  static float jiggleTime = 0;
  jiggleTime += 0.08;
  
  gfx->fillScreen(BLACK);
  
  // Bouncing pink spheres
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int puff = 0; puff < 5; puff++) {
    float angle = puff * 2.0 * PI / 5 + jiggleTime;
    float bounce = sin(jiggleTime * 3 + puff) * 20;
    float x = centerX + cos(angle) * (60 + bounce);
    float y = centerY + sin(angle) * (40 + bounce);
    
    // Draw jiggly sphere
    for (int r = 15; r > 0; r--) {
      float wobble = sin(jiggleTime * 4 + r * 0.3) * 2;
      int intensity = 255 - r * 8;
      uint16_t color = gfx->color565(intensity, intensity / 3, intensity / 2);
      
      for (int i = 0; i < 20; i++) {
        float a = i * PI / 10;
        int px = x + (cos(a) * (r + wobble));
        int py = y + (sin(a) * (r + wobble));
        if (px >= 0 && px < gfx->width() && py >= 0 && py < gfx->height()) {
          gfx->drawPixel(px, py, color);
        }
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(MAGENTA);
  gfx->printf("JIGGLYPUFF");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Bounce: %.0f%%", sin(jiggleTime * 2) * 50 + 50);
}

void drawJigsaw() {
  static float jigsawTime = 0;
  jigsawTime += 0.03;
  
  gfx->fillScreen(BLACK);
  
  // Create jigsaw puzzle pieces
  int pieceW = gfx->width() / 8;
  int pieceH = gfx->height() / 6;
  
  for (int px = 0; px < 8; px++) {
    for (int py = 0; py < 6; py++) {
      int baseX = px * pieceW;
      int baseY = py * pieceH;
      
      // Animated offset for pieces
      float offsetX = sin(jigsawTime + px * 0.5) * 3;
      float offsetY = cos(jigsawTime + py * 0.5) * 3;
      
      int x = baseX + offsetX;
      int y = baseY + offsetY;
      
      // Piece color based on position
      uint8_t r = ((px + py) * 40) % 255;
      uint8_t g = (px * 60) % 255;
      uint8_t b = (py * 80) % 255;
      uint16_t color = gfx->color565(r, g, b);
      
      // Draw puzzle piece shape
      gfx->fillRect(x + 2, y + 2, pieceW - 4, pieceH - 4, color);
      
      // Add connecting tabs
      if (px > 0) {
        int tabY = y + pieceH / 2 - 3;
        gfx->fillRect(x - 2, tabY, 4, 6, color);
      }
      if (py > 0) {
        int tabX = x + pieceW / 2 - 3;
        gfx->fillRect(tabX, y - 2, 6, 4, color);
      }
      
      // Draw piece outline
      gfx->drawRect(x + 2, y + 2, pieceW - 4, pieceH - 4, WHITE);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(CYAN);
  gfx->printf("JIGSAW");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Pieces: %d", 8 * 6);
}

void drawJuggle() {
  static float juggleTime = 0;
  juggleTime += 0.04;
  
  gfx->fillScreen(BLACK);
  
  // Juggler position
  int jugX = gfx->width() / 2;
  int jugY = gfx->height() - 30;
  
  // Draw juggler (simple stick figure)
  gfx->drawLine(jugX, jugY, jugX, jugY - 20, WHITE);
  gfx->fillCircle(jugX, jugY - 25, 3, WHITE);
  
  // Arms
  int armOffset = sin(juggleTime * 4) * 10;
  gfx->drawLine(jugX, jugY - 15, jugX - 15 + armOffset, jugY - 10, WHITE);
  gfx->drawLine(jugX, jugY - 15, jugX + 15 - armOffset, jugY - 10, WHITE);
  
  // Juggling balls
  for (int ball = 0; ball < 3; ball++) {
    float ballTime = juggleTime + ball * (2 * PI / 3);
    
    // Parabolic trajectory
    float t = fmod(ballTime, PI);
    float ballX = jugX + sin(ballTime * 2) * 40;
    float ballY = jugY - 40 - abs(sin(t) * 50);
    
    uint16_t ballColor = (ball == 0) ? RED : (ball == 1) ? GREEN : BLUE;
    gfx->fillCircle(ballX, ballY, 3, ballColor);
    
    // Ball trail
    for (int i = 1; i < 5; i++) {
      float prevT = t - i * 0.1;
      if (prevT > 0) {
        float prevX = jugX + sin((ballTime - i * 0.1) * 2) * 40;
        float prevY = jugY - 40 - abs(sin(prevT) * 50);
        gfx->drawPixel(prevX, prevY, gfx->color565(
          (ballColor >> 11) << 3,
          ((ballColor >> 5) & 0x3F) << 2,
          (ballColor & 0x1F) << 3
        ) >> (i * 2));
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(YELLOW);
  gfx->printf("JUGGLE");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Balls: 3");
}

void drawJuggler3d() {
  static float juggler3dTime = 0;
  juggler3dTime += 0.05;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // 3D juggling pattern - figure-8 in 3D space
  for (int ball = 0; ball < 4; ball++) {
    float ballTime = juggler3dTime + ball * (PI / 2);
    
    // 3D coordinates
    float x3d = sin(ballTime) * 60;
    float y3d = sin(ballTime * 2) * 40;
    float z3d = cos(ballTime) * 30;
    
    // Project to 2D
    float perspective = 200 / (200 + z3d);
    int screenX = centerX + (x3d * perspective);
    int screenY = centerY + (y3d * perspective);
    
    // Ball size based on depth
    int ballSize = 2 + (perspective * 3);
    
    // Color based on depth
    uint8_t intensity = 100 + (z3d + 30) * 2;
    uint16_t ballColor = gfx->color565(
      (ball == 0) ? intensity : intensity / 3,
      (ball == 1) ? intensity : intensity / 3,
      (ball == 2) ? intensity : intensity / 3
    );
    
    gfx->fillCircle(screenX, screenY, ballSize, ballColor);
    
    // Draw 3D trajectory lines
    for (int i = 1; i < 10; i++) {
      float prevTime = ballTime - i * 0.1;
      float prevX3d = sin(prevTime) * 60;
      float prevY3d = sin(prevTime * 2) * 40;
      float prevZ3d = cos(prevTime) * 30;
      
      float prevPerspective = 200 / (200 + prevZ3d);
      int prevScreenX = centerX + (prevX3d * prevPerspective);
      int prevScreenY = centerY + (prevY3d * prevPerspective);
      
      if (i == 1) {
        gfx->drawLine(screenX, screenY, prevScreenX, prevScreenY, 
                     gfx->color565(intensity/4, intensity/4, intensity/4));
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(MAGENTA);
  gfx->printf("JUGGLER3D");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("3D Pattern");
}

void drawRotor() {
  static float rotorTime = 0;
  rotorTime += 0.08;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Rotating rotor blades
  for (int blade = 0; blade < 4; blade++) {
    float angle = rotorTime * 3 + blade * PI / 2;
    
    // Main rotor blade
    int x1 = centerX + cos(angle) * 80;
    int y1 = centerY + sin(angle) * 80;
    int x2 = centerX + cos(angle + PI) * 20;
    int y2 = centerY + sin(angle + PI) * 20;
    
    gfx->drawLine(x1, y1, x2, y2, WHITE);
    gfx->drawLine(x1+1, y1, x2+1, y2, WHITE);
    
    // Blade tip circles
    gfx->fillCircle(x1, y1, 3, RED);
  }
  
  // Central hub
  gfx->fillCircle(centerX, centerY, 8, YELLOW);
  gfx->drawCircle(centerX, centerY, 12, WHITE);
  
  // Motion blur effect
  for (int i = 1; i < 5; i++) {
    float prevAngle = rotorTime * 3 - i * 0.1;
    for (int blade = 0; blade < 4; blade++) {
      float angle = prevAngle + blade * PI / 2;
      int x = centerX + cos(angle) * 80;
      int y = centerY + sin(angle) * 80;
      gfx->drawPixel(x, y, gfx->color565(60 >> i, 60 >> i, 60 >> i));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("ROTOR");
}

void drawRotzoomer() {
  static float rotzoomTime = 0;
  rotzoomTime += 0.06;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Rotating and zooming pattern
  float zoom = 1.0 + sin(rotzoomTime * 0.8) * 0.5;
  float rotation = rotzoomTime * 1.5;
  
  for (int y = 0; y < gfx->height(); y += 4) {
    for (int x = 0; x < gfx->width(); x += 4) {
      // Transform coordinates
      float dx = (x - centerX) / zoom;
      float dy = (y - centerY) / zoom;
      
      // Rotate
      float rx = dx * cos(rotation) - dy * sin(rotation);
      float ry = dx * sin(rotation) + dy * cos(rotation);
      
      // Create pattern
      int pattern = (int)(sin(rx * 0.1) * sin(ry * 0.1) * 128) + 128;
      
      if (pattern > 100) {
        uint16_t color = gfx->color565(pattern, pattern/2, 255-pattern);
        gfx->drawPixel(x, y, color);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(CYAN);
  gfx->printf("ROTZOOMER");
}

void drawRubikblocks() {
  static float rubikTime = 0;
  rubikTime += 0.05;
  
  gfx->fillScreen(BLACK);
  
  // 3x3 grid of rotating cubes
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      int cubeX = 40 + col * 80;
      int cubeY = 40 + row * 60;
      
      float rotation = rubikTime + (row * 3 + col) * 0.3;
      int size = 25 + sin(rubikTime + row + col) * 5;
      
      // Cube faces with Rubik's colors
      uint16_t colors[6] = {RED, GREEN, BLUE, YELLOW, MAGENTA, CYAN};
      int colorIndex = (int)(rubikTime + row + col) % 6;
      
      // Draw cube with perspective
      int x1 = cubeX - size/2;
      int y1 = cubeY - size/2;
      int x2 = cubeX + size/2;
      int y2 = cubeY + size/2;
      
      // Front face
      gfx->fillRect(x1, y1, size, size, colors[colorIndex]);
      gfx->drawRect(x1, y1, size, size, WHITE);
      
      // Top face (isometric)
      int offset = size/4;
      gfx->fillTriangle(x1, y1, x2, y1, x2-offset, y1-offset, colors[(colorIndex+1)%6]);
      gfx->fillTriangle(x1, y1, x1+offset, y1-offset, x2-offset, y1-offset, colors[(colorIndex+1)%6]);
      
      // Right face
      gfx->fillTriangle(x2, y1, x2, y2, x2+offset, y2-offset, colors[(colorIndex+2)%6]);
      gfx->fillTriangle(x2, y1, x2+offset, y1-offset, x2+offset, y2-offset, colors[(colorIndex+2)%6]);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("RUBIKBLOCKS");
}

void drawRubik() {
  static float rubikCubeTime = 0;
  rubikCubeTime += 0.04;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Full Rubik's cube rotation
  float rotX = rubikCubeTime * 0.8;
  float rotY = rubikCubeTime * 1.2;
  
  // Draw 3x3x3 cube structure
  for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++) {
      for (int z = -1; z <= 1; z++) {
        // 3D rotation
        float px = x * cos(rotY) - z * sin(rotY);
        float pz = x * sin(rotY) + z * cos(rotY);
        float py = y * cos(rotX) - pz * sin(rotX);
        pz = y * sin(rotX) + pz * cos(rotX);
        
        // Perspective projection
        if (pz > -2) {
          int screenX = centerX + (px * 40) / (pz + 3);
          int screenY = centerY + (py * 40) / (pz + 3);
          
          // Color based on position
          uint16_t color = gfx->color565(
            (x + 1) * 127,
            (y + 1) * 127,
            (z + 1) * 127
          );
          
          int cubeSize = 15 / (pz + 3);
          gfx->fillRect(screenX - cubeSize/2, screenY - cubeSize/2, 
                       cubeSize, cubeSize, color);
          gfx->drawRect(screenX - cubeSize/2, screenY - cubeSize/2, 
                       cubeSize, cubeSize, WHITE);
        }
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(RED);
  gfx->printf("RUBIK");
}

void drawSballs() {
  static float sballsTime = 0;
  sballsTime += 0.07;
  
  gfx->fillScreen(BLACK);
  
  // Spinning balls in formation
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int ball = 0; ball < 8; ball++) {
    float angle = sballsTime + ball * PI / 4;
    float radius = 60 + sin(sballsTime * 2 + ball) * 20;
    
    int ballX = centerX + cos(angle) * radius;
    int ballY = centerY + sin(angle) * radius;
    
    // Ball with spin effect
    float spinAngle = sballsTime * 5 + ball;
    int ballSize = 8 + sin(spinAngle) * 3;
    
    uint16_t ballColor = gfx->color565(
      128 + sin(angle + sballsTime) * 127,
      128 + cos(angle + sballsTime * 1.3) * 127,
      128 + sin(angle * 2 + sballsTime * 0.7) * 127
    );
    
    gfx->fillCircle(ballX, ballY, ballSize, ballColor);
    
    // Highlight
    gfx->fillCircle(ballX - 2, ballY - 2, 2, WHITE);
    
    // Trail
    for (int t = 1; t < 6; t++) {
      float trailAngle = sballsTime - t * 0.1 + ball * PI / 4;
      float trailRadius = 60 + sin((sballsTime - t * 0.1) * 2 + ball) * 20;
      int trailX = centerX + cos(trailAngle) * trailRadius;
      int trailY = centerY + sin(trailAngle) * trailRadius;
      
      gfx->drawPixel(trailX, trailY, ballColor >> (t * 2));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(YELLOW);
  gfx->printf("SBALLS");
}

void drawScooter() {
  static float scooterTime = 0;
  scooterTime += 0.08;
  
  gfx->fillScreen(BLACK);
  
  int scooterX = 50 + sin(scooterTime * 0.5) * 100;
  int scooterY = gfx->height() / 2;
  
  // Scooter body
  gfx->fillRect(scooterX - 20, scooterY - 5, 40, 10, BLUE);
  
  // Wheels
  int wheelRadius = 8;
  float wheelSpin = scooterTime * 3;
  
  // Front wheel
  gfx->fillCircle(scooterX + 15, scooterY + 10, wheelRadius, WHITE);
  gfx->drawCircle(scooterX + 15, scooterY + 10, wheelRadius, BLACK);
  // Spokes
  for (int spoke = 0; spoke < 6; spoke++) {
    float spokeAngle = wheelSpin + spoke * PI / 3;
    int spokeX = scooterX + 15 + cos(spokeAngle) * (wheelRadius - 2);
    int spokeY = scooterY + 10 + sin(spokeAngle) * (wheelRadius - 2);
    gfx->drawLine(scooterX + 15, scooterY + 10, spokeX, spokeY, BLACK);
  }
  
  // Rear wheel
  gfx->fillCircle(scooterX - 15, scooterY + 10, wheelRadius, WHITE);
  gfx->drawCircle(scooterX - 15, scooterY + 10, wheelRadius, BLACK);
  for (int spoke = 0; spoke < 6; spoke++) {
    float spokeAngle = wheelSpin + spoke * PI / 3;
    int spokeX = scooterX - 15 + cos(spokeAngle) * (wheelRadius - 2);
    int spokeY = scooterY + 10 + sin(spokeAngle) * (wheelRadius - 2);
    gfx->drawLine(scooterX - 15, scooterY + 10, spokeX, spokeY, BLACK);
  }
  
  // Handle bars
  gfx->drawLine(scooterX + 10, scooterY - 5, scooterX + 10, scooterY - 20, WHITE);
  gfx->drawLine(scooterX + 5, scooterY - 20, scooterX + 15, scooterY - 20, WHITE);
  
  // Rider (stick figure)
  int riderX = scooterX - 5;
  int riderY = scooterY - 25;
  gfx->fillCircle(riderX, riderY, 4, YELLOW); // Head
  gfx->drawLine(riderX, riderY + 4, riderX, riderY + 20, WHITE); // Body
  gfx->drawLine(riderX, riderY + 10, riderX + 8, riderY - 15, WHITE); // Arms to handlebars
  gfx->drawLine(riderX, riderY + 20, riderX - 5, riderY + 35, WHITE); // Legs
  gfx->drawLine(riderX, riderY + 20, riderX + 10, riderY + 15, WHITE);
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(GREEN);
  gfx->printf("SCOOTER");
}

void drawSetuid() {
  static float setuidTime = 0;
  setuidTime += 0.06;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Security warning display
  int warningState = (int)(setuidTime * 2) % 4;
  
  if (warningState == 0 || warningState == 2) {
    // Warning box
    gfx->fillRect(centerX - 80, centerY - 60, 160, 120, RED);
    gfx->fillRect(centerX - 75, centerY - 55, 150, 110, BLACK);
    
    gfx->setTextColor(RED);
    gfx->setCursor(centerX - 35, centerY - 40);
    gfx->printf("SETUID");
    
    gfx->setCursor(centerX - 45, centerY - 20);
    gfx->printf("SECURITY");
    
    gfx->setCursor(centerX - 35, centerY);
    gfx->printf("WARNING");
    
    gfx->setCursor(centerX - 45, centerY + 20);
    gfx->printf("PRIVILEGE");
    
    gfx->setCursor(centerX - 35, centerY + 40);
    gfx->printf("ESCALATION");
  }
  
  // Blinking border
  if ((int)(setuidTime * 8) % 2) {
    gfx->drawRect(5, 5, gfx->width() - 10, gfx->height() - 10, YELLOW);
    gfx->drawRect(6, 6, gfx->width() - 12, gfx->height() - 12, YELLOW);
  }
  
  // User ID display
  gfx->setTextColor(WHITE);
  gfx->setCursor(10, 10);
  gfx->printf("UID: %d", (int)(setuidTime * 100) % 9999);
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("EUID: %d", (int)(setuidTime * 150) % 9999);
}

void drawShadebobs() {
  static float shadeBobTime = 0;
  shadeBobTime += 0.05;
  
  gfx->fillScreen(BLACK);
  
  // Multiple shadebobs (metaballs with shading)
  for (int bob = 0; bob < 6; bob++) {
    float bobAngle = shadeBobTime + bob * PI / 3;
    int bobX = gfx->width()/2 + cos(bobAngle) * 80;
    int bobY = gfx->height()/2 + sin(bobAngle * 1.3) * 60;
    
    int bobRadius = 25 + sin(shadeBobTime * 2 + bob) * 10;
    
    // Draw metaball with gradient shading
    for (int r = bobRadius; r > 0; r -= 2) {
      int intensity = (r * 255) / bobRadius;
      uint16_t color = gfx->color565(
        intensity * (bob % 3 == 0 ? 1 : 0),
        intensity * (bob % 3 == 1 ? 1 : 0),
        intensity * (bob % 3 == 2 ? 1 : 0)
      );
      
      gfx->drawCircle(bobX, bobY, r, color);
    }
  }
  
  // Connecting lines between bobs
  for (int i = 0; i < 6; i++) {
    for (int j = i + 1; j < 6; j++) {
      float angle1 = shadeBobTime + i * PI / 3;
      float angle2 = shadeBobTime + j * PI / 3;
      
      int x1 = gfx->width()/2 + cos(angle1) * 80;
      int y1 = gfx->height()/2 + sin(angle1 * 1.3) * 60;
      int x2 = gfx->width()/2 + cos(angle2) * 80;
      int y2 = gfx->height()/2 + sin(angle2 * 1.3) * 60;
      
      float distance = sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
      if (distance < 100) {
        uint16_t lineColor = gfx->color565(100, 100, 255 - distance * 2);
        gfx->drawLine(x1, y1, x2, y2, lineColor);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(CYAN);
  gfx->printf("SHADEBOBS");
}

void drawShark() {
  static float sharkTime = 0;
  sharkTime += 0.07;
  
  gfx->fillScreen(gfx->color565(0, 0, 100)); // Deep blue water
  
  int sharkX = 50 + sin(sharkTime * 0.8) * 120;
  int sharkY = gfx->height() / 2 + sin(sharkTime * 1.2) * 30;
  
  // Shark body
  gfx->fillEllipse(sharkX, sharkY, 40, 15, gfx->color565(100, 100, 100));
  
  // Shark head (triangle)
  gfx->fillTriangle(sharkX + 30, sharkY, 
                    sharkX + 50, sharkY - 8,
                    sharkX + 50, sharkY + 8, 
                    gfx->color565(120, 120, 120));
  
  // Fins
  gfx->fillTriangle(sharkX - 10, sharkY - 10,
                    sharkX + 10, sharkY - 5,
                    sharkX, sharkY - 25,
                    gfx->color565(80, 80, 80)); // Dorsal fin
  
  gfx->fillTriangle(sharkX - 20, sharkY + 5,
                    sharkX - 5, sharkY + 10,
                    sharkX - 15, sharkY + 20,
                    gfx->color565(80, 80, 80)); // Side fin
  
  // Tail
  gfx->fillTriangle(sharkX - 40, sharkY,
                    sharkX - 25, sharkY - 12,
                    sharkX - 25, sharkY + 12,
                    gfx->color565(90, 90, 90));
  
  // Eye
  gfx->fillCircle(sharkX + 20, sharkY - 5, 3, BLACK);
  gfx->fillCircle(sharkX + 21, sharkY - 6, 1, WHITE);
  
  // Teeth
  for (int tooth = 0; tooth < 5; tooth++) {
    int toothX = sharkX + 35 + tooth * 3;
    gfx->drawLine(toothX, sharkY - 2, toothX, sharkY + 2, WHITE);
  }
  
  // Bubbles
  for (int bubble = 0; bubble < 10; bubble++) {
    float bubbleTime = sharkTime + bubble * 0.5;
    int bubX = 20 + (int)(bubbleTime * 30) % (gfx->width() - 40);
    int bubY = 20 + (bubble * 25) % (gfx->height() - 40);
    
    int bubSize = 1 + (bubble % 3);
    gfx->drawCircle(bubX, bubY, bubSize, gfx->color565(150, 200, 255));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("SHARK");
}

void drawSlidescreen() {
  static float slideTime = 0;
  slideTime += 0.04;
  
  gfx->fillScreen(BLACK);
  
  // Sliding panels effect
  int numPanels = 8;
  int panelWidth = gfx->width() / numPanels;
  
  for (int panel = 0; panel < numPanels; panel++) {
    float panelOffset = slideTime + panel * 0.5;
    int slideY = sin(panelOffset) * 50;
    
    uint16_t panelColor = gfx->color565(
      (panel * 32) % 255,
      (panel * 64) % 255,
      (panel * 96) % 255
    );
    
    // Main panel
    gfx->fillRect(panel * panelWidth, gfx->height()/2 + slideY - 25, 
                  panelWidth - 2, 50, panelColor);
    
    // Panel border
    gfx->drawRect(panel * panelWidth, gfx->height()/2 + slideY - 25, 
                  panelWidth - 2, 50, WHITE);
    
    // Panel number
    gfx->setCursor(panel * panelWidth + 5, gfx->height()/2 + slideY - 5);
    gfx->setTextColor(BLACK);
    gfx->printf("%d", panel + 1);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(YELLOW);
  gfx->printf("SLIDESCREEN");
}

void drawSlip() {
  static float slipTime = 0;
  slipTime += 0.08;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Slipping/sliding geometric shapes
  for (int shape = 0; shape < 6; shape++) {
    float shapeAngle = slipTime + shape * PI / 3;
    
    // Slip motion - elliptical with phase
    int shapeX = centerX + cos(shapeAngle) * 80 + sin(slipTime * 2 + shape) * 30;
    int shapeY = centerY + sin(shapeAngle * 1.3) * 60;
    
    uint16_t shapeColor = gfx->color565(
      128 + sin(slipTime + shape) * 127,
      128 + cos(slipTime * 1.5 + shape) * 127,
      128 + sin(slipTime * 2 + shape) * 127
    );
    
    int shapeSize = 15 + sin(slipTime * 3 + shape) * 8;
    
    if (shape % 2 == 0) {
      // Rectangles
      gfx->fillRect(shapeX - shapeSize/2, shapeY - shapeSize/2, 
                   shapeSize, shapeSize, shapeColor);
    } else {
      // Circles
      gfx->fillCircle(shapeX, shapeY, shapeSize/2, shapeColor);
    }
    
    // Motion trail
    for (int trail = 1; trail < 5; trail++) {
      float trailAngle = slipTime - trail * 0.1 + shape * PI / 3;
      int trailX = centerX + cos(trailAngle) * 80 + sin((slipTime - trail * 0.1) * 2 + shape) * 30;
      int trailY = centerY + sin(trailAngle * 1.3) * 60;
      
      gfx->drawPixel(trailX, trailY, shapeColor >> (trail * 2));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("SLIP");
}

void drawSonar() {
  static float sonarTime = 0;
  sonarTime += 0.08;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Sonar sweep
  float sweepAngle = sonarTime * 2;
  
  // Sonar circles
  for (int ring = 1; ring <= 5; ring++) {
    int radius = ring * 30;
    gfx->drawCircle(centerX, centerY, radius, gfx->color565(0, 100, 0));
  }
  
  // Sweep line
  int sweepX = centerX + cos(sweepAngle) * 150;
  int sweepY = centerY + sin(sweepAngle) * 150;
  gfx->drawLine(centerX, centerY, sweepX, sweepY, gfx->color565(0, 255, 0));
  
  // Fade trail behind sweep
  for (int trail = 1; trail < 20; trail++) {
    float trailAngle = sweepAngle - trail * 0.1;
    int trailX = centerX + cos(trailAngle) * 150;
    int trailY = centerY + sin(trailAngle) * 150;
    
    uint16_t trailColor = gfx->color565(0, 255 - trail * 10, 0);
    gfx->drawLine(centerX, centerY, trailX, trailY, trailColor);
  }
  
  // Detected objects
  for (int obj = 0; obj < 4; obj++) {
    float objAngle = sonarTime * 0.3 + obj * PI / 2;
    int objDistance = 60 + sin(sonarTime + obj) * 40;
    int objX = centerX + cos(objAngle) * objDistance;
    int objY = centerY + sin(objAngle) * objDistance;
    
    // Pulse when sweep passes over
    float angleDiff = abs(sweepAngle - objAngle);
    if (angleDiff > PI) angleDiff = 2*PI - angleDiff;
    
    if (angleDiff < 0.3) {
      gfx->fillCircle(objX, objY, 5, gfx->color565(255, 255, 0));
      // Ping rings
      gfx->drawCircle(objX, objY, 8, gfx->color565(200, 200, 0));
      gfx->drawCircle(objX, objY, 12, gfx->color565(150, 150, 0));
    } else {
      gfx->fillCircle(objX, objY, 3, gfx->color565(100, 100, 100));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(0, 255, 0));
  gfx->printf("SONAR");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Range: 150m");
}

void drawSpeedmine() {
  static float speedmineTime = 0;
  speedmineTime += 0.1;
  
  gfx->fillScreen(BLACK);
  
  // High-speed mining operation
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Drilling motion
  float drillSpeed = speedmineTime * 8;
  int drillX = centerX + sin(speedmineTime * 0.5) * 50;
  int drillY = centerY;
  
  // Drill bit
  for (int bit = 0; bit < 8; bit++) {
    float bitAngle = drillSpeed + bit * PI / 4;
    int bitX = drillX + cos(bitAngle) * 15;
    int bitY = drillY + sin(bitAngle) * 15;
    
    gfx->drawLine(drillX, drillY, bitX, bitY, YELLOW);
  }
  
  gfx->fillCircle(drillX, drillY, 8, gfx->color565(200, 200, 0));
  
  // Mining particles
  for (int particle = 0; particle < 20; particle++) {
    float pTime = speedmineTime + particle * 0.2;
    int pX = drillX + cos(pTime * 5) * (20 + particle * 3);
    int pY = drillY + sin(pTime * 3) * (10 + particle * 2);
    
    uint16_t pColor = gfx->color565(
      255 - particle * 10,
      200 - particle * 8,
      100 - particle * 4
    );
    
    gfx->fillCircle(pX, pY, 2, pColor);
  }
  
  // Ore deposits
  for (int ore = 0; ore < 6; ore++) {
    int oreX = 30 + ore * 40;
    int oreY = centerY + 50 + sin(speedmineTime + ore) * 20;
    
    uint16_t oreColor = (ore % 3 == 0) ? gfx->color565(200, 100, 0) :
                        (ore % 3 == 1) ? gfx->color565(100, 100, 200) :
                        gfx->color565(100, 200, 100);
    
    gfx->fillRect(oreX - 8, oreY - 5, 16, 10, oreColor);
    gfx->drawRect(oreX - 8, oreY - 5, 16, 10, WHITE);
  }
  
  // Speed indicator
  gfx->setCursor(10, 10);
  gfx->setTextColor(RED);
  gfx->printf("SPEEDMINE");
  
  gfx->setCursor(10, 25);
  gfx->printf("RPM: %d", (int)(drillSpeed * 100) % 9999);
}

void drawSphere() {
  static float sphereTime = 0;
  sphereTime += 0.06;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // 3D sphere with latitude/longitude lines
  float rotX = sphereTime * 0.8;
  float rotY = sphereTime * 1.2;
  
  int sphereRadius = 80;
  
  // Draw latitude lines
  for (int lat = -4; lat <= 4; lat++) {
    float latAngle = lat * PI / 8;
    float latRadius = cos(latAngle) * sphereRadius;
    float latY = sin(latAngle) * sphereRadius;
    
    // Draw longitude segments
    for (int segment = 0; segment < 32; segment++) {
      float lon1 = segment * 2 * PI / 32;
      float lon2 = (segment + 1) * 2 * PI / 32;
      
      // Rotate points
      float x1 = cos(lon1) * latRadius;
      float z1 = sin(lon1) * latRadius;
      float x2 = cos(lon2) * latRadius;
      float z2 = sin(lon2) * latRadius;
      
      // Apply rotation
      float rx1 = x1 * cos(rotY) - z1 * sin(rotY);
      float rz1 = x1 * sin(rotY) + z1 * cos(rotY);
      float ry1 = latY * cos(rotX) - rz1 * sin(rotX);
      
      float rx2 = x2 * cos(rotY) - z2 * sin(rotY);
      float rz2 = x2 * sin(rotY) + z2 * cos(rotY);
      float ry2 = latY * cos(rotX) - rz2 * sin(rotX);
      
      // Project to screen
      if (rz1 > -sphereRadius && rz2 > -sphereRadius) {
        int sx1 = centerX + rx1;
        int sy1 = centerY + ry1;
        int sx2 = centerX + rx2;
        int sy2 = centerY + ry2;
        
        uint16_t lineColor = gfx->color565(100 + rz1, 100, 200 - rz1);
        gfx->drawLine(sx1, sy1, sx2, sy2, lineColor);
      }
    }
  }
  
  // Draw longitude lines
  for (int lon = 0; lon < 8; lon++) {
    float lonAngle = lon * PI / 4;
    
    for (int segment = 0; segment < 16; segment++) {
      float lat1 = (segment - 8) * PI / 16;
      float lat2 = (segment - 7) * PI / 16;
      
      float x1 = cos(lonAngle) * cos(lat1) * sphereRadius;
      float y1 = sin(lat1) * sphereRadius;
      float z1 = sin(lonAngle) * cos(lat1) * sphereRadius;
      
      float x2 = cos(lonAngle) * cos(lat2) * sphereRadius;
      float y2 = sin(lat2) * sphereRadius;
      float z2 = sin(lonAngle) * cos(lat2) * sphereRadius;
      
      // Rotate
      float rx1 = x1 * cos(rotY) - z1 * sin(rotY);
      float rz1 = x1 * sin(rotY) + z1 * cos(rotY);
      float ry1 = y1 * cos(rotX) - rz1 * sin(rotX);
      
      float rx2 = x2 * cos(rotY) - z2 * sin(rotY);
      float rz2 = x2 * sin(rotY) + z2 * cos(rotY);
      float ry2 = y2 * cos(rotX) - rz2 * sin(rotX);
      
      if (rz1 > -sphereRadius && rz2 > -sphereRadius) {
        int sx1 = centerX + rx1;
        int sy1 = centerY + ry1;
        int sx2 = centerX + rx2;
        int sy2 = centerY + ry2;
        
        uint16_t lineColor = gfx->color565(200, 100 + rz1, 100);
        gfx->drawLine(sx1, sy1, sx2, sy2, lineColor);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("SPHERE");
}

void drawSpheremonics() {
  static float spheremonicsTime = 0;
  spheremonicsTime += 0.04;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Spherical harmonics visualization
  float rotation = spheremonicsTime * 0.5;
  
  // Draw spherical harmonic surface
  for (int theta = 0; theta < 32; theta++) {
    for (int phi = 0; phi < 16; phi++) {
      float thetaAngle = theta * 2 * PI / 32;
      float phiAngle = phi * PI / 16;
      
      // Spherical harmonic function (simplified)
      float harmonic = sin(3 * thetaAngle) * cos(2 * phiAngle) * sin(spheremonicsTime);
      float radius = 60 + harmonic * 20;
      
      // Convert to Cartesian
      float x = radius * sin(phiAngle) * cos(thetaAngle);
      float y = radius * sin(phiAngle) * sin(thetaAngle);
      float z = radius * cos(phiAngle);
      
      // Rotate
      float rx = x * cos(rotation) - z * sin(rotation);
      float rz = x * sin(rotation) + z * cos(rotation);
      
      // Project
      if (rz > -100) {
        int sx = centerX + rx * 0.8;
        int sy = centerY + y * 0.8;
        
        uint16_t color = gfx->color565(
          128 + harmonic * 127,
          128 + sin(thetaAngle + spheremonicsTime) * 127,
          128 + cos(phiAngle + spheremonicsTime) * 127
        );
        
        gfx->drawPixel(sx, sy, color);
        
        // Add some thickness
        if (abs(harmonic) > 0.3) {
          gfx->drawPixel(sx + 1, sy, color);
          gfx->drawPixel(sx, sy + 1, color);
        }
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(MAGENTA);
  gfx->printf("SPHEREMONICS");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Harmonic: Y3,2");
}

void drawSpiral() {
  static float spiralTime = 0;
  spiralTime += 0.08;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple spirals
  for (int spiral = 0; spiral < 3; spiral++) {
    float spiralOffset = spiral * 2 * PI / 3;
    
    // Draw spiral arms
    for (int point = 0; point < 200; point++) {
      float t = point * 0.1;
      float angle = t + spiralTime + spiralOffset;
      float radius = t * 2;
      
      if (radius < 120) {
        int x = centerX + cos(angle) * radius;
        int y = centerY + sin(angle) * radius;
        
        // Color changes along spiral
        uint16_t color = gfx->color565(
          128 + sin(t + spiralTime) * 127,
          128 + cos(t + spiralTime + spiralOffset) * 127,
          128 + sin(t * 2 + spiralTime) * 127
        );
        
        int dotSize = 1 + (int)(sin(t + spiralTime) * 2);
        gfx->fillCircle(x, y, dotSize, color);
      }
    }
  }
  
  // Central core
  gfx->fillCircle(centerX, centerY, 8, WHITE);
  
  // Pulsing ring
  int ringRadius = 15 + sin(spiralTime * 3) * 5;
  gfx->drawCircle(centerX, centerY, ringRadius, YELLOW);
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(CYAN);
  gfx->printf("SPIRAL");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Arms: 3");
}

void drawSpotlight() {
  static float spotTime = 0;
  spotTime += 0.06;
  
  gfx->fillScreen(BLACK);
  
  // Moving spotlight effect
  int spotX = gfx->width()/2 + sin(spotTime * 0.8) * 100;
  int spotY = gfx->height()/2 + cos(spotTime * 1.2) * 60;
  
  // Spotlight cone
  int spotRadius = 60;
  
  // Draw illuminated area with gradient
  for (int r = spotRadius; r > 0; r -= 3) {
    float intensity = (float)(spotRadius - r) / spotRadius;
    uint16_t color = gfx->color565(
      intensity * 255,
      intensity * 255,
      intensity * 200
    );
    
    gfx->drawCircle(spotX, spotY, r, color);
  }
  
  // Objects in scene (only visible when lit)
  struct Object {
    int x, y, size;
    uint16_t color;
  };
  
  Object objects[] = {
    {80, 100, 15, RED},
    {200, 150, 20, GREEN},
    {120, 200, 12, BLUE},
    {250, 80, 18, YELLOW},
    {50, 180, 16, MAGENTA}
  };
  
  for (int i = 0; i < 5; i++) {
    float distance = sqrt((objects[i].x - spotX) * (objects[i].x - spotX) + 
                         (objects[i].y - spotY) * (objects[i].y - spotY));
    
    if (distance < spotRadius) {
      // Object is lit - draw with full brightness
      float brightness = (spotRadius - distance) / spotRadius;
      uint16_t litColor = gfx->color565(
        (((objects[i].color >> 11) & 0x1F) << 3) * brightness,
        (((objects[i].color >> 5) & 0x3F) << 2) * brightness,
        ((objects[i].color & 0x1F) << 3) * brightness
      );
      
      gfx->fillCircle(objects[i].x, objects[i].y, objects[i].size, litColor);
    } else {
      // Object in shadow - barely visible
      gfx->drawCircle(objects[i].x, objects[i].y, objects[i].size, 
                     gfx->color565(20, 20, 20));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("SPOTLIGHT");
}

void drawSproingies() {
  static float sproingTime = 0;
  sproingTime += 0.08;
  
  gfx->fillScreen(BLACK);
  
  // Bouncing spring creatures
  for (int sproing = 0; sproing < 5; sproing++) {
    float bouncePhase = sproingTime * 2 + sproing * PI / 2.5;
    
    int baseX = 30 + sproing * 50;
    int bounceHeight = abs(sin(bouncePhase)) * 80;
    int sproingY = gfx->height() - 30 - bounceHeight;
    
    // Spring body
    int springSegments = 12;
    for (int seg = 0; seg < springSegments; seg++) {
      float segmentRatio = (float)seg / springSegments;
      int segY = sproingY + segmentRatio * bounceHeight * 0.3;
      
      // Compression effect - spring gets squished when bouncing
      float compression = (1.0 - abs(sin(bouncePhase))) * 0.5 + 0.5;
      int segX = baseX + sin(segmentRatio * PI * 4 + sproingTime) * 8 * compression;
      
      uint16_t springColor = gfx->color565(100, 200, 100);
      if (seg > 0) {
        int prevX = baseX + sin((segmentRatio - 1.0/springSegments) * PI * 4 + sproingTime) * 8 * compression;
        int prevY = sproingY + (segmentRatio - 1.0/springSegments) * bounceHeight * 0.3;
        gfx->drawLine(prevX, prevY, segX, segY, springColor);
      }
      
      gfx->fillCircle(segX, segY, 2, springColor);
    }
    
    // Sproingies head
    int headX = baseX + sin(sproingTime + sproing) * 5;
    int headY = sproingY - 15;
    
    gfx->fillCircle(headX, headY, 8, YELLOW);
    
    // Eyes
    gfx->fillCircle(headX - 3, headY - 2, 2, BLACK);
    gfx->fillCircle(headX + 3, headY - 2, 2, BLACK);
    gfx->drawPixel(headX - 3, headY - 3, WHITE);
    gfx->drawPixel(headX + 3, headY - 3, WHITE);
    
    // Mouth
    if (bounceHeight > 40) { // Open mouth when bouncing high
      gfx->fillCircle(headX, headY + 2, 2, BLACK);
    } else {
      gfx->drawLine(headX - 2, headY + 2, headX + 2, headY + 2, BLACK);
    }
    
    // Ground impact effect
    if (bounceHeight < 5) {
      for (int particle = 0; particle < 6; particle++) {
        int pX = baseX + (particle - 3) * 8;
        int pY = gfx->height() - 25;
        gfx->drawPixel(pX, pY, gfx->color565(200, 100, 50));
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(GREEN);
  gfx->printf("SPROINGIES");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Boing! Boing!");
}

void drawStairs() {
  static float stairsTime = 0;
  stairsTime += 0.05;
  
  gfx->fillScreen(BLACK);
  
  // 3D staircase with perspective
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  float rotation = stairsTime * 0.3;
  
  // Draw staircase steps
  for (int step = 0; step < 12; step++) {
    float stepZ = step * 15;
    float stepY = -step * 12;
    
    // Step corners in 3D space
    float corners[4][3] = {
      {-40, stepY, stepZ},
      {40, stepY, stepZ},
      {40, stepY - 12, stepZ},
      {-40, stepY - 12, stepZ}
    };
    
    // Project to 2D with rotation
    int projectedCorners[4][2];
    bool allVisible = true;
    
    for (int corner = 0; corner < 4; corner++) {
      float x = corners[corner][0];
      float y = corners[corner][1];
      float z = corners[corner][2];
      
      // Rotate around Y axis
      float rx = x * cos(rotation) - z * sin(rotation);
      float rz = x * sin(rotation) + z * cos(rotation);
      
      // Perspective projection
      if (rz > -200) {
        projectedCorners[corner][0] = centerX + (rx * 200) / (rz + 250);
        projectedCorners[corner][1] = centerY + (y * 200) / (rz + 250);
      } else {
        allVisible = false;
        break;
      }
    }
    
    if (allVisible) {
      // Step top face
      uint16_t stepColor = gfx->color565(
        100 + step * 10,
        150 - step * 5,
        200 - step * 8
      );
      
      // Draw step as quadrilateral (simplified as lines)
      gfx->drawLine(projectedCorners[0][0], projectedCorners[0][1],
                   projectedCorners[1][0], projectedCorners[1][1], stepColor);
      gfx->drawLine(projectedCorners[1][0], projectedCorners[1][1],
                   projectedCorners[2][0], projectedCorners[2][1], stepColor);
      gfx->drawLine(projectedCorners[2][0], projectedCorners[2][1],
                   projectedCorners[3][0], projectedCorners[3][1], stepColor);
      gfx->drawLine(projectedCorners[3][0], projectedCorners[3][1],
                   projectedCorners[0][0], projectedCorners[0][1], stepColor);
      
      // Step side face (darker)
      uint16_t sideColor = gfx->color565(
        (100 + step * 10) >> 1,
        (150 - step * 5) >> 1,
        (200 - step * 8) >> 1
      );
      
      // Side face bottom corners
      int sideBottom[2][2] = {
        {projectedCorners[1][0], projectedCorners[1][1] + 12},
        {projectedCorners[2][0], projectedCorners[2][1] + 12}
      };
      
      gfx->drawLine(projectedCorners[1][0], projectedCorners[1][1],
                   sideBottom[0][0], sideBottom[0][1], sideColor);
      gfx->drawLine(projectedCorners[2][0], projectedCorners[2][1],
                   sideBottom[1][0], sideBottom[1][1], sideColor);
      gfx->drawLine(sideBottom[0][0], sideBottom[0][1],
                   sideBottom[1][0], sideBottom[1][1], sideColor);
    }
  }
  
  // Person climbing stairs (stick figure)
  int personStep = (int)(stairsTime * 2) % 12;
  float personZ = personStep * 15;
  float personY = -personStep * 12 - 20;
  
  float px = 0;
  float pz = personZ;
  
  // Rotate person position
  float rpx = px * cos(rotation) - pz * sin(rotation);
  float rpz = px * sin(rotation) + pz * cos(rotation);
  
  if (rpz > -200) {
    int screenX = centerX + (rpx * 200) / (rpz + 250);
    int screenY = centerY + (personY * 200) / (rpz + 250);
    
    // Draw stick figure
    gfx->fillCircle(screenX, screenY, 3, YELLOW); // Head
    gfx->drawLine(screenX, screenY + 3, screenX, screenY + 15, WHITE); // Body
    gfx->drawLine(screenX, screenY + 8, screenX - 5, screenY + 5, WHITE); // Arms
    gfx->drawLine(screenX, screenY + 8, screenX + 5, screenY + 5, WHITE);
    gfx->drawLine(screenX, screenY + 15, screenX - 4, screenY + 25, WHITE); // Legs
    gfx->drawLine(screenX, screenY + 15, screenX + 4, screenY + 25, WHITE);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("STAIRS");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Step: %d/12", personStep + 1);
}

void drawStarfish() {
  static float starfishTime = 0;
  starfishTime += 0.06;
  
  gfx->fillScreen(gfx->color565(0, 20, 60)); // Deep ocean blue
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Main starfish - 5 arms
  int numArms = 5;
  float armLength = 50 + sin(starfishTime) * 10;
  
  for (int arm = 0; arm < numArms; arm++) {
    float armAngle = arm * 2 * PI / numArms + starfishTime * 0.3;
    
    // Arm segments
    for (int segment = 0; segment < 8; segment++) {
      float segmentRatio = (float)segment / 8;
      float segmentLength = armLength * segmentRatio;
      
      // Arm gets thinner towards tip
      int armWidth = (8 - segment) * 2;
      
      int armX = centerX + cos(armAngle) * segmentLength;
      int armY = centerY + sin(armAngle) * segmentLength;
      
      // Pulsing color
      uint16_t armColor = gfx->color565(
        200 + sin(starfishTime + segment * 0.5) * 55,
        100 + sin(starfishTime * 1.3 + segment * 0.3) * 55,
        50 + sin(starfishTime * 0.7 + segment * 0.8) * 50
      );
      
      gfx->fillCircle(armX, armY, armWidth, armColor);
      
      // Arm texture - spots
      if (segment % 2 == 0) {
        gfx->fillCircle(armX + 2, armY - 1, 1, gfx->color565(255, 200, 150));
        gfx->fillCircle(armX - 1, armY + 2, 1, gfx->color565(255, 200, 150));
      }
    }
  }
  
  // Central body
  gfx->fillCircle(centerX, centerY, 15, gfx->color565(220, 120, 80));
  
  // Body pattern
  for (int dot = 0; dot < 8; dot++) {
    float dotAngle = dot * 2 * PI / 8;
    int dotX = centerX + cos(dotAngle) * 8;
    int dotY = centerY + sin(dotAngle) * 8;
    gfx->fillCircle(dotX, dotY, 2, gfx->color565(255, 180, 120));
  }
  
  // Baby starfish
  for (int baby = 0; baby < 3; baby++) {
    float babyAngle = starfishTime + baby * 2;
    int babyX = centerX + cos(babyAngle) * 80;
    int babyY = centerY + sin(babyAngle * 1.3) * 60;
    
    // Mini 5-pointed star
    for (int arm = 0; arm < 5; arm++) {
      float miniAngle = arm * 2 * PI / 5 + starfishTime;
      int tipX = babyX + cos(miniAngle) * 12;
      int tipY = babyY + sin(miniAngle) * 12;
      
      gfx->drawLine(babyX, babyY, tipX, tipY, gfx->color565(150, 80, 40));
    }
    gfx->fillCircle(babyX, babyY, 4, gfx->color565(180, 100, 60));
  }
  
  // Ocean current particles
  for (int particle = 0; particle < 15; particle++) {
    float pTime = starfishTime + particle * 0.3;
    int pX = (int)(pTime * 20) % (gfx->width() + 20) - 10;
    int pY = 20 + particle * 12 + sin(pTime) * 5;
    
    gfx->drawPixel(pX, pY, gfx->color565(100, 150, 200));
    gfx->drawPixel(pX - 1, pY, gfx->color565(80, 120, 160));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(200, 255, 255));
  gfx->printf("STARFISH");
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("Depth: 20m");
}

void drawPetri() {
  static float petriTime = 0;
  petriTime += 0.02;
  gfx->fillScreen(BLACK);
  
  for (int i = 0; i < 25; i++) {
    float cellAge = petriTime + i * 0.3;
    int x = (int)(gfx->width()/2 + 80 * sin(cellAge + i));
    int y = (int)(gfx->height()/2 + 60 * cos(cellAge * 1.2 + i));
    int radius = (int)(8 + 6 * sin(cellAge * 2));
    uint16_t color = gfx->color565((int)(128 + 100 * sin(cellAge)), 
                                   (int)(150 + 80 * cos(cellAge * 1.5)), 
                                   100);
    gfx->fillCircle(x, y, radius, color);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(GREEN);
  gfx->printf("PETRI");
}

void drawPhosphor() {
  static int phosphorLines[240];
  static bool phosphorInit = false;
  if (!phosphorInit) {
    for (int i = 0; i < 240; i++) phosphorLines[i] = random(320);
    phosphorInit = true;
  }
  
  gfx->fillScreen(BLACK);
  for (int y = 0; y < 240; y++) {
    phosphorLines[y] = (phosphorLines[y] + random(-2, 3)) % 320;
    if (phosphorLines[y] < 0) phosphorLines[y] += 320;
    
    uint16_t color = gfx->color565(0, (int)(200 + 55 * sin(y * 0.1)), 0);
    gfx->drawPixel(phosphorLines[y], y, color);
    if (phosphorLines[y] > 0) gfx->drawPixel(phosphorLines[y]-1, y, gfx->color565(0, 100, 0));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(GREEN);
  gfx->printf("PHOSPHOR");
}

void drawPhotopile() {
  static float photoTime = 0;
  photoTime += 0.03;
  gfx->fillScreen(gfx->color565(20, 20, 30));
  
  for (int i = 0; i < 8; i++) {
    int x = (int)(40 + (i % 4) * 70 + 20 * sin(photoTime + i));
    int y = (int)(40 + (i / 4) * 80 + 15 * cos(photoTime * 1.3 + i));
    int w = (int)(50 + 10 * sin(photoTime * 2 + i));
    int h = (int)(40 + 8 * cos(photoTime * 1.8 + i));
    
    uint16_t frameColor = gfx->color565(100 + i * 15, 80 + i * 10, 60 + i * 8);
    gfx->drawRect(x-2, y-2, w+4, h+4, frameColor);
    gfx->fillRect(x, y, w, h, gfx->color565(200 - i * 20, 150 - i * 15, 100 - i * 10));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("PHOTOPILE");
}

void drawPiecewise() {
  static float pieceTime = 0;
  pieceTime += 0.04;
  gfx->fillScreen(BLACK);
  
  for (int segment = 0; segment < 6; segment++) {
    float angle = segment * PI / 3 + pieceTime;
    int x1 = gfx->width()/2;
    int y1 = gfx->height()/2;
    int x2 = (int)(x1 + 80 * cos(angle));
    int y2 = (int)(y1 + 80 * sin(angle));
    
    for (int piece = 0; piece < 10; piece++) {
      float t = piece / 9.0;
      int px = (int)(x1 + t * (x2 - x1) + 15 * sin(pieceTime * 3 + piece));
      int py = (int)(y1 + t * (y2 - y1) + 10 * cos(pieceTime * 4 + piece));
      
      uint16_t color = gfx->color565((int)(255 * t), (int)(200 * (1-t)), 128);
      gfx->fillCircle(px, py, 3, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(YELLOW);
  gfx->printf("PIECEWISE");
}

void drawPinion() {
  static float pinionAngle = 0;
  pinionAngle += 0.1;
  gfx->fillScreen(gfx->color565(30, 30, 40));
  
  int centerX = gfx->width()/2;
  int centerY = gfx->height()/2;
  
  for (int gear = 0; gear < 3; gear++) {
    int gx = centerX + (gear - 1) * 60;
    int gy = centerY;
    int teeth = 12 + gear * 4;
    float gearAngle = pinionAngle * (1 + gear * 0.7);
    
    for (int tooth = 0; tooth < teeth; tooth++) {
      float angle = tooth * 2 * PI / teeth + gearAngle;
      int x1 = (int)(gx + 25 * cos(angle));
      int y1 = (int)(gy + 25 * sin(angle));
      int x2 = (int)(gx + 35 * cos(angle));
      int y2 = (int)(gy + 35 * sin(angle));
      
      uint16_t color = gfx->color565(150 + gear * 30, 100 + gear * 20, 50);
      gfx->drawLine(x1, y1, x2, y2, color);
    }
    gfx->fillCircle(gx, gy, 20, gfx->color565(80, 60, 40));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(ORANGE);
  gfx->printf("PINION");
}

void drawPipeobjs() {
  static float pipeTime = 0;
  pipeTime += 0.05;
  gfx->fillScreen(BLACK);
  
  for (int pipe = 0; pipe < 5; pipe++) {
    float pipePhase = pipeTime + pipe * 1.2;
    int x = (int)(50 + pipe * 50 + 30 * sin(pipePhase));
    int y = (int)(gfx->height()/2 + 40 * cos(pipePhase * 1.5));
    int length = (int)(60 + 20 * sin(pipePhase * 2));
    
    uint16_t color = gfx->color565(100 + pipe * 30, 150, 200 - pipe * 20);
    
    // Vertical pipe
    gfx->fillRect(x-5, y-length/2, 10, length, color);
    // Horizontal connector
    gfx->fillRect(x-15, y-5, 30, 10, color);
    
    // Joint circles
    gfx->fillCircle(x, y-length/2, 8, gfx->color565(180, 180, 180));
    gfx->fillCircle(x, y+length/2, 8, gfx->color565(180, 180, 180));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(CYAN);
  gfx->printf("PIPEOBJS");
}

void drawPipes() {
  static float pipeFlow = 0;
  pipeFlow += 0.08;
  gfx->fillScreen(gfx->color565(10, 10, 20));
  
  // Draw pipe network
  for (int i = 0; i < 4; i++) {
    int x1 = 20 + i * 70;
    int y1 = 50;
    int x2 = x1;
    int y2 = gfx->height() - 50;
    
    // Pipe body
    gfx->fillRect(x1-8, y1, 16, y2-y1, gfx->color565(100, 100, 120));
    
    // Flowing liquid
    for (int j = 0; j < 8; j++) {
      int fy = (int)(y1 + (j + pipeFlow) * 20) % (y2 - y1);
      if (fy + y1 < y2) {
        gfx->fillRect(x1-6, y1 + fy, 12, 8, gfx->color565(0, 100 + j * 15, 255));
      }
    }
    
    // Horizontal connectors
    if (i < 3) {
      int cy = gfx->height()/2;
      gfx->fillRect(x1+8, cy-6, 54, 12, gfx->color565(100, 100, 120));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(BLUE);
  gfx->printf("PIPES");
}

void drawPlatonicfolding() {
  static float foldTime = 0;
  foldTime += 0.03;
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width()/2;
  int centerY = gfx->height()/2;
  
  for (int face = 0; face < 6; face++) {
    float faceAngle = face * PI / 3 + foldTime;
    float foldAmount = sin(foldTime * 2 + face) * 0.5 + 0.5;
    
    int x1 = (int)(centerX + 60 * cos(faceAngle) * foldAmount);
    int y1 = (int)(centerY + 60 * sin(faceAngle) * foldAmount);
    int x2 = (int)(centerX + 80 * cos(faceAngle + PI/3));
    int y2 = (int)(centerY + 80 * sin(faceAngle + PI/3));
    int x3 = (int)(centerX + 80 * cos(faceAngle - PI/3));
    int y3 = (int)(centerY + 80 * sin(faceAngle - PI/3));
    
    uint16_t color = gfx->color565((int)(255 * foldAmount), 
                                   (int)(200 * (1-foldAmount)), 
                                   (int)(150 + 100 * foldAmount));
    
    gfx->drawTriangle(x1, y1, x2, y2, x3, y3, color);
    gfx->drawLine(centerX, centerY, x1, y1, WHITE);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(MAGENTA);
  gfx->printf("PLATONICFOLDING");
}

void drawPolyhedra() {
  static float polyTime = 0;
  polyTime += 0.04;
  gfx->fillScreen(gfx->color565(20, 0, 40));
  
  int centerX = gfx->width()/2;
  int centerY = gfx->height()/2;
  
  // Draw rotating polyhedron edges
  for (int edge = 0; edge < 12; edge++) {
    float angle1 = edge * PI / 6 + polyTime;
    float angle2 = edge * PI / 6 + PI/6 + polyTime * 1.2;
    
    int x1 = (int)(centerX + 70 * cos(angle1) * cos(polyTime));
    int y1 = (int)(centerY + 70 * sin(angle1) + 20 * sin(polyTime * 3));
    int x2 = (int)(centerX + 70 * cos(angle2) * cos(polyTime + PI/4));
    int y2 = (int)(centerY + 70 * sin(angle2) + 20 * cos(polyTime * 3));
    
    uint16_t color = gfx->color565((int)(200 + 55 * sin(angle1)), 
                                   (int)(150 + 100 * cos(angle2)), 
                                   255);
    gfx->drawLine(x1, y1, x2, y2, color);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("POLYHEDRA");
}

void drawPolyhedraGl() {
  static float glTime = 0;
  glTime += 0.06;
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width()/2;
  int centerY = gfx->height()/2;
  
  // Simulate 3D GL-style polyhedron
  for (int vertex = 0; vertex < 20; vertex++) {
    float vAngle = vertex * PI / 10 + glTime;
    float depth = sin(glTime * 2 + vertex) + 2;
    
    int x = (int)(centerX + 80 * cos(vAngle) / depth);
    int y = (int)(centerY + 60 * sin(vAngle) / depth);
    int size = (int)(8 / depth);
    
    uint16_t color = gfx->color565((int)(255 / depth), 
                                   (int)(200 / depth), 
                                   (int)(150 + 100 / depth));
    
    gfx->fillCircle(x, y, size, color);
    
    // Connect to center with depth-based alpha
    if (vertex % 3 == 0) {
      gfx->drawLine(centerX, centerY, x, y, gfx->color565((int)(100 / depth), (int)(150 / depth), (int)(200 / depth)));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(GREEN);
  gfx->printf("POLYHEDRA-GL");
}

void drawPolyominoes() {
  static float tetrominoTime = 0;
  tetrominoTime += 0.05;
  gfx->fillScreen(gfx->color565(30, 30, 50));
  
  // Draw falling tetromino-like pieces
  for (int piece = 0; piece < 6; piece++) {
    float fallSpeed = tetrominoTime + piece * 0.8;
    int x = 40 + piece * 40;
    int y = ((int)(fallSpeed * 30) % (gfx->height() + 80)) - 40;
    
    // Different piece shapes
    uint16_t color = gfx->color565((piece * 40) % 255, (piece * 60) % 255, (piece * 80) % 255);
    
    switch (piece % 4) {
      case 0: // L-shape
        gfx->fillRect(x, y, 12, 12, color);
        gfx->fillRect(x, y+12, 12, 12, color);
        gfx->fillRect(x, y+24, 12, 12, color);
        gfx->fillRect(x+12, y+24, 12, 12, color);
        break;
      case 1: // Line
        for (int i = 0; i < 4; i++) gfx->fillRect(x, y + i*12, 12, 12, color);
        break;
      case 2: // Square
        gfx->fillRect(x, y, 24, 24, color);
        break;
      case 3: // T-shape
        gfx->fillRect(x, y, 36, 12, color);
        gfx->fillRect(x+12, y+12, 12, 12, color);
        break;
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(YELLOW);
  gfx->printf("POLYOMINOES");
}

void drawPolytopes() {
  static float polytopeTime = 0;
  polytopeTime += 0.04;
  gfx->fillScreen(gfx->color565(40, 20, 60));
  
  int centerX = gfx->width()/2;
  int centerY = gfx->height()/2;
  
  // 4D polytope projection
  for (int dimension = 0; dimension < 4; dimension++) {
    for (int point = 0; point < 8; point++) {
      float angle = point * PI / 4 + polytopeTime * (1 + dimension * 0.3);
      float radius = 60 + dimension * 15;
      float w = cos(polytopeTime + dimension) + 2; // 4th dimension
      
      int x = (int)(centerX + radius * cos(angle) / w);
      int y = (int)(centerY + radius * sin(angle) / w);
      int size = (int)(6 / w);
      
      uint16_t color = gfx->color565((int)(200 + 55 * cos(angle)), 
                                     (int)(150 + 100 * sin(angle + dimension)), 
                                     (int)(100 + 155 / w));
      
      gfx->fillCircle(x, y, size, color);
      
      // Connect points within same dimension
      if (point > 0) {
        float prevAngle = (point-1) * PI / 4 + polytopeTime * (1 + dimension * 0.3);
        int px = (int)(centerX + radius * cos(prevAngle) / w);
        int py = (int)(centerY + radius * sin(prevAngle) / w);
        gfx->drawLine(x, y, px, py, gfx->color565((int)(100/w), (int)(150/w), (int)(200/w)));
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(CYAN);
  gfx->printf("POLYTOPES");
}

void drawPong() {
  static float ballX = 160, ballY = 120;
  static float ballVX = 2, ballVY = 1.5;
  static float paddle1Y = 100, paddle2Y = 100;
  static int score1 = 0, score2 = 0;
  
  gfx->fillScreen(BLACK);
  
  // Update ball
  ballX += ballVX;
  ballY += ballVY;
  
  // Ball collision with top/bottom
  if (ballY <= 5 || ballY >= gfx->height() - 5) ballVY = -ballVY;
  
  // Ball collision with paddles
  if (ballX <= 15 && ballY >= paddle1Y && ballY <= paddle1Y + 40) {
    ballVX = abs(ballVX);
  }
  if (ballX >= gfx->width() - 15 && ballY >= paddle2Y && ballY <= paddle2Y + 40) {
    ballVX = -abs(ballVX);
  }
  
  // Reset ball if out of bounds
  if (ballX < 0) { score2++; ballX = 160; ballVX = 2; }
  if (ballX > gfx->width()) { score1++; ballX = 160; ballVX = -2; }
  
  // AI paddles
  paddle1Y += (ballY - paddle1Y - 20) * 0.1;
  paddle2Y += (ballY - paddle2Y - 20) * 0.1;
  
  // Draw game
  gfx->fillRect(10, paddle1Y, 5, 40, WHITE);
  gfx->fillRect(gfx->width()-15, paddle2Y, 5, 40, WHITE);
  gfx->fillCircle(ballX, ballY, 3, WHITE);
  
  // Center line
  for (int i = 0; i < gfx->height(); i += 10) {
    gfx->fillRect(gfx->width()/2 - 1, i, 2, 5, WHITE);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("PONG %d-%d", score1, score2);
}

void drawPopsquares() {
  static float popTime = 0;
  popTime += 0.06;
  gfx->fillScreen(gfx->color565(20, 20, 40));
  
  for (int row = 0; row < 6; row++) {
    for (int col = 0; col < 8; col++) {
      float squarePhase = popTime + (row + col) * 0.2;
      float popAmount = (sin(squarePhase) + 1) / 2;
      
      int x = col * 40 + 10;
      int y = row * 40 + 10;
      int size = (int)(25 + 10 * popAmount);
      
      uint16_t color = gfx->color565((int)(255 * popAmount), 
                                     (int)(200 * (1 - popAmount)), 
                                     (int)(150 + 100 * popAmount));
      
      gfx->fillRect(x + (30-size)/2, y + (30-size)/2, size, size, color);
      
      if (popAmount > 0.7) {
        gfx->drawRect(x + (30-size)/2 - 2, y + (30-size)/2 - 2, size + 4, size + 4, WHITE);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("POPSQUARES");
}

void drawPow2() {
  static float powTime = 0;
  powTime += 0.07;
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width()/2;
  int centerY = gfx->height()/2;
  
  // Draw powers of 2 visualization
  for (int power = 0; power < 8; power++) {
    int value = 1 << power; // 2^power
    float angle = power * PI / 4 + powTime;
    float radius = 30 + power * 8;
    
    int x = (int)(centerX + radius * cos(angle));
    int y = (int)(centerY + radius * sin(angle));
    
    // Size based on power
    int nodeSize = 4 + power;
    
    uint16_t color = gfx->color565((power * 30) % 255, 
                                   (power * 50) % 255, 
                                   255 - (power * 25));
    
    gfx->fillCircle(x, y, nodeSize, color);
    
    // Draw value
    if (power < 5) { // Only show smaller values
      gfx->setCursor(x - 10, y - 4);
      gfx->setTextColor(WHITE);
      gfx->printf("%d", value);
    }
    
    // Connect to center
    gfx->drawLine(centerX, centerY, x, y, gfx->color565(100, 100, 150));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(CYAN);
  gfx->printf("POW2");
}

void drawPrefs() {
  static float prefTime = 0;
  prefTime += 0.04;
  gfx->fillScreen(gfx->color565(40, 40, 60));
  
  // Simulate preference/settings interface
  for (int item = 0; item < 6; item++) {
    int y = 30 + item * 35;
    float itemPhase = prefTime + item * 0.5;
    bool selected = (int)(prefTime * 2) % 6 == item;
    
    uint16_t bgColor = selected ? gfx->color565(100, 150, 200) : gfx->color565(60, 60, 80);
    uint16_t textColor = selected ? WHITE : gfx->color565(200, 200, 200);
    
    gfx->fillRect(10, y - 12, gfx->width() - 20, 24, bgColor);
    
    gfx->setCursor(20, y - 6);
    gfx->setTextColor(textColor);
    
    switch (item) {
      case 0: gfx->printf("Effect Speed: %.1f", 1.0 + sin(itemPhase)); break;
      case 1: gfx->printf("Color Mode: %s", (int)(itemPhase * 2) % 2 ? "RGB" : "HSV"); break;
      case 2: gfx->printf("Brightness: %d%%", (int)(50 + 50 * sin(itemPhase))); break;
      case 3: gfx->printf("Auto-change: %s", (int)itemPhase % 2 ? "ON" : "OFF"); break;
      case 4: gfx->printf("Timer: %d min", 1 + (int)(itemPhase * 3) % 10); break;
      case 5: gfx->printf("Save Settings"); break;
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(YELLOW);
  gfx->printf("PREFERENCES");
}

void drawPrefsw() {
  static float prefswTime = 0;
  prefswTime += 0.05;
  gfx->fillScreen(gfx->color565(30, 50, 70));
  
  // Widget-based preferences interface
  for (int widget = 0; widget < 4; widget++) {
    int x = (widget % 2) * 150 + 20;
    int y = (widget / 2) * 100 + 40;
    float widgetPhase = prefswTime + widget;
    
    // Widget background
    gfx->fillRect(x, y, 140, 80, gfx->color565(50, 70, 90));
    gfx->drawRect(x, y, 140, 80, gfx->color565(100, 120, 150));
    
    gfx->setCursor(x + 5, y + 5);
    gfx->setTextColor(WHITE);
    
    switch (widget) {
      case 0: {// Slider widget
        gfx->printf("Volume");
        int sliderPos = (int)(100 + 30 * sin(widgetPhase));
        gfx->fillRect(x + 10, y + 30, 120, 4, gfx->color565(80, 80, 100));
        gfx->fillCircle(sliderPos, y + 32, 8, gfx->color565(150, 200, 255));
        break;
      }
      case 1: {// Checkbox widget
        gfx->printf("Enable FX");
        bool checked = (int)(widgetPhase * 2) % 2;
        gfx->drawRect(x + 10, y + 30, 20, 20, WHITE);
        if (checked) {
          gfx->drawLine(x + 13, y + 38, x + 18, y + 43, GREEN);
          gfx->drawLine(x + 18, y + 43, x + 27, y + 32, GREEN);
        }
        break;
      }
      case 2: // Color picker
        gfx->printf("Color");
        for (int c = 0; c < 8; c++) {
          uint16_t color = gfx->color565((c * 32) % 255, (c * 64) % 255, (c * 96) % 255);
          gfx->fillRect(x + 10 + c * 15, y + 30, 12, 20, color);
        }
        break;
        
      case 3: // Spinner widget
        gfx->printf("Speed: %d", (int)(5 + 5 * sin(widgetPhase)));
        gfx->drawRect(x + 10, y + 30, 60, 20, WHITE);
        gfx->setCursor(x + 15, y + 35);
        gfx->printf("%d", (int)(5 + 5 * sin(widgetPhase)));
        break;
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(CYAN);
  gfx->printf("PREFS-WIDGETS");
}

void drawProjectiveplane() {
  static float projTime = 0;
  projTime += 0.03;
  gfx->fillScreen(gfx->color565(20, 10, 40));
  
  int centerX = gfx->width()/2;
  int centerY = gfx->height()/2;
  
  // Projective plane topology visualization
  for (int u = 0; u < 20; u++) {
    for (int v = 0; v < 15; v++) {
      float uParam = u * PI / 10 + projTime;
      float vParam = v * PI / 7.5;
      
      // Projective plane parametric equations
      float x = cos(uParam) * sin(vParam);
      float y = sin(uParam) * sin(vParam);
      float z = cos(vParam) + projTime * 0.1;
      
      // Project to 2D
      float scale = 80 / (2 + z);
      int px = (int)(centerX + x * scale);
      int py = (int)(centerY + y * scale);
      
      if (px >= 0 && px < gfx->width() && py >= 0 && py < gfx->height()) {
        uint16_t color = gfx->color565((int)(128 + 127 * x), 
                                       (int)(128 + 127 * y), 
                                       (int)(100 + 155 * (z + 1) / 2));
        gfx->drawPixel(px, py, color);
        
        // Draw connections
        if (u > 0 && v > 0) {
          float prevU = (u-1) * PI / 10 + projTime;
          float px2 = cos(prevU) * sin(vParam);
          float py2 = sin(prevU) * sin(vParam);
          int px2Screen = (int)(centerX + px2 * scale);
          int py2Screen = (int)(centerY + py2 * scale);
          
          gfx->drawLine(px, py, px2Screen, py2Screen, gfx->color565(100, 100, 150));
        }
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(MAGENTA);
  gfx->printf("PROJECTIVE PLANE");
}

void drawProvidence() {
  static float provTime = 0;
  provTime += 0.04;
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width()/2;
  int centerY = gfx->height()/2;
  
  // All-seeing eye effect
  // Eye outline
  for (int i = 0; i < 360; i += 5) {
    float angle = i * PI / 180;
    float radius = 60 + 10 * sin(provTime * 3 + angle * 4);
    int x = (int)(centerX + radius * cos(angle));
    int y = (int)(centerY + radius * sin(angle) * 0.6);
    
    uint16_t color = gfx->color565((int)(200 + 55 * sin(provTime + angle)), 
                                   (int)(150 + 100 * cos(provTime * 1.5 + angle)), 
                                   100);
    gfx->drawPixel(x, y, color);
  }
  
  // Pupil
  int pupilSize = (int)(15 + 5 * sin(provTime * 4));
  gfx->fillCircle(centerX, centerY, pupilSize, BLACK);
  gfx->drawCircle(centerX, centerY, pupilSize, YELLOW);
  
  // Iris
  for (int ring = pupilSize; ring < 40; ring += 3) {
    uint16_t irisColor = gfx->color565((int)(100 + ring * 2), 
                                       (int)(200 - ring), 
                                       (int)(150 + ring));
    gfx->drawCircle(centerX, centerY, ring, irisColor);
  }
  
  // Radiating lines
  for (int ray = 0; ray < 12; ray++) {
    float rayAngle = ray * PI / 6 + provTime;
    int x1 = (int)(centerX + 80 * cos(rayAngle));
    int y1 = (int)(centerY + 48 * sin(rayAngle));
    int x2 = (int)(centerX + 120 * cos(rayAngle));
    int y2 = (int)(centerY + 72 * sin(rayAngle));
    
    gfx->drawLine(x1, y1, x2, y2, gfx->color565(255, 215, 0));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(YELLOW);
  gfx->printf("PROVIDENCE");
}

void drawM6502() {
  static float cpuTime = 0;
  cpuTime += 0.03;
  
  gfx->fillScreen(BLACK);
  gfx->setTextColor(GREEN);
  
  // Simulate 6502 processor display
  for (int y = 0; y < gfx->height(); y += 12) {
    for (int x = 0; x < gfx->width(); x += 48) {
      if (random(100) < 30) {
        gfx->setCursor(x, y);
        gfx->printf("%04X", (int)(random(0xFFFF) + cpuTime * 100) & 0xFFFF);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(CYAN);
  gfx->printf("M6502");
}

void drawMapscroller() {
  static float scrollTime = 0;
  scrollTime += 0.02;
  
  gfx->fillScreen(BLACK);
  
  // Scrolling map tiles
  for (int y = -32; y < gfx->height() + 32; y += 32) {
    for (int x = -32; x < gfx->width() + 32; x += 32) {
      int mapX = (int)(x + scrollTime * 50) % 64;
      int mapY = (int)(y + scrollTime * 30) % 64;
      
      uint16_t tileColor = gfx->color565(
        abs((int)(sin(mapX * 0.1) * 128)),
        abs((int)(cos(mapY * 0.1) * 128)),
        abs((int)(sin((mapX + mapY) * 0.05) * 128))
      );
      
      gfx->fillRect(x, y, 30, 30, tileColor);
      gfx->drawRect(x, y, 30, 30, WHITE);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("MAPSCROLLER");
}

void drawMarbling() {
  static float marbleTime = 0;
  marbleTime += 0.01;
  
  gfx->fillScreen(BLACK);
  
  // Marble-like swirls
  for (int y = 0; y < gfx->height(); y += 4) {
    for (int x = 0; x < gfx->width(); x += 4) {
      float noise = sin(x * 0.02 + marbleTime * 2) * cos(y * 0.02 + marbleTime) * 
                   sin((x + y) * 0.01 + marbleTime * 1.5);
      
      uint16_t color = gfx->color565(
        128 + (int)(noise * 127),
        64 + (int)(sin(noise * 2) * 63),
        192 + (int)(cos(noise * 3) * 63)
      );
      
      gfx->fillRect(x, y, 4, 4, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("MARBLING");
}

void drawMarching() {
  static float marchTime = 0;
  marchTime += 0.05;
  
  gfx->fillScreen(BLACK);
  gfx->setTextColor(RED);
  
  // Marching squares algorithm visualization
  for (int y = 0; y < gfx->height() - 16; y += 16) {
    for (int x = 0; x < gfx->width() - 16; x += 16) {
      float field = sin((x + marchTime * 20) * 0.05) * cos((y + marchTime * 15) * 0.05);
      
      if (field > 0) {
        gfx->fillRect(x, y, 16, 16, gfx->color565(255, (int)(field * 255), 0));
      } else {
        gfx->drawRect(x, y, 16, 16, gfx->color565(0, 0, abs((int)(field * 255))));
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(YELLOW);
  gfx->printf("MARCHING");
}

void drawMaze3d() {
  static float mazeTime = 0;
  mazeTime += 0.02;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // 3D maze corridor effect
  for (int depth = 10; depth > 0; depth--) {
    float scale = depth * 0.1;
    int wallWidth = (int)(gfx->width() * scale);
    int wallHeight = (int)(gfx->height() * scale);
    
    uint16_t wallColor = gfx->color565(depth * 25, depth * 20, depth * 15);
    
    // Left wall
    gfx->drawLine(centerX - wallWidth/2, centerY - wallHeight/2, 
                  centerX - wallWidth/2, centerY + wallHeight/2, wallColor);
    // Right wall
    gfx->drawLine(centerX + wallWidth/2, centerY - wallHeight/2,
                  centerX + wallWidth/2, centerY + wallHeight/2, wallColor);
    // Top wall
    gfx->drawLine(centerX - wallWidth/2, centerY - wallHeight/2,
                  centerX + wallWidth/2, centerY - wallHeight/2, wallColor);
    // Bottom wall
    gfx->drawLine(centerX - wallWidth/2, centerY + wallHeight/2,
                  centerX + wallWidth/2, centerY + wallHeight/2, wallColor);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("MAZE3D");
}

void drawMaze() {
  static float mazeTime = 0;
  static int mazeGrid[20][15];
  static bool initialized = false;
  
  if (!initialized) {
    // Initialize maze
    for (int y = 0; y < 15; y++) {
      for (int x = 0; x < 20; x++) {
        mazeGrid[x][y] = random(3);
      }
    }
    initialized = true;
  }
  
  mazeTime += 0.1;
  if ((int)mazeTime % 100 == 0) {
    initialized = false; // Regenerate maze
  }
  
  gfx->fillScreen(BLACK);
  
  // Draw maze
  for (int y = 0; y < 15; y++) {
    for (int x = 0; x < 20; x++) {
      int cellX = x * 16;
      int cellY = y * 16;
      
      if (mazeGrid[x][y] == 1) {
        gfx->fillRect(cellX, cellY, 16, 16, BLUE);
      } else if (mazeGrid[x][y] == 2) {
        gfx->drawRect(cellX, cellY, 16, 16, GREEN);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("MAZE");
}

void drawMemscroller() {
  static float memTime = 0;
  memTime += 0.05;
  
  gfx->fillScreen(BLACK);
  gfx->setTextColor(GREEN);
  
  // Scrolling memory dump
  for (int line = 0; line < 20; line++) {
    int y = (line * 12 - (int)(memTime * 30)) % (gfx->height() + 24);
    if (y > -12 && y < gfx->height()) {
      gfx->setCursor(0, y);
      gfx->printf("%04X: ", ((int)memTime * 16 + line * 16) & 0xFFFF);
      
      for (int i = 0; i < 8; i++) {
        gfx->printf("%02X ", random(256));
      }
    }
  }
  
  gfx->setCursor(10, 200);
  gfx->setTextColor(CYAN);
  gfx->printf("MEMSCROLLER");
}

void drawMenger() {
  static float mengerTime = 0;
  mengerTime += 0.02;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Menger sponge fractal approximation
  for (int level = 0; level < 4; level++) {
    int size = 120 - level * 25;
    float angle = mengerTime + level * PI/4;
    
    for (int face = 0; face < 6; face++) {
      float faceAngle = face * PI/3 + angle;
      int x = centerX + (int)(cos(faceAngle) * (size/2));
      int y = centerY + (int)(sin(faceAngle) * (size/2));
      
      uint16_t color = gfx->color565(255 - level * 50, level * 60, 128 + level * 30);
      
      if (level % 3 != 1) { // Menger sponge holes
        gfx->fillRect(x - size/6, y - size/6, size/3, size/3, color);
      }
      gfx->drawRect(x - size/2, y - size/2, size, size, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("MENGER");
}

void drawMetaballs() {
  static float ballTime = 0;
  ballTime += 0.03;
  
  gfx->fillScreen(BLACK);
  
  // Multiple metaballs
  for (int y = 0; y < gfx->height(); y += 2) {
    for (int x = 0; x < gfx->width(); x += 2) {
      float field = 0;
      
      // Calculate field from 4 moving balls
      for (int ball = 0; ball < 4; ball++) {
        float ballX = gfx->width()/2 + cos(ballTime + ball * PI/2) * 80;
        float ballY = gfx->height()/2 + sin(ballTime * 0.7 + ball * PI/2) * 60;
        
        float dist = sqrt((x - ballX) * (x - ballX) + (y - ballY) * (y - ballY));
        if (dist > 0) field += 1000 / (dist * dist);
      }
      
      if (field > 1) {
        uint16_t color = gfx->color565(
          min(255, (int)(field * 50)),
          min(255, (int)(field * 30)),
          min(255, (int)(field * 80))
        );
        gfx->drawPixel(x, y, color);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("METABALLS");
}

void drawMinixpm() {
  static float xpmTime = 0;
  xpmTime += 0.04;
  
  gfx->fillScreen(BLACK);
  
  // Mini XPM-style pixelated graphics
  int pixelSize = 8;
  for (int y = 0; y < gfx->height(); y += pixelSize) {
    for (int x = 0; x < gfx->width(); x += pixelSize) {
      float pattern = sin((x + xpmTime * 50) * 0.1) * cos((y + xpmTime * 30) * 0.1);
      
      uint16_t color;
      if (pattern > 0.5) color = RED;
      else if (pattern > 0) color = YELLOW;
      else if (pattern > -0.5) color = GREEN;
      else color = BLUE;
      
      gfx->fillRect(x, y, pixelSize-1, pixelSize-1, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("MINIXPM");
}

void drawMirrorblob() {
  static float blobTime = 0;
  blobTime += 0.03;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Mirrored blob across 4 quadrants
  for (int quadrant = 0; quadrant < 4; quadrant++) {
    float qAngle = quadrant * PI/2;
    int mirrorX = (quadrant % 2) ? -1 : 1;
    int mirrorY = (quadrant > 1) ? -1 : 1;
    
    for (int i = 0; i < 20; i++) {
      float angle = i * PI/10 + blobTime;
      float radius = 50 + sin(blobTime * 2 + i * 0.5) * 30;
      
      int x = centerX + mirrorX * (int)(cos(angle) * radius);
      int y = centerY + mirrorY * (int)(sin(angle) * radius);
      
      uint16_t color = gfx->color565(
        128 + (int)(sin(angle) * 127),
        128 + (int)(cos(angle * 1.3) * 127),
        128 + (int)(sin(angle * 0.7) * 127)
      );
      
      gfx->fillCircle(x, y, 8, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("MIRRORBLOB");
}

void drawMoebius() {
  static float moebiusTime = 0;
  moebiusTime += 0.02;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Möbius strip visualization
  for (int t = 0; t < 100; t++) {
    float u = t * 2 * PI / 100;
    float v = sin(moebiusTime + u * 3) * 0.3;
    
    // Möbius strip parametric equations (simplified for 2D)
    float x = (2 + v * cos(u/2)) * cos(u);
    float y = (2 + v * cos(u/2)) * sin(u);
    
    int screenX = centerX + (int)(x * 30);
    int screenY = centerY + (int)(y * 30);
    
    uint16_t color = gfx->color565(
      128 + (int)(sin(u) * 127),
      128 + (int)(cos(u * 1.3) * 127),
      255
    );
    
    if (screenX >= 0 && screenX < gfx->width() && screenY >= 0 && screenY < gfx->height()) {
      gfx->drawPixel(screenX, screenY, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("MOEBIUS");
}

void drawMoebiusgears() {
  static float gearTime = 0;
  gearTime += 0.05;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Two interlocking gears in Möbius configuration
  for (int gear = 0; gear < 2; gear++) {
    float gearAngle = gearTime * (gear ? -1 : 1);
    int gearX = centerX + (gear ? 40 : -40);
    int gearY = centerY;
    
    // Draw gear teeth
    for (int tooth = 0; tooth < 12; tooth++) {
      float toothAngle = tooth * PI/6 + gearAngle;
      int innerRadius = 30;
      int outerRadius = 45;
      
      int x1 = gearX + (int)(cos(toothAngle) * innerRadius);
      int y1 = gearY + (int)(sin(toothAngle) * innerRadius);
      int x2 = gearX + (int)(cos(toothAngle) * outerRadius);
      int y2 = gearY + (int)(sin(toothAngle) * outerRadius);
      
      uint16_t color = gear ? CYAN : YELLOW;
      gfx->drawLine(x1, y1, x2, y2, color);
    }
    
    gfx->drawCircle(gearX, gearY, 30, gear ? CYAN : YELLOW);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("MOEBIUSGEARS");
}

void drawMoire2() {
  static float moireTime = 0;
  moireTime += 0.02;
  
  gfx->fillScreen(BLACK);
  
  // Second moiré pattern with different frequencies
  for (int y = 0; y < gfx->height(); y += 2) {
    for (int x = 0; x < gfx->width(); x += 2) {
      float pattern1 = sin((x + moireTime * 30) * 0.08) * sin((y + moireTime * 20) * 0.08);
      float pattern2 = sin((x - moireTime * 25) * 0.12) * sin((y - moireTime * 35) * 0.06);
      
      float interference = pattern1 * pattern2;
      
      if (interference > 0.3) {
        uint16_t intensity = (int)(interference * 255);
        gfx->drawPixel(x, y, gfx->color565(intensity, intensity/2, intensity));
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("MOIRE2");
}

void drawMoire() {
  static float moireTime = 0;
  moireTime += 0.03;
  
  gfx->fillScreen(BLACK);
  
  // Classic moiré interference pattern
  for (int y = 0; y < gfx->height(); y += 1) {
    for (int x = 0; x < gfx->width(); x += 1) {
      float pattern1 = sin((x + moireTime * 20) * 0.1);
      float pattern2 = sin((x + moireTime * 15) * 0.11) * sin((y + moireTime * 10) * 0.1);
      
      float interference = pattern1 * pattern2;
      
      if (interference > 0.5) {
        gfx->drawPixel(x, y, WHITE);
      } else if (interference < -0.5) {
        gfx->drawPixel(x, y, gfx->color565(100, 100, 255));
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(YELLOW);
  gfx->printf("MOIRE");
}

void drawMorph3d() {
  static float morphTime = 0;
  morphTime += 0.03;
  
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Morphing 3D object
  for (int vertex = 0; vertex < 12; vertex++) {
    float angle = vertex * PI/6 + morphTime;
    
    // Morph between cube and sphere
    float morphFactor = (sin(morphTime * 2) + 1) / 2;
    float cubeRadius = 60;
    float sphereRadius = 50 + sin(morphTime * 3) * 20;
    
    float radius = cubeRadius * (1 - morphFactor) + sphereRadius * morphFactor;
    
    // Add 3D rotation
    float x3d = cos(angle) * radius;
    float y3d = sin(angle) * radius;
    float z3d = sin(morphTime * 1.5 + vertex * 0.5) * 30;
    
    // Project to 2D
    int x = centerX + (int)(x3d * (200 / (200 + z3d)));
    int y = centerY + (int)(y3d * (200 / (200 + z3d)));
    
    uint16_t color = gfx->color565(
      128 + (int)(sin(angle) * 127),
      128 + (int)(cos(angle * 1.3) * 127),
      128 + (int)(z3d * 2)
    );
    
    gfx->fillCircle(x, y, 6, color);
    
    // Connect to next vertex
    if (vertex < 11) {
      float nextAngle = (vertex + 1) * PI/6 + morphTime;
      float nextRadius = cubeRadius * (1 - morphFactor) + sphereRadius * morphFactor;
      float nextX3d = cos(nextAngle) * nextRadius;
      float nextY3d = sin(nextAngle) * nextRadius;
      float nextZ3d = sin(morphTime * 1.5 + (vertex + 1) * 0.5) * 30;
      
      int nextX = centerX + (int)(nextX3d * (200 / (200 + nextZ3d)));
      int nextY = centerY + (int)(nextY3d * (200 / (200 + nextZ3d)));
      
      gfx->drawLine(x, y, nextX, nextY, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("MORPH3D");
}

void drawMunch() {
  static float munchTime = 0;
  munchTime += 0.05;
  
  gfx->fillScreen(BLACK);
  
  // Munching squares (classic bit manipulation pattern)
  for (int y = 0; y < gfx->height(); y += 4) {
    for (int x = 0; x < gfx->width(); x += 4) {
      int value = (x + (int)(munchTime * 10)) ^ (y + (int)(munchTime * 8));
      
      uint16_t color = gfx->color565(
        value & 255,
        (value >> 1) & 255,
        (value >> 2) & 255
      );
      
      gfx->fillRect(x, y, 4, 4, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("MUNCH");
}

void drawNakagin() {
  // Nakagin capsule tower effect - modular living pods
  gfx->fillScreen(gfx->color565(20, 25, 35));
  
  for (int i = 0; i < 8; i++) {
    int x = 30 + (i % 4) * 45;
    int y = 40 + (i / 4) * 80 + sin(animationTime + i) * 10;
    
    // Pod structure
    gfx->fillRoundRect(x-15, y-20, 30, 40, 8, gfx->color565(180, 180, 180));
    gfx->drawRoundRect(x-15, y-20, 30, 40, 8, WHITE);
    
    // Window
    gfx->fillCircle(x, y-5, 8, gfx->color565(100, 150, 255));
    if (sin(animationTime * 2 + i) > 0.5) {
      gfx->fillCircle(x, y-5, 6, YELLOW); // Light on
    }
    
    // Connection pipes
    if (i < 4) {
      gfx->drawLine(x, y+20, x, y+60, gfx->color565(150, 150, 150));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("NAKAGIN");
}

void drawNerverot() {
  // Nerve rot effect - chaotic neural network
  gfx->fillScreen(BLACK);
  
  static int neurons[20][3]; // x, y, charge
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 20; i++) {
      neurons[i][0] = random(20, gfx->width()-20);
      neurons[i][1] = random(20, gfx->height()-20);
      neurons[i][2] = random(0, 255);
    }
    initialized = true;
  }
  
  // Update neuron charges
  for (int i = 0; i < 20; i++) {
    neurons[i][2] = (neurons[i][2] + random(-30, 31)) % 256;
    if (neurons[i][2] < 0) neurons[i][2] = 0;
  }
  
  // Draw connections
  for (int i = 0; i < 20; i++) {
    for (int j = i+1; j < 20; j++) {
      int dx = neurons[i][0] - neurons[j][0];
      int dy = neurons[i][1] - neurons[j][1];
      int dist = sqrt(dx*dx + dy*dy);
      
      if (dist < 80 && (neurons[i][2] > 128 || neurons[j][2] > 128)) {
        int intensity = 255 - (dist * 3);
        if (intensity > 0) {
          gfx->drawLine(neurons[i][0], neurons[i][1], neurons[j][0], neurons[j][1], 
                       gfx->color565(intensity, 0, intensity/2));
        }
      }
    }
  }
  
  // Draw neurons
  for (int i = 0; i < 20; i++) {
    int size = 2 + neurons[i][2] / 64;
    uint16_t color = gfx->color565(neurons[i][2], neurons[i][2]/4, neurons[i][2]/2);
    gfx->fillCircle(neurons[i][0], neurons[i][1], size, color);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("NERVEROT");
}

void drawNoof() {
  // Noof effect - floating spheres with trails
  gfx->fillScreen(gfx->color565(10, 10, 30));
  
  static float spheres[8][4]; // x, y, vx, vy
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 8; i++) {
      spheres[i][0] = random(0, gfx->width());
      spheres[i][1] = random(0, gfx->height());
      spheres[i][2] = random(-2, 3) * 0.5;
      spheres[i][3] = random(-2, 3) * 0.5;
    }
    initialized = true;
  }
  
  // Update and draw spheres
  for (int i = 0; i < 8; i++) {
    // Update position
    spheres[i][0] += spheres[i][2];
    spheres[i][1] += spheres[i][3];
    
    // Bounce off edges
    if (spheres[i][0] < 10 || spheres[i][0] > gfx->width()-10) spheres[i][2] *= -1;
    if (spheres[i][1] < 10 || spheres[i][1] > gfx->height()-10) spheres[i][3] *= -1;
    
    // Draw trail
    for (int t = 0; t < 5; t++) {
      int tx = spheres[i][0] - spheres[i][2] * t * 3;
      int ty = spheres[i][1] - spheres[i][3] * t * 3;
      int alpha = 255 - t * 50;
      if (alpha > 0) {
        gfx->fillCircle(tx, ty, 3-t/2, gfx->color565(alpha/3, alpha/2, alpha));
      }
    }
    
    // Draw main sphere
    gfx->fillCircle((int)spheres[i][0], (int)spheres[i][1], 6, 
                   gfx->color565(100 + i*20, 150 + sin(animationTime + i)*50, 255));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("NOOF");
}

void drawNormals() {
  // Normals effect - 3D surface normal visualization
  gfx->fillScreen(BLACK);
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int i = 0; i < 15; i++) {
    for (int j = 0; j < 12; j++) {
      float x = (i - 7.5) * 15;
      float y = (j - 6) * 15;
      float z = sin(animationTime + x*0.1) * cos(animationTime*1.2 + y*0.1) * 20;
      
      // Calculate normal vector (simplified)
      float nx = cos(animationTime + x*0.1) * 0.1 * cos(animationTime*1.2 + y*0.1);
      float ny = sin(animationTime + x*0.1) * (-sin(animationTime*1.2 + y*0.1)) * 0.1;
      float nz = 1.0;
      
      // Normalize
      float len = sqrt(nx*nx + ny*ny + nz*nz);
      nx /= len; ny /= len; nz /= len;
      
      int px = centerX + x;
      int py = centerY + y + z*0.5;
      
      // Draw point
      gfx->drawPixel(px, py, WHITE);
      
      // Draw normal vector
      int ex = px + nx * 15;
      int ey = py + ny * 15;
      
      uint16_t color = gfx->color565((nz + 1) * 127, abs(nx) * 255, abs(ny) * 255);
      gfx->drawLine(px, py, ex, ey, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("NORMALS");
}

void drawNoseguy() {
  // Noseguy effect - animated character walking
  gfx->fillScreen(gfx->color565(50, 100, 150));
  
  static float noseX = 0;
  static float walkCycle = 0;
  
  noseX += 1.5;
  if (noseX > gfx->width() + 50) noseX = -50;
  walkCycle += 0.3;
  
  int x = (int)noseX;
  int y = gfx->height() / 2;
  int bounce = sin(walkCycle) * 3;
  
  // Body
  gfx->fillCircle(x, y + bounce, 12, gfx->color565(255, 200, 150));
  
  // Head
  gfx->fillCircle(x, y - 15 + bounce, 10, gfx->color565(255, 220, 180));
  
  // Big nose
  gfx->fillCircle(x + 8, y - 15 + bounce, 6, gfx->color565(255, 180, 120));
  
  // Eyes
  gfx->fillCircle(x - 3, y - 18 + bounce, 2, BLACK);
  gfx->fillCircle(x + 2, y - 18 + bounce, 2, BLACK);
  
  // Legs (walking animation)
  int leg1Y = y + 12 + bounce + abs(sin(walkCycle)) * 5;
  int leg2Y = y + 12 + bounce + abs(sin(walkCycle + PI)) * 5;
  
  gfx->drawLine(x - 5, y + 5 + bounce, x - 8, leg1Y, BLACK);
  gfx->drawLine(x + 5, y + 5 + bounce, x + 8, leg2Y, BLACK);
  
  // Arms (swinging)
  int arm1X = x - 10 + sin(walkCycle) * 3;
  int arm2X = x + 10 + sin(walkCycle + PI) * 3;
  gfx->drawLine(x - 8, y - 5 + bounce, arm1X, y + 5 + bounce, BLACK);
  gfx->drawLine(x + 8, y - 5 + bounce, arm2X, y + 5 + bounce, BLACK);
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("NOSEGUY");
}

void drawOverlay() {
  // Overlay effect - multiple layered patterns
  gfx->fillScreen(BLACK);
  
  // Layer 1: Background grid
  for (int x = 0; x < gfx->width(); x += 20) {
    for (int y = 0; y < gfx->height(); y += 20) {
      if ((x/20 + y/20) % 2 == 0) {
        gfx->fillRect(x, y, 20, 20, gfx->color565(20, 20, 40));
      }
    }
  }
  
  // Layer 2: Rotating circles
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int i = 0; i < 6; i++) {
    float angle = animationTime * 0.5 + i * PI / 3;
    int x = centerX + cos(angle) * 40;
    int y = centerY + sin(angle) * 30;
    
    gfx->drawCircle(x, y, 15, gfx->color565(100 + i*25, 150, 200 - i*20));
  }
  
  // Layer 3: Scanning lines
  for (int i = 0; i < 5; i++) {
    int lineY = (int)(sin(animationTime * 2 + i) * gfx->height()/2 + gfx->height()/2);
    for (int x = 0; x < gfx->width(); x += 3) {
      gfx->drawPixel(x, lineY, gfx->color565(255, 255, 0));
    }
  }
  
  // Layer 4: Crosshair overlay
  gfx->drawLine(centerX - 30, centerY, centerX + 30, centerY, RED);
  gfx->drawLine(centerX, centerY - 30, centerX, centerY + 30, RED);
  gfx->drawCircle(centerX, centerY, 25, RED);
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("OVERLAY");
}

void drawPacmanAI() {
  // Pacman AI effect - intelligent ghost movement
  gfx->fillScreen(BLACK);
  
  static int pacX = 50, pacY = 100;
  static int ghostX = 200, ghostY = 150;
  static float aiTimer = 0;
  
  aiTimer += 0.1;
  
  // AI movement for ghost (chase Pacman)
  if (aiTimer > 1.0) {
    if (ghostX < pacX) ghostX += 2;
    else if (ghostX > pacX) ghostX -= 2;
    
    if (ghostY < pacY) ghostY += 2;
    else if (ghostY > pacY) ghostY -= 2;
    
    aiTimer = 0;
  }
  
  // Pacman movement (random walk)
  pacX += random(-3, 4);
  pacY += random(-3, 4);
  
  // Keep in bounds
  pacX = constrain(pacX, 20, gfx->width()-20);
  pacY = constrain(pacY, 20, gfx->height()-20);
  ghostX = constrain(ghostX, 20, gfx->width()-20);
  ghostY = constrain(ghostY, 20, gfx->height()-20);
  
  // Draw maze background
  for (int x = 0; x < gfx->width(); x += 40) {
    gfx->drawLine(x, 0, x, gfx->height(), gfx->color565(0, 0, 100));
  }
  for (int y = 0; y < gfx->height(); y += 40) {
    gfx->drawLine(0, y, gfx->width(), y, gfx->color565(0, 0, 100));
  }
  
  // Draw dots
  for (int x = 20; x < gfx->width(); x += 20) {
    for (int y = 20; y < gfx->height(); y += 20) {
      gfx->fillCircle(x, y, 2, WHITE);
    }
  }
  
  // Draw Pacman
  gfx->fillCircle(pacX, pacY, 12, YELLOW);
  int mouthX = pacX + cos(animationTime * 5) * 8;
  int mouthY = pacY + sin(animationTime * 5) * 8;
  gfx->drawLine(pacX, pacY, mouthX, mouthY, BLACK);
  
  // Draw Ghost with AI indicator
  gfx->fillCircle(ghostX, ghostY, 12, RED);
  gfx->fillCircle(ghostX - 4, ghostY - 4, 3, WHITE); // Eyes
  gfx->fillCircle(ghostX + 4, ghostY - 4, 3, WHITE);
  gfx->fillCircle(ghostX - 4, ghostY - 4, 1, BLACK);
  gfx->fillCircle(ghostX + 4, ghostY - 4, 1, BLACK);
  
  // AI decision visualization
  gfx->drawLine(ghostX, ghostY, pacX, pacY, gfx->color565(255, 100, 100));
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("PACMAN AI");
}

void drawPacman() {
  // Classic Pacman effect
  gfx->fillScreen(BLACK);
  
  static float pacmanX = 0;
  static float pacmanY = 120;
  static float mouthAngle = 0;
  
  pacmanX += 2;
  if (pacmanX > gfx->width() + 20) {
    pacmanX = -20;
    pacmanY = random(50, gfx->height() - 50);
  }
  
  mouthAngle += 0.3;
  
  // Draw dots in path
  for (int x = 0; x < gfx->width(); x += 15) {
    if (x > pacmanX + 20) {
      gfx->fillCircle(x, pacmanY, 2, WHITE);
    }
  }
  
  // Draw Pacman
  gfx->fillCircle(pacmanX, pacmanY, 15, YELLOW);
  
  // Mouth animation
  float mouthOpen = abs(sin(mouthAngle)) * 10 + 5;
  int mouthX1 = pacmanX + cos(PI/6) * mouthOpen;
  int mouthY1 = pacmanY - sin(PI/6) * mouthOpen;
  int mouthX2 = pacmanX + cos(-PI/6) * mouthOpen;
  int mouthY2 = pacmanY + sin(PI/6) * mouthOpen;
  
  gfx->drawLine(pacmanX, pacmanY, mouthX1, mouthY1, BLACK);
  gfx->drawLine(pacmanX, pacmanY, mouthX2, mouthY2, BLACK);
  gfx->drawLine(pacmanX, pacmanY, pacmanX + 15, pacmanY, BLACK);
  
  // Ghosts following
  static float ghost1X = -50, ghost2X = -80, ghost3X = -110;
  ghost1X += 1.5;
  ghost2X += 1.3;
  ghost3X += 1.1;
  
  if (ghost1X > gfx->width() + 20) ghost1X = -50;
  if (ghost2X > gfx->width() + 20) ghost2X = -80;
  if (ghost3X > gfx->width() + 20) ghost3X = -110;
  
  // Draw ghosts
  uint16_t ghostColors[] = {RED, gfx->color565(255, 100, 255), gfx->color565(0, 255, 255)};
  float ghostXs[] = {ghost1X, ghost2X, ghost3X};
  
  for (int i = 0; i < 3; i++) {
    if (ghostXs[i] > -20) {
      gfx->fillCircle(ghostXs[i], pacmanY, 12, ghostColors[i]);
      gfx->fillCircle(ghostXs[i] - 4, pacmanY - 4, 2, WHITE);
      gfx->fillCircle(ghostXs[i] + 4, pacmanY - 4, 2, WHITE);
      gfx->fillCircle(ghostXs[i] - 4, pacmanY - 4, 1, BLACK);
      gfx->fillCircle(ghostXs[i] + 4, pacmanY - 4, 1, BLACK);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("PACMAN");
}

void drawPacmanLevel() {
  // Pacman level effect - full maze view
  gfx->fillScreen(BLACK);
  
  // Draw maze walls
  uint16_t wallColor = gfx->color565(0, 0, 255);
  
  // Outer walls
  gfx->drawRect(10, 10, gfx->width()-20, gfx->height()-20, wallColor);
  
  // Internal maze structure
  for (int i = 0; i < 8; i++) {
    int x = 30 + i * 30;
    int y = 40 + (i % 3) * 40;
    
    if (i % 2 == 0) {
      gfx->fillRect(x, y, 15, 30, wallColor);
    } else {
      gfx->fillRect(x, y, 30, 15, wallColor);
    }
  }
  
  // Draw dots
  for (int x = 20; x < gfx->width()-20; x += 20) {
    for (int y = 20; y < gfx->height()-20; y += 20) {
      bool isWall = false;
      
      // Check if position is on a wall (simplified)
      for (int i = 0; i < 8; i++) {
        int wx = 30 + i * 30;
        int wy = 40 + (i % 3) * 40;
        
        if ((x >= wx && x <= wx+30 && y >= wy && y <= wy+30)) {
          isWall = true;
          break;
        }
      }
      
      if (!isWall) {
        if ((x + y) % 40 == 0) {
          gfx->fillCircle(x, y, 3, YELLOW); // Power pellet
        } else {
          gfx->fillCircle(x, y, 1, WHITE); // Regular dot
        }
      }
    }
  }
  
  // Moving Pacman
  static float levelPacX = 20, levelPacY = 20;
  static int direction = 0; // 0=right, 1=down, 2=left, 3=up
  
  switch (direction) {
    case 0: levelPacX += 1; break;
    case 1: levelPacY += 1; break;
    case 2: levelPacX -= 1; break;
    case 3: levelPacY -= 1; break;
  }
  
  // Change direction randomly
  if (random(0, 100) < 5) {
    direction = random(0, 4);
  }
  
  // Keep in bounds
  if (levelPacX < 20) { levelPacX = 20; direction = 0; }
  if (levelPacX > gfx->width()-20) { levelPacX = gfx->width()-20; direction = 2; }
  if (levelPacY < 20) { levelPacY = 20; direction = 1; }
  if (levelPacY > gfx->height()-20) { levelPacY = gfx->height()-20; direction = 3; }
  
  gfx->fillCircle(levelPacX, levelPacY, 8, YELLOW);
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("PACMAN LEVEL");
}

void drawPapercube() {
  // Paper cube effect - origami-style folding cube
  gfx->fillScreen(gfx->color565(240, 230, 210)); // Paper background
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  float cubeSize = 40 + sin(animationTime) * 10;
  float rotX = animationTime * 0.7;
  float rotY = animationTime * 0.5;
  
  // Cube vertices (8 corners)
  float vertices[8][3] = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
    {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}
  };
  
  int projected[8][2];
  
  // Project vertices
  for (int i = 0; i < 8; i++) {
    float x = vertices[i][0] * cubeSize;
    float y = vertices[i][1] * cubeSize;
    float z = vertices[i][2] * cubeSize;
    
    // Rotate
    float rotatedX = x * cos(rotY) - z * sin(rotY);
    float rotatedZ = x * sin(rotY) + z * cos(rotY);
    float rotatedY = y * cos(rotX) - rotatedZ * sin(rotX);
    rotatedZ = y * sin(rotX) + rotatedZ * cos(rotX);
    
    // Project to 2D
    projected[i][0] = centerX + rotatedX;
    projected[i][1] = centerY + rotatedY;
  }
  
  // Draw cube faces with paper texture
  int faces[6][4] = {
    {0, 1, 2, 3}, {4, 7, 6, 5}, {0, 4, 5, 1},
    {2, 6, 7, 3}, {0, 3, 7, 4}, {1, 5, 6, 2}
  };
  
  uint16_t faceColors[] = {
    gfx->color565(200, 150, 100), gfx->color565(180, 130, 80),
    gfx->color565(220, 170, 120), gfx->color565(160, 110, 60),
    gfx->color565(240, 190, 140), gfx->color565(140, 90, 40)
  };
  
  for (int f = 0; f < 6; f++) {
    // Draw face outline with fold lines
    for (int i = 0; i < 4; i++) {
      int curr = faces[f][i];
      int next = faces[f][(i + 1) % 4];
      
      gfx->drawLine(projected[curr][0], projected[curr][1],
                   projected[next][0], projected[next][1], 
                   gfx->color565(100, 50, 0)); // Fold line color
    }
    
    // Fill face center with paper texture simulation
    int centerFaceX = 0, centerFaceY = 0;
    for (int i = 0; i < 4; i++) {
      centerFaceX += projected[faces[f][i]][0];
      centerFaceY += projected[faces[f][i]][1];
    }
    centerFaceX /= 4;
    centerFaceY /= 4;
    
    gfx->fillCircle(centerFaceX, centerFaceY, 8, faceColors[f]);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(100, 50, 0));
  gfx->printf("PAPERCUBE");
}

void drawPasswd() {
  // Password effect - matrix-style login screen
  gfx->fillScreen(BLACK);
  
  static char passwd[16] = "************";
  static int cursorPos = 0;
  static float blinkTimer = 0;
  
  blinkTimer += 0.1;
  cursorPos = (int)(animationTime * 2) % 12;
  
  // Terminal background
  gfx->fillRect(20, 40, gfx->width()-40, gfx->height()-80, gfx->color565(0, 20, 0));
  gfx->drawRect(20, 40, gfx->width()-40, gfx->height()-80, GREEN);
  
  // Header
  gfx->setCursor(30, 50);
  gfx->setTextColor(GREEN);
  gfx->setTextSize(1);
  gfx->printf("SYSTEM LOGIN v2.1");
  
  gfx->setCursor(30, 70);
  gfx->printf("Username: admin");
  
  gfx->setCursor(30, 90);
  gfx->printf("Password: ");
  
  // Animated password field
  for (int i = 0; i < 12; i++) {
    char c = '*';
    if (i <= cursorPos && sin(blinkTimer * 10) > 0) {
      c = (char)('A' + random(0, 26)); // Random letter
    }
    gfx->printf("%c", c);
  }
  
  // Cursor
  if (sin(blinkTimer * 5) > 0) {
    gfx->setCursor(30 + (12 + 1) * 6, 90);
    gfx->printf("_");
  }
  
  // Status messages
  gfx->setCursor(30, 120);
  if (cursorPos < 8) {
    gfx->setTextColor(YELLOW);
    gfx->printf("Authenticating...");
  } else if (cursorPos < 10) {
    gfx->setTextColor(RED);
    gfx->printf("Access Denied");
  } else {
    gfx->setTextColor(GREEN);
    gfx->printf("Access Granted");
  }
  
  // Matrix rain effect in background
  for (int i = 0; i < 10; i++) {
    int x = 200 + i * 15;
    int y = (int)(animationTime * 50 + i * 30) % (gfx->height() + 50);
    
    gfx->setCursor(x, y);
    gfx->setTextColor(gfx->color565(0, 100 + random(0, 155), 0));
    gfx->printf("%c", 'A' + random(0, 26));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("PASSWD");
}

void drawPasswdKerberos() {
  // Kerberos authentication effect
  gfx->fillScreen(BLACK);
  
  // Kerberos 3-way authentication visualization
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Draw authentication triangle
  int points[3][2] = {
    {centerX, centerY - 50},        // KDC (top)
    {centerX - 60, centerY + 30},   // Client (bottom left)
    {centerX + 60, centerY + 30}    // Server (bottom right)
  };
  
  // Draw connection lines with data flow animation
  for (int i = 0; i < 3; i++) {
    int next = (i + 1) % 3;
    
    // Animated data packets
    float progress = fmod(animationTime * 2 + i * 2, 6.0) / 6.0;
    int packetX = points[i][0] + (points[next][0] - points[i][0]) * progress;
    int packetY = points[i][1] + (points[next][1] - points[i][1]) * progress;
    
    gfx->drawLine(points[i][0], points[i][1], points[next][0], points[next][1], YELLOW);
    gfx->fillCircle(packetX, packetY, 3, RED);
  }
  
  // Draw nodes
  gfx->fillCircle(points[0][0], points[0][1], 15, gfx->color565(100, 100, 255)); // KDC
  gfx->fillCircle(points[1][0], points[1][1], 12, GREEN);   // Client
  gfx->fillCircle(points[2][0], points[2][1], 12, gfx->color565(255, 100, 100)); // Server
  
  // Labels
  gfx->setCursor(points[0][0] - 15, points[0][1] - 25);
  gfx->setTextColor(WHITE);
  gfx->printf("KDC");
  
  gfx->setCursor(points[1][0] - 25, points[1][1] + 20);
  gfx->printf("CLIENT");
  
  gfx->setCursor(points[2][0] - 10, points[2][1] + 20);
  gfx->printf("SERVER");
  
  // Authentication steps
  gfx->setCursor(10, gfx->height() - 40);
  int step = ((int)(animationTime * 2)) % 4;
  switch (step) {
    case 0:
      gfx->setTextColor(YELLOW);
      gfx->printf("1. Client -> KDC: Request TGT");
      break;
    case 1:
      gfx->setTextColor(GREEN);
      gfx->printf("2. KDC -> Client: TGT + Session Key");
      break;
    case 2:
      gfx->setTextColor(gfx->color565(255, 100, 255));
      gfx->printf("3. Client -> Server: Service Request");
      break;
    case 3:
      gfx->setTextColor(gfx->color565(100, 255, 100));
      gfx->printf("4. Server -> Client: Service Access");
      break;
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("KERBEROS");
}

void drawPasswdPam() {
  // PAM (Pluggable Authentication Modules) effect
  gfx->fillScreen(gfx->color565(20, 20, 40));
  
  // PAM module stack visualization
  const char* modules[] = {"pam_unix", "pam_ldap", "pam_krb5", "pam_radius"};
  int moduleCount = 4;
  
  for (int i = 0; i < moduleCount; i++) {
    int y = 50 + i * 40;
    int moduleWidth = 150;
    int x = (gfx->width() - moduleWidth) / 2;
    
    // Module background
    uint16_t moduleColor = gfx->color565(60 + i*30, 80 + i*20, 120 + i*10);
    gfx->fillRect(x, y, moduleWidth, 30, moduleColor);
    gfx->drawRect(x, y, moduleWidth, 30, WHITE);
    
    // Module label
    gfx->setCursor(x + 10, y + 10);
    gfx->setTextColor(WHITE);
    gfx->printf("%s", modules[i]);
    
    // Authentication flow indicator
    float progress = fmod(animationTime - i * 0.5, 4.0) / 4.0;
    if (progress > 0 && progress < 1) {
      int indicatorX = x + progress * moduleWidth;
      gfx->drawLine(indicatorX, y, indicatorX, y + 30, YELLOW);
      
      // Status indicator
      if (progress > 0.8) {
        gfx->fillCircle(x + moduleWidth + 10, y + 15, 5, GREEN);
      } else {
        gfx->fillCircle(x + moduleWidth + 10, y + 15, 5, RED);
      }
    }
  }
  
  // Authentication result
  gfx->setCursor(50, 220);
  gfx->setTextColor(GREEN);
  int authState = ((int)(animationTime * 2)) % 3;
  switch (authState) {
    case 0:
      gfx->printf("Authentication in progress...");
      break;
    case 1:
      gfx->setTextColor(YELLOW);
      gfx->printf("Checking credentials...");
      break;
    case 2:
      gfx->setTextColor(GREEN);
      gfx->printf("Authentication successful!");
      break;
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("PAM AUTH");
}

void drawPasswdPwent() {
  // Password entry effect - /etc/passwd visualization
  gfx->fillScreen(BLACK);
  
  // Simulated /etc/passwd entries
  const char* users[] = {"root", "daemon", "bin", "sys", "admin", "user", "guest"};
  int userCount = 7;
  
  static float scrollOffset = 0;
  scrollOffset += 0.5;
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(GREEN);
  gfx->printf("/etc/passwd");
  
  for (int i = 0; i < userCount; i++) {
    int y = 40 + i * 25 - (int)scrollOffset % 175;
    
    if (y > -20 && y < gfx->height()) {
      // Username
      gfx->setCursor(10, y);
      gfx->setTextColor(YELLOW);
      gfx->printf("%s", users[i]);
      
      // UID/GID
      gfx->setCursor(80, y);
      gfx->setTextColor(WHITE);
      gfx->printf(":%d:%d:", 1000 + i, 1000 + i);
      
      // Home directory
      gfx->setCursor(140, y);
      gfx->setTextColor(gfx->color565(100, 200, 255));
      gfx->printf("/home/%s", users[i]);
      
      // Password hash visualization (animated)
      if (sin(animationTime + i) > 0) {
        gfx->setCursor(220, y);
        gfx->setTextColor(RED);
        gfx->printf("$6$...");
      }
    }
  }
  
  // Highlight current entry
  int highlightY = 40 + ((int)(animationTime * 3) % userCount) * 25 - (int)scrollOffset % 175;
  if (highlightY > 20 && highlightY < gfx->height() - 20) {
    gfx->drawRect(5, highlightY - 2, gfx->width() - 10, 20, GREEN);
  }
  
  // Security warning
  gfx->setCursor(10, gfx->height() - 20);
  gfx->setTextColor(RED);
  if (sin(animationTime * 4) > 0) {
    gfx->printf("WARNING: Password file access detected!");
  }
  
  gfx->setCursor(10, 25);
  gfx->setTextColor(WHITE);
  gfx->printf("PWENT");
}

void drawPedal() {
  // Pedal effect - bicycle pedaling motion
  gfx->fillScreen(gfx->color565(100, 150, 255)); // Sky blue
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Bicycle frame
  gfx->drawCircle(centerX - 40, centerY, 25, BLACK);     // Rear wheel
  gfx->drawCircle(centerX + 40, centerY, 25, BLACK);     // Front wheel
  gfx->drawLine(centerX - 15, centerY - 25, centerX + 15, centerY - 25, BLACK); // Top tube
  gfx->drawLine(centerX - 15, centerY - 25, centerX - 15, centerY + 15, BLACK); // Seat tube
  gfx->drawLine(centerX + 15, centerY - 25, centerX + 15, centerY + 15, BLACK); // Head tube
  
  // Pedal crank and pedals
  float pedalAngle = animationTime * 3;
  int crankLength = 20;
  
  // Crank arms
  int leftPedalX = centerX - 15 + cos(pedalAngle) * crankLength;
  int leftPedalY = centerY + sin(pedalAngle) * crankLength;
  int rightPedalX = centerX - 15 + cos(pedalAngle + PI) * crankLength;
  int rightPedalY = centerY + sin(pedalAngle + PI) * crankLength;
  
  gfx->drawLine(centerX - 15, centerY, leftPedalX, leftPedalY, BLACK);
  gfx->drawLine(centerX - 15, centerY, rightPedalX, rightPedalY, BLACK);
  
  // Pedals (always horizontal)
  gfx->fillRect(leftPedalX - 8, leftPedalY - 3, 16, 6, YELLOW);
  gfx->fillRect(rightPedalX - 8, rightPedalY - 3, 16, 6, YELLOW);
  
  // Chain (simplified)
  gfx->drawCircle(centerX - 15, centerY, 8, gfx->color565(150, 150, 150));
  gfx->drawCircle(centerX - 40, centerY, 15, gfx->color565(150, 150, 150));
  
  // Rider legs (animated)
  float legAngle1 = pedalAngle;
  float legAngle2 = pedalAngle + PI;
  
  int hipX = centerX - 15;
  int hipY = centerY - 35;
  
  // Left leg
  int kneeX = hipX + cos(legAngle1 + PI/4) * 15;
  int kneeY = hipY + sin(legAngle1 + PI/4) * 15;
  gfx->drawLine(hipX, hipY, kneeX, kneeY, gfx->color565(255, 200, 150)); // Thigh
  gfx->drawLine(kneeX, kneeY, leftPedalX, leftPedalY, gfx->color565(255, 200, 150)); // Shin
  
  // Right leg
  kneeX = hipX + cos(legAngle2 + PI/4) * 15;
  kneeY = hipY + sin(legAngle2 + PI/4) * 15;
  gfx->drawLine(hipX, hipY, kneeX, kneeY, gfx->color565(255, 200, 150)); // Thigh
  gfx->drawLine(kneeX, kneeY, rightPedalX, rightPedalY, gfx->color565(255, 200, 150)); // Shin
  
  // Ground
  gfx->drawLine(0, gfx->height() - 10, gfx->width(), gfx->height() - 10, GREEN);
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("PEDAL");
}

void drawPeepers() {
  // Peepers effect - many eyes watching
  gfx->fillScreen(gfx->color565(30, 0, 50)); // Dark purple background
  
  static int eyes[15][4]; // x, y, size, blinkTimer
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 15; i++) {
      eyes[i][0] = random(20, gfx->width() - 20);
      eyes[i][1] = random(20, gfx->height() - 20);
      eyes[i][2] = random(8, 20);
      eyes[i][3] = random(0, 100);
    }
    initialized = true;
  }
  
  // Update and draw eyes
  for (int i = 0; i < 15; i++) {
    int x = eyes[i][0];
    int y = eyes[i][1];
    int size = eyes[i][2];
    eyes[i][3]++;
    
    // Blink animation
    bool isBlinking = (eyes[i][3] % 120) > 115;
    
    if (!isBlinking) {
      // Draw eye
      gfx->fillCircle(x, y, size, WHITE);
      gfx->fillCircle(x, y, size - 3, gfx->color565(100, 150, 255)); // Blue iris
      
      // Pupil follows a target (screen center)
      int targetX = gfx->width() / 2 + sin(animationTime) * 50;
      int targetY = gfx->height() / 2 + cos(animationTime * 1.2) * 30;
      
      float dx = targetX - x;
      float dy = targetY - y;
      float distance = sqrt(dx * dx + dy * dy);
      
      if (distance > 0) {
        int pupilOffsetX = (dx / distance) * (size - 8);
        int pupilOffsetY = (dy / distance) * (size - 8);
        
        gfx->fillCircle(x + pupilOffsetX, y + pupilOffsetY, size / 3, BLACK);
        
        // Highlight
        gfx->fillCircle(x + pupilOffsetX - 2, y + pupilOffsetY - 2, 2, WHITE);
      }
    } else {
      // Blinking - draw closed eye
      gfx->drawLine(x - size, y, x + size, y, WHITE);
    }
    
    // Eye glow effect
    if (sin(animationTime * 2 + i) > 0.8) {
      gfx->drawCircle(x, y, size + 5, gfx->color565(255, 100, 100));
    }
  }
  
  // Spooky atmosphere effects
  for (int i = 0; i < 8; i++) {
    int x = random(0, gfx->width());
    int y = random(0, gfx->height());
    int brightness = random(20, 60);
    gfx->drawPixel(x, y, gfx->color565(brightness, 0, brightness/2));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("PEEPERS");
}

void drawPenetrate() {
  // Penetrate effect - objects passing through barriers
  gfx->fillScreen(BLACK);
  
  // Draw barriers
  for (int i = 0; i < 5; i++) {
    int x = 50 + i * 50;
    gfx->fillRect(x, 50, 10, gfx->height() - 100, gfx->color565(100, 100, 255));
  }
  
  // Penetrating objects
  static float objects[8][3]; // x, y, phase
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 8; i++) {
      objects[i][0] = random(-20, 20);
      objects[i][1] = random(60, gfx->height() - 60);
      objects[i][2] = i * 0.5;
    }
    initialized = true;
  }
  
  for (int i = 0; i < 8; i++) {
    objects[i][0] += 2;
    if (objects[i][0] > gfx->width() + 20) {
      objects[i][0] = -20;
      objects[i][1] = random(60, gfx->height() - 60);
    }
    
    int x = (int)objects[i][0];
    int y = (int)objects[i][1];
    
    // Check if object is intersecting with barrier
    bool penetrating = false;
    for (int b = 0; b < 5; b++) {
      int barrierX = 50 + b * 50;
      if (x >= barrierX && x <= barrierX + 10) {
        penetrating = true;
        break;
      }
    }
    
    // Draw object with penetration effect
    if (penetrating) {
      // Penetration visualization - object becomes translucent and distorted
      for (int px = -6; px <= 6; px++) {
        for (int py = -6; py <= 6; py++) {
          if (px*px + py*py <= 36) {
            int alpha = 128 + sin(animationTime * 8 + px + py) * 64;
            uint16_t color = gfx->color565(alpha, alpha/2, 0);
            gfx->drawPixel(x + px + sin(animationTime * 10) * 2, 
                          y + py + cos(animationTime * 12) * 2, color);
          }
        }
      }
      
      // Particle effect during penetration
      for (int p = 0; p < 5; p++) {
        int px = x + random(-15, 16);
        int py = y + random(-15, 16);
        gfx->drawPixel(px, py, gfx->color565(255, 255, 0));
      }
    } else {
      // Normal solid object
      gfx->fillCircle(x, y, 6, RED);
      gfx->drawCircle(x, y, 6, WHITE);
    }
    
    // Trail effect
    for (int t = 1; t < 5; t++) {
      int trailX = x - t * 8;
      int trailAlpha = 255 - t * 50;
      if (trailAlpha > 0 && trailX > 0) {
        gfx->fillCircle(trailX, y, 6 - t, gfx->color565(trailAlpha, 0, 0));
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("PENETRATE");
}

void drawPenrose() {
  // Penrose triangle effect - impossible geometry
  gfx->fillScreen(gfx->color565(40, 40, 60));
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  float rotation = animationTime * 0.3;
  int size = 60 + sin(animationTime) * 10;
  
  // Calculate the three vertices of the Penrose triangle
  float angles[] = {rotation, rotation + 2*PI/3, rotation + 4*PI/3};
  int vertices[3][2];
  
  for (int i = 0; i < 3; i++) {
    vertices[i][0] = centerX + cos(angles[i]) * size;
    vertices[i][1] = centerY + sin(angles[i]) * size;
  }
  
  // Draw the impossible triangle structure
  for (int i = 0; i < 3; i++) {
    int next = (i + 1) % 3;
    int prev = (i + 2) % 3;
    
    // Each "beam" of the triangle
    int midX = (vertices[i][0] + vertices[next][0]) / 2;
    int midY = (vertices[i][1] + vertices[next][1]) / 2;
    
    // Inner edge (darker)
    gfx->drawLine(vertices[i][0], vertices[i][1], midX, midY, gfx->color565(80, 80, 120));
    
    // Outer edge (lighter) 
    int outerX = midX + (midX - centerX) * 0.3;
    int outerY = midY + (midY - centerY) * 0.3;
    gfx->drawLine(midX, midY, outerX, outerY, gfx->color565(160, 160, 200));
    
    // Side faces to create 3D illusion
    gfx->drawLine(vertices[i][0], vertices[i][1], outerX, outerY, gfx->color565(120, 120, 160));
  }
  
  // Draw connecting impossibility
  for (int i = 0; i < 3; i++) {
    int next = (i + 1) % 3;
    
    // Create the "impossible" connections
    int connX1 = vertices[i][0] + (vertices[next][0] - vertices[i][0]) * 0.7;
    int connY1 = vertices[i][1] + (vertices[next][1] - vertices[i][1]) * 0.7;
    
    int connX2 = vertices[next][0] + (vertices[(next+1)%3][0] - vertices[next][0]) * 0.3;
    int connY2 = vertices[next][1] + (vertices[(next+1)%3][1] - vertices[next][1]) * 0.3;
    
    // Draw the paradoxical connection with alternating visibility
    if (sin(animationTime * 2 + i * PI/3) > 0) {
      gfx->drawLine(connX1, connY1, connX2, connY2, WHITE);
    } else {
      // Draw as dashed line when "behind"
      for (int d = 0; d < 20; d += 6) {
        float t = d / 20.0;
        int dashX = connX1 + (connX2 - connX1) * t;
        int dashY = connY1 + (connY2 - connY1) * t;
        gfx->drawPixel(dashX, dashY, gfx->color565(100, 100, 100));
      }
    }
  }
  
  // Add some dimensional distortion effects
  for (int i = 0; i < 6; i++) {
    float distAngle = animationTime + i * PI/3;
    int distX = centerX + cos(distAngle) * (size + 20);
    int distY = centerY + sin(distAngle) * (size + 20);
    
    if (sin(animationTime * 3 + i) > 0.7) {
      gfx->fillCircle(distX, distY, 3, gfx->color565(255, 255, 0));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("PENROSE");
}

void drawPulsar() {
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  float pulse = sin(animationTime * 2) * 0.5 + 0.5;
  int radius = pulse * 80 + 20;
  gfx->fillCircle(centerX, centerY, radius, gfx->color565(255 * pulse, 100, 255 - 255 * pulse));
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("PULSAR");
}

void drawPyro() {
  gfx->fillScreen(BLACK);
  for (int i = 0; i < 15; i++) {
    int x = (sin(animationTime + i) * 100) + gfx->width() / 2;
    int y = (cos(animationTime * 1.5 + i) * 80) + gfx->height() / 2;
    int size = abs(sin(animationTime * 2 + i)) * 15;
    gfx->fillCircle(x, y, size, gfx->color565(255, 200 - i * 10, 0));
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("PYRO");
}

void drawQix() {
  gfx->fillScreen(BLACK);
  static int lastX = 160, lastY = 120;
  int x = sin(animationTime * 1.5) * 120 + gfx->width() / 2;
  int y = cos(animationTime * 2.3) * 90 + gfx->height() / 2;
  gfx->drawLine(lastX, lastY, x, y, gfx->color565(255, 255 * sin(animationTime), 255 * cos(animationTime)));
  lastX = x; lastY = y;
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("QIX");
}

void drawQuasicrystal() {
  gfx->fillScreen(BLACK);
  for (int x = 0; x < gfx->width(); x += 4) {
    for (int y = 0; y < gfx->height(); y += 4) {
      float wave = sin((x + y) * 0.02 + animationTime) * sin(x * 0.03 + animationTime * 0.7);
      int color = abs(wave) * 255;
      gfx->fillRect(x, y, 4, 4, gfx->color565(color, color / 2, 255 - color));
    }
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("QUASICRYSTAL");
}

void drawQueens() {
  gfx->fillScreen(BLACK);
  int size = 30;
  for (int i = 0; i < 8; i++) {
    int x = (i * size) + ((int)(animationTime * 10) % size);
    int y = ((i + (int)(animationTime)) % 8) * size;
    if (x < gfx->width() && y < gfx->height()) {
      gfx->fillRect(x, y, size - 2, size - 2, gfx->color565(255, 255, 0));
    }
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("QUEENS");
}

void drawQuickhull() {
  gfx->fillScreen(BLACK);
  int points[][2] = {{50, 50}, {270, 80}, {200, 200}, {80, 180}, {160, 120}};
  for (int i = 0; i < 5; i++) {
    int x = points[i][0] + sin(animationTime + i) * 20;
    int y = points[i][1] + cos(animationTime + i) * 15;
    gfx->fillCircle(x, y, 5, gfx->color565(255, 100, 100));
    if (i > 0) gfx->drawLine(points[i-1][0], points[i-1][1], x, y, WHITE);
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("QUICKHULL");
}

void drawRaverhoop() {
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  for (int i = 0; i < 20; i++) {
    float angle = (i / 20.0) * 2 * PI + animationTime;
    int x = centerX + cos(angle) * (60 + sin(animationTime * 3 + i) * 20);
    int y = centerY + sin(angle) * (60 + sin(animationTime * 3 + i) * 20);
    gfx->fillCircle(x, y, 3, gfx->color565(255 * sin(animationTime + i), 255, 255 * cos(animationTime + i)));
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("RAVERHOOP");
}

void drawRazzledazzle() {
  gfx->fillScreen(BLACK);
  for (int i = 0; i < 10; i++) {
    int x1 = sin(animationTime * (i + 1)) * 120 + gfx->width() / 2;
    int y1 = cos(animationTime * (i + 1)) * 90 + gfx->height() / 2;
    int x2 = sin(animationTime * (i + 2)) * 100 + gfx->width() / 2;
    int y2 = cos(animationTime * (i + 2)) * 70 + gfx->height() / 2;
    gfx->drawLine(x1, y1, x2, y2, gfx->color565(255 * (i % 2), 255 * ((i + 1) % 2), 255));
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("RAZZLEDAZZLE");
}

void drawRdbomb() {
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  float explosion = sin(animationTime * 3) * 0.5 + 0.5;
  for (int i = 0; i < 12; i++) {
    float angle = (i / 12.0) * 2 * PI;
    int x = centerX + cos(angle) * explosion * 80;
    int y = centerY + sin(angle) * explosion * 80;
    gfx->fillCircle(x, y, 5 + explosion * 10, gfx->color565(255, 255 * (1 - explosion), 0));
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("RDBOMB");
}

void drawRecanim() {
  gfx->fillScreen(BLACK);
  int size = 20;
  for (int x = 0; x < gfx->width(); x += size) {
    for (int y = 0; y < gfx->height(); y += size) {
      if (((x + y + (int)(animationTime * 50)) / size) % 2 == 0) {
        gfx->fillRect(x, y, size - 1, size - 1, gfx->color565(100, 255, 100));
      }
    }
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("RECANIM");
}

void drawRemote() {
  gfx->fillScreen(BLACK);
  gfx->fillRect(100, 80, 120, 80, gfx->color565(50, 50, 50));
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 4; j++) {
      int x = 110 + j * 25;
      int y = 90 + i * 20;
      if ((i + j + (int)animationTime) % 2 == 0) {
        gfx->fillRect(x, y, 15, 15, gfx->color565(255, 100, 100));
      }
    }
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("REMOTE");
}

void drawResources() {
  gfx->fillScreen(BLACK);
  for (int i = 0; i < 8; i++) {
    int height = sin(animationTime + i) * 50 + 100;
    int x = i * 40;
    gfx->fillRect(x, gfx->height() - height, 35, height, gfx->color565(255, 255 - i * 30, i * 30));
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("RESOURCES");
}

void drawRipples() {
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  for (int i = 0; i < 5; i++) {
    int radius = ((int)(animationTime * 50) + i * 30) % 150;
    int intensity = 255 - (radius * 2);
    if (intensity > 0) {
      gfx->drawCircle(centerX, centerY, radius, gfx->color565(0, intensity, 255));
    }
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("RIPPLES");
}

void drawRobot() {
  gfx->fillScreen(BLACK);
  int x = gfx->width() / 2 + sin(animationTime) * 50;
  int y = gfx->height() / 2;
  gfx->fillRect(x - 20, y - 30, 40, 60, gfx->color565(150, 150, 150));
  gfx->fillRect(x - 15, y - 25, 10, 10, gfx->color565(255, 0, 0));
  gfx->fillRect(x + 5, y - 25, 10, 10, gfx->color565(255, 0, 0));
  gfx->fillRect(x - 5, y - 10, 10, 5, gfx->color565(255, 255, 255));
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("ROBOT");
}

void drawRobotWireframe() {
  gfx->fillScreen(BLACK);
  int x = gfx->width() / 2;
  int y = gfx->height() / 2;
  gfx->drawRect(x - 20, y - 30, 40, 60, WHITE);
  gfx->drawRect(x - 15, y - 25, 10, 10, gfx->color565(255, 0, 0));
  gfx->drawRect(x + 5, y - 25, 10, 10, gfx->color565(255, 0, 0));
  gfx->drawLine(x - 20, y, x + 20, y, WHITE);
  gfx->drawLine(x, y - 30, x, y + 30, WHITE);
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("ROBOT_WIREFRAME");
}

void drawRocks() {
  gfx->fillScreen(BLACK);
  for (int i = 0; i < 8; i++) {
    int x = (sin(animationTime * 0.5 + i) * 120) + gfx->width() / 2;
    int y = (cos(animationTime * 0.7 + i) * 90) + gfx->height() / 2;
    int size = 10 + i * 3;
    gfx->fillCircle(x, y, size, gfx->color565(100 + i * 15, 80 + i * 10, 60));
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("ROCKS");
}

void drawRomanboy() {
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  for (int i = 0; i < 20; i++) {
    float t = i / 20.0 * 2 * PI + animationTime;
    int x = centerX + cos(t) * (50 + sin(t * 3) * 20);
    int y = centerY + sin(t) * (30 + cos(t * 2) * 15);
    gfx->fillCircle(x, y, 3, gfx->color565(255, 100 + sin(t) * 100, 200));
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("ROMANBOY");
}

void drawRorschach() {
  gfx->fillScreen(BLACK);
  for (int x = 0; x < gfx->width() / 2; x += 5) {
    for (int y = 0; y < gfx->height(); y += 5) {
      if (sin((x + y) * 0.1 + animationTime) > 0.3) {
        int color = sin(animationTime + x + y) * 255;
        gfx->fillRect(x, y, 5, 5, gfx->color565(color, color, color));
        gfx->fillRect(gfx->width() - x - 5, y, 5, 5, gfx->color565(color, color, color));
      }
    }
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("RORSCHACH");
}

void drawRotator() {
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  for (int i = 0; i < 8; i++) {
    float angle = animationTime + (i * PI / 4);
    int x1 = centerX + cos(angle) * 60;
    int y1 = centerY + sin(angle) * 60;
    int x2 = centerX + cos(angle) * 30;
    int y2 = centerY + sin(angle) * 30;
    gfx->drawLine(x1, y1, x2, y2, gfx->color565(255, 255 * sin(angle), 255 * cos(angle)));
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("ROTATOR");
}

void drawShips() {
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  for (int i = 0; i < 5; i++) {
    float t = animationTime + i * 0.5;
    int x = centerX + sin(t) * 80;
    int y = centerY + cos(t * 1.3) * 40;
    gfx->fillTriangle(x, y, x-10, y+15, x+10, y+15, gfx->color565(100+i*30, 200, 255));
    gfx->drawLine(x, y-5, x, y-20, WHITE);
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("SHIPS");
}

void drawSierpinski3d() {
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  for (int level = 0; level < 4; level++) {
    int size = 60 - level * 15;
    float offset = animationTime * (level + 1) * 0.2;
    for (int i = 0; i < 3; i++) {
      float angle = i * 2 * PI / 3 + offset;
      int x = centerX + cos(angle) * size;
      int y = centerY + sin(angle) * size;
      gfx->fillCircle(x, y, 4 - level, gfx->color565(255-level*40, 100+level*50, 200));
    }
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("SIERPINSKI3D");
}

void drawSierpinski() {
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  float scale = 50 + sin(animationTime) * 20;
  for (int i = 0; i < 100; i++) {
    float x = random(-100, 100) / 100.0 * scale;
    float y = random(-100, 100) / 100.0 * scale;
    if ((int(x) ^ int(y)) & 1) {
      gfx->drawPixel(centerX + x, centerY + y, gfx->color565(255, 200, 100));
    }
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("SIERPINSKI");
}

void drawSkullModel() {
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  float rotation = animationTime * 0.5;
  gfx->fillCircle(centerX, centerY-20, 25, WHITE);
  gfx->fillCircle(centerX-8, centerY-25, 4, BLACK);
  gfx->fillCircle(centerX+8, centerY-25, 4, BLACK);
  gfx->fillTriangle(centerX, centerY-15, centerX-3, centerY-8, centerX+3, centerY-8, BLACK);
  for (int i = 0; i < 6; i++) {
    int x = centerX - 10 + i * 4;
    gfx->drawLine(x, centerY-5, x, centerY, BLACK);
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("SKULL_MODEL");
}

void drawSkulloop() {
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  for (int i = 0; i < 8; i++) {
    float angle = animationTime + i * PI / 4;
    int x = centerX + cos(angle) * (30 + sin(animationTime * 2) * 10);
    int y = centerY + sin(angle) * (30 + cos(animationTime * 2) * 10);
    gfx->fillCircle(x, y, 3, gfx->color565(255, 150, 50 + i * 20));
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("SKULLOOP");
}

void drawSkytentacles() {
  gfx->fillScreen(gfx->color565(0, 20, 50));
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  for (int i = 0; i < 12; i++) {
    float baseAngle = i * 2 * PI / 12;
    for (int j = 0; j < 5; j++) {
      float t = animationTime + j * 0.2;
      float angle = baseAngle + sin(t) * 0.3;
      int x = centerX + cos(angle) * (j * 15 + 20);
      int y = centerY + sin(angle) * (j * 15 + 20);
      gfx->fillCircle(x, y, 3-j/2, gfx->color565(100+j*30, 50+j*40, 255));
    }
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("SKYTENTACLES");
}

void drawSphereversionAnalytic() {
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  float phase = animationTime * 0.3;
  for (int i = 0; i < 20; i++) {
    for (int j = 0; j < 20; j++) {
      float u = i / 20.0 * PI;
      float v = j / 20.0 * 2 * PI + phase;
      float x = sin(u) * cos(v) * 40;
      float y = sin(u) * sin(v) * 40;
      int px = centerX + x;
      int py = centerY + y;
      if (px >= 0 && px < gfx->width() && py >= 0 && py < gfx->height()) {
        gfx->drawPixel(px, py, gfx->color565(255 * sin(u), 255 * cos(v), 200));
      }
    }
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("SPHEREEVERSION_A");
}

void drawSphereversion() {
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  float t = animationTime * 0.5;
  for (int i = 0; i < 15; i++) {
    float angle = i * 2 * PI / 15 + t;
    float radius = 30 + sin(t + i * 0.2) * 15;
    int x = centerX + cos(angle) * radius;
    int y = centerY + sin(angle) * radius;
    gfx->fillCircle(x, y, 4, gfx->color565(255, 100 + sin(angle) * 100, 150));
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("SPHEREVERSION");
}

void drawSphereversionCorrugations() {
  gfx->fillScreen(BLACK);
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  for (int x = 0; x < gfx->width(); x += 4) {
    for (int y = 0; y < gfx->height(); y += 4) {
      float dx = x - centerX;
      float dy = y - centerY;
      float dist = sqrt(dx*dx + dy*dy);
      float wave = sin(dist * 0.1 + animationTime) * sin(dx * 0.05 + animationTime * 0.3);
      if (wave > 0.3) {
        gfx->fillRect(x, y, 3, 3, gfx->color565(255 * wave, 100, 200 * wave));
      }
    }
  }
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("SPHEREVERSION_C");
}

void drawSplitflap() {
  static float flapTime = 0;
  flapTime += 0.05;
  
  gfx->fillScreen(BLACK);
  
  // Draw split-flap display board
  int boardX = 20, boardY = 30;
  int cellW = 25, cellH = 35;
  int cols = 10, rows = 6;
  
  // Board background
  gfx->fillRect(boardX-5, boardY-5, cols*cellW+10, rows*cellH+10, gfx->color565(40, 40, 40));
  
  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      int x = boardX + col * cellW;
      int y = boardY + row * cellH;
      
      // Flap animation
      float flapPhase = flapTime + row * 0.2 + col * 0.1;
      int flipState = ((int)(flapPhase * 2)) % 4;
      
      // Draw flap cell
      gfx->drawRect(x, y, cellW-1, cellH-1, gfx->color565(80, 80, 80));
      
      // Animated character flip
      char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
      char currentChar = letters[((int)(flapPhase) + row + col) % 36];
      
      if (flipState < 2) {
        // Top half visible
        gfx->fillRect(x+1, y+1, cellW-3, cellH/2-2, gfx->color565(200, 180, 50));
        gfx->setCursor(x+6, y+8);
        gfx->setTextColor(BLACK);
        gfx->printf("%c", currentChar);
      } else {
        // Bottom half visible  
        gfx->fillRect(x+1, y+cellH/2+1, cellW-3, cellH/2-2, gfx->color565(160, 140, 40));
        gfx->setCursor(x+6, y+20);
        gfx->setTextColor(BLACK);
        gfx->printf("%c", currentChar);
      }
      
      // Split line
      gfx->drawLine(x+1, y+cellH/2, x+cellW-2, y+cellH/2, gfx->color565(20, 20, 20));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(200, 180, 50));
  gfx->printf("SPLITFLAP");
}

void drawSplodesic() {
  static float splodeTime = 0;
  splodeTime += 0.08;
  
  gfx->fillScreen(BLACK);
  
  // Geodesic explosion patterns
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int layer = 0; layer < 5; layer++) {
    float layerTime = splodeTime + layer * 0.3;
    float radius = 20 + layer * 25 + sin(layerTime) * 15;
    int segments = 6 + layer * 2;
    
    for (int seg = 0; seg < segments; seg++) {
      float angle = (seg * 2 * PI / segments) + layerTime * 0.5;
      float x = centerX + cos(angle) * radius;
      float y = centerY + sin(angle) * radius;
      
      // Exploding triangular facets
      float facetSize = 8 + sin(layerTime * 2 + seg) * 4;
      uint16_t color = gfx->color565(
        255 - layer * 40,
        100 + layer * 30,
        layer * 50 + sin(layerTime + seg) * 50
      );
      
      // Draw triangular facet
      for (int i = 0; i < 3; i++) {
        float triAngle = angle + i * 2 * PI / 3;
        int tx = x + cos(triAngle) * facetSize;
        int ty = y + sin(triAngle) * facetSize;
        gfx->drawLine(x, y, tx, ty, color);
      }
      
      // Center dot
      gfx->fillCircle(x, y, 2, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(255, 150, 100));
  gfx->printf("SPLODESIC");
}

void drawSquiral() {
  static float squiralTime = 0;
  squiralTime += 0.03;
  
  gfx->fillScreen(BLACK);
  
  // Squiral (square spiral) pattern
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  float angle = 0;
  float radius = 2;
  int x = centerX, y = centerY;
  int lastX = x, lastY = y;
  
  for (int i = 0; i < 150; i++) {
    // Square spiral increment
    angle += 0.1 + sin(squiralTime + i * 0.01) * 0.05;
    radius += 0.8;
    
    // Calculate position with square-like motion
    float squareAngle = floor(angle * 2 / PI) * PI / 2;
    float blend = (angle * 2 / PI) - floor(angle * 2 / PI);
    blend = blend * blend * (3 - 2 * blend); // smooth step
    
    float finalAngle = squareAngle + blend * PI / 2;
    
    x = centerX + cos(finalAngle) * radius;
    y = centerY + sin(finalAngle) * radius;
    
    if (x >= 0 && x < gfx->width() && y >= 0 && y < gfx->height()) {
      uint16_t color = gfx->color565(
        (int)(128 + 127 * sin(squiralTime + i * 0.1)),
        (int)(128 + 127 * sin(squiralTime + i * 0.15 + PI/3)),
        (int)(128 + 127 * sin(squiralTime + i * 0.2 + 2*PI/3))
      );
      
      gfx->drawLine(lastX, lastY, x, y, color);
      gfx->fillCircle(x, y, 2, color);
    }
    
    lastX = x;
    lastY = y;
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(150, 255, 150));
  gfx->printf("SQUIRAL");
}

void drawSquirtorus() {
  static float torusTime = 0;
  torusTime += 0.06;
  
  gfx->fillScreen(BLACK);
  
  // Squirting torus effect
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int particle = 0; particle < 40; particle++) {
    float pTime = torusTime + particle * 0.15;
    
    // Torus parameters
    float majorR = 60;
    float minorR = 20 + sin(pTime * 3) * 15;
    float torusAngle = pTime * 2;
    float tubeAngle = particle * PI / 20 + pTime * 5;
    
    // Torus position
    float torusX = majorR * cos(torusAngle);
    float torusY = majorR * sin(torusAngle);
    
    // Tube position
    float tubeX = minorR * cos(tubeAngle);
    float tubeY = minorR * sin(tubeAngle);
    
    // Final position
    int x = centerX + torusX + tubeX;
    int y = centerY + torusY + tubeY;
    
    // Squirting jets
    if (sin(pTime * 4) > 0.7) {
      float jetLength = 30 + sin(pTime * 6) * 20;
      float jetAngle = tubeAngle;
      
      for (int jet = 0; jet < jetLength; jet += 2) {
        int jetX = x + cos(jetAngle) * jet;
        int jetY = y + sin(jetAngle) * jet;
        
        if (jetX >= 0 && jetX < gfx->width() && jetY >= 0 && jetY < gfx->height()) {
          uint16_t jetColor = gfx->color565(
            50 + jet * 4,
            100 + jet * 3,
            200 - jet * 2
          );
          gfx->drawPixel(jetX, jetY, jetColor);
        }
      }
    }
    
    // Torus particles
    if (x >= 0 && x < gfx->width() && y >= 0 && y < gfx->height()) {
      uint16_t color = gfx->color565(
        (int)(150 + 105 * sin(pTime + particle)),
        (int)(100 + 80 * cos(pTime * 1.3 + particle)),
        (int)(200 + 55 * sin(pTime * 1.7 + particle))
      );
      gfx->fillCircle(x, y, 3, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(100, 200, 255));
  gfx->printf("SQUIRTORUS");
}

void drawStarwars() {
  static float starwarsTime = 0;
  static int textLine = 0;
  starwarsTime += 0.02;
  
  gfx->fillScreen(BLACK);
  
  // Star field
  for (int star = 0; star < 50; star++) {
    int x = (star * 37) % gfx->width();
    int y = (int)(star * 23 + starwarsTime * 10) % gfx->height();
    gfx->drawPixel(x, y, WHITE);
  }
  
  // Scrolling text effect
  const char* crawlText[] = {
    "A long time ago in a",
    "galaxy far, far away...",
    "",
    "Episode IV",
    "A NEW HOPE",
    "",
    "It is a period of civil war.",
    "Rebel spaceships, striking",
    "from a hidden base, have won",
    "their first victory against",
    "the evil Galactic Empire.",
    "",
    "During the battle, Rebel",
    "spies managed to steal secret",
    "plans to the Empire's ultimate",
    "weapon, the DEATH STAR..."
  };
  
  int numLines = sizeof(crawlText) / sizeof(crawlText[0]);
  
  for (int i = 0; i < 8; i++) {
    int lineIndex = (textLine + i) % numLines;
    int yPos = gfx->height() - 20 + i * 25 - (int)(starwarsTime * 30) % (25 * numLines);
    
    if (yPos > -15 && yPos < gfx->height() + 15) {
      // Perspective scaling
      float scale = (float)(gfx->height() - yPos) / gfx->height();
      scale = scale * scale; // More dramatic perspective
      
      if (scale > 0.1 && scale < 2.0) {
        gfx->setCursor(gfx->width()/2 - strlen(crawlText[lineIndex]) * 3, yPos);
        gfx->setTextSize(1);
        gfx->setTextColor(gfx->color565(255, 255, 100 + scale * 100));
        gfx->printf("%s", crawlText[lineIndex]);
      }
    }
  }
  
  // Update text line periodically
  if ((int)(starwarsTime * 10) % 100 == 0) {
    textLine = (textLine + 1) % numLines;
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(255, 255, 100));
  gfx->printf("STAR WARS");
}

void drawStonerview() {
  static float stoneTime = 0;
  stoneTime += 0.04;
  
  gfx->fillScreen(gfx->color565(20, 15, 10));
  
  // Stone tunnel effect
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int ring = 0; ring < 8; ring++) {
    float ringTime = stoneTime + ring * 0.2;
    float radius = 20 + ring * 25 + sin(ringTime) * 5;
    
    int stones = 8 + ring;
    for (int stone = 0; stone < stones; stone++) {
      float angle = (stone * 2 * PI / stones) + ringTime * 0.3;
      int x = centerX + cos(angle) * radius;
      int y = centerY + sin(angle) * radius;
      
      // Stone texture
      int stoneSize = 8 - ring;
      uint16_t stoneColor = gfx->color565(
        80 + ring * 10 + sin(ringTime + stone) * 20,
        60 + ring * 8 + sin(ringTime * 1.2 + stone) * 15,
        40 + ring * 5 + sin(ringTime * 1.5 + stone) * 10
      );
      
      // Irregular stone shape
      for (int px = -stoneSize; px <= stoneSize; px++) {
        for (int py = -stoneSize; py <= stoneSize; py++) {
          if (px*px + py*py <= stoneSize*stoneSize) {
            int plotX = x + px + sin(ringTime + stone + px*0.1) * 2;
            int plotY = y + py + cos(ringTime + stone + py*0.1) * 2;
            if (plotX >= 0 && plotX < gfx->width() && plotY >= 0 && plotY < gfx->height()) {
              gfx->drawPixel(plotX, plotY, stoneColor);
            }
          }
        }
      }
      
      // Stone highlights
      gfx->drawPixel(x-1, y-1, gfx->color565(
        min(255, (80 + ring * 10) + 40),
        min(255, (60 + ring * 8) + 30), 
        min(255, (40 + ring * 5) + 20)
      ));
    }
  }
  
  // Moving particles (dust)
  for (int dust = 0; dust < 15; dust++) {
    float dustTime = stoneTime * 3 + dust * 0.4;
    int dustX = (int)(dustTime * 50) % (gfx->width() + 20) - 10;
    int dustY = 50 + dust * 10 + sin(dustTime) * 20;
    gfx->drawPixel(dustX, dustY, gfx->color565(100, 80, 60));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(200, 180, 120));
  gfx->printf("STONERVIEW");
}

void drawStrange() {
  static float strangeTime = 0;
  strangeTime += 0.05;
  
  gfx->fillScreen(BLACK);
  
  // Strange attractor visualization
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  static float x = 0.1, y = 0.1, z = 0.1;
  
  for (int i = 0; i < 200; i++) {
    // Lorenz attractor equations
    float dt = 0.01;
    float sigma = 10.0;
    float rho = 28.0;
    float beta = 8.0/3.0;
    
    float dx = sigma * (y - x);
    float dy = x * (rho - z) - y;
    float dz = x * y - beta * z;
    
    x += dx * dt;
    y += dy * dt;  
    z += dz * dt;
    
    // Project 3D to 2D
    int plotX = centerX + (int)(x * 3 + sin(strangeTime) * 10);
    int plotY = centerY + (int)(y * 3 + cos(strangeTime) * 10);
    
    if (plotX >= 0 && plotX < gfx->width() && plotY >= 0 && plotY < gfx->height()) {
      uint16_t color = gfx->color565(
        (int)(128 + 127 * sin(strangeTime + i * 0.05)),
        (int)(128 + 127 * sin(strangeTime + i * 0.07 + PI/2)),
        (int)(128 + 127 * sin(strangeTime + i * 0.03 + PI))
      );
      gfx->drawPixel(plotX, plotY, color);
    }
  }
  
  // Reset periodically to prevent overflow
  if ((int)(strangeTime * 10) % 1000 == 0) {
    x = 0.1 + sin(strangeTime) * 0.05;
    y = 0.1 + cos(strangeTime) * 0.05;
    z = 0.1;
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(255, 100, 200));
  gfx->printf("STRANGE ATTRACTOR");
}

void drawSubstrate() {
  static float substrateTime = 0;
  substrateTime += 0.03;
  
  gfx->fillScreen(gfx->color565(10, 10, 10));
  
  // Organic growth patterns like substrate art
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int crack = 0; crack < 8; crack++) {
    float crackTime = substrateTime + crack * 0.5;
    float crackAngle = crack * PI / 4 + crackTime * 0.1;
    
    // Main crack line
    float startRadius = 10;
    float endRadius = 80 + sin(crackTime) * 20;
    
    for (int step = 0; step < 50; step++) {
      float stepRatio = (float)step / 50;
      float radius = startRadius + stepRatio * endRadius;
      
      // Organic deviation
      float deviation = sin(crackTime + step * 0.2) * 15;
      float currentAngle = crackAngle + deviation * 0.1;
      
      int x = centerX + cos(currentAngle) * radius;
      int y = centerY + sin(currentAngle) * radius;
      
      if (x >= 0 && x < gfx->width() && y >= 0 && y < gfx->height()) {
        // Main crack color
        uint16_t crackColor = gfx->color565(
          100 + crack * 15,
          80 + sin(crackTime + step * 0.1) * 50,
          60 + crack * 10
        );
        gfx->fillCircle(x, y, 2, crackColor);
        
        // Branch cracks
        if (step % 8 == 0) {
          for (int branch = 0; branch < 2; branch++) {
            float branchAngle = currentAngle + (branch - 0.5) * PI / 3;
            float branchLength = 20 + sin(crackTime + step + branch) * 10;
            
            for (int bStep = 0; bStep < 10; bStep++) {
              float bx = x + cos(branchAngle) * bStep * 2;
              float by = y + sin(branchAngle) * bStep * 2;
              
              if (bx >= 0 && bx < gfx->width() && by >= 0 && by < gfx->height()) {
                uint16_t branchColor = gfx->color565(
                  80 + crack * 10,
                  60 + bStep * 8,
                  40 + branch * 20
                );
                gfx->drawPixel(bx, by, branchColor);
              }
            }
          }
        }
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(150, 120, 80));
  gfx->printf("SUBSTRATE");
}

void drawSuperquadrics() {
  static float sqTime = 0;
  sqTime += 0.04;
  
  gfx->fillScreen(BLACK);
  
  // 2D projection of superquadric shapes
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple superquadrics with different parameters
  for (int shape = 0; shape < 3; shape++) {
    float shapeTime = sqTime + shape * PI / 1.5;
    float n1 = 2.0 + sin(shapeTime) * 1.5;      // Shape parameter 1
    float n2 = 2.0 + cos(shapeTime * 1.3) * 1.5; // Shape parameter 2
    float scale = 40 + shape * 20;
    
    int steps = 60;
    for (int i = 0; i < steps; i++) {
      float u = (i * 2.0 * PI) / steps;
      
      // Superquadric equations (2D cross-section)
      float cosU = cos(u);
      float sinU = sin(u);
      
      // Sign-preserving power function
      auto signPow = [](float val, float exp) {
        return (val >= 0) ? pow(val, exp) : -pow(-val, exp);
      };
      
      float x = signPow(abs(cosU), 2.0/n1) * ((cosU >= 0) ? 1 : -1);
      float y = signPow(abs(sinU), 2.0/n2) * ((sinU >= 0) ? 1 : -1);
      
      // 3D rotation projection
      float rotX = shapeTime * 0.5;
      float rotY = shapeTime * 0.7;
      
      float x3d = x * cos(rotY) - y * sin(rotX) * sin(rotY);
      float y3d = y * cos(rotX);
      
      int plotX = centerX + x3d * scale;
      int plotY = centerY + y3d * scale;
      
      if (plotX >= 0 && plotX < gfx->width() && plotY >= 0 && plotY < gfx->height()) {
        uint16_t color = gfx->color565(
          100 + shape * 50 + sin(shapeTime + i * 0.1) * 50,
          150 + cos(shapeTime + i * 0.15) * 80,
          200 - shape * 40 + sin(shapeTime + i * 0.08) * 40
        );
        gfx->fillCircle(plotX, plotY, 3, color);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(200, 150, 255));
  gfx->printf("SUPERQUADRICS");
}

void drawSurfaces() {
  static float surfaceTime = 0;
  surfaceTime += 0.03;
  
  gfx->fillScreen(BLACK);
  
  // Mathematical surface visualization
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Klein bottle projection
  for (int u = 0; u < 32; u++) {
    for (int v = 0; v < 16; v++) {
      float uf = (u * 2.0 * PI) / 32;
      float vf = (v * 2.0 * PI) / 16;
      
      float time_uf = uf + surfaceTime * 0.5;
      float time_vf = vf + surfaceTime * 0.3;
      
      // Klein bottle parametric equations (simplified)
      float x = (2 + cos(time_vf/2) * sin(time_uf) - sin(time_vf/2) * sin(2*time_uf)) * cos(time_vf/2);
      float y = (2 + cos(time_vf/2) * sin(time_uf) - sin(time_vf/2) * sin(2*time_uf)) * sin(time_vf/2);
      float z = sin(time_vf/2) * sin(time_uf) + cos(time_vf/2) * sin(2*time_uf);
      
      // 3D to 2D projection
      float scale = 15;
      int plotX = centerX + x * scale;
      int plotY = centerY + y * scale;
      
      if (plotX >= 0 && plotX < gfx->width() && plotY >= 0 && plotY < gfx->height()) {
        uint16_t color = gfx->color565(
          (int)(128 + z * 60),
          (int)(100 + sin(surfaceTime + uf) * 80),
          (int)(150 + cos(surfaceTime + vf) * 70)
        );
        gfx->fillCircle(plotX, plotY, 2, color);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(150, 200, 255));
  gfx->printf("SURFACES");
}

void drawSwim() {
  static float swimTime = 0;
  swimTime += 0.05;
  
  gfx->fillScreen(gfx->color565(0, 20, 40));
  
  // Swimming fish patterns
  for (int fish = 0; fish < 12; fish++) {
    float fishTime = swimTime + fish * 0.4;
    
    // Fish swimming path
    float pathX = gfx->width() * 0.2 + (gfx->width() * 0.6) * (sin(fishTime * 0.3) * 0.5 + 0.5);
    float pathY = gfx->height() * 0.3 + fish * (gfx->height() * 0.4 / 12);
    
    // Fish body undulation
    float bodyWave = sin(fishTime * 8) * 5;
    
    // Draw fish
    int fishX = pathX + bodyWave;
    int fishY = pathY;
    
    // Fish body
    gfx->fillCircle(fishX, fishY, 8, gfx->color565(
      100 + fish * 10,
      150 + sin(fishTime) * 50,
      200 + cos(fishTime * 1.2) * 40
    ));
    
    // Fish tail
    int tailX = fishX - 12 - bodyWave;
    int tailY = fishY + sin(fishTime * 8) * 3;
    gfx->fillTriangle(
      tailX, tailY,
      tailX - 8, tailY - 5,
      tailX - 8, tailY + 5,
      gfx->color565(80 + fish * 8, 120, 160)
    );
    
    // Fish eye
    gfx->fillCircle(fishX + 3, fishY - 2, 2, WHITE);
    gfx->fillCircle(fishX + 3, fishY - 2, 1, BLACK);
    
    // Bubbles
    if (fish % 3 == 0) {
      for (int bubble = 0; bubble < 3; bubble++) {
        float bubbleTime = fishTime * 2 + bubble * 0.5;
        int bubbleX = fishX + 10 + bubble * 8;
        int bubbleY = fishY - 15 - (int)(bubbleTime * 20) % 40;
        
        if (bubbleY > 0) {
          gfx->drawCircle(bubbleX, bubbleY, 2, gfx->color565(100, 200, 255));
        }
      }
    }
  }
  
  // Water plants
  for (int plant = 0; plant < 6; plant++) {
    int plantX = plant * (gfx->width() / 6) + 20;
    int plantBase = gfx->height() - 10;
    
    for (int segment = 0; segment < 8; segment++) {
      float segmentTime = swimTime + plant * 0.3 + segment * 0.1;
      int segmentY = plantBase - segment * 8;
      int sway = sin(segmentTime * 2) * (segment + 1);
      
      gfx->fillCircle(plantX + sway, segmentY, 3, gfx->color565(20, 80 + segment * 10, 40));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(100, 200, 255));
  gfx->printf("SWIM");
}

void drawSwirl() {
  static float swirlTime = 0;
  swirlTime += 0.02;
  
  gfx->fillScreen(BLACK);
  
  // Color swirl patterns
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int layer = 0; layer < 5; layer++) {
    int numPoints = 30 + layer * 10;
    float layerRadius = 20 + layer * 25;
    
    for (int point = 0; point < numPoints; point++) {
      float baseAngle = (point * 2.0 * PI) / numPoints;
      
      // Swirl effect - angle increases with radius and time
      float swirlFactor = (swirlTime * 2) + (layerRadius * 0.05);
      float angle = baseAngle + swirlFactor;
      
      float radius = layerRadius + sin(swirlTime * 3 + point * 0.2) * 8;
      
      int x = centerX + cos(angle) * radius;
      int y = centerY + sin(angle) * radius;
      
      if (x >= 0 && x < gfx->width() && y >= 0 && y < gfx->height()) {
        // Color cycling
        float hue = fmod(swirlTime * 50 + angle * 180/PI + layer * 60, 360);
        
        uint16_t color;
        if (hue < 120) {
          color = gfx->color565(255 * (120 - hue) / 120, 255 * hue / 120, 0);
        } else if (hue < 240) {
          color = gfx->color565(0, 255 * (240 - hue) / 120, 255 * (hue - 120) / 120);
        } else {
          color = gfx->color565(255 * (hue - 240) / 120, 0, 255 * (360 - hue) / 120);
        }
        
        gfx->fillCircle(x, y, 3 - layer/2, color);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("SWIRL");
}

void drawT3d() {
  static float t3dTime = 0;
  t3dTime += 0.06;
  
  gfx->fillScreen(BLACK);
  
  // 3D rotating objects
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Draw rotating wireframe cube
  float cubeSize = 40;
  
  // Cube vertices
  float vertices[8][3] = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
    {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}
  };
  
  // Rotation matrices
  float rotX = t3dTime * 0.7;
  float rotY = t3dTime * 0.5;
  float rotZ = t3dTime * 0.3;
  
  int projectedVerts[8][2];
  
  for (int i = 0; i < 8; i++) {
    float x = vertices[i][0] * cubeSize;
    float y = vertices[i][1] * cubeSize;
    float z = vertices[i][2] * cubeSize;
    
    // Rotate around X
    float y1 = y * cos(rotX) - z * sin(rotX);
    float z1 = y * sin(rotX) + z * cos(rotX);
    
    // Rotate around Y  
    float x2 = x * cos(rotY) + z1 * sin(rotY);
    float z2 = -x * sin(rotY) + z1 * cos(rotY);
    
    // Rotate around Z
    float x3 = x2 * cos(rotZ) - y1 * sin(rotZ);
    float y3 = x2 * sin(rotZ) + y1 * cos(rotZ);
    
    // Project to 2D
    projectedVerts[i][0] = centerX + x3;
    projectedVerts[i][1] = centerY + y3;
  }
  
  // Draw cube edges
  int edges[12][2] = {
    {0,1}, {1,2}, {2,3}, {3,0}, // bottom face
    {4,5}, {5,6}, {6,7}, {7,4}, // top face
    {0,4}, {1,5}, {2,6}, {3,7}  // vertical edges
  };
  
  for (int i = 0; i < 12; i++) {
    int v1 = edges[i][0];
    int v2 = edges[i][1];
    
    uint16_t edgeColor = gfx->color565(
      150 + sin(t3dTime + i) * 100,
      100 + cos(t3dTime + i * 1.2) * 80,
      200 + sin(t3dTime + i * 0.8) * 50
    );
    
    gfx->drawLine(
      projectedVerts[v1][0], projectedVerts[v1][1],
      projectedVerts[v2][0], projectedVerts[v2][1],
      edgeColor
    );
  }
  
  // Draw vertices
  for (int i = 0; i < 8; i++) {
    gfx->fillCircle(projectedVerts[i][0], projectedVerts[i][1], 3, 
      gfx->color565(255, 200, 100));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(200, 150, 255));
  gfx->printf("T3D");
}

void drawTangram() {
  static float tangramTime = 0;
  tangramTime += 0.03;
  
  gfx->fillScreen(gfx->color565(20, 20, 30));
  
  // Tangram puzzle pieces animation
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Define tangram pieces (simplified)
  struct TangramPiece {
    int numPoints;
    float points[8][2]; // max 4 points for triangles/parallelograms
    uint16_t color;
  };
  
  TangramPiece pieces[7];
  
  // Large triangle 1
  pieces[0] = {3, {{-40, -40}, {40, -40}, {0, 0}}, gfx->color565(255, 100, 100)};
  
  // Large triangle 2  
  pieces[1] = {3, {{-40, 40}, {40, 40}, {0, 0}}, gfx->color565(100, 255, 100)};
  
  // Medium triangle
  pieces[2] = {3, {{40, -40}, {40, 0}, {20, -20}}, gfx->color565(100, 100, 255)};
  
  // Small triangle 1
  pieces[3] = {3, {{-20, -20}, {0, -40}, {0, 0}}, gfx->color565(255, 255, 100)};
  
  // Small triangle 2
  pieces[4] = {3, {{0, 0}, {20, -20}, {20, 0}}, gfx->color565(255, 100, 255)};
  
  // Square (as two triangles)
  pieces[5] = {4, {{-20, 20}, {-20, 40}, {0, 40}, {0, 20}}, gfx->color565(100, 255, 255)};
  
  // Parallelogram
  pieces[6] = {4, {{20, 0}, {40, 20}, {20, 40}, {0, 20}}, gfx->color565(255, 200, 100)};
  
  for (int i = 0; i < 7; i++) {
    // Animate piece rotation and position
    float pieceTime = tangramTime + i * 0.3;
    float rotation = pieceTime * 0.5;
    float offsetX = sin(pieceTime * 0.8) * 20;
    float offsetY = cos(pieceTime * 0.6) * 15;
    
    // Rotate and translate piece
    int rotatedPoints[8][2];
    for (int j = 0; j < pieces[i].numPoints; j++) {
      float x = pieces[i].points[j][0];
      float y = pieces[i].points[j][1];
      
      // Rotate
      float rx = x * cos(rotation) - y * sin(rotation);
      float ry = x * sin(rotation) + y * cos(rotation);
      
      // Translate
      rotatedPoints[j][0] = centerX + rx + offsetX;
      rotatedPoints[j][1] = centerY + ry + offsetY;
    }
    
    // Draw piece
    if (pieces[i].numPoints == 3) {
      // Triangle
      gfx->fillTriangle(
        rotatedPoints[0][0], rotatedPoints[0][1],
        rotatedPoints[1][0], rotatedPoints[1][1],
        rotatedPoints[2][0], rotatedPoints[2][1],
        pieces[i].color
      );
    } else {
      // Quadrilateral - draw as two triangles
      gfx->fillTriangle(
        rotatedPoints[0][0], rotatedPoints[0][1],
        rotatedPoints[1][0], rotatedPoints[1][1],
        rotatedPoints[2][0], rotatedPoints[2][1],
        pieces[i].color
      );
      gfx->fillTriangle(
        rotatedPoints[0][0], rotatedPoints[0][1],
        rotatedPoints[2][0], rotatedPoints[2][1],
        rotatedPoints[3][0], rotatedPoints[3][1],
        pieces[i].color
      );
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("TANGRAM");
}

void drawTeapot() {
  static float teapotTime = 0;
  teapotTime += 0.04;
  
  gfx->fillScreen(BLACK);
  
  // Classic Utah teapot wireframe
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Simplified teapot outline
  float rotation = teapotTime * 0.5;
  
  // Teapot body (ellipse)
  int bodyRadiusX = 50;
  int bodyRadiusY = 35;
  
  for (int angle = 0; angle < 360; angle += 10) {
    float rad = angle * PI / 180 + rotation;
    int x1 = centerX + cos(rad) * bodyRadiusX;
    int y1 = centerY + sin(rad) * bodyRadiusY;
    
    float rad2 = (angle + 10) * PI / 180 + rotation;
    int x2 = centerX + cos(rad2) * bodyRadiusX;
    int y2 = centerY + sin(rad2) * bodyRadiusY;
    
    uint16_t bodyColor = gfx->color565(150, 200, 255);
    gfx->drawLine(x1, y1, x2, y2, bodyColor);
  }
  
  // Teapot lid
  int lidY = centerY - 35;
  for (int angle = 0; angle < 360; angle += 15) {
    float rad = angle * PI / 180 + rotation;
    int x = centerX + cos(rad) * 35;
    int y = lidY + sin(rad) * 10;
    gfx->drawPixel(x, y, gfx->color565(200, 150, 100));
  }
  
  // Teapot handle
  float handleCenterX = centerX + 40;
  float handleCenterY = centerY;
  for (int angle = 30; angle < 330; angle += 10) {
    float rad = angle * PI / 180 + rotation * 0.3;
    int x = handleCenterX + cos(rad) * 25;
    int y = handleCenterY + sin(rad) * 30;
    gfx->fillCircle(x, y, 2, gfx->color565(100, 150, 200));
  }
  
  // Teapot spout
  float spoutStartX = centerX - 45;
  float spoutStartY = centerY - 10;
  for (int i = 0; i < 20; i++) {
    float t = i / 20.0;
    float spoutX = spoutStartX - t * 30;
    float spoutY = spoutStartY - t * t * 15; // Curved spout
    
    uint16_t spoutColor = gfx->color565(180, 120, 200);
    gfx->fillCircle(spoutX, spoutY, 3 - i/7, spoutColor);
  }
  
  // Teapot feet
  for (int foot = 0; foot < 3; foot++) {
    float footAngle = foot * 2 * PI / 3 + rotation;
    int footX = centerX + cos(footAngle) * 30;
    int footY = centerY + 35;
    gfx->fillCircle(footX, footY, 4, gfx->color565(100, 100, 150));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(200, 150, 255));
  gfx->printf("TEAPOT");
}

void drawTessellimage() {
  static float tessellTime = 0;
  tessellTime += 0.02;
  
  gfx->fillScreen(gfx->color565(10, 10, 20));
  
  // Tessellation pattern
  int tileSize = 20;
  int tilesX = gfx->width() / tileSize + 1;
  int tilesY = gfx->height() / tileSize + 1;
  
  for (int tileX = 0; tileX < tilesX; tileX++) {
    for (int tileY = 0; tileY < tilesY; tileY++) {
      float tileTime = tessellTime + tileX * 0.1 + tileY * 0.15;
      
      int x = tileX * tileSize;
      int y = tileY * tileSize;
      
      // Tessellation pattern type
      int patternType = (tileX + tileY) % 4;
      
      uint16_t tileColor = gfx->color565(
        (int)(100 + 100 * sin(tileTime)),
        (int)(100 + 100 * sin(tileTime + PI/2)),
        (int)(100 + 100 * sin(tileTime + PI))
      );
      
      switch (patternType) {
        case 0: // Square
          gfx->fillRect(x + 2, y + 2, tileSize - 4, tileSize - 4, tileColor);
          break;
          
        case 1: // Circle
          gfx->fillCircle(x + tileSize/2, y + tileSize/2, tileSize/3, tileColor);
          break;
          
        case 2: // Triangle
          gfx->fillTriangle(
            x + tileSize/2, y + 2,
            x + 2, y + tileSize - 2,
            x + tileSize - 2, y + tileSize - 2,
            tileColor
          );
          break;
          
        case 3: // Diamond
          gfx->fillTriangle(
            x + tileSize/2, y + 2,
            x + 2, y + tileSize/2,
            x + tileSize/2, y + tileSize - 2,
            tileColor
          );
          gfx->fillTriangle(
            x + tileSize/2, y + 2,
            x + tileSize - 2, y + tileSize/2,
            x + tileSize/2, y + tileSize - 2,
            tileColor
          );
          break;
      }
      
      // Animated overlay
      if (sin(tileTime * 2) > 0.5) {
        gfx->drawRect(x, y, tileSize, tileSize, WHITE);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("TESSELLIMAGE");
}

void drawThornbird() {
  static float thornTime = 0;
  thornTime += 0.01;
  
  gfx->fillScreen(BLACK);
  
  // Thornbird fractal (Bird in a Thornbush)
  // Parameters that vary over time
  float a = 1.99 + sin(thornTime * 0.3) * 0.1;
  float b = -1.0 + cos(thornTime * 0.2) * 0.2;
  float c = -0.6 + sin(thornTime * 0.4) * 0.1;
  
  static float x = 0.1, y = 0.1;
  
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Iterate the Thornbird equations
  for (int i = 0; i < 200; i++) {
    float newX = sin(y * b) + c * sin(x * b);
    float newY = sin(x * a) + c * sin(y * a);
    
    x = newX;
    y = newY;
    
    // Map to screen coordinates
    int plotX = centerX + x * 80;
    int plotY = centerY + y * 80;
    
    if (plotX >= 0 && plotX < gfx->width() && plotY >= 0 && plotY < gfx->height()) {
      // Color based on iteration and position
      uint16_t color = gfx->color565(
        (int)(128 + 127 * sin(thornTime + i * 0.05 + x)),
        (int)(128 + 127 * sin(thornTime + i * 0.07 + y)),
        (int)(128 + 127 * sin(thornTime + i * 0.03 + x + y))
      );
      
      gfx->fillCircle(plotX, plotY, 2, color);
      
      // Fade trail
      if (i > 100) {
        gfx->drawPixel(plotX, plotY, gfx->color565(50, 30, 70));
      }
    }
  }
  
  // Reset position periodically to prevent overflow
  if ((int)(thornTime * 100) % 500 == 0) {
    x = 0.1 + sin(thornTime) * 0.1;
    y = 0.1 + cos(thornTime) * 0.1;
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(255, 150, 100));
  gfx->printf("THORNBIRD");
}

void drawTimetunnel() {
  static float tunnelTime = 0;
  tunnelTime += 0.08;
  
  gfx->fillScreen(BLACK);
  
  // Time tunnel effect
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple concentric rings moving toward viewer
  for (int ring = 0; ring < 15; ring++) {
    float ringTime = tunnelTime + ring * 0.3;
    
    // Ring expands over time
    float baseRadius = 10 + fmod(ringTime * 40, 120);
    
    // Ring segments
    int segments = 8 + ring % 4;
    for (int seg = 0; seg < segments; seg++) {
      float angle = (seg * 2.0 * PI / segments) + ringTime * 0.5;
      
      // Pulsing radius
      float radius = baseRadius + sin(ringTime * 3 + seg) * 8;
      
      // Tunnel perspective - closer rings are brighter/larger
      float perspective = 1.0 / (1.0 + baseRadius * 0.01);
      
      int x = centerX + cos(angle) * radius;
      int y = centerY + sin(angle) * radius;
      
      if (x >= 0 && x < gfx->width() && y >= 0 && y < gfx->height()) {
        // Color shifts through time spectrum
        float hue = fmod(tunnelTime * 100 + ring * 30 + seg * 45, 360);
        uint16_t color;
        
        int brightness = 255 * perspective;
        
        if (hue < 120) {
          color = gfx->color565(
            brightness * (120 - hue) / 120, 
            brightness * hue / 120, 
            0
          );
        } else if (hue < 240) {
          color = gfx->color565(
            0,
            brightness * (240 - hue) / 120, 
            brightness * (hue - 120) / 120
          );
        } else {
          color = gfx->color565(
            brightness * (hue - 240) / 120, 
            0, 
            brightness * (360 - hue) / 120
          );
        }
        
        int dotSize = 1 + perspective * 4;
        gfx->fillCircle(x, y, dotSize, color);
        
        // Connect segments with lines for tunnel effect
        if (seg > 0) {
          float prevAngle = ((seg-1) * 2.0 * PI / segments) + ringTime * 0.5;
          int prevX = centerX + cos(prevAngle) * radius;
          int prevY = centerY + sin(prevAngle) * radius;
          gfx->drawLine(x, y, prevX, prevY, color);
        }
      }
    }
  }
  
  // Central vortex
  for (int vortex = 0; vortex < 20; vortex++) {
    float vortexTime = tunnelTime * 5 + vortex * 0.2;
    float vortexRadius = vortex * 2 + sin(vortexTime) * 3;
    float vortexAngle = vortexTime + vortex * 0.3;
    
    int vx = centerX + cos(vortexAngle) * vortexRadius;
    int vy = centerY + sin(vortexAngle) * vortexRadius;
    
    if (vx >= 0 && vx < gfx->width() && vy >= 0 && vy < gfx->height()) {
      gfx->fillCircle(vx, vy, 2, gfx->color565(255, 255, 200));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(200, 255, 255));
  gfx->printf("TIMETUNNEL");
}

void drawTimezones() {
  static float clockTime = 0;
  clockTime += 0.02;
  
  gfx->fillScreen(gfx->color565(10, 10, 30));
  
  // World timezone clocks
  struct Timezone {
    const char* name;
    float offset;  // UTC offset in hours
    int x, y;     // Position on screen
    uint16_t color;
  };
  
  Timezone zones[] = {
    {"NYC", -5, 40, 40, gfx->color565(255, 100, 100)},
    {"LON", 0, 140, 40, gfx->color565(100, 255, 100)},
    {"TKY", 9, 240, 40, gfx->color565(100, 100, 255)},
    {"SYD", 11, 40, 120, gfx->color565(255, 255, 100)},
    {"LAX", -8, 140, 120, gfx->color565(255, 100, 255)},
    {"BER", 1, 240, 120, gfx->color565(100, 255, 255)}
  };
  
  int numZones = sizeof(zones) / sizeof(zones[0]);
  
  for (int i = 0; i < numZones; i++) {
    Timezone& zone = zones[i];
    
    // Calculate local time for this zone
    float localTime = clockTime + zone.offset * 0.1; // Simulate time progression
    
    // Draw clock face
    int radius = 25;
    gfx->drawCircle(zone.x, zone.y, radius, zone.color);
    
    // Draw hour markers
    for (int hour = 0; hour < 12; hour++) {
      float hourAngle = (hour * PI / 6) - PI/2;
      int markX1 = zone.x + cos(hourAngle) * (radius - 3);
      int markY1 = zone.y + sin(hourAngle) * (radius - 3);
      int markX2 = zone.x + cos(hourAngle) * (radius - 6);
      int markY2 = zone.y + sin(hourAngle) * (radius - 6);
      gfx->drawLine(markX1, markY1, markX2, markY2, zone.color);
    }
    
    // Draw clock hands
    float hours = fmod(localTime * 2, 12); // 2x speed for demo
    float minutes = fmod(localTime * 24, 60);
    
    // Hour hand
    float hourAngle = (hours * PI / 6) - PI/2;
    int hourX = zone.x + cos(hourAngle) * (radius * 0.5);
    int hourY = zone.y + sin(hourAngle) * (radius * 0.5);
    gfx->drawLine(zone.x, zone.y, hourX, hourY, WHITE);
    
    // Minute hand
    float minuteAngle = (minutes * PI / 30) - PI/2;
    int minuteX = zone.x + cos(minuteAngle) * (radius * 0.8);
    int minuteY = zone.y + sin(minuteAngle) * (radius * 0.8);
    gfx->drawLine(zone.x, zone.y, minuteX, minuteY, WHITE);
    
    // Center dot
    gfx->fillCircle(zone.x, zone.y, 2, WHITE);
    
    // Zone label
    gfx->setCursor(zone.x - 10, zone.y + radius + 8);
    gfx->setTextColor(zone.color);
    gfx->setTextSize(1);
    gfx->printf("%s", zone.name);
    
    // Digital time display
    int displayHour = (int)hours;
    int displayMin = (int)minutes;
    gfx->setCursor(zone.x - 15, zone.y + radius + 20);
    gfx->printf("%02d:%02d", displayHour, displayMin);
  }
  
  // World map silhouette (simplified)
  // Draw some continent outlines
  gfx->drawLine(60, 200, 120, 180, gfx->color565(80, 80, 100)); // North America
  gfx->drawLine(120, 180, 160, 185, gfx->color565(80, 80, 100)); // Atlantic
  gfx->drawLine(160, 185, 200, 175, gfx->color565(80, 80, 100)); // Europe
  gfx->drawLine(200, 175, 260, 190, gfx->color565(80, 80, 100)); // Asia
  
  // Time zones indicator lines
  for (int tz = -12; tz <= 12; tz += 3) {
    int lineX = 20 + (tz + 12) * (gfx->width() - 40) / 24;
    gfx->drawLine(lineX, 195, lineX, 210, gfx->color565(60, 60, 80));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("TIMEZONES");
}

void drawToaster() {
  static float toasterTime = 0;
  toasterTime += 0.04;
  
  gfx->fillScreen(gfx->color565(40, 30, 20));
  
  // Flying Toasters homage
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple flying toasters
  for (int toaster = 0; toaster < 4; toaster++) {
    float flyTime = toasterTime + toaster * 1.5;
    
    // Flying path
    float pathX = (gfx->width() * 0.8) * (sin(flyTime * 0.3) * 0.5 + 0.5) + 20;
    float pathY = 40 + toaster * 40 + sin(flyTime * 0.8) * 15;
    
    // Wing flapping
    float wingFlap = sin(flyTime * 8) * 0.3;
    
    // Draw toaster body
    int toasterW = 30, toasterH = 20;
    uint16_t bodyColor = gfx->color565(180, 150, 120);
    gfx->fillRect(pathX - toasterW/2, pathY - toasterH/2, toasterW, toasterH, bodyColor);
    
    // Toaster slots
    gfx->fillRect(pathX - 10, pathY - 8, 3, 16, gfx->color565(100, 80, 60));
    gfx->fillRect(pathX - 3, pathY - 8, 3, 16, gfx->color565(100, 80, 60));
    gfx->fillRect(pathX + 4, pathY - 8, 3, 16, gfx->color565(100, 80, 60));
    
    // Wings
    int wingSpan = 25;
    int wingY1 = pathY - 5 + wingFlap * 10;
    int wingY2 = pathY + 5 + wingFlap * 10;
    
    // Left wing
    gfx->fillTriangle(
      pathX - toasterW/2, pathY,
      pathX - toasterW/2 - wingSpan, wingY1,
      pathX - toasterW/2 - wingSpan + 10, wingY2,
      gfx->color565(255, 255, 255)
    );
    
    // Right wing  
    gfx->fillTriangle(
      pathX + toasterW/2, pathY,
      pathX + toasterW/2 + wingSpan, wingY1,
      pathX + toasterW/2 + wingSpan - 10, wingY2,
      gfx->color565(255, 255, 255)
    );
    
    // Flying toast
    if (sin(flyTime * 6) > 0.3) {
      for (int toast = 0; toast < 2; toast++) {
        float toastX = pathX + (toast - 0.5) * 8;
        float toastY = pathY - 25 - sin(flyTime * 4 + toast) * 8;
        
        gfx->fillRect(toastX - 3, toastY - 3, 6, 6, gfx->color565(200, 160, 100));
        gfx->drawRect(toastX - 3, toastY - 3, 6, 6, gfx->color565(150, 120, 80));
      }
    }
  }
  
  // Background clouds
  for (int cloud = 0; cloud < 6; cloud++) {
    float cloudTime = toasterTime * 0.2 + cloud;
    int cloudX = (int)(cloudTime * 20) % (gfx->width() + 40) - 20;
    int cloudY = 180 + cloud * 8 + sin(cloudTime) * 5;
    
    gfx->fillCircle(cloudX, cloudY, 8, gfx->color565(220, 220, 255));
    gfx->fillCircle(cloudX + 6, cloudY, 6, gfx->color565(220, 220, 255));
    gfx->fillCircle(cloudX - 6, cloudY, 6, gfx->color565(220, 220, 255));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(255, 200, 100));
  gfx->printf("TOASTER");
}

void drawTopblock() {
  static float blockTime = 0;
  blockTime += 0.05;
  
  gfx->fillScreen(BLACK);
  
  // Falling blocks puzzle game style
  int centerX = gfx->width() / 2;
  
  // Game grid
  int gridW = 12, gridH = 15;
  int blockSize = 12;
  int gridStartX = centerX - (gridW * blockSize) / 2;
  int gridStartY = 20;
  
  // Draw grid outline
  gfx->drawRect(gridStartX - 2, gridStartY - 2, gridW * blockSize + 4, gridH * blockSize + 4, WHITE);
  
  // Static blocks at bottom
  for (int x = 0; x < gridW; x++) {
    for (int y = gridH - 4; y < gridH; y++) {
      if ((x + y) % 3 != 0) {
        int blockX = gridStartX + x * blockSize;
        int blockY = gridStartY + y * blockSize;
        
        uint16_t blockColor = gfx->color565(
          100 + (x * 20) % 155,
          100 + (y * 30) % 155, 
          100 + ((x + y) * 15) % 155
        );
        gfx->fillRect(blockX, blockY, blockSize - 1, blockSize - 1, blockColor);
        gfx->drawRect(blockX, blockY, blockSize - 1, blockSize - 1, WHITE);
      }
    }
  }
  
  // Falling pieces
  for (int piece = 0; piece < 3; piece++) {
    float pieceTime = blockTime + piece * 2.0;
    int fallY = ((int)(pieceTime * 50)) % (gridH * blockSize + 60) - 60;
    int pieceX = gridStartX + (piece * 4 + 2) * blockSize;
    
    // Different piece shapes
    switch (piece) {
      case 0: // L-piece
        gfx->fillRect(pieceX, gridStartY + fallY, blockSize - 1, blockSize * 3 - 1, gfx->color565(255, 100, 100));
        gfx->fillRect(pieceX + blockSize, gridStartY + fallY + blockSize * 2, blockSize - 1, blockSize - 1, gfx->color565(255, 100, 100));
        break;
        
      case 1: // T-piece  
        gfx->fillRect(pieceX, gridStartY + fallY, blockSize * 3 - 1, blockSize - 1, gfx->color565(100, 255, 100));
        gfx->fillRect(pieceX + blockSize, gridStartY + fallY + blockSize, blockSize - 1, blockSize - 1, gfx->color565(100, 255, 100));
        break;
        
      case 2: // Square
        gfx->fillRect(pieceX, gridStartY + fallY, blockSize * 2 - 1, blockSize * 2 - 1, gfx->color565(100, 100, 255));
        break;
    }
  }
  
  // Score display
  gfx->setCursor(10, gfx->height() - 40);
  gfx->setTextColor(WHITE);
  gfx->printf("SCORE: %d", (int)(blockTime * 10) % 9999);
  
  gfx->setCursor(10, gfx->height() - 25);
  gfx->printf("LEVEL: %d", 1 + (int)(blockTime * 0.1) % 10);
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(255, 255, 100));
  gfx->printf("TOPBLOCK");
}

void drawTrackball() {
  static float ballTime = 0;
  ballTime += 0.06;
  
  gfx->fillScreen(BLACK);
  
  // 3D trackball visualization
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  int ballRadius = 60;
  
  // Ball rotation
  float rotX = ballTime * 0.7;
  float rotY = ballTime * 0.5;
  
  // Draw sphere with latitude/longitude lines
  for (int lat = -90; lat <= 90; lat += 30) {
    for (int lon = 0; lon < 360; lon += 10) {
      float latRad = lat * PI / 180;
      float lonRad = lon * PI / 180;
      
      // 3D sphere coordinates
      float x = cos(latRad) * cos(lonRad + rotY);
      float y = cos(latRad) * sin(lonRad + rotY);
      float z = sin(latRad);
      
      // Rotate around X
      float y2 = y * cos(rotX) - z * sin(rotX);
      float z2 = y * sin(rotX) + z * cos(rotX);
      
      // Project to 2D
      if (z2 > 0) { // Only draw front-facing points
        int plotX = centerX + x * ballRadius;
        int plotY = centerY + y2 * ballRadius;
        
        if (plotX >= 0 && plotX < gfx->width() && plotY >= 0 && plotY < gfx->height()) {
          // Brightness based on Z depth
          int brightness = 128 + z2 * 127;
          uint16_t color;
          
          if (lat == 0) {
            color = gfx->color565(brightness, brightness/2, brightness/4); // Equator - red
          } else if (lon % 90 == 0) {
            color = gfx->color565(brightness/4, brightness, brightness/2); // Meridians - green
          } else {
            color = gfx->color565(brightness/2, brightness/2, brightness); // Other lines - blue
          }
          
          gfx->fillCircle(plotX, plotY, 2, color);
        }
      }
    }
  }
  
  // Trackball cursor
  float cursorX = centerX + sin(ballTime * 2) * 80;
  float cursorY = centerY + cos(ballTime * 1.3) * 60;
  gfx->fillCircle(cursorX, cursorY, 5, gfx->color565(255, 255, 100));
  gfx->drawCircle(cursorX, cursorY, 8, WHITE);
  
  // Axis indicators
  gfx->drawLine(centerX - 80, centerY, centerX + 80, centerY, gfx->color565(100, 100, 100));
  gfx->drawLine(centerX, centerY - 80, centerX, centerY + 80, gfx->color565(100, 100, 100));
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->printf("TRACKBALL");
}

void drawTriangle() {
  static float triTime = 0;
  triTime += 0.04;
  
  gfx->fillScreen(BLACK);
  
  // Animated triangular patterns
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple triangle layers
  for (int layer = 0; layer < 6; layer++) {
    float layerTime = triTime + layer * 0.5;
    float radius = 20 + layer * 20;
    float rotation = layerTime * (0.5 + layer * 0.1);
    
    // Triangle vertices
    for (int tri = 0; tri < 3; tri++) {
      float angle1 = rotation + tri * 2 * PI / 3;
      float angle2 = rotation + (tri + 1) * 2 * PI / 3;
      
      int x1 = centerX + cos(angle1) * radius;
      int y1 = centerY + sin(angle1) * radius;
      int x2 = centerX + cos(angle2) * radius;
      int y2 = centerY + sin(angle2) * radius;
      
      uint16_t triColor = gfx->color565(
        128 + 127 * sin(layerTime + tri * 0.8),
        128 + 127 * sin(layerTime + tri * 0.8 + PI/2),
        128 + 127 * sin(layerTime + tri * 0.8 + PI)
      );
      
      // Draw triangle
      gfx->fillTriangle(centerX, centerY, x1, y1, x2, y2, triColor);
      gfx->drawTriangle(centerX, centerY, x1, y1, x2, y2, WHITE);
    }
  }
  
  // Sierpinski triangle elements
  for (int iter = 0; iter < 100; iter++) {
    float iterTime = triTime + iter * 0.1;
    
    // Sierpinski point calculation (simplified)
    float x = 160 + 50 * sin(iterTime * 3 + iter * 0.2);
    float y = 120 + 50 * cos(iterTime * 2.5 + iter * 0.15);
    
    uint16_t dotColor = gfx->color565(
      255 - iter,
      100 + iter,
      150 + sin(iterTime) * 50
    );
    
    if (x >= 0 && x < gfx->width() && y >= 0 && y < gfx->height()) {
      gfx->drawPixel(x, y, dotColor);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(255, 200, 100));
  gfx->printf("TRIANGLE");
}

void drawTronbit() {
  static float tronTime = 0;
  tronTime += 0.06;
  
  gfx->fillScreen(BLACK);
  
  // TRON-style digital world
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Grid lines
  for (int x = 0; x < gfx->width(); x += 20) {
    gfx->drawLine(x, 0, x, gfx->height(), gfx->color565(0, 50, 100));
  }
  for (int y = 0; y < gfx->height(); y += 20) {
    gfx->drawLine(0, y, gfx->width(), y, gfx->color565(0, 50, 100));
  }
  
  // Digital entities/bits
  for (int bit = 0; bit < 8; bit++) {
    float bitTime = tronTime + bit * 0.8;
    
    // Bit position
    float bitX = centerX + sin(bitTime * 2) * (40 + bit * 8);
    float bitY = centerY + cos(bitTime * 1.5) * (30 + bit * 6);
    
    // Bit state animation
    bool bitState = (sin(bitTime * 4) > 0);
    
    if (bitState) {
      // "1" bit - bright cyan cube
      gfx->fillRect(bitX - 4, bitY - 4, 8, 8, gfx->color565(0, 255, 255));
      gfx->drawRect(bitX - 4, bitY - 4, 8, 8, WHITE);
      
      // Data trails
      for (int trail = 1; trail < 6; trail++) {
        int trailX = bitX - cos(bitTime * 2) * trail * 8;
        int trailY = bitY - sin(bitTime * 1.5) * trail * 6;
        int brightness = 255 - trail * 40;
        gfx->fillRect(trailX - 2, trailY - 2, 4, 4, 
          gfx->color565(0, brightness/4, brightness/2));
      }
    } else {
      // "0" bit - dimmer orange
      gfx->fillRect(bitX - 3, bitY - 3, 6, 6, gfx->color565(100, 50, 0));
      gfx->drawRect(bitX - 3, bitY - 3, 6, 6, gfx->color565(150, 100, 50));
    }
  }
  
  // Central processing unit
  int cpuSize = 20 + sin(tronTime * 3) * 5;
  gfx->fillRect(centerX - cpuSize/2, centerY - cpuSize/2, cpuSize, cpuSize, 
    gfx->color565(255, 100, 0));
  gfx->drawRect(centerX - cpuSize/2, centerY - cpuSize/2, cpuSize, cpuSize, 
    gfx->color565(255, 200, 100));
    
  // CPU activity lines
  for (int line = 0; line < 4; line++) {
    float angle = tronTime * 2 + line * PI/2;
    int x1 = centerX + cos(angle) * cpuSize/2;
    int y1 = centerY + sin(angle) * cpuSize/2;
    int x2 = centerX + cos(angle) * (cpuSize/2 + 15);
    int y2 = centerY + sin(angle) * (cpuSize/2 + 15);
    
    gfx->drawLine(x1, y1, x2, y2, gfx->color565(255, 255, 0));
  }
  
  // Binary status display
  gfx->setCursor(10, gfx->height() - 30);
  gfx->setTextColor(gfx->color565(0, 255, 255));
  gfx->printf("01101001");
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(0, 255, 255));
  gfx->printf("TRONBIT");
}

void drawTruchet() {
  static float truchetTime = 0;
  truchetTime += 0.02;
  
  gfx->fillScreen(BLACK);
  
  // Truchet tile patterns
  int tileSize = 20;
  int tilesX = gfx->width() / tileSize + 1;
  int tilesY = gfx->height() / tileSize + 1;
  
  for (int tileX = 0; tileX < tilesX; tileX++) {
    for (int tileY = 0; tileY < tilesY; tileY++) {
      int x = tileX * tileSize;
      int y = tileY * tileSize;
      
      // Tile animation phase
      float tileTime = truchetTime + tileX * 0.1 + tileY * 0.15;
      
      // Tile type based on position and time
      int tileType = ((int)(tileTime * 2) + tileX + tileY) % 4;
      
      uint16_t tileColor = gfx->color565(
        100 + 100 * sin(tileTime + tileX * 0.3),
        100 + 100 * sin(tileTime + tileY * 0.4), 
        100 + 100 * sin(tileTime + (tileX + tileY) * 0.2)
      );
      
      switch (tileType) {
        case 0: // Quarter circles - top-left to bottom-right
          for (int r = 2; r < tileSize/2; r += 2) {
            gfx->drawCircle(x, y, r, tileColor);
            gfx->drawCircle(x + tileSize, y + tileSize, r, tileColor);
          }
          break;
          
        case 1: // Quarter circles - top-right to bottom-left  
          for (int r = 2; r < tileSize/2; r += 2) {
            gfx->drawCircle(x + tileSize, y, r, tileColor);
            gfx->drawCircle(x, y + tileSize, r, tileColor);
          }
          break;
          
        case 2: // Diagonal lines
          gfx->drawLine(x, y, x + tileSize, y + tileSize, tileColor);
          gfx->drawLine(x + 2, y, x + tileSize, y + tileSize - 2, tileColor);
          gfx->drawLine(x, y + 2, x + tileSize - 2, y + tileSize, tileColor);
          break;
          
        case 3: // Anti-diagonal lines
          gfx->drawLine(x + tileSize, y, x, y + tileSize, tileColor);
          gfx->drawLine(x + tileSize - 2, y, x, y + tileSize - 2, tileColor);
          gfx->drawLine(x + tileSize, y + 2, x + 2, y + tileSize, tileColor);
          break;
      }
      
      // Highlight active tiles
      if (sin(tileTime * 4) > 0.8) {
        gfx->drawRect(x, y, tileSize, tileSize, WHITE);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("TRUCHET");
}

void drawTube() {
  static float tubeTime = 0;
  tubeTime += 0.05;
  
  gfx->fillScreen(BLACK);
  
  // 3D tube/tunnel effect
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple concentric tube rings
  for (int ring = 0; ring < 12; ring++) {
    float ringTime = tubeTime + ring * 0.2;
    float ringZ = fmod(ringTime * 60, 300) - 100; // Z depth
    
    if (ringZ > 0) { // Only draw rings in front
      float scale = 200.0 / ringZ; // Perspective scaling
      int radius = 20 * scale;
      
      if (radius > 2 && radius < 200) {
        // Ring segments
        int segments = 16;
        for (int seg = 0; seg < segments; seg++) {
          float angle = (seg * 2.0 * PI / segments) + ringTime * 0.3;
          
          int x = centerX + cos(angle) * radius;
          int y = centerY + sin(angle) * radius;
          
          // Brightness based on distance
          int brightness = 255 * scale;
          if (brightness > 255) brightness = 255;
          
          uint16_t color = gfx->color565(brightness/2, brightness/3, brightness);
          gfx->fillCircle(x, y, 2 + scale, color);
          
          // Connect to next segment
          if (seg < segments - 1) {
            float nextAngle = ((seg + 1) * 2.0 * PI / segments) + ringTime * 0.3;
            int nextX = centerX + cos(nextAngle) * radius;
            int nextY = centerY + sin(nextAngle) * radius;
            gfx->drawLine(x, y, nextX, nextY, color);
          }
        }
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(100, 150, 255));
  gfx->printf("TUBE");
}

void drawTwang() {
  static float twangTime = 0;
  twangTime += 0.04;
  
  gfx->fillScreen(BLACK);
  
  // Guitar string vibration effect
  int stringY = gfx->height() / 2;
  
  // Multiple strings
  for (int string = 0; string < 6; string++) {
    float stringTime = twangTime + string * 0.3;
    int yOffset = (string - 2.5) * 25;
    
    // String amplitude and frequency
    float amplitude = 15 * exp(-stringTime * 0.5); // Decay over time
    float frequency = 0.2 + string * 0.05;
    
    // Draw vibrating string
    for (int x = 0; x < gfx->width(); x += 2) {
      float vibration = amplitude * sin(stringTime * 8 + x * frequency);
      int y = stringY + yOffset + vibration;
      
      uint16_t stringColor = gfx->color565(
        200 + string * 10,
        150 - string * 20,
        100 + sin(stringTime) * 50
      );
      
      if (y >= 0 && y < gfx->height()) {
        gfx->drawPixel(x, y, stringColor);
        
        // Harmonic overtones
        int harmonic2Y = y + vibration * 0.3 * sin(stringTime * 16 + x * frequency * 2);
        int harmonic3Y = y + vibration * 0.1 * sin(stringTime * 24 + x * frequency * 3);
        
        if (harmonic2Y >= 0 && harmonic2Y < gfx->height()) {
          uint16_t dimColor = gfx->color565((200 + string * 10) / 2, (150 - string * 20) / 2, (int)(100 + sin(stringTime) * 50) / 2);
          gfx->drawPixel(x, harmonic2Y, dimColor);
        }
        if (harmonic3Y >= 0 && harmonic3Y < gfx->height()) {
          uint16_t dimColor2 = gfx->color565((200 + string * 10) / 4, (150 - string * 20) / 4, (int)(100 + sin(stringTime) * 50) / 4);
          gfx->drawPixel(x, harmonic3Y, dimColor2);
        }
      }
    }
    
    // String endpoints (tuning pegs and bridge)
    gfx->fillCircle(5, stringY + yOffset, 3, gfx->color565(150, 150, 100));
    gfx->fillCircle(gfx->width() - 5, stringY + yOffset, 2, gfx->color565(200, 200, 150));
  }
  
  // Sound waves visualization
  for (int wave = 0; wave < 20; wave++) {
    float waveTime = twangTime * 3 + wave * 0.1;
    int waveX = (int)(waveTime * 50) % gfx->width();
    int waveAmplitude = 30 * exp(-waveTime * 0.2);
    
    for (int y = 0; y < gfx->height(); y += 3) {
      int intensity = waveAmplitude * sin(y * 0.1 + waveTime);
      if (intensity > 5) {
        gfx->drawPixel(waveX, y, gfx->color565(intensity, intensity/2, 100));
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(255, 200, 100));
  gfx->printf("TWANG");
}

void drawUnicrud() {
  static float unicrudTime = 0;
  unicrudTime += 0.03;
  
  gfx->fillScreen(gfx->color565(20, 0, 40));
  
  // Unicode character matrix effect
  const char* unicodeChars[] = {"α", "β", "γ", "δ", "ε", "ζ", "η", "θ", "λ", "μ", "π", "ρ", "σ", "τ", "φ", "χ", "ψ", "ω"};
  int numChars = sizeof(unicodeChars) / sizeof(unicodeChars[0]);
  
  int charW = 12, charH = 16;
  int cols = gfx->width() / charW;
  int rows = gfx->height() / charH;
  
  for (int col = 0; col < cols; col++) {
    for (int row = 0; row < rows; row++) {
      float charTime = unicrudTime + col * 0.1 + row * 0.05;
      
      // Character animation phase
      float phase = fmod(charTime * 2, 4);
      
      int x = col * charW;
      int y = row * charH;
      
      if (phase < 1) {
        // Appearing phase
        uint16_t fadeColor = gfx->color565(
          (int)(255 * phase),
          (int)(200 * phase),
          (int)(100 * phase)
        );
        
        int charIndex = (col + row + (int)charTime) % numChars;
        gfx->setCursor(x, y);
        gfx->setTextColor(fadeColor);
        gfx->setTextSize(1);
        gfx->printf("%s", unicodeChars[charIndex]);
      } else if (phase < 3) {
        // Stable phase - bright
        uint16_t stableColor = gfx->color565(
          150 + 105 * sin(charTime * 3),
          100 + 100 * cos(charTime * 2.5),
          50 + 50 * sin(charTime * 4)
        );
        
        int charIndex = (col + row + (int)charTime) % numChars;
        gfx->setCursor(x, y);
        gfx->setTextColor(stableColor);
        gfx->setTextSize(1);
        gfx->printf("%s", unicodeChars[charIndex]);
      }
      // Else: disappeared phase (draw nothing)
      
      // Random glitch effects
      if (sin(charTime * 10 + col + row) > 0.95) {
        gfx->drawRect(x - 1, y - 1, charW + 2, charH + 2, gfx->color565(255, 0, 255));
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(255, 255, 100));
  gfx->printf("UNICRUD");
}

void drawUnknownpleasures() {
  static float pleasuresTime = 0;
  pleasuresTime += 0.02;
  
  gfx->fillScreen(BLACK);
  
  // Joy Division "Unknown Pleasures" album cover homage
  int numLines = 24;
  int lineSpacing = gfx->height() / numLines;
  
  for (int line = 0; line < numLines; line++) {
    float lineTime = pleasuresTime + line * 0.1;
    int baseY = line * lineSpacing + 10;
    
    // Each line represents a pulsar signal
    for (int x = 0; x < gfx->width(); x += 2) {
      float normalizedX = (float)x / gfx->width();
      
      // Create the characteristic pulsar pulse shape
      float pulse1 = exp(-pow((normalizedX - 0.3) * 8, 2)) * 30;
      float pulse2 = exp(-pow((normalizedX - 0.5) * 12, 2)) * 40;
      float pulse3 = exp(-pow((normalizedX - 0.7) * 6, 2)) * 20;
      
      // Add time-varying modulation
      float modulation = sin(lineTime * 2 + line * 0.3) * 0.3 + 1;
      float totalPulse = (pulse1 + pulse2 + pulse3) * modulation;
      
      // Add noise
      float noise = (sin(lineTime * 50 + x * 0.5) * 0.1 + sin(lineTime * 37 + x * 0.3) * 0.05) * 10;
      totalPulse += noise;
      
      int y = baseY - totalPulse;
      
      if (y >= 0 && y < gfx->height()) {
        uint16_t pulseColor = gfx->color565(255, 255, 255);
        gfx->drawPixel(x, y, pulseColor);
        
        // Draw connecting lines to create solid waveform
        if (x > 0 && totalPulse > 2) {
          int prevY = baseY - totalPulse;
          gfx->drawLine(x - 2, prevY, x, y, pulseColor);
        }
      }
    }
  }
  
  gfx->setCursor(10, gfx->height() - 20);
  gfx->setTextColor(WHITE);
  gfx->printf("UNKNOWN PLEASURES");
}

void drawVermiculate() {
  static float vermTime = 0;
  vermTime += 0.03;
  
  gfx->fillScreen(gfx->color565(40, 20, 10));
  
  // Worm-like creatures crawling
  for (int worm = 0; worm < 6; worm++) {
    float wormTime = vermTime + worm * 1.2;
    
    // Worm path
    float pathPhase = wormTime * 0.5;
    int segments = 12;
    
    for (int seg = 0; seg < segments; seg++) {
      float segTime = pathPhase - seg * 0.1;
      
      // Worm body position
      float wormX = gfx->width() * 0.5 + sin(segTime) * (60 + worm * 10);
      float wormY = gfx->height() * 0.3 + worm * 25 + cos(segTime * 1.3) * 20;
      
      // Segment size (head larger than tail)
      int segSize = 6 - seg/3;
      if (segSize < 2) segSize = 2;
      
      // Worm color (varies along body)
      uint16_t segColor = gfx->color565(
        100 + seg * 8 + sin(wormTime) * 30,
        80 + seg * 5 + cos(wormTime * 1.2) * 20,
        60 + seg * 3
      );
      
      gfx->fillCircle(wormX, wormY, segSize, segColor);
      
      // Connect segments
      if (seg > 0) {
        float prevSegTime = pathPhase - (seg - 1) * 0.1;
        float prevX = gfx->width() * 0.5 + sin(prevSegTime) * (60 + worm * 10);
        float prevY = gfx->height() * 0.3 + worm * 25 + cos(prevSegTime * 1.3) * 20;
        
        gfx->drawLine(prevX, prevY, wormX, wormY, segColor);
      }
    }
  }
  
  // Dirt particles
  for (int particle = 0; particle < 30; particle++) {
    float partTime = vermTime * 2 + particle * 0.4;
    int partX = (int)(partTime * 15) % (gfx->width() + 20) - 10;
    int partY = 40 + particle * 6 + sin(partTime) * 8;
    
    gfx->drawPixel(partX, partY, gfx->color565(80, 60, 40));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(150, 120, 80));
  gfx->printf("VERMICULATE");
}

void drawVfeedback() {
  static float feedbackTime = 0;
  feedbackTime += 0.06;
  
  gfx->fillScreen(BLACK);
  
  // Video feedback effect
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Recursive feedback layers
  for (int layer = 0; layer < 8; layer++) {
    float layerTime = feedbackTime + layer * 0.2;
    float scale = 0.9 - layer * 0.08;
    float rotation = layerTime * 0.1 + layer * 0.3;
    
    // Feedback rectangle
    int rectW = gfx->width() * scale;
    int rectH = gfx->height() * scale;
    
    // Rotate around center
    for (int edge = 0; edge < 4; edge++) {
      float angle1 = rotation + edge * PI/2;
      float angle2 = rotation + (edge + 1) * PI/2;
      
      int x1 = centerX + cos(angle1) * rectW/2;
      int y1 = centerY + sin(angle1) * rectH/2;
      int x2 = centerX + cos(angle2) * rectW/2;
      int y2 = centerY + sin(angle2) * rectH/2;
      
      uint16_t edgeColor = gfx->color565(
        255 - layer * 25,
        100 + layer * 15 + sin(layerTime) * 50,
        layer * 30 + cos(layerTime * 1.5) * 40
      );
      
      gfx->drawLine(x1, y1, x2, y2, edgeColor);
    }
    
    // Feedback pixels
    if (layer % 2 == 0) {
      for (int i = 0; i < 20; i++) {
        float pixelAngle = layerTime * 3 + i * 0.5;
        int pixelX = centerX + cos(pixelAngle) * rectW/3;
        int pixelY = centerY + sin(pixelAngle) * rectH/3;
        
        uint16_t pixelColor = gfx->color565(
          255 - layer * 20,
          255 - layer * 30,
          100 + layer * 20
        );
        
        gfx->fillCircle(pixelX, pixelY, 2, pixelColor);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(255, 200, 100));
  gfx->printf("VFEEDBACK");
}

void drawVigilance() {
  static float vigilTime = 0;
  vigilTime += 0.05;
  
  gfx->fillScreen(gfx->color565(10, 0, 20));
  
  // Surveillance/monitoring system
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Radar sweep
  float sweepAngle = vigilTime * 2;
  int radarRadius = 80;
  
  // Draw radar circles
  for (int ring = 1; ring <= 4; ring++) {
    int radius = ring * radarRadius / 4;
    gfx->drawCircle(centerX, centerY, radius, gfx->color565(0, 100, 0));
  }
  
  // Radar sweep beam
  int beamX = centerX + cos(sweepAngle) * radarRadius;
  int beamY = centerY + sin(sweepAngle) * radarRadius;
  gfx->drawLine(centerX, centerY, beamX, beamY, gfx->color565(0, 255, 0));
  
  // Detected targets
  for (int target = 0; target < 8; target++) {
    float targetTime = vigilTime + target * 0.8;
    float targetAngle = target * PI/4 + sin(targetTime * 0.3) * 0.2;
    float targetDistance = 30 + target * 8 + sin(targetTime) * 10;
    
    int targetX = centerX + cos(targetAngle) * targetDistance;
    int targetY = centerY + sin(targetAngle) * targetDistance;
    
    // Target visibility based on sweep
    float angleDiff = abs(fmod(targetAngle - sweepAngle + PI, 2*PI) - PI);
    if (angleDiff < 0.5) {
      // Target detected - bright
      gfx->fillCircle(targetX, targetY, 4, gfx->color565(255, 0, 0));
      gfx->drawCircle(targetX, targetY, 6, gfx->color565(255, 100, 100));
      
      // Target info
      gfx->setCursor(targetX + 10, targetY - 3);
      gfx->setTextColor(gfx->color565(255, 255, 0));
      gfx->setTextSize(1);
      gfx->printf("T%d", target + 1);
    } else {
      // Target fading
      int fade = 100 - angleDiff * 200;
      if (fade > 0) {
        gfx->fillCircle(targetX, targetY, 2, gfx->color565(fade, 0, 0));
      }
    }
  }
  
  // Status display
  gfx->setCursor(10, gfx->height() - 40);
  gfx->setTextColor(gfx->color565(0, 255, 255));
  gfx->printf("SCAN: %d%%", (int)(fmod(vigilTime * 50, 100)));
  
  gfx->setCursor(10, gfx->height() - 25);
  gfx->printf("TARGETS: 8");
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(255, 100, 100));
  gfx->printf("VIGILANCE");
}

void drawVines() {
  static float vineTime = 0;
  vineTime += 0.02;
  
  gfx->fillScreen(gfx->color565(20, 40, 20));
  
  // Growing vines
  for (int vine = 0; vine < 5; vine++) {
    float vineStartTime = vineTime + vine * 2.0;
    
    // Vine growth from bottom
    int startX = 30 + vine * 50;
    int startY = gfx->height() - 10;
    
    int segments = (int)(vineStartTime * 10) % 25;
    
    float currentX = startX;
    float currentY = startY;
    float currentAngle = -PI/2; // Start growing upward
    
    for (int seg = 0; seg < segments; seg++) {
      float segTime = vineStartTime - seg * 0.1;
      
      // Vine growth direction (mostly up, some random)
      currentAngle += (sin(segTime * 3 + vine) * 0.3);
      
      float segLength = 8 + sin(segTime * 2) * 3;
      float newX = currentX + cos(currentAngle) * segLength;
      float newY = currentY + sin(currentAngle) * segLength;
      
      // Draw vine segment
      uint16_t vineColor = gfx->color565(
        50 + seg * 2,
        100 + seg * 3 + sin(segTime) * 20,
        50 + seg * 1
      );
      
      gfx->drawLine(currentX, currentY, newX, newY, vineColor);
      
      // Leaves occasionally
      if (seg % 4 == 0 && seg > 0) {
        float leafAngle1 = currentAngle + PI/3;
        float leafAngle2 = currentAngle - PI/3;
        
        int leafX1 = currentX + cos(leafAngle1) * 6;
        int leafY1 = currentY + sin(leafAngle1) * 6;
        int leafX2 = currentX + cos(leafAngle2) * 6;
        int leafY2 = currentY + sin(leafAngle2) * 6;
        
        uint16_t leafColor = gfx->color565(30, 150, 30);
        gfx->fillTriangle(currentX, currentY, leafX1, leafY1, leafX2, leafY2, leafColor);
      }
      
      currentX = newX;
      currentY = newY;
      
      // Stop if out of bounds
      if (currentY < 0 || currentX < 0 || currentX >= gfx->width()) break;
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(100, 255, 100));
  gfx->printf("VINES");
}

void drawVoronoi() {
  static float voronoiTime = 0;
  voronoiTime += 0.03;
  
  gfx->fillScreen(BLACK);
  
  // Voronoi diagram with moving seed points
  int numSeeds = 8;
  int seeds[8][2];
  
  // Calculate seed positions
  for (int i = 0; i < numSeeds; i++) {
    float seedTime = voronoiTime + i * 0.8;
    seeds[i][0] = gfx->width() * 0.5 + sin(seedTime * 0.7 + i) * 60;
    seeds[i][1] = gfx->height() * 0.5 + cos(seedTime * 0.5 + i) * 50;
  }
  
  // For each pixel, find closest seed
  for (int x = 0; x < gfx->width(); x += 2) {
    for (int y = 0; y < gfx->height(); y += 2) {
      int closestSeed = 0;
      float minDistance = 99999;
      
      // Find closest seed
      for (int seed = 0; seed < numSeeds; seed++) {
        float dx = x - seeds[seed][0];
        float dy = y - seeds[seed][1];
        float distance = sqrt(dx*dx + dy*dy);
        
        if (distance < minDistance) {
          minDistance = distance;
          closestSeed = seed;
        }
      }
      
      // Color based on closest seed
      uint16_t cellColor = gfx->color565(
        100 + closestSeed * 20 + sin(voronoiTime + closestSeed) * 50,
        80 + closestSeed * 15 + cos(voronoiTime + closestSeed * 1.3) * 40,
        120 + closestSeed * 10 + sin(voronoiTime + closestSeed * 0.8) * 30
      );
      
      gfx->fillRect(x, y, 2, 2, cellColor);
    }
  }
  
  // Draw seed points
  for (int i = 0; i < numSeeds; i++) {
    uint16_t seedColor = gfx->color565(255, 255, 255);
    gfx->fillCircle(seeds[i][0], seeds[i][1], 3, seedColor);
    gfx->drawCircle(seeds[i][0], seeds[i][1], 5, seedColor);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("VORONOI");
}

void drawWander() {
  static float wanderTime = 0;
  wanderTime += 0.02;
  
  gfx->fillScreen(gfx->color565(5, 5, 15));
  
  // Wandering particles/creatures
  for (int wanderer = 0; wanderer < 12; wanderer++) {
    float wandererTime = wanderTime + wanderer * 0.6;
    
    // Random walk path
    static float wandererX[12] = {160, 80, 240, 120, 200, 60, 280, 40, 180, 220, 100, 260};
    static float wandererY[12] = {120, 60, 180, 90, 150, 200, 100, 170, 80, 50, 190, 130};
    
    // Update position with random walk
    wandererX[wanderer] += sin(wandererTime * 3 + wanderer) * 2;
    wandererY[wanderer] += cos(wandererTime * 2.5 + wanderer) * 2;
    
    // Wrap around screen
    if (wandererX[wanderer] < 0) wandererX[wanderer] = gfx->width();
    if (wandererX[wanderer] > gfx->width()) wandererX[wanderer] = 0;
    if (wandererY[wanderer] < 0) wandererY[wanderer] = gfx->height();
    if (wandererY[wanderer] > gfx->height()) wandererY[wanderer] = 0;
    
    // Draw wanderer
    uint16_t wandererColor = gfx->color565(
      150 + sin(wandererTime + wanderer) * 105,
      100 + cos(wandererTime * 1.3 + wanderer) * 80,
      200 + sin(wandererTime * 0.7 + wanderer) * 55
    );
    
    gfx->fillCircle(wandererX[wanderer], wandererY[wanderer], 3, wandererColor);
    
    // Trail
    for (int trail = 1; trail < 8; trail++) {
      float trailX = wandererX[wanderer] - sin(wandererTime * 3 + wanderer) * 2 * trail;
      float trailY = wandererY[wanderer] - cos(wandererTime * 2.5 + wanderer) * 2 * trail;
      
      int trailBrightness = 255 - trail * 30;
      if (trailBrightness > 0) {
        uint16_t trailColor = gfx->color565(
          trailBrightness * (150 + sin(wandererTime + wanderer) * 105) / 255,
          trailBrightness * (100 + cos(wandererTime * 1.3 + wanderer) * 80) / 255,
          trailBrightness * (200 + sin(wandererTime * 0.7 + wanderer) * 55) / 255
        );
        gfx->drawPixel(trailX, trailY, trailColor);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->printf("WANDER");
}

void drawWhale() {
  static float whaleTime = 0;
  whaleTime += 0.03;
  
  gfx->fillScreen(gfx->color565(0, 20, 60));
  
  // Underwater whale scene
  int whaleX = gfx->width() * 0.3 + sin(whaleTime * 0.4) * 40;
  int whaleY = gfx->height() * 0.4 + cos(whaleTime * 0.3) * 20;
  
  // Draw whale body
  gfx->fillCircle(whaleX, whaleY, 25, gfx->color565(80, 100, 140));
  gfx->fillCircle(whaleX - 15, whaleY, 20, gfx->color565(70, 90, 130));
  
  // Whale tail
  int tailX = whaleX - 35;
  int tailY = whaleY + sin(whaleTime * 3) * 8;
  gfx->fillTriangle(
    tailX, tailY,
    tailX - 20, tailY - 15,
    tailX - 20, tailY + 15,
    gfx->color565(60, 80, 120)
  );
  
  // Whale eye
  gfx->fillCircle(whaleX + 8, whaleY - 5, 4, gfx->color565(200, 200, 255));
  gfx->fillCircle(whaleX + 10, whaleY - 3, 2, BLACK);
  
  // Water spout
  if (sin(whaleTime * 2) > 0.7) {
    for (int spout = 0; spout < 8; spout++) {
      int spoutY = whaleY - 30 - spout * 8;
      int spoutX = whaleX + sin(whaleTime * 6 + spout) * 3;
      gfx->fillCircle(spoutX, spoutY, 2, gfx->color565(150, 200, 255));
    }
  }
  
  // Ocean bubbles
  for (int bubble = 0; bubble < 15; bubble++) {
    float bubbleTime = whaleTime * 2 + bubble * 0.3;
    int bubbleX = 20 + bubble * 20 + sin(bubbleTime) * 10;
    int bubbleY = gfx->height() - 20 - (int)(bubbleTime * 30) % (gfx->height() - 40);
    
    if (bubbleY > 0) {
      gfx->drawCircle(bubbleX, bubbleY, 3, gfx->color565(100, 150, 200));
    }
  }
  
  // Sea floor
  for (int x = 0; x < gfx->width(); x += 5) {
    int floorY = gfx->height() - 15 + sin(whaleTime + x * 0.1) * 5;
    gfx->fillRect(x, floorY, 5, gfx->height() - floorY, gfx->color565(40, 60, 20));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(100, 200, 255));
  gfx->printf("WHALE");
}

void drawWhirlwindwarp() {
  static float warpTime = 0;
  warpTime += 0.08;
  
  gfx->fillScreen(BLACK);
  
  // Whirlwind warp effect
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  for (int layer = 0; layer < 6; layer++) {
    float layerTime = warpTime + layer * 0.4;
    float rotation = layerTime * (1 + layer * 0.2);
    
    int numSpirals = 8 + layer * 2;
    for (int spiral = 0; spiral < numSpirals; spiral++) {
      float spiralAngle = (spiral * 2.0 * PI / numSpirals) + rotation;
      
      // Spiral positions with increasing radius
      for (int point = 0; point < 20; point++) {
        float radius = point * 3 + layer * 10;
        float warpRadius = radius + sin(layerTime * 3 + point * 0.2) * 8;
        
        int x = centerX + cos(spiralAngle + point * 0.3) * warpRadius;
        int y = centerY + sin(spiralAngle + point * 0.3) * warpRadius;
        
        if (x >= 0 && x < gfx->width() && y >= 0 && y < gfx->height()) {
          uint16_t warpColor = gfx->color565(
            255 - layer * 30 - point * 8,
            100 + layer * 20 + sin(layerTime + point) * 60,
            layer * 40 + cos(layerTime + spiral) * 80
          );
          
          gfx->fillCircle(x, y, 2, warpColor);
          
          // Connect points for spiral effect
          if (point > 0) {
            float prevRadius = (point - 1) * 3 + layer * 10;
            float prevWarpRadius = prevRadius + sin(layerTime * 3 + (point - 1) * 0.2) * 8;
            int prevX = centerX + cos(spiralAngle + (point - 1) * 0.3) * prevWarpRadius;
            int prevY = centerY + sin(spiralAngle + (point - 1) * 0.3) * prevWarpRadius;
            
            gfx->drawLine(prevX, prevY, x, y, warpColor);
          }
        }
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(255, 150, 200));
  gfx->printf("WHIRLWINDWARP");
}

void drawWhirlygig() {
  static float gigTime = 0;
  gigTime += 0.06;
  
  gfx->fillScreen(gfx->color565(10, 10, 30));
  
  // Spinning whirligig patterns
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple spinning elements
  for (int gig = 0; gig < 4; gig++) {
    float gigRotation = gigTime * (2 + gig * 0.5);
    int gigRadius = 30 + gig * 20;
    
    // Gig center position
    float gigCenterAngle = gigTime * 0.3 + gig * PI/2;
    int gigCenterX = centerX + cos(gigCenterAngle) * 40;
    int gigCenterY = centerY + sin(gigCenterAngle) * 30;
    
    // Spinning arms
    for (int arm = 0; arm < 6; arm++) {
      float armAngle = gigRotation + arm * PI/3;
      
      // Arm segments
      for (int seg = 1; seg <= 4; seg++) {
        float segRadius = seg * gigRadius / 4;
        int armX = gigCenterX + cos(armAngle) * segRadius;
        int armY = gigCenterY + sin(armAngle) * segRadius;
        
        uint16_t segColor = gfx->color565(
          150 + gig * 20 + seg * 15,
          100 + arm * 25 + cos(gigTime + gig + arm) * 60,
          200 - gig * 30 + sin(gigTime + seg) * 50
        );
        
        gfx->fillCircle(armX, armY, 4 - seg/2, segColor);
        
        // Connect segments
        if (seg > 1) {
          float prevSegRadius = (seg - 1) * gigRadius / 4;
          int prevArmX = gigCenterX + cos(armAngle) * prevSegRadius;
          int prevArmY = gigCenterY + sin(armAngle) * prevSegRadius;
          gfx->drawLine(prevArmX, prevArmY, armX, armY, segColor);
        }
      }
    }
    
    // Central hub
    gfx->fillCircle(gigCenterX, gigCenterY, 6, gfx->color565(255, 255, 100));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(255, 200, 150));
  gfx->printf("WHIRLYGIG");
}

void drawWinduprobot() {
  static float robotTime = 0;
  robotTime += 0.04;
  
  gfx->fillScreen(gfx->color565(20, 30, 40));
  
  // Wind-up toy robot
  int robotX = gfx->width() / 2;
  int robotY = gfx->height() / 2;
  
  // Robot walking animation
  float walkCycle = sin(robotTime * 4);
  int robotOffset = walkCycle * 3;
  
  // Robot body
  gfx->fillRect(robotX - 15, robotY - 20, 30, 35, gfx->color565(150, 150, 180));
  gfx->drawRect(robotX - 15, robotY - 20, 30, 35, gfx->color565(100, 100, 120));
  
  // Robot head
  gfx->fillRect(robotX - 12, robotY - 35, 24, 15, gfx->color565(180, 180, 200));
  gfx->drawRect(robotX - 12, robotY - 35, 24, 15, gfx->color565(120, 120, 140));
  
  // Robot eyes
  gfx->fillCircle(robotX - 6, robotY - 28, 3, gfx->color565(255, 100, 100));
  gfx->fillCircle(robotX + 6, robotY - 28, 3, gfx->color565(255, 100, 100));
  
  // Robot legs (walking)
  int leftLegY = robotY + 15 + walkCycle * 2;
  int rightLegY = robotY + 15 - walkCycle * 2;
  
  gfx->fillRect(robotX - 12, leftLegY, 8, 20, gfx->color565(120, 120, 150));
  gfx->fillRect(robotX + 4, rightLegY, 8, 20, gfx->color565(120, 120, 150));
  
  // Robot arms (swinging)
  int leftArmX = robotX - 20 + walkCycle * 3;
  int rightArmX = robotX + 20 - walkCycle * 3;
  
  gfx->fillRect(leftArmX, robotY - 5, 6, 15, gfx->color565(140, 140, 170));
  gfx->fillRect(rightArmX, robotY - 5, 6, 15, gfx->color565(140, 140, 170));
  
  // Wind-up key
  float keyAngle = robotTime * 3;
  int keyX = robotX + 20;
  int keyY = robotY - 10;
  
  gfx->fillCircle(keyX, keyY, 4, gfx->color565(200, 200, 100));
  
  int keyEndX = keyX + cos(keyAngle) * 8;
  int keyEndY = keyY + sin(keyAngle) * 8;
  gfx->drawLine(keyX, keyY, keyEndX, keyEndY, gfx->color565(180, 180, 80));
  
  // Sparks from mechanical movement
  for (int spark = 0; spark < 5; spark++) {
    float sparkTime = robotTime * 8 + spark * 0.8;
    if (sin(sparkTime) > 0.8) {
      int sparkX = robotX + sin(sparkTime * 3) * 25;
      int sparkY = robotY + cos(sparkTime * 2.5) * 20;
      gfx->drawPixel(sparkX, sparkY, gfx->color565(255, 255, 100));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(200, 200, 255));
  gfx->printf("WINDUPROBOT");
}

void drawWorm() {
  static float wormTime = 0;
  wormTime += 0.04;
  
  gfx->fillScreen(gfx->color565(30, 20, 10));
  
  // Multiple segmented worms
  for (int wormNum = 0; wormNum < 4; wormNum++) {
    float wormPhase = wormTime + wormNum * 1.5;
    
    int segments = 15;
    for (int seg = 0; seg < segments; seg++) {
      float segTime = wormPhase - seg * 0.1;
      
      // Worm movement path
      float wormX = gfx->width() * 0.2 + sin(segTime * 0.8) * 80 + wormNum * 60;
      float wormY = gfx->height() * 0.3 + wormNum * 40 + cos(segTime * 1.2) * 30;
      
      // Segment size (head larger)
      int segSize = (seg == 0) ? 8 : (6 - seg/4);
      if (segSize < 3) segSize = 3;
      
      // Worm colors
      uint16_t wormColor = gfx->color565(
        120 + wormNum * 20 + seg * 5,
        80 + wormNum * 15 + sin(wormPhase + seg) * 30,
        60 + wormNum * 10 + seg * 2
      );
      
      gfx->fillCircle(wormX, wormY, segSize, wormColor);
      
      // Connect segments
      if (seg > 0) {
        float prevSegTime = wormPhase - (seg - 1) * 0.1;
        float prevWormX = gfx->width() * 0.2 + sin(prevSegTime * 0.8) * 80 + wormNum * 60;
        float prevWormY = gfx->height() * 0.3 + wormNum * 40 + cos(prevSegTime * 1.2) * 30;
        
        gfx->drawLine(prevWormX, prevWormY, wormX, wormY, wormColor);
      }
    }
  }
  
  // Dirt particles
  for (int dirt = 0; dirt < 20; dirt++) {
    float dirtTime = wormTime * 3 + dirt * 0.4;
    int dirtX = (int)(dirtTime * 20) % (gfx->width() + 30) - 15;
    int dirtY = 50 + dirt * 8 + sin(dirtTime) * 5;
    
    gfx->drawPixel(dirtX, dirtY, gfx->color565(100, 70, 50));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(200, 150, 100));
  gfx->printf("WORM");
}

void drawWormhole() {
  static float holeTime = 0;
  holeTime += 0.08;
  
  gfx->fillScreen(BLACK);
  
  // Wormhole space-time distortion
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple concentric rings moving toward center
  for (int ring = 0; ring < 15; ring++) {
    float ringTime = holeTime + ring * 0.3;
    
    // Ring contracts over time
    float baseRadius = 120 - fmod(ringTime * 40, 140);
    if (baseRadius <= 0) continue;
    
    // Ring distortion
    int segments = 16;
    for (int seg = 0; seg < segments; seg++) {
      float angle = (seg * 2.0 * PI / segments) + ringTime * 0.5;
      
      // Wormhole distortion
      float distortion = sin(ringTime * 2 + seg * 0.5) * 10;
      float radius = baseRadius + distortion;
      
      int x = centerX + cos(angle) * radius;
      int y = centerY + sin(angle) * radius;
      
      if (x >= 0 && x < gfx->width() && y >= 0 && y < gfx->height()) {
        // Color based on distance from center
        float perspective = baseRadius / 120.0;
        uint16_t ringColor = gfx->color565(
          (int)(255 * perspective + sin(ringTime + seg) * 50),
          (int)(150 * perspective + cos(ringTime + seg) * 40),
          (int)(100 * perspective + sin(ringTime * 1.5 + seg) * 60)
        );
        
        gfx->fillCircle(x, y, 3, ringColor);
        
        // Connect to next segment
        if (seg < segments - 1) {
          float nextAngle = ((seg + 1) * 2.0 * PI / segments) + ringTime * 0.5;
          float nextRadius = baseRadius + sin(ringTime * 2 + (seg + 1) * 0.5) * 10;
          int nextX = centerX + cos(nextAngle) * nextRadius;
          int nextY = centerY + sin(nextAngle) * nextRadius;
          
          gfx->drawLine(x, y, nextX, nextY, ringColor);
        }
      }
    }
  }
  
  // Central vortex
  for (int vortex = 0; vortex < 8; vortex++) {
    float vortexAngle = holeTime * 4 + vortex * PI/4;
    int vortexRadius = 5 + vortex * 3;
    
    int vortexX = centerX + cos(vortexAngle) * vortexRadius;
    int vortexY = centerY + sin(vortexAngle) * vortexRadius;
    
    gfx->fillCircle(vortexX, vortexY, 2, gfx->color565(255, 255, 255));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(200, 150, 255));
  gfx->printf("WORMHOLE");
}

void drawXanalogtv() {
  static float tvTime = 0;
  tvTime += 0.1;
  
  gfx->fillScreen(BLACK);
  
  // Analog TV static and interference
  
  // TV screen frame
  gfx->drawRect(20, 20, gfx->width() - 40, gfx->height() - 40, gfx->color565(100, 100, 100));
  gfx->drawRect(21, 21, gfx->width() - 42, gfx->height() - 42, gfx->color565(80, 80, 80));
  
  // Static noise
  for (int x = 25; x < gfx->width() - 25; x += 2) {
    for (int y = 25; y < gfx->height() - 25; y += 2) {
      float noise = sin(tvTime * 50 + x * 0.1 + y * 0.1) * sin(tvTime * 37 + x * 0.07 + y * 0.13);
      if (noise > 0.3) {
        int intensity = 150 + noise * 105;
        gfx->fillRect(x, y, 2, 2, gfx->color565(intensity, intensity, intensity));
      }
    }
  }
  
  // Horizontal scan lines
  for (int y = 25; y < gfx->height() - 25; y += 3) {
    float scanLine = sin(tvTime * 8 + y * 0.2) * 0.5 + 0.5;
    uint16_t lineColor = gfx->color565(scanLine * 80, scanLine * 80, scanLine * 80);
    gfx->drawLine(25, y, gfx->width() - 25, y, lineColor);
  }
  
  // Rolling interference
  float rollY = fmod(tvTime * 30, gfx->height() - 50) + 25;
  for (int x = 25; x < gfx->width() - 25; x++) {
    int interferenceY = rollY + sin(tvTime * 3 + x * 0.1) * 5;
    if (interferenceY >= 25 && interferenceY < gfx->height() - 25) {
      gfx->drawLine(x, interferenceY - 2, x, interferenceY + 2, gfx->color565(200, 200, 200));
    }
  }
  
  // Ghost image
  if (sin(tvTime * 2) > 0.5) {
    int ghostOffset = 10 + sin(tvTime * 5) * 5;
    for (int i = 0; i < 50; i++) {
      int x = 40 + i * 4;
      int y = gfx->height() / 2 + sin(tvTime + i * 0.2) * 20;
      
      gfx->fillRect(x, y, 3, 8, gfx->color565(100, 150, 100));
      gfx->fillRect(x + ghostOffset, y, 2, 6, gfx->color565(50, 75, 50)); // Ghost
    }
  }
  
  // Channel number overlay
  gfx->setCursor(30, 30);
  gfx->setTextColor(gfx->color565(200, 200, 200));
  gfx->setTextSize(1);
  gfx->printf("CH %d", 3 + (int)(tvTime * 0.5) % 10);
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(255, 255, 255));
  gfx->printf("XANALOGTV");
}

void drawXflame() {
  static float flameTime = 0;
  flameTime += 0.06;
  
  gfx->fillScreen(BLACK);
  
  // X11 flame effect
  
  // Multiple flame sources
  for (int flame = 0; flame < 5; flame++) {
    int flameX = 30 + flame * 50;
    int baseY = gfx->height() - 20;
    
    // Flame particles
    for (int particle = 0; particle < 20; particle++) {
      float particleTime = flameTime + flame * 0.5 + particle * 0.1;
      
      float particleX = flameX + sin(particleTime * 3 + particle) * (particle * 2);
      float particleY = baseY - particle * 8 - sin(particleTime * 4) * 5;
      
      if (particleY > 0) {
        // Flame color based on height
        int red = 255;
        int green = 255 - particle * 8;
        int blue = max(0, 100 - particle * 15);
        
        // Add flicker
        red += sin(particleTime * 10 + particle) * 30;
        green += cos(particleTime * 8 + particle) * 40;
        
        red = max(0, min(255, red));
        green = max(0, min(255, green));
        
        uint16_t flameColor = gfx->color565(red, green, blue);
        gfx->fillCircle(particleX, particleY, 3 + sin(particleTime * 6) * 2, flameColor);
      }
    }
    
    // Flame base
    gfx->fillRect(flameX - 5, baseY, 10, 5, gfx->color565(100, 50, 0));
  }
  
  // Heat shimmer effect
  for (int x = 0; x < gfx->width(); x += 3) {
    for (int y = gfx->height() / 2; y < gfx->height(); y += 3) {
      float shimmer = sin(flameTime * 5 + x * 0.1 + y * 0.05) * 0.3;
      if (shimmer > 0.1) {
        int shimmery = y + shimmer * 10;
        if (shimmery >= 0 && shimmery < gfx->height()) {
          gfx->drawPixel(x, shimmery, gfx->color565(255, 200, 100));
        }
      }
    }
  }
  
  // Sparks
  for (int spark = 0; spark < 10; spark++) {
    float sparkTime = flameTime * 4 + spark * 0.6;
    int sparkX = 50 + spark * 25 + sin(sparkTime) * 20;
    int sparkY = gfx->height() - 50 - (int)(sparkTime * 40) % 80;
    
    if (sparkY > 0 && sin(sparkTime * 8) > 0.7) {
      gfx->fillCircle(sparkX, sparkY, 2, gfx->color565(255, 255, 100));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(255, 200, 100));
  gfx->printf("XFLAME");
}

void drawXjack() {
  static float jackTime = 0;
  jackTime += 0.05;
  
  gfx->fillScreen(BLACK);
  
  // "All work and no play makes Jack a dull boy" typing effect
  const char* jackText[] = {
    "All work and no play makes Jack a dull boy.",
    "All work and no play makes Jack a dull boy.",
    "All WORK and no PLAY makes Jack a dull boy.",
    "ALL WORK AND NO PLAY MAKES JACK A DULL BOY!",
    "All work and no play makes Jack a dull boy.",
    "All work and no play makes Jack a dull boy."
  };
  int numLines = sizeof(jackText) / sizeof(jackText[0]);
  
  // Typing animation
  float typingSpeed = jackTime * 10;
  int currentLine = (int)(typingSpeed / 50) % numLines;
  int currentChar = ((int)typingSpeed % 50);
  
  gfx->setTextSize(1);
  
  // Draw previous lines
  for (int line = 0; line < currentLine && line < 12; line++) {
    gfx->setCursor(5, 10 + line * 12);
    
    // Glitch effect on random lines
    if (sin(jackTime * 3 + line) > 0.8) {
      gfx->setTextColor(gfx->color565(255, 100, 100)); // Red glitch
    } else {
      gfx->setTextColor(gfx->color565(200, 200, 200)); // Normal
    }
    
    int lineIndex = line % numLines;
    gfx->printf("%s", jackText[lineIndex]);
  }
  
  // Draw current line being typed
  if (currentLine < 12) {
    gfx->setCursor(5, 10 + currentLine * 12);
    gfx->setTextColor(gfx->color565(255, 255, 100)); // Bright typing
    
    char buffer[50];
    int lineLength = strlen(jackText[currentLine % numLines]);
    int charsToShow = min(currentChar, lineLength);
    
    strncpy(buffer, jackText[currentLine % numLines], charsToShow);
    buffer[charsToShow] = '\0';
    gfx->printf("%s", buffer);
    
    // Cursor blink
    if (sin(jackTime * 8) > 0) {
      gfx->printf("_");
    }
  }
  
  // Typewriter key strike effects
  if (currentChar > 0 && sin(jackTime * 20) > 0.7) {
    for (int strike = 0; strike < 5; strike++) {
      int strikeX = 50 + strike * 40 + sin(jackTime * 15 + strike) * 10;
      int strikeY = gfx->height() - 40 + cos(jackTime * 12 + strike) * 5;
      
      gfx->fillRect(strikeX, strikeY, 8, 3, gfx->color565(150, 150, 150));
    }
  }
  
  // Psychological tension effects
  if (sin(jackTime * 0.5) > 0.9) {
    // Screen flicker
    gfx->fillScreen(gfx->color565(255, 0, 0));
    delay(10);
  }
  
  gfx->setCursor(10, gfx->height() - 15);
  gfx->setTextColor(gfx->color565(255, 255, 255));
  gfx->printf("XJACK - The Shining");
}

void drawXlyap() {
  static float lyapTime = 0;
  lyapTime += 0.02;
  
  gfx->fillScreen(BLACK);
  
  // Lyapunov fractal visualization
  int width = gfx->width();
  int height = gfx->height();
  
  // Lyapunov parameters that change over time
  float paramA = 3.0 + sin(lyapTime * 0.3) * 0.8;
  float paramB = 3.0 + cos(lyapTime * 0.4) * 0.8;
  
  // Sample points across screen
  for (int px = 0; px < width; px += 3) {
    for (int py = 0; py < height; py += 3) {
      // Map pixel to parameter space
      float a = paramA + (px / (float)width) * 1.0;
      float b = paramB + (py / (float)height) * 1.0;
      
      // Lyapunov exponent calculation (simplified)
      float x = 0.5;
      float lyapunov = 0.0;
      
      // Iterate sequence
      for (int i = 0; i < 50; i++) {
        float r = (i % 2 == 0) ? a : b; // Alternate between A and B
        
        if (x > 0 && x < 1) {
          x = r * x * (1 - x); // Logistic map
          lyapunov += log(abs(r - 2 * r * x));
        }
      }
      
      lyapunov /= 50.0;
      
      // Color based on Lyapunov exponent
      uint16_t color;
      if (lyapunov > 0) {
        // Chaotic (positive exponent) - red/yellow
        int intensity = min(255, (int)(lyapunov * 500 + 100));
        color = gfx->color565(intensity, intensity/2, 0);
      } else {
        // Stable (negative exponent) - blue/green
        int intensity = min(255, (int)(-lyapunov * 300 + 100));
        color = gfx->color565(0, intensity/2, intensity);
      }
      
      gfx->fillRect(px, py, 3, 3, color);
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("XLYAP - A:%.1f B:%.1f", paramA, paramB);
}

void drawXmatrix() {
  static float matrixTime = 0;
  matrixTime += 0.08;
  
  gfx->fillScreen(BLACK);
  
  // Classic Matrix digital rain effect
  static int columnState[40]; // State for each column
  static float columnSpeed[40]; // Speed for each column
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 40; i++) {
      columnState[i] = random(gfx->height());
      columnSpeed[i] = 1 + (random(100) / 100.0);
    }
    initialized = true;
  }
  
  int charWidth = 8;
  int charHeight = 12;
  int columns = gfx->width() / charWidth;
  
  // Matrix characters (simplified to ASCII)
  const char matrixChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()";
  int numChars = strlen(matrixChars);
  
  for (int col = 0; col < min(columns, 40); col++) {
    // Update column position
    columnState[col] += columnSpeed[col];
    if (columnState[col] > gfx->height() + 20) {
      columnState[col] = -20;
      columnSpeed[col] = 1 + (random(200) / 100.0);
    }
    
    // Draw falling characters
    for (int row = 0; row < 15; row++) {
      int charY = columnState[col] - row * charHeight;
      
      if (charY >= 0 && charY < gfx->height()) {
        int charX = col * charWidth;
        
        // Character selection (changes over time)
        char displayChar = matrixChars[(int)(matrixTime * 10 + col + row) % numChars];
        
        // Color intensity based on position in stream
        int intensity;
        if (row == 0) {
          intensity = 255; // Bright white head
        } else if (row < 4) {
          intensity = 255 - row * 40; // Bright green fade
        } else {
          intensity = 150 - row * 10; // Dark green fade
        }
        
        if (intensity > 0) {
          uint16_t charColor;
          if (row == 0) {
            charColor = gfx->color565(255, 255, 255); // White head
          } else {
            charColor = gfx->color565(0, intensity, 0); // Green trail
          }
          
          gfx->setCursor(charX, charY);
          gfx->setTextColor(charColor);
          gfx->setTextSize(1);
          gfx->printf("%c", displayChar);
        }
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(0, 255, 0));
  gfx->printf("XMATRIX - DIGITAL RAIN");
}

void drawXrayswarm() {
  static float swarmTime = 0;
  swarmTime += 0.04;
  
  gfx->fillScreen(BLACK);
  
  // X-ray style particle swarm
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple swarm entities
  for (int swarm = 0; swarm < 8; swarm++) {
    float swarmPhase = swarmTime + swarm * 0.8;
    
    // Swarm center motion
    float swarmCenterX = centerX + sin(swarmPhase * 0.6) * 60;
    float swarmCenterY = centerY + cos(swarmPhase * 0.4) * 40;
    
    // Individual particles in swarm
    for (int particle = 0; particle < 20; particle++) {
      float particlePhase = swarmPhase + particle * 0.3;
      
      // Particle position relative to swarm center
      float radius = 20 + sin(particlePhase * 2) * 15;
      float angle = particlePhase * 3 + particle * 0.5;
      
      float particleX = swarmCenterX + cos(angle) * radius;
      float particleY = swarmCenterY + sin(angle) * radius;
      
      if (particleX >= 0 && particleX < gfx->width() && 
          particleY >= 0 && particleY < gfx->height()) {
        
        // X-ray effect - particles glow through each other
        uint16_t particleColor = gfx->color565(
          200 + sin(particlePhase + swarm) * 55,
          150 + cos(particlePhase + particle) * 80,
          255 - swarm * 20 - particle * 5
        );
        
        int particleSize = 2 + sin(particlePhase * 4) * 2;
        gfx->fillCircle(particleX, particleY, particleSize, particleColor);
        
        // X-ray trails
        for (int trail = 1; trail < 5; trail++) {
          float trailX = particleX - cos(angle) * trail * 3;
          float trailY = particleY - sin(angle) * trail * 3;
          
          int trailIntensity = 255 - trail * 50;
          if (trailIntensity > 0 && trailX >= 0 && trailX < gfx->width() && 
              trailY >= 0 && trailY < gfx->height()) {
            
            uint16_t trailColor = gfx->color565(
              trailIntensity * (200 + sin(particlePhase + swarm) * 55) / 255,
              trailIntensity * (150 + cos(particlePhase + particle) * 80) / 255,
              trailIntensity * (255 - swarm * 20 - particle * 5) / 255
            );
            
            gfx->drawPixel(trailX, trailY, trailColor);
          }
        }
      }
    }
  }
  
  // X-ray scanner lines
  float scanY = fmod(swarmTime * 50, gfx->height());
  gfx->drawLine(0, scanY, gfx->width(), scanY, gfx->color565(100, 255, 100));
  gfx->drawLine(0, scanY + 1, gfx->width(), scanY + 1, gfx->color565(50, 200, 50));
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(200, 255, 200));
  gfx->printf("XRAYSWARM");
}

void drawXspirograph() {
  static float spiroTime = 0;
  spiroTime += 0.03;
  
  gfx->fillScreen(BLACK);
  
  // Spirograph mathematical art
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple spirograph patterns
  for (int pattern = 0; pattern < 3; pattern++) {
    float patternTime = spiroTime + pattern * 2.0;
    
    // Spirograph parameters that change over time
    float R = 40 + pattern * 15; // Fixed circle radius
    float r = 10 + sin(patternTime * 0.2) * 5; // Rolling circle radius  
    float d = 15 + cos(patternTime * 0.3) * 8; // Distance from rolling circle center
    
    // Draw spirograph curve
    for (int step = 0; step < 200; step++) {
      float t = step * 0.1 + patternTime;
      
      // Spirograph parametric equations
      float x = (R - r) * cos(t) + d * cos((R - r) * t / r);
      float y = (R - r) * sin(t) - d * sin((R - r) * t / r);
      
      int plotX = centerX + x;
      int plotY = centerY + y;
      
      if (plotX >= 0 && plotX < gfx->width() && plotY >= 0 && plotY < gfx->height()) {
        // Color based on pattern and position
        uint16_t spiroColor = gfx->color565(
          150 + pattern * 40 + sin(t + patternTime) * 60,
          100 + cos(t + patternTime * 1.2) * 80,
          200 - pattern * 30 + sin(t * 2 + patternTime) * 50
        );
        
        gfx->fillCircle(plotX, plotY, 2, spiroColor);
        
        // Connect to previous point
        if (step > 0) {
          float prevT = (step - 1) * 0.1 + patternTime;
          float prevX = (R - r) * cos(prevT) + d * cos((R - r) * prevT / r);
          float prevY = (R - r) * sin(prevT) - d * sin((R - r) * prevT / r);
          
          int prevPlotX = centerX + prevX;
          int prevPlotY = centerY + prevY;
          
          gfx->drawLine(prevPlotX, prevPlotY, plotX, plotY, spiroColor);
        }
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(255, 200, 150));
  gfx->printf("XSPIROGRAPH");
}

void drawXsublim() {
  static float sublimTime = 0;
  sublimTime += 0.1;
  
  gfx->fillScreen(BLACK);
  
  // Subliminal message effect
  const char* messages[] = {
    "OBEY", "CONSUME", "SLEEP", "WORK", "BUY", "SUBMIT", 
    "CONFORM", "WATCH TV", "NO THINKING", "STAY ASLEEP"
  };
  int numMessages = sizeof(messages) / sizeof(messages[0]);
  
  // Main display - normal content
  gfx->setTextSize(1);
  gfx->setTextColor(gfx->color565(200, 200, 200));
  
  gfx->setCursor(10, 30);
  gfx->printf("Regular TV Programming");
  gfx->setCursor(10, 50);
  gfx->printf("News - Weather - Sports");
  gfx->setCursor(10, 70);
  gfx->printf("Family Entertainment");
  
  // Fake TV content
  for (int i = 0; i < 10; i++) {
    int boxY = 100 + i * 15;
    int boxWidth = 50 + sin(sublimTime + i) * 30;
    gfx->fillRect(20, boxY, boxWidth, 8, gfx->color565(100, 150, 200));
  }
  
  // Subliminal flashes (very brief)
  float flashPhase = fmod(sublimTime * 10, 30);
  if (flashPhase < 0.2) { // Very brief flash
    int messageIndex = (int)(sublimTime) % numMessages;
    
    // Full screen subliminal message
    gfx->fillScreen(gfx->color565(255, 0, 0));
    gfx->setTextSize(2);
    gfx->setTextColor(WHITE);
    
    // Center the message
    int textWidth = strlen(messages[messageIndex]) * 12;
    int textX = (gfx->width() - textWidth) / 2;
    int textY = (gfx->height() - 16) / 2;
    
    gfx->setCursor(textX, textY);
    gfx->printf("%s", messages[messageIndex]);
    
    delay(20); // Brief flash duration
  }
  
  // Barely visible subliminal text (mixed in with normal content)
  if (sin(sublimTime * 5) > 0.95) {
    int hiddenMessageIndex = ((int)(sublimTime * 3)) % numMessages;
    gfx->setTextSize(1);
    gfx->setTextColor(gfx->color565(30, 30, 30)); // Very dark, barely visible
    
    gfx->setCursor(150, 80 + sin(sublimTime * 7) * 20);
    gfx->printf("%s", messages[hiddenMessageIndex]);
  }
  
  // Static interference to mask subliminals
  for (int static_dot = 0; static_dot < 50; static_dot++) {
    int staticX = random(gfx->width());
    int staticY = random(gfx->height());
    if (sin(sublimTime * 20 + static_dot) > 0.7) {
      gfx->drawPixel(staticX, staticY, gfx->color565(200, 200, 200));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("XSUBLIM - They Live");
}

void drawZoom() {
  static float zoomTime = 0;
  zoomTime += 0.05;
  
  gfx->fillScreen(BLACK);
  
  // Zoom/tunnel effect
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple zoom layers
  for (int layer = 0; layer < 8; layer++) {
    float layerTime = zoomTime + layer * 0.5;
    
    // Zoom scale (starts small, grows large)
    float scale = fmod(layerTime * 2, 6) + 0.1;
    
    // Pattern type rotates through different shapes
    int patternType = (layer + (int)(zoomTime * 0.5)) % 4;
    
    switch (patternType) {
      case 0: { // Concentric circles
        int radius = 10 * scale;
        if (radius > 3 && radius < 150) {
          uint16_t circleColor = gfx->color565(
            255 - layer * 20,
            100 + layer * 15 + sin(layerTime) * 80,
            layer * 30 + cos(layerTime * 1.5) * 50
          );
          gfx->drawCircle(centerX, centerY, radius, circleColor);
        }
        break;
      }
      
      case 1: { // Square spirals
        int size = 15 * scale;
        if (size > 5 && size < 120) {
          uint16_t squareColor = gfx->color565(
            layer * 30 + sin(layerTime) * 60,
            255 - layer * 25,
            100 + cos(layerTime * 1.2) * 70
          );
          gfx->drawRect(centerX - size/2, centerY - size/2, size, size, squareColor);
        }
        break;
      }
      
      case 2: { // Star patterns
        int starRadius = 8 * scale;
        if (starRadius > 2 && starRadius < 100) {
          uint16_t starColor = gfx->color565(
            200 + sin(layerTime + layer) * 55,
            200 + cos(layerTime + layer) * 55,
            100 + layer * 20
          );
          
          // Draw simple star
          for (int point = 0; point < 5; point++) {
            float angle = point * 2 * PI / 5 + layerTime;
            int x = centerX + cos(angle) * starRadius;
            int y = centerY + sin(angle) * starRadius;
            gfx->drawLine(centerX, centerY, x, y, starColor);
          }
        }
        break;
      }
      
      case 3: { // Cross patterns
        int crossSize = 12 * scale;
        if (crossSize > 4 && crossSize < 80) {
          uint16_t crossColor = gfx->color565(
            100 + layer * 25,
            layer * 35 + sin(layerTime * 2) * 40,
            255 - layer * 30
          );
          
          gfx->drawLine(centerX - crossSize, centerY, centerX + crossSize, centerY, crossColor);
          gfx->drawLine(centerX, centerY - crossSize, centerX, centerY + crossSize, crossColor);
        }
        break;
      }
    }
  }
  
  // Central vortex
  for (int vortex = 0; vortex < 12; vortex++) {
    float vortexAngle = zoomTime * 4 + vortex * PI/6;
    float vortexRadius = 3 + vortex + sin(zoomTime * 3 + vortex) * 2;
    
    int vortexX = centerX + cos(vortexAngle) * vortexRadius;
    int vortexY = centerY + sin(vortexAngle) * vortexRadius;
    
    gfx->fillCircle(vortexX, vortexY, 2, gfx->color565(255, 255, 200));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("ZOOM - INFINITE TUNNEL");
}

void drawAbstractile() {
  static float abstractTime = 0;
  abstractTime += 0.04;
  
  gfx->fillScreen(BLACK);
  
  // Abstract tile patterns
  int tileSize = 20;
  int tilesX = gfx->width() / tileSize + 1;
  int tilesY = gfx->height() / tileSize + 1;
  
  for (int tx = 0; tx < tilesX; tx++) {
    for (int ty = 0; ty < tilesY; ty++) {
      float tileTime = abstractTime + tx * 0.1 + ty * 0.15;
      
      int x = tx * tileSize;
      int y = ty * tileSize;
      
      // Abstract pattern based on position and time
      float pattern = sin(tileTime + tx * 0.3) * cos(tileTime + ty * 0.4);
      
      // Tile type selection
      int tileType = (int)(pattern * 4 + tileTime * 2) % 6;
      
      uint16_t tileColor = gfx->color565(
        128 + 127 * sin(tileTime + tx * 0.2),
        128 + 127 * cos(tileTime + ty * 0.3),
        128 + 127 * sin(tileTime + (tx + ty) * 0.1)
      );
      
      switch (tileType) {
        case 0: // Solid fill
          gfx->fillRect(x, y, tileSize - 1, tileSize - 1, tileColor);
          break;
          
        case 1: // Diagonal lines
          for (int line = 0; line < tileSize; line += 3) {
            gfx->drawLine(x + line, y, x, y + line, tileColor);
            gfx->drawLine(x + tileSize, y + line, x + line, y + tileSize, tileColor);
          }
          break;
          
        case 2: // Concentric squares
          for (int square = 2; square < tileSize / 2; square += 3) {
            gfx->drawRect(x + square, y + square, 
                         tileSize - 2 * square, tileSize - 2 * square, tileColor);
          }
          break;
          
        case 3: // Cross pattern
          gfx->drawLine(x + tileSize/2, y, x + tileSize/2, y + tileSize, tileColor);
          gfx->drawLine(x, y + tileSize/2, x + tileSize, y + tileSize/2, tileColor);
          break;
          
        case 4: // Dots
          for (int dot = 0; dot < 4; dot++) {
            int dotX = x + 4 + (dot % 2) * 10;
            int dotY = y + 4 + (dot / 2) * 10;
            gfx->fillCircle(dotX, dotY, 2, tileColor);
          }
          break;
          
        case 5: // Triangle
          gfx->fillTriangle(
            x + tileSize/2, y + 2,
            x + 2, y + tileSize - 2,
            x + tileSize - 2, y + tileSize - 2,
            tileColor
          );
          break;
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("ABSTRACTILE");
}

void drawAnemone() {
  static float anemoneTime = 0;
  anemoneTime += 0.03;
  
  gfx->fillScreen(gfx->color565(0, 10, 40)); // Deep ocean blue
  
  // Sea anemone tentacles
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2 + 20;
  
  // Multiple anemones
  for (int anemone = 0; anemone < 3; anemone++) {
    float anemonePhase = anemoneTime + anemone * 2.0;
    int anemoneX = centerX + (anemone - 1) * 80;
    int anemoneY = centerY + anemone * 10;
    
    // Anemone base
    gfx->fillCircle(anemoneX, anemoneY, 15, gfx->color565(100, 60, 40));
    
    // Tentacles
    int numTentacles = 12 + anemone * 2;
    for (int tentacle = 0; tentacle < numTentacles; tentacle++) {
      float tentacleAngle = (tentacle * 2 * PI / numTentacles) + anemonePhase * 0.5;
      
      // Tentacle segments
      float currentX = anemoneX;
      float currentY = anemoneY;
      
      for (int segment = 0; segment < 8; segment++) {
        float segmentPhase = anemonePhase + tentacle * 0.2 + segment * 0.3;
        
        // Tentacle movement (swaying in current)
        float sway = sin(segmentPhase) * (segment * 2);
        float segmentAngle = tentacleAngle + sway * 0.1;
        
        float segmentLength = 8 + sin(segmentPhase) * 3;
        float newX = currentX + cos(segmentAngle) * segmentLength;
        float newY = currentY + sin(segmentAngle) * segmentLength;
        
        // Tentacle color (bright at tip)
        uint16_t tentacleColor = gfx->color565(
          80 + segment * 20 + anemone * 30,
          40 + segment * 15 + sin(segmentPhase) * 40,
          100 + segment * 10
        );
        
        // Draw tentacle segment
        int thickness = max(1, 4 - segment / 2);
        for (int thick = 0; thick < thickness; thick++) {
          gfx->drawLine(
            currentX + thick - thickness/2, currentY,
            newX + thick - thickness/2, newY,
            tentacleColor
          );
        }
        
        currentX = newX;
        currentY = newY;
      }
    }
  }
  
  // Ocean particles/plankton
  for (int particle = 0; particle < 20; particle++) {
    float particleTime = anemoneTime * 2 + particle * 0.3;
    int particleX = (int)(particleTime * 15) % (gfx->width() + 20) - 10;
    int particleY = 20 + particle * 8 + sin(particleTime * 2) * 10;
    
    if (sin(particleTime * 5) > 0.5) {
      gfx->fillCircle(particleX, particleY, 1, gfx->color565(100, 150, 255));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(150, 200, 255));
  gfx->printf("ANEMONE");
}

void drawAnemotaxis() {
  static float taxisTime = 0;
  taxisTime += 0.05;
  
  gfx->fillScreen(gfx->color565(5, 20, 40));
  
  // Anemotaxis - movement toward/away from stimuli
  int numOrganisms = 8;
  
  // Chemical gradient source
  int sourceX = gfx->width() / 2 + sin(taxisTime * 0.3) * 50;
  int sourceY = gfx->height() / 2 + cos(taxisTime * 0.4) * 30;
  
  // Draw gradient visualization
  for (int radius = 10; radius < 120; radius += 20) {
    int intensity = max(0, 100 - radius);
    gfx->drawCircle(sourceX, sourceY, radius, gfx->color565(intensity, intensity/2, 0));
  }
  
  // Organisms responding to gradient
  static float organismX[8];
  static float organismY[8];
  static float organismAngle[8];
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < numOrganisms; i++) {
      organismX[i] = random(gfx->width());
      organismY[i] = random(gfx->height());
      organismAngle[i] = random(360) * PI / 180;
    }
    initialized = true;
  }
  
  for (int org = 0; org < numOrganisms; org++) {
    // Calculate distance to source
    float dx = sourceX - organismX[org];
    float dy = sourceY - organismY[org];
    float distance = sqrt(dx * dx + dy * dy);
    
    // Movement behavior based on organism type
    bool isAttracted = (org % 2 == 0); // Half attracted, half repelled
    
    if (distance > 5) {
      float targetAngle = atan2(dy, dx);
      if (!isAttracted) {
        targetAngle += PI; // Move away from source
      }
      
      // Gradual angle adjustment
      float angleDiff = targetAngle - organismAngle[org];
      while (angleDiff > PI) angleDiff -= 2 * PI;
      while (angleDiff < -PI) angleDiff += 2 * PI;
      
      organismAngle[org] += angleDiff * 0.1;
    }
    
    // Movement
    float speed = 2 + sin(taxisTime + org) * 1;
    organismX[org] += cos(organismAngle[org]) * speed;
    organismY[org] += sin(organismAngle[org]) * speed;
    
    // Boundary wrapping
    if (organismX[org] < 0) organismX[org] = gfx->width();
    if (organismX[org] > gfx->width()) organismX[org] = 0;
    if (organismY[org] < 0) organismY[org] = gfx->height();
    if (organismY[org] > gfx->height()) organismY[org] = 0;
    
    // Draw organism
    uint16_t orgColor;
    if (isAttracted) {
      orgColor = gfx->color565(100, 255, 100); // Green for attracted
    } else {
      orgColor = gfx->color565(255, 100, 100); // Red for repelled
    }
    
    gfx->fillCircle(organismX[org], organismY[org], 3, orgColor);
    
    // Direction indicator
    int headX = organismX[org] + cos(organismAngle[org]) * 6;
    int headY = organismY[org] + sin(organismAngle[org]) * 6;
    gfx->drawLine(organismX[org], organismY[org], headX, headY, WHITE);
    
    // Trail
    for (int trail = 1; trail < 5; trail++) {
      int trailX = organismX[org] - cos(organismAngle[org]) * trail * 3;
      int trailY = organismY[org] - sin(organismAngle[org]) * trail * 3;
      
      if (trailX >= 0 && trailX < gfx->width() && trailY >= 0 && trailY < gfx->height()) {
        int trailBrightness = 255 - trail * 50;
        uint16_t trailColor;
        if (isAttracted) {
          trailColor = gfx->color565(0, trailBrightness/2, 0);
        } else {
          trailColor = gfx->color565(trailBrightness/2, 0, 0);
        }
        gfx->drawPixel(trailX, trailY, trailColor);
      }
    }
  }
  
  // Source pulsing
  gfx->fillCircle(sourceX, sourceY, 5 + sin(taxisTime * 4) * 3, gfx->color565(255, 200, 0));
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("ANEMOTAXIS");
}

void drawAnt() {
  static float antTime = 0;
  antTime += 0.06;
  
  gfx->fillScreen(gfx->color565(20, 15, 10)); // Sandy background
  
  // Langton's Ant cellular automaton (memory optimized)
  static int antX = 160, antY = 120;
  static int antDir = 0; // 0=North, 1=East, 2=South, 3=West
  static bool initialized = false;
  
  if (!initialized) {
    antX = gfx->width() / 2;
    antY = gfx->height() / 2;
    initialized = true;
  }
  
  // Simplified ant simulation (memory efficient)
  for (int step = 0; step < 3; step++) {
    // Move in current direction
    switch (antDir) {
      case 0: antY--; break; // North
      case 1: antX++; break; // East
      case 2: antY++; break; // South
      case 3: antX--; break; // West
    }
    
    // Boundary wrapping
    if (antX < 0) antX = gfx->width() - 1;
    if (antX >= gfx->width()) antX = 0;
    if (antY < 0) antY = gfx->height() - 1;
    if (antY >= gfx->height()) antY = 0;
    
    // Random turning based on position and time
    if (sin(antTime + antX * 0.1 + antY * 0.1) > 0.3) {
      antDir = (antDir + 1) % 4; // Turn right
    } else if (cos(antTime + antX * 0.1 + antY * 0.1) > 0.7) {
      antDir = (antDir + 3) % 4; // Turn left
    }
  }
  
  // Draw trail pattern based on mathematical function
  for (int x = 0; x < gfx->width(); x += 3) {
    for (int y = 0; y < gfx->height(); y += 3) {
      float pattern = sin((x + antTime * 20) * 0.1) * cos((y + antTime * 15) * 0.1);
      if (pattern > 0.5) {
        uint16_t cellColor = gfx->color565(
          150 + sin(antTime + x * 0.05) * 50,
          100 + cos(antTime + y * 0.05) * 40,
          50 + sin(antTime + x + y * 0.02) * 30
        );
        gfx->fillRect(x, y, 2, 2, cellColor);
      }
    }
  }
  
  // Draw ant
  gfx->fillCircle(antX, antY, 3, gfx->color565(255, 0, 0));
  
  // Ant direction indicator
  int dirX = 0, dirY = 0;
  switch (antDir) {
    case 0: dirY = -6; break;
    case 1: dirX = 6; break;
    case 2: dirY = 6; break;
    case 3: dirX = -6; break;
  }
  gfx->drawLine(antX, antY, antX + dirX, antY + dirY, WHITE);
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("ANT - Langton");
}

void drawApollonian() {
  static float apolloTime = 0;
  apolloTime += 0.02;
  
  gfx->fillScreen(BLACK);
  
  // Apollonian gasket - circle packing
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Base circles that change over time
  float baseRadius = 60 + sin(apolloTime * 0.5) * 20;
  
  // Main containing circle
  gfx->drawCircle(centerX, centerY, baseRadius, WHITE);
  
  // Three mutually tangent circles inside
  float innerRadius = baseRadius / 3;
  float angle1 = apolloTime * 0.3;
  float angle2 = angle1 + 2 * PI / 3;
  float angle3 = angle2 + 2 * PI / 3;
  
  int circle1X = centerX + cos(angle1) * (baseRadius - innerRadius) * 0.5;
  int circle1Y = centerY + sin(angle1) * (baseRadius - innerRadius) * 0.5;
  
  int circle2X = centerX + cos(angle2) * (baseRadius - innerRadius) * 0.5;
  int circle2Y = centerY + sin(angle2) * (baseRadius - innerRadius) * 0.5;
  
  int circle3X = centerX + cos(angle3) * (baseRadius - innerRadius) * 0.5;
  int circle3Y = centerY + sin(angle3) * (baseRadius - innerRadius) * 0.5;
  
  uint16_t color1 = gfx->color565(255, 100, 100);
  uint16_t color2 = gfx->color565(100, 255, 100);
  uint16_t color3 = gfx->color565(100, 100, 255);
  
  gfx->drawCircle(circle1X, circle1Y, innerRadius, color1);
  gfx->drawCircle(circle2X, circle2Y, innerRadius, color2);
  gfx->drawCircle(circle3X, circle3Y, innerRadius, color3);
  
  // Smaller circles in the gaps (simplified Apollonian construction)
  for (int layer = 0; layer < 3; layer++) {
    float layerTime = apolloTime + layer * 0.7;
    int numSmallCircles = 6 + layer * 2;
    
    for (int i = 0; i < numSmallCircles; i++) {
      float smallAngle = layerTime + i * 2 * PI / numSmallCircles;
      float smallRadius = (baseRadius * 0.8) / (2 + layer);
      
      int smallX = centerX + cos(smallAngle) * smallRadius;
      int smallY = centerY + sin(smallAngle) * smallRadius;
      int smallSize = max(2, 15 - layer * 4);
      
      uint16_t smallColor = gfx->color565(
        128 + 127 * sin(layerTime + i * 0.5),
        128 + 127 * cos(layerTime + i * 0.7),
        128 + 127 * sin(layerTime + i * 0.3 + layer)
      );
      
      gfx->fillCircle(smallX, smallY, smallSize, smallColor);
    }
  }
  
  // Fractal detail circles
  for (int detail = 0; detail < 20; detail++) {
    float detailTime = apolloTime * 2 + detail * 0.4;
    float detailAngle = detailTime;
    float detailDist = 20 + sin(detailTime) * 15;
    
    int detailX = centerX + cos(detailAngle) * detailDist;
    int detailY = centerY + sin(detailAngle) * detailDist;
    
    if (detailX >= 0 && detailX < gfx->width() && detailY >= 0 && detailY < gfx->height()) {
      gfx->fillCircle(detailX, detailY, 1 + sin(detailTime * 3), 
        gfx->color565(255, 255, 200));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("APOLLONIAN");
}

void drawApple2() {
  static float appleTime = 0;
  appleTime += 0.1;
  
  // Classic Apple II green screen
  gfx->fillScreen(BLACK);
  
  // Apple II boot sequence simulation
  int lineHeight = 12;
  int currentLine = (int)(appleTime * 2) % 20;
  
  gfx->setTextSize(1);
  gfx->setTextColor(gfx->color565(0, 255, 0)); // Apple II green
  
  // Boot messages
  const char* bootMessages[] = {
    "APPLE ][",
    "",
    "DOS VERSION 3.3  SYSTEM MASTER",
    "COPYRIGHT APPLE, 1980",
    "",
    "]CATALOG",
    "",
    " A 002 HELLO",
    " B 034 APPLESOFT BASIC", 
    " A 003 STARTUP",
    " B 042 COPY",
    " A 005 FID",
    "",
    "]RUN HELLO",
    "",
    "LOADING...",
    "",
    "*** XSCREENSAVER APPLE II ***",
    "",
    "READY"
  };
  
  int numMessages = sizeof(bootMessages) / sizeof(bootMessages[0]);
  
  for (int line = 0; line <= currentLine && line < numMessages; line++) {
    gfx->setCursor(10, 20 + line * lineHeight);
    gfx->printf("%s", bootMessages[line]);
  }
  
  // Cursor blink on current line
  if (currentLine < numMessages && sin(appleTime * 4) > 0) {
    gfx->setCursor(10 + strlen(bootMessages[currentLine]) * 6, 20 + currentLine * lineHeight);
    gfx->printf("_");
  }
  
  // Apple II style graphics demo
  if (currentLine >= numMessages - 2) {
    // Simple graphics
    for (int i = 0; i < 10; i++) {
      float graphicsTime = appleTime + i * 0.5;
      int barY = 180 + i * 3;
      int barWidth = 50 + sin(graphicsTime) * 30;
      
      // Apple II color palette (simplified)
      uint16_t appleColors[] = {
        gfx->color565(0, 0, 0),     // Black
        gfx->color565(255, 0, 255), // Magenta  
        gfx->color565(0, 0, 255),   // Blue
        gfx->color565(255, 255, 255), // White
        gfx->color565(0, 255, 0),   // Green
        gfx->color565(255, 165, 0), // Orange
      };
      
      uint16_t barColor = appleColors[i % 6];
      gfx->fillRect(200, barY, barWidth, 2, barColor);
    }
  }
  
  // Scanlines for CRT effect
  for (int y = 0; y < gfx->height(); y += 2) {
    if (sin(appleTime * 10 + y * 0.1) > 0.8) {
      gfx->drawLine(0, y, gfx->width(), y, gfx->color565(0, 50, 0));
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(0, 255, 0));
  gfx->printf("APPLE ][ - 1977");
}

void drawAtlantis() {
  static float atlantisTime = 0;
  atlantisTime += 0.03;
  
  gfx->fillScreen(gfx->color565(0, 30, 80)); // Deep ocean
  
  // Underwater Atlantis ruins
  
  // Ancient columns
  for (int col = 0; col < 4; col++) {
    int colX = 50 + col * 60;
    int colHeight = 80 + sin(atlantisTime + col) * 20;
    int colY = gfx->height() - 20 - colHeight;
    
    // Column body
    gfx->fillRect(colX, colY, 15, colHeight, gfx->color565(100, 120, 80));
    
    // Column segments
    for (int seg = 0; seg < colHeight; seg += 12) {
      gfx->drawLine(colX, colY + seg, colX + 15, colY + seg, gfx->color565(80, 100, 60));
    }
    
    // Column capital
    gfx->fillRect(colX - 3, colY, 21, 8, gfx->color565(120, 140, 100));
  }
  
  // Ruins and debris
  for (int ruin = 0; ruin < 8; ruin++) {
    float ruinTime = atlantisTime + ruin * 0.8;
    int ruinX = 30 + ruin * 35 + sin(ruinTime) * 10;
    int ruinY = gfx->height() - 30 + cos(ruinTime * 0.7) * 5;
    
    // Broken stones
    gfx->fillRect(ruinX, ruinY, 12 + ruin % 8, 8 + ruin % 6, 
      gfx->color565(80 + ruin * 10, 90 + ruin * 8, 60 + ruin * 5));
  }
  
  // Swimming fish
  for (int fish = 0; fish < 6; fish++) {
    float fishTime = atlantisTime * 2 + fish * 1.2;
    
    float fishX = gfx->width() * 0.8 * (sin(fishTime * 0.5) * 0.5 + 0.5) + 20;
    float fishY = 30 + fish * 25 + sin(fishTime * 1.5) * 15;
    
    // Fish body
    gfx->fillCircle(fishX, fishY, 6, gfx->color565(255, 200, 100));
    
    // Fish tail
    int tailX = fishX - 10;
    int tailY = fishY + sin(fishTime * 8) * 3;
    gfx->fillTriangle(
      tailX, tailY,
      tailX - 8, tailY - 4,
      tailX - 8, tailY + 4,
      gfx->color565(200, 150, 80)
    );
    
    // Fish eye
    gfx->fillCircle(fishX + 2, fishY - 1, 1, BLACK);
  }
  
  // Ocean bubbles rising
  for (int bubble = 0; bubble < 12; bubble++) {
    float bubbleTime = atlantisTime * 3 + bubble * 0.5;
    int bubbleX = 20 + bubble * 25 + sin(bubbleTime) * 8;
    int bubbleY = gfx->height() - 10 - (int)(bubbleTime * 40) % (gfx->height() - 20);
    
    if (bubbleY > 10) {
      int bubbleSize = 1 + sin(bubbleTime * 2) * 2;
      gfx->drawCircle(bubbleX, bubbleY, bubbleSize, gfx->color565(150, 200, 255));
    }
  }
  
  // Ancient glyphs on walls
  for (int glyph = 0; glyph < 6; glyph++) {
    int glyphX = 280;
    int glyphY = 40 + glyph * 25;
    
    // Simple ancient symbols
    switch (glyph % 4) {
      case 0: // Eye symbol
        gfx->drawCircle(glyphX, glyphY, 4, gfx->color565(150, 150, 100));
        gfx->fillCircle(glyphX, glyphY, 2, gfx->color565(100, 100, 60));
        break;
      case 1: // Trident
        gfx->drawLine(glyphX, glyphY - 6, glyphX, glyphY + 6, gfx->color565(150, 150, 100));
        gfx->drawLine(glyphX - 3, glyphY - 6, glyphX + 3, glyphY - 6, gfx->color565(150, 150, 100));
        break;
      case 2: // Wave
        for (int w = 0; w < 8; w++) {
          int waveY = glyphY + sin(w * 0.8) * 3;
          gfx->drawPixel(glyphX - 4 + w, waveY, gfx->color565(150, 150, 100));
        }
        break;
      case 3: // Sun/star
        for (int ray = 0; ray < 8; ray++) {
          float rayAngle = ray * PI / 4;
          int rayX = glyphX + cos(rayAngle) * 4;
          int rayY = glyphY + sin(rayAngle) * 4;
          gfx->drawLine(glyphX, glyphY, rayX, rayY, gfx->color565(150, 150, 100));
        }
        break;
    }
  }
  
  // Ocean current particles
  for (int current = 0; current < 15; current++) {
    float currentTime = atlantisTime * 4 + current * 0.3;
    int currentX = (int)(currentTime * 25) % (gfx->width() + 20) - 10;
    int currentY = 60 + current * 10 + sin(currentTime * 2) * 8;
    
    gfx->drawPixel(currentX, currentY, gfx->color565(100, 150, 200));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(150, 200, 255));
  gfx->printf("ATLANTIS - Lost City");
}

void drawAttraction() {
  static float attractTime = 0;
  attractTime += 0.04;
  
  gfx->fillScreen(BLACK);
  
  // Particle attraction simulation
  static float particleX[20];
  static float particleY[20];
  static float particleVX[20];
  static float particleVY[20];
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 20; i++) {
      particleX[i] = random(gfx->width());
      particleY[i] = random(gfx->height());
      particleVX[i] = (random(100) - 50) / 25.0;
      particleVY[i] = (random(100) - 50) / 25.0;
    }
    initialized = true;
  }
  
  // Attractor points
  int attractorX = gfx->width() / 2 + sin(attractTime * 0.3) * 60;
  int attractorY = gfx->height() / 2 + cos(attractTime * 0.4) * 40;
  
  int attractor2X = gfx->width() * 0.3 + sin(attractTime * 0.5) * 30;
  int attractor2Y = gfx->height() * 0.7 + cos(attractTime * 0.6) * 25;
  
  // Draw attractors
  gfx->fillCircle(attractorX, attractorY, 8, gfx->color565(255, 200, 0));
  gfx->fillCircle(attractor2X, attractor2Y, 6, gfx->color565(0, 255, 200));
  
  // Update particles
  for (int i = 0; i < 20; i++) {
    // Calculate forces from attractors
    float dx1 = attractorX - particleX[i];
    float dy1 = attractorY - particleY[i];
    float dist1 = sqrt(dx1 * dx1 + dy1 * dy1) + 1;
    
    float dx2 = attractor2X - particleX[i];
    float dy2 = attractor2Y - particleY[i];
    float dist2 = sqrt(dx2 * dx2 + dy2 * dy2) + 1;
    
    // Attraction forces
    float forceStrength1 = 500.0 / (dist1 * dist1);
    float forceStrength2 = 300.0 / (dist2 * dist2);
    
    particleVX[i] += (dx1 / dist1) * forceStrength1 * 0.01;
    particleVY[i] += (dy1 / dist1) * forceStrength1 * 0.01;
    
    particleVX[i] += (dx2 / dist2) * forceStrength2 * 0.01;
    particleVY[i] += (dy2 / dist2) * forceStrength2 * 0.01;
    
    // Damping
    particleVX[i] *= 0.99;
    particleVY[i] *= 0.99;
    
    // Update position
    particleX[i] += particleVX[i];
    particleY[i] += particleVY[i];
    
    // Boundary wrapping
    if (particleX[i] < 0) particleX[i] = gfx->width();
    if (particleX[i] > gfx->width()) particleX[i] = 0;
    if (particleY[i] < 0) particleY[i] = gfx->height();
    if (particleY[i] > gfx->height()) particleY[i] = 0;
    
    // Draw particle
    uint16_t particleColor = gfx->color565(
      150 + sin(attractTime + i * 0.5) * 100,
      100 + cos(attractTime + i * 0.7) * 80,
      200 - i * 8
    );
    
    gfx->fillCircle(particleX[i], particleY[i], 2, particleColor);
    
    // Draw trails
    for (int trail = 1; trail < 4; trail++) {
      int trailX = particleX[i] - particleVX[i] * trail * 3;
      int trailY = particleY[i] - particleVY[i] * trail * 3;
      
      if (trailX >= 0 && trailX < gfx->width() && trailY >= 0 && trailY < gfx->height()) {
        int trailBrightness = 255 - trail * 60;
        uint16_t trailColor = gfx->color565(trailBrightness/3, trailBrightness/4, trailBrightness/2);
        gfx->drawPixel(trailX, trailY, trailColor);
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("ATTRACTION");
}

void drawAtunnel() {
  static float tunnelTime = 0;
  tunnelTime += 0.06;
  
  gfx->fillScreen(BLACK);
  
  // 3D tunnel effect
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple tunnel rings
  for (int ring = 0; ring < 12; ring++) {
    float ringTime = tunnelTime + ring * 0.2;
    
    // Ring moves toward viewer
    float z = fmod(ringTime * 50, 100) + 10;
    float scale = 100.0 / z;
    
    if (scale > 0.1 && scale < 5) {
      int radius = 50 * scale;
      
      // Ring segments
      int segments = 16;
      for (int seg = 0; seg < segments; seg++) {
        float angle = (seg * 2.0 * PI / segments) + ringTime * 2;
        
        int x1 = centerX + cos(angle) * radius;
        int y1 = centerY + sin(angle) * radius;
        
        float nextAngle = ((seg + 1) * 2.0 * PI / segments) + ringTime * 2;
        int x2 = centerX + cos(nextAngle) * radius;
        int y2 = centerY + sin(nextAngle) * radius;
        
        // Color based on distance
        uint16_t tunnelColor = gfx->color565(
          255 * scale + sin(ringTime + seg) * 100,
          150 * scale + cos(ringTime + seg) * 80,
          200 * scale
        );
        
        gfx->drawLine(x1, y1, x2, y2, tunnelColor);
        
        // Connect to next ring
        if (ring > 0) {
          float prevZ = fmod((ringTime - 0.2) * 50, 100) + 10;
          float prevScale = 100.0 / prevZ;
          int prevRadius = 50 * prevScale;
          
          int prevX = centerX + cos(angle) * prevRadius;
          int prevY = centerY + sin(angle) * prevRadius;
          
          gfx->drawLine(x1, y1, prevX, prevY, tunnelColor);
        }
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("ATUNNEL");
}

void drawBarcode() {
  static float barcodeTime = 0;
  barcodeTime += 0.08;
  
  gfx->fillScreen(WHITE);
  
  // Animated barcode patterns
  int barWidth = 4;
  int currentX = 0;
  
  // Generate barcode pattern
  for (int bar = 0; bar < gfx->width() / barWidth; bar++) {
    float barTime = barcodeTime + bar * 0.1;
    
    // Bar height variation
    int barHeight = gfx->height() * 0.6 + sin(barTime * 2) * 40;
    int barY = (gfx->height() - barHeight) / 2;
    
    // Bar color (black/white pattern with digital effects)
    bool isBlack = (sin(barTime * 3) > 0) || ((bar + (int)(barcodeTime * 5)) % 7 < 3);
    
    uint16_t barColor;
    if (isBlack) {
      // Digital glitch effects
      if (sin(barcodeTime * 15 + bar) > 0.9) {
        barColor = gfx->color565(255, 0, 0); // Red glitch
      } else if (cos(barcodeTime * 12 + bar) > 0.95) {
        barColor = gfx->color565(0, 255, 0); // Green glitch
      } else {
        barColor = BLACK; // Normal black
      }
    } else {
      barColor = WHITE; // White bars
    }
    
    gfx->fillRect(currentX, barY, barWidth, barHeight, barColor);
    
    // Add scanline effect
    if (bar % 5 == 0) {
      gfx->drawLine(currentX, barY, currentX, barY + barHeight, gfx->color565(100, 100, 100));
    }
    
    currentX += barWidth;
  }
  
  // Scanning laser line
  int scanX = (int)(barcodeTime * 30) % gfx->width();
  gfx->drawLine(scanX, 0, scanX, gfx->height(), gfx->color565(255, 0, 0));
  gfx->drawLine(scanX + 1, 0, scanX + 1, gfx->height(), gfx->color565(255, 100, 100));
  
  // Barcode numbers
  gfx->setTextSize(1);
  gfx->setTextColor(BLACK);
  gfx->setCursor(10, gfx->height() - 20);
  gfx->printf("UPC: %08d %05d", 12345678 + (int)(barcodeTime * 100) % 1000, 67890);
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(BLACK);
  gfx->printf("BARCODE");
}

void drawBinaryhorizon() {
  static float horizonTime = 0;
  horizonTime += 0.05;
  
  gfx->fillScreen(BLACK);
  
  // Binary data horizon
  int horizonY = gfx->height() / 2 + sin(horizonTime * 0.5) * 20;
  
  // Draw horizon line
  gfx->drawLine(0, horizonY, gfx->width(), horizonY, gfx->color565(0, 255, 0));
  
  // Binary data streams above horizon
  for (int stream = 0; stream < 15; stream++) {
    float streamTime = horizonTime * 2 + stream * 0.4;
    int streamX = 20 + stream * 20;
    
    // Data blocks moving up from horizon
    for (int block = 0; block < 8; block++) {
      float blockTime = streamTime + block * 0.3;
      int blockY = horizonY - block * 15 - (int)(streamTime * 20) % 60;
      
      if (blockY > 10 && blockY < horizonY) {
        // Binary digit (0 or 1)
        bool isOne = sin(blockTime * 5 + stream) > 0;
        
        uint16_t digitColor = gfx->color565(
          isOne ? 255 : 100,
          255 - block * 20,
          isOne ? 100 : 255
        );
        
        gfx->setTextSize(1);
        gfx->setTextColor(digitColor);
        gfx->setCursor(streamX, blockY);
        gfx->printf("%d", isOne ? 1 : 0);
        
        // Connecting lines
        if (block > 0) {
          int prevBlockY = horizonY - (block - 1) * 15 - (int)(streamTime * 20) % 60;
          if (prevBlockY < horizonY) {
            gfx->drawLine(streamX + 3, blockY, streamX + 3, prevBlockY + 8, digitColor);
          }
        }
      }
    }
  }
  
  // Binary data streams below horizon (inverted)
  for (int stream = 0; stream < 12; stream++) {
    float streamTime = horizonTime * 1.5 + stream * 0.5;
    int streamX = 25 + stream * 25;
    
    for (int block = 0; block < 6; block++) {
      float blockTime = streamTime + block * 0.4;
      int blockY = horizonY + 20 + block * 18 + (int)(streamTime * 15) % 50;
      
      if (blockY < gfx->height() - 10 && blockY > horizonY) {
        bool isOne = cos(blockTime * 4 + stream * 1.5) > 0;
        
        uint16_t digitColor = gfx->color565(
          isOne ? 200 : 80,
          100 + block * 15,
          isOne ? 80 : 200
        );
        
        gfx->setTextSize(1);
        gfx->setTextColor(digitColor);
        gfx->setCursor(streamX, blockY);
        gfx->printf("%d", isOne ? 1 : 0);
      }
    }
  }
  
  // Grid overlay
  for (int x = 0; x < gfx->width(); x += 40) {
    gfx->drawLine(x, 0, x, gfx->height(), gfx->color565(0, 50, 0));
  }
  for (int y = 0; y < gfx->height(); y += 30) {
    gfx->drawLine(0, y, gfx->width(), y, gfx->color565(0, 50, 0));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(gfx->color565(0, 255, 0));
  gfx->printf("BINARYHORIZON");
}

void drawBinaryring() {
  static float ringTime = 0;
  ringTime += 0.06;
  
  gfx->fillScreen(BLACK);
  
  // Binary data arranged in rings
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple concentric rings
  for (int ring = 0; ring < 6; ring++) {
    float ringPhase = ringTime + ring * 0.8;
    int radius = 20 + ring * 18;
    
    // Number of binary digits per ring
    int numDigits = 8 + ring * 4;
    
    for (int digit = 0; digit < numDigits; digit++) {
      float digitAngle = (digit * 2.0 * PI / numDigits) + ringPhase * (1 + ring * 0.2);
      
      int digitX = centerX + cos(digitAngle) * radius;
      int digitY = centerY + sin(digitAngle) * radius;
      
      if (digitX >= 0 && digitX < gfx->width() - 8 && 
          digitY >= 0 && digitY < gfx->height() - 8) {
        
        // Binary value based on position and time
        bool isOne = sin(ringPhase * 3 + digit * 0.5 + ring) > 0;
        
        // Color coding by ring
        uint16_t digitColor = gfx->color565(
          255 - ring * 30,
          100 + ring * 25 + sin(ringPhase + digit) * 60,
          ring * 40 + cos(ringPhase + digit) * 80
        );
        
        gfx->setTextSize(1);
        gfx->setTextColor(digitColor);
        gfx->setCursor(digitX, digitY);
        gfx->printf("%d", isOne ? 1 : 0);
        
        // Connecting lines to center
        if (sin(ringPhase * 2 + digit) > 0.7) {
          gfx->drawLine(centerX, centerY, digitX + 3, digitY + 4, digitColor);
        }
        
        // Ring connections
        if (digit > 0) {
          float prevAngle = ((digit - 1) * 2.0 * PI / numDigits) + ringPhase * (1 + ring * 0.2);
          int prevX = centerX + cos(prevAngle) * radius;
          int prevY = centerY + sin(prevAngle) * radius;
          
          gfx->drawLine(prevX + 3, prevY + 4, digitX + 3, digitY + 4, digitColor);
        }
      }
    }
    
    // Close the ring
    if (numDigits > 0) {
      float firstAngle = ringPhase * (1 + ring * 0.2);
      float lastAngle = ((numDigits - 1) * 2.0 * PI / numDigits) + ringPhase * (1 + ring * 0.2);
      
      int firstX = centerX + cos(firstAngle) * radius;
      int firstY = centerY + sin(firstAngle) * radius;
      int lastX = centerX + cos(lastAngle) * radius;
      int lastY = centerY + sin(lastAngle) * radius;
      
      uint16_t ringColor = gfx->color565(100 + ring * 20, 150, 200 - ring * 25);
      gfx->drawLine(firstX + 3, firstY + 4, lastX + 3, lastY + 4, ringColor);
    }
  }
  
  // Central data core
  gfx->fillCircle(centerX, centerY, 8, gfx->color565(255, 255, 0));
  gfx->setTextSize(1);
  gfx->setTextColor(BLACK);
  gfx->setCursor(centerX - 6, centerY - 4);
  gfx->printf("BIN");
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("BINARYRING");
}

void drawBlaster() {
  static float blasterTime = 0;
  blasterTime += 0.07;
  
  gfx->fillScreen(BLACK);
  
  // Space blaster shooting game
  static float blasterX = 160;
  static float blasterY = 200;
  static float shots[10][4]; // x, y, vx, vy
  static float enemies[8][3]; // x, y, type
  static bool initialized = false;
  
  if (!initialized) {
    blasterX = gfx->width() / 2;
    blasterY = gfx->height() - 30;
    
    // Initialize shots
    for (int i = 0; i < 10; i++) {
      shots[i][0] = -1; // inactive
    }
    
    // Initialize enemies
    for (int i = 0; i < 8; i++) {
      enemies[i][0] = random(gfx->width());
      enemies[i][1] = random(gfx->height() / 2);
      enemies[i][2] = random(3); // enemy type
    }
    
    initialized = true;
  }
  
  // Move blaster (auto-control)
  blasterX += sin(blasterTime * 2) * 30;
  if (blasterX < 10) blasterX = 10;
  if (blasterX > gfx->width() - 10) blasterX = gfx->width() - 10;
  
  // Draw blaster ship
  gfx->fillTriangle(
    blasterX, blasterY - 10,
    blasterX - 8, blasterY + 5,
    blasterX + 8, blasterY + 5,
    gfx->color565(0, 255, 0)
  );
  
  // Engine glow
  if (sin(blasterTime * 20) > 0) {
    gfx->fillCircle(blasterX, blasterY + 8, 3, gfx->color565(255, 100, 0));
  }
  
  // Auto-fire shots
  if ((int)(blasterTime * 10) % 8 == 0) {
    for (int i = 0; i < 10; i++) {
      if (shots[i][0] < 0) { // Find inactive shot
        shots[i][0] = blasterX;
        shots[i][1] = blasterY - 10;
        shots[i][2] = sin(blasterTime) * 2; // slight angle
        shots[i][3] = -8; // upward velocity
        break;
      }
    }
  }
  
  // Update and draw shots
  for (int i = 0; i < 10; i++) {
    if (shots[i][0] >= 0) { // active shot
      shots[i][0] += shots[i][2];
      shots[i][1] += shots[i][3];
      
      // Remove if off screen
      if (shots[i][1] < 0 || shots[i][0] < 0 || shots[i][0] > gfx->width()) {
        shots[i][0] = -1; // deactivate
      } else {
        gfx->fillCircle(shots[i][0], shots[i][1], 2, gfx->color565(255, 255, 0));
        
        // Shot trail
        gfx->drawPixel(shots[i][0], shots[i][1] + 3, gfx->color565(255, 200, 0));
        gfx->drawPixel(shots[i][0], shots[i][1] + 6, gfx->color565(255, 150, 0));
      }
    }
  }
  
  // Update and draw enemies
  for (int i = 0; i < 8; i++) {
    // Move enemies
    enemies[i][1] += 1 + sin(blasterTime + i) * 0.5;
    enemies[i][0] += sin(blasterTime * 2 + i * 0.7) * 2;
    
    // Reset if off screen
    if (enemies[i][1] > gfx->height()) {
      enemies[i][0] = random(gfx->width());
      enemies[i][1] = -20;
      enemies[i][2] = random(3);
    }
    
    // Boundary check
    if (enemies[i][0] < 0) enemies[i][0] = gfx->width();
    if (enemies[i][0] > gfx->width()) enemies[i][0] = 0;
    
    // Draw enemy based on type
    uint16_t enemyColor;
    switch ((int)enemies[i][2]) {
      case 0: // Red fighter
        enemyColor = gfx->color565(255, 0, 0);
        gfx->fillRect(enemies[i][0] - 4, enemies[i][1] - 2, 8, 4, enemyColor);
        break;
      case 1: // Blue bomber
        enemyColor = gfx->color565(0, 100, 255);
        gfx->fillCircle(enemies[i][0], enemies[i][1], 5, enemyColor);
        break;
      case 2: // Green scout
        enemyColor = gfx->color565(0, 255, 100);
        gfx->fillTriangle(
          enemies[i][0], enemies[i][1] - 4,
          enemies[i][0] - 3, enemies[i][1] + 2,
          enemies[i][0] + 3, enemies[i][1] + 2,
          enemyColor
        );
        break;
    }
    
    // Check collisions with shots
    for (int shot = 0; shot < 10; shot++) {
      if (shots[shot][0] >= 0) {
        float dx = enemies[i][0] - shots[shot][0];
        float dy = enemies[i][1] - shots[shot][1];
        if (sqrt(dx*dx + dy*dy) < 8) {
          // Hit! Create explosion effect
          for (int exp = 0; exp < 5; exp++) {
            int expX = enemies[i][0] + random(10) - 5;
            int expY = enemies[i][1] + random(10) - 5;
            gfx->fillCircle(expX, expY, 2, gfx->color565(255, 255, 0));
          }
          
          // Reset enemy and shot
          enemies[i][0] = random(gfx->width());
          enemies[i][1] = -20;
          shots[shot][0] = -1;
        }
      }
    }
  }
  
  // Starfield
  for (int star = 0; star < 30; star++) {
    int starX = (star * 37) % gfx->width();
    int starY = (int)(blasterTime * 100 + star * 23) % gfx->height();
    gfx->drawPixel(starX, starY, gfx->color565(200, 200, 255));
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("BLASTER");
}

void drawBlitspin() {
  static float spinTime = 0;
  spinTime += 0.08;
  
  gfx->fillScreen(BLACK);
  
  // Rotating bitmap-like patterns
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple spinning elements
  for (int spinner = 0; spinner < 4; spinner++) {
    float spinPhase = spinTime * (1 + spinner * 0.3) + spinner * PI/2;
    
    int spinnerX = centerX + cos(spinTime * 0.4 + spinner) * 40;
    int spinnerY = centerY + sin(spinTime * 0.3 + spinner) * 30;
    
    // Create bitmap-like pattern
    int patternSize = 16;
    for (int px = 0; px < patternSize; px++) {
      for (int py = 0; py < patternSize; py++) {
        // Create pattern based on position
        bool isSet = ((px ^ py) & 3) == 0 || (px & py & 1);
        
        if (isSet) {
          // Rotate the pattern
          float relX = px - patternSize/2;
          float relY = py - patternSize/2;
          
          float rotX = relX * cos(spinPhase) - relY * sin(spinPhase);
          float rotY = relX * sin(spinPhase) + relY * cos(spinPhase);
          
          int finalX = spinnerX + rotX * 2;
          int finalY = spinnerY + rotY * 2;
          
          if (finalX >= 0 && finalX < gfx->width() && 
              finalY >= 0 && finalY < gfx->height()) {
            
            uint16_t pixelColor = gfx->color565(
              150 + spinner * 25 + sin(spinPhase + px) * 60,
              100 + cos(spinPhase + py) * 80,
              200 - spinner * 30 + sin(spinPhase + px + py) * 50
            );
            
            gfx->fillRect(finalX, finalY, 2, 2, pixelColor);
          }
        }
      }
    }
  }
  
  // Additional spinning lines
  for (int line = 0; line < 8; line++) {
    float lineAngle = spinTime * 2 + line * PI/4;
    
    int startRadius = 20;
    int endRadius = 60;
    
    int startX = centerX + cos(lineAngle) * startRadius;
    int startY = centerY + sin(lineAngle) * startRadius;
    int endX = centerX + cos(lineAngle) * endRadius;
    int endY = centerY + sin(lineAngle) * endRadius;
    
    uint16_t lineColor = gfx->color565(
      255 * abs(sin(spinTime + line)),
      255 * abs(cos(spinTime + line * 1.3)),
      255 * abs(sin(spinTime * 1.7 + line))
    );
    
    gfx->drawLine(startX, startY, endX, endY, lineColor);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("BLITSPIN");
}

void drawBlocktube() {
  static float tubeTime = 0;
  tubeTime += 0.05;
  
  gfx->fillScreen(BLACK);
  
  // 3D block tube tunnel
  int centerX = gfx->width() / 2;
  int centerY = gfx->height() / 2;
  
  // Multiple tube segments
  for (int segment = 0; segment < 15; segment++) {
    float segmentTime = tubeTime + segment * 0.3;
    
    // Segment moves toward viewer
    float z = fmod(segmentTime * 30, 80) + 5;
    float scale = 50.0 / z;
    
    if (scale > 0.05 && scale < 3) {
      // Tube made of blocks
      int blockSize = max(1, (int)(8 * scale));
      int tubeRadius = 40 * scale;
      
      // Blocks around the tube
      int numBlocks = 12;
      for (int block = 0; block < numBlocks; block++) {
        float blockAngle = (block * 2.0 * PI / numBlocks) + segmentTime;
        
        int blockX = centerX + cos(blockAngle) * tubeRadius;
        int blockY = centerY + sin(blockAngle) * tubeRadius;
        
        if (blockX >= 0 && blockX < gfx->width() && 
            blockY >= 0 && blockY < gfx->height()) {
          
          // Block color based on distance and position
          uint16_t blockColor = gfx->color565(
            255 * scale + sin(segmentTime + block) * 100,
            150 * scale + cos(segmentTime + block) * 60,
            200 * scale + sin(segmentTime * 2 + block) * 80
          );
          
          // 3D block effect
          gfx->fillRect(blockX, blockY, blockSize, blockSize, blockColor);
          
          // Block highlights
          uint16_t highlightColor = gfx->color565(
            min(255, (int)(255 * scale * 1.5)),
            min(255, (int)(200 * scale * 1.5)),
            min(255, (int)(255 * scale * 1.5))
          );
          
          gfx->drawLine(blockX, blockY, blockX + blockSize - 1, blockY, highlightColor);
          gfx->drawLine(blockX, blockY, blockX, blockY + blockSize - 1, highlightColor);
          
          // Block shadows
          uint16_t shadowColor = gfx->color565(
            (int)(100 * scale),
            (int)(50 * scale),
            (int)(80 * scale)
          );
          
          gfx->drawLine(blockX + blockSize - 1, blockY + 1, 
                       blockX + blockSize - 1, blockY + blockSize - 1, shadowColor);
          gfx->drawLine(blockX + 1, blockY + blockSize - 1, 
                       blockX + blockSize - 1, blockY + blockSize - 1, shadowColor);
        }
      }
    }
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("BLOCKTUBE");
}

void drawBouboule() {
  static float boubouleTime = 0;
  boubouleTime += 0.04;
  
  gfx->fillScreen(BLACK);
  
  // French "Bouboule" effect - bouncing balls/bubbles
  static float ballX[12];
  static float ballY[12];
  static float ballVX[12];
  static float ballVY[12];
  static float ballSize[12];
  static bool initialized = false;
  
  if (!initialized) {
    for (int i = 0; i < 12; i++) {
      ballX[i] = random(gfx->width());
      ballY[i] = random(gfx->height());
      ballVX[i] = (random(100) - 50) / 10.0;
      ballVY[i] = (random(100) - 50) / 10.0;
      ballSize[i] = 5 + random(10);
    }
    initialized = true;
  }
  
  // Update and draw balls
  for (int i = 0; i < 12; i++) {
    // Update position
    ballX[i] += ballVX[i];
    ballY[i] += ballVY[i];
    
    // Bounce off walls
    if (ballX[i] <= ballSize[i] || ballX[i] >= gfx->width() - ballSize[i]) {
      ballVX[i] = -ballVX[i];
      ballX[i] = max(ballSize[i], min(gfx->width() - ballSize[i], ballX[i]));
    }
    
    if (ballY[i] <= ballSize[i] || ballY[i] >= gfx->height() - ballSize[i]) {
      ballVY[i] = -ballVY[i];
      ballY[i] = max(ballSize[i], min(gfx->height() - ballSize[i], ballY[i]));
    }
    
    // Ball-to-ball collisions (simplified)
    for (int j = i + 1; j < 12; j++) {
      float dx = ballX[j] - ballX[i];
      float dy = ballY[j] - ballY[i];
      float distance = sqrt(dx * dx + dy * dy);
      float minDistance = ballSize[i] + ballSize[j];
      
      if (distance < minDistance && distance > 0) {
        // Simple collision response
        float nx = dx / distance;
        float ny = dy / distance;
        
        // Separate balls
        float overlap = minDistance - distance;
        ballX[i] -= nx * overlap * 0.5;
        ballY[i] -= ny * overlap * 0.5;
        ballX[j] += nx * overlap * 0.5;
        ballY[j] += ny * overlap * 0.5;
        
        // Exchange velocities (simplified)
        float tempVX = ballVX[i];
        float tempVY = ballVY[i];
        ballVX[i] = ballVX[j];
        ballVY[i] = ballVY[j];
        ballVX[j] = tempVX;
        ballVY[j] = tempVY;
      }
    }
    
    // Draw ball with gradient effect
    int ballRadius = ballSize[i];
    
    // Main ball body
    uint16_t ballColor = gfx->color565(
      150 + sin(boubouleTime + i * 0.7) * 100,
      100 + cos(boubouleTime + i * 1.1) * 80,
      200 + sin(boubouleTime * 1.3 + i) * 55
    );
    
    gfx->fillCircle(ballX[i], ballY[i], ballRadius, ballColor);
    
    // Highlight
    uint16_t highlightColor = gfx->color565(255, 255, 255);
    gfx->fillCircle(ballX[i] - ballRadius/3, ballY[i] - ballRadius/3, ballRadius/3, highlightColor);
    
    // Shadow/reflection
    uint16_t shadowColor = gfx->color565(
      (150 + sin(boubouleTime + i * 0.7) * 100) / 3,
      (100 + cos(boubouleTime + i * 1.1) * 80) / 3,
      (200 + sin(boubouleTime * 1.3 + i) * 55) / 3
    );
    gfx->fillCircle(ballX[i] + ballRadius/4, ballY[i] + ballRadius/4, ballRadius/4, shadowColor);
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("BOUBOULE");
}

void drawBouncingcow() {
  static float cowTime = 0;
  cowTime += 0.06;
  
  gfx->fillScreen(gfx->color565(0, 50, 100)); // Sky blue
  
  // Bouncing cow animation
  static float cowX = 160;
  static float cowY = 120;
  static float cowVX = 2;
  static float cowVY = -3;
  
  // Update cow position
  cowX += cowVX + sin(cowTime * 3) * 0.5;
  cowY += cowVY;
  
  // Gravity
  cowVY += 0.2;
  
  // Bounce off ground
  if (cowY > gfx->height() - 40) {
    cowY = gfx->height() - 40;
    cowVY = -abs(cowVY) * 0.8; // Bounce with energy loss
  }
  
  // Bounce off walls
  if (cowX <= 20 || cowX >= gfx->width() - 20) {
    cowVX = -cowVX;
    cowX = max(20.0f, min((float)(gfx->width() - 20), cowX));
  }
  
  // Draw cow (simplified)
  
  // Cow body
  gfx->fillCircle(cowX, cowY, 12, WHITE);
  gfx->fillCircle(cowX - 8, cowY, 8, WHITE); // Head
  
  // Cow spots
  gfx->fillCircle(cowX - 3, cowY - 5, 3, BLACK);
  gfx->fillCircle(cowX + 5, cowY + 2, 4, BLACK);
  gfx->fillCircle(cowX - 2, cowY + 6, 2, BLACK);
  
  // Cow legs
  int legOffset = sin(cowTime * 8) * 2; // Walking animation
  gfx->drawLine(cowX - 8, cowY + 8, cowX - 8 + legOffset, cowY + 15, BLACK);
  gfx->drawLine(cowX - 2, cowY + 8, cowX - 2 - legOffset, cowY + 15, BLACK);
  gfx->drawLine(cowX + 4, cowY + 8, cowX + 4 + legOffset, cowY + 15, BLACK);
  gfx->drawLine(cowX + 10, cowY + 8, cowX + 10 - legOffset, cowY + 15, BLACK);
  
  // Cow face
  gfx->fillCircle(cowX - 12, cowY - 3, 2, BLACK); // Eye
  gfx->fillCircle(cowX - 12, cowY + 1, 1, BLACK); // Nose
  
  // Cow tail (wagging)
  int tailX = cowX + 12 + sin(cowTime * 6) * 4;
  int tailY = cowY + sin(cowTime * 6) * 3;
  gfx->drawLine(cowX + 10, cowY, tailX, tailY, BLACK);
  
  // Motion trail
  for (int trail = 1; trail < 5; trail++) {
    int trailX = cowX - cowVX * trail * 2;
    int trailY = cowY - cowVY * trail * 2;
    
    if (trailX >= 0 && trailX < gfx->width() && trailY >= 0 && trailY < gfx->height()) {
      int trailAlpha = 255 - trail * 50;
      uint16_t trailColor = gfx->color565(trailAlpha, trailAlpha, trailAlpha);
      gfx->fillCircle(trailX, trailY, 12 - trail * 2, trailColor);
    }
  }
  
  // Ground
  gfx->fillRect(0, gfx->height() - 20, gfx->width(), 20, gfx->color565(0, 100, 0));
  
  // Clouds
  for (int cloud = 0; cloud < 4; cloud++) {
    float cloudTime = cowTime * 0.5 + cloud * 1.5;
    int cloudX = (int)(cloudTime * 20) % (gfx->width() + 60) - 30;
    int cloudY = 20 + cloud * 15;
    
    gfx->fillCircle(cloudX, cloudY, 8, WHITE);
    gfx->fillCircle(cloudX + 10, cloudY, 6, WHITE);
    gfx->fillCircle(cloudX - 8, cloudY, 5, WHITE);
  }
  
  // "Moo" speech bubble occasionally
  if (sin(cowTime * 2) > 0.9) {
    gfx->fillCircle(cowX - 20, cowY - 25, 12, WHITE);
    gfx->drawCircle(cowX - 20, cowY - 25, 12, BLACK);
    gfx->setTextSize(1);
    gfx->setTextColor(BLACK);
    gfx->setCursor(cowX - 26, cowY - 29);
    gfx->printf("MOO!");
    
    // Speech bubble pointer
    gfx->fillTriangle(
      cowX - 15, cowY - 15,
      cowX - 10, cowY - 10,
      cowX - 8, cowY - 15,
      WHITE
    );
  }
  
  gfx->setCursor(10, 10);
  gfx->setTextColor(WHITE);
  gfx->printf("BOUNCINGCOW");
}
