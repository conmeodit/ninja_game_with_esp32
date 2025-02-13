#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define BUTTON_JUMP 18
#define BUTTON_DUCK 19
#define EEPROM_HIGH_SCORE 0

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const float GRAVITY = 0.6;
const float JUMP_FORCE = -7.0;
const float MAX_FALL_SPEED = 12.0;
const float GROUND_Y = SCREEN_HEIGHT - 16;

const unsigned char PROGMEM ninja_run1[] = {
  0x06, 0xFC, 0xB5, 0x92, 0x59, 0x8A, 0x02, 0x92,  
  0x62, 0x82, 0xBC, 0x7C, 0x00, 0x10, 0x31, 0xF0,  
  0x3B, 0x38, 0x1E, 0x68, 0x0E, 0x4C, 0x0E, 0x44,  
  0x00, 0x80, 0x01, 0x40, 0x02, 0x20, 0x04, 0x20
};

const unsigned char PROGMEM ninja_run2[] = {
  0xB2, 0x7C, 0x4D, 0x92, 0x01, 0x8A, 0x96, 0x92,  
  0x68, 0x82, 0x00, 0x7C, 0x00, 0x10, 0x30, 0xF8,  
  0x39, 0xAC, 0x1F, 0x64, 0x0E, 0x42, 0x0E, 0x40,  
  0x00, 0x80, 0x01, 0x40, 0x01, 0x80, 0x01, 0x80  
};

const unsigned char PROGMEM ninja_jump[] = {
  0xB2, 0x7C, 0x4D, 0x92, 0x01, 0x8A, 0x96, 0x92,  
  0x68, 0x82, 0x00, 0x7C, 0x00, 0x10, 0x00, 0xF8,  
  0x01, 0xAC, 0x03, 0x67, 0x00, 0x43, 0x00, 0x47,  
  0x00, 0x8E, 0x01, 0x4C, 0x02, 0x20, 0x04, 0x20
};

const unsigned char PROGMEM ninja_duck[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  
  0x00, 0x00, 0x0B, 0xBC, 0x04, 0x5C, 0x0C, 0xEE,  
  0xC3, 0x11, 0xE0, 0xF1, 0x79, 0xD1, 0x3E, 0xAE,  
  0x38, 0x90, 0x01, 0x48, 0x02, 0x20, 0x04, 0x20
};

const unsigned char PROGMEM tree_small[] = {
  0x1E, 0x7E, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F,  
  0x7E, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18  
};

const unsigned char PROGMEM tree_large[] = {
  0x08, 0x00, 0x1E, 0x00, 0x3F, 0x00, 0x7D, 0x80, 0xFF, 0xC0, 0xFF, 0xE0, 0xFF, 0xE0, 0xFF, 0xF0,  
  0x3F, 0xF0, 0x37, 0xF0, 0x33, 0xE0, 0x30, 0xC0, 0x30, 0xC0, 0x30, 0xC0, 0x30, 0xC0, 0x30, 0xC0  
};

const unsigned char PROGMEM kunai[] = {
  0x08, 0x00, 0x18, 0x06, 0x78, 0x09, 0xFF, 0xF9,  
  0x78, 0x09, 0x18, 0x06, 0x08, 0x00, 0x00, 0x00  
};

const unsigned char PROGMEM cloud[] = {
  0x00, 0x38, 0x00, 0x7C, 0x38, 0xFE, 0x7D, 0xFF,  
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF 
};

struct GameObject {
  float x;
  float y;
  int width;
  int height;
  float velocity_x;
  float velocity_y;
  bool active;
  uint8_t type;
  uint8_t frame;
};

enum GameState {
  MENU,
  PLAYING,
  GAME_OVER
};

enum ObstacleType {
  TREE_SMALL,
  TREE_LARGE,
  KUNAI_LOW,
  KUNAI_HIGH
};

struct Sprite {
  const unsigned char* bitmap;
  int width;
  int height;
};

GameObject player;
GameObject obstacles[3];
GameObject clouds[2];
GameState gameState = MENU;
unsigned long lastFrameTime = 0;
unsigned long frameCount = 0;
unsigned long animationTimer = 0;
int score = 0;
int highScore = 0;
float gameSpeed = 1.0;
bool isNight = false;
unsigned long dayNightTimer = 0;
const unsigned long DAY_NIGHT_DURATION = 10000;

bool getBitmapPixel(const unsigned char* bitmap, int bmpWidth, int bmpHeight, int x, int y) {
  if (x < 0 || x >= bmpWidth || y < 0 || y >= bmpHeight)
    return false;
  int bytesPerRow = (bmpWidth + 7) / 8;
  int byteIndex = y * bytesPerRow + (x / 8);
  uint8_t byteVal = pgm_read_byte(&bitmap[byteIndex]);
  int bitIndex = 7 - (x % 8);
  return (byteVal >> bitIndex) & 0x01;
}

bool pixelCollision(float ax, float ay, int aw, int ah, const unsigned char *bitmapA,
                    float bx, float by, int bw, int bh, const unsigned char *bitmapB) {
  int left = max((int)ax, (int)bx);
  int right = min((int)(ax + aw), (int)(bx + bw));
  int top = max((int)ay, (int)by);
  int bottom = min((int)(ay + ah), (int)(by + bh));
  if (left >= right || top >= bottom)
    return false;
  for (int y = top; y < bottom; y++) {
    for (int x = left; x < right; x++) {
      int axRel = x - (int)ax;
      int ayRel = y - (int)ay;
      int bxRel = x - (int)bx;
      int byRel = y - (int)by;
      bool pixelA = getBitmapPixel(bitmapA, aw, ah, axRel, ayRel);
      bool pixelB = getBitmapPixel(bitmapB, bw, bh, bxRel, byRel);
      if (pixelA && pixelB)
        return true;
    }
  }
  return false;
}

Sprite getPlayerSprite() {
  Sprite s;
  if (digitalRead(BUTTON_DUCK) == LOW) {
    s.bitmap = ninja_duck;
    s.width = 16;
    s.height = 16;
  } else if (player.y < GROUND_Y) {
    s.bitmap = ninja_jump;
    s.width = 16;
    s.height = 16;
  } else {
    s.bitmap = (player.frame ? ninja_run1 : ninja_run2);
    s.width = 16;
    s.height = 16;
  }
  return s;
}

Sprite getObstacleSprite(GameObject &ob) {
  Sprite s;
  switch (ob.type) {
    case TREE_SMALL:
      s.bitmap = tree_small;
      s.width = 8;
      s.height = 16;
      break;
    case TREE_LARGE:
      s.bitmap = tree_large;
      s.width = 12;
      s.height = 16;
      break;
    case KUNAI_LOW:
    case KUNAI_HIGH:
      s.bitmap = kunai;
      s.width = 16;
      s.height = 8;
      break;
    default:
      s.bitmap = NULL;
      s.width = 0;
      s.height = 0;
  }
  return s;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_JUMP, INPUT_PULLUP);
  pinMode(BUTTON_DUCK, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  EEPROM.begin(512);
  highScore = EEPROM.read(EEPROM_HIGH_SCORE);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  initializeGame();
  showSplashScreen();
}

void loop() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastFrameTime >= 16) {
    switch (gameState) {
      case MENU:
        handleMenu();
        break;
      case PLAYING:
        handleGameplay();
        break;
      case GAME_OVER:
        handleGameOver();
        break;
    }
    
    lastFrameTime = currentTime;
    frameCount++;
  }
}

void handleGameplay() {
  handleInput();
  updateGame();
  handleCollisions();
  updateDayNightCycle();
  drawGame();
}

void updateGame() {
  if (player.y < GROUND_Y || player.velocity_y < 0) {
    if (player.y < GROUND_Y && digitalRead(BUTTON_DUCK) == LOW) {
      player.velocity_y += GRAVITY * 2;
    } else {
      player.velocity_y += GRAVITY;
    }
    
    if (player.velocity_y > MAX_FALL_SPEED) {
      player.velocity_y = MAX_FALL_SPEED;
    }
    
    player.y += player.velocity_y;
    
    if (player.y > GROUND_Y) {
      player.y = GROUND_Y;
      player.velocity_y = 0;
    }
  }
  
  if (player.y >= GROUND_Y && frameCount % 6 == 0) {
    player.frame = !player.frame;
  }
  
  for (int i = 0; i < 3; i++) {
    if (obstacles[i].active) {
      obstacles[i].x += obstacles[i].velocity_x * gameSpeed;
      
      if (obstacles[i].type == KUNAI_LOW || obstacles[i].type == KUNAI_HIGH) {
        if (frameCount % 10 == 0) {
          obstacles[i].frame = !obstacles[i].frame;
        }
      }
      
      if (obstacles[i].x < -obstacles[i].width) {
        resetObstacle(i);
        score++;
        
        if (score % 15 == 0) {
          gameSpeed += 0.1;
        }
      }
    }
  }
  
  for (int i = 0; i < 2; i++) {
    if (clouds[i].active) {
      clouds[i].x += clouds[i].velocity_x;
      if (clouds[i].x < -clouds[i].width) {
        clouds[i].x = SCREEN_WIDTH;
        clouds[i].y = random(0, SCREEN_HEIGHT/2);
      }
    }
  }
}

void handleCollisions() {
  for (int i = 0; i < 3; i++) {
    if (obstacles[i].active) {
      Sprite playerSprite = getPlayerSprite();
      Sprite obstacleSprite = getObstacleSprite(obstacles[i]);
      if (pixelCollision(player.x, player.y, playerSprite.width, playerSprite.height, playerSprite.bitmap,
                         obstacles[i].x, obstacles[i].y, obstacleSprite.width, obstacleSprite.height, obstacleSprite.bitmap)) {
        gameOver();
        return;
      }
    }
  }
}

void drawGame() {
  display.clearDisplay();
  
  if (isNight) {
    for (int i = 0; i < 5; i++) {
      int x = (SCREEN_WIDTH - 1) - ((frameCount * 2 + i * 30) % SCREEN_WIDTH);
      display.drawPixel(x, i * 10 + 5, WHITE);
    }
  }
  
  for (int i = 0; i < 2; i++) {
    if (clouds[i].active) {
      display.drawBitmap(clouds[i].x, clouds[i].y, cloud, 16, 8, WHITE);
    }
  }
  
  display.drawLine(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH, SCREEN_HEIGHT - 1, WHITE);
  
  if (digitalRead(BUTTON_DUCK) == LOW) {
    display.drawBitmap(player.x, player.y, ninja_duck, 16, 16, WHITE);
  } else if (player.y < GROUND_Y) {
    display.drawBitmap(player.x, player.y, ninja_jump, 16, 16, WHITE);
  } else {
    display.drawBitmap(player.x, player.y, player.frame ? ninja_run1 : ninja_run2, 16, 16, WHITE);
  }

  for (int i = 0; i < 3; i++) {
    if (obstacles[i].active) {
      switch (obstacles[i].type) {
        case TREE_SMALL:
          display.drawBitmap(obstacles[i].x, obstacles[i].y, tree_small, 8, 16, WHITE);
          break;
        case TREE_LARGE:
          display.drawBitmap(obstacles[i].x, obstacles[i].y, tree_large, 12, 16, WHITE);
          break;
        case KUNAI_LOW:
        case KUNAI_HIGH:
          display.drawBitmap(obstacles[i].x, obstacles[i].y, kunai, 16, 8, WHITE);
          break;
      }
    }
  }
  
  display.setCursor(SCREEN_WIDTH - 40, 5);
  display.print(score);
  
  display.display();
}

void handleInput() {
  static bool canJump = true;
  static bool jumpButtonPrevious = true;
  bool jumpButtonCurrent = digitalRead(BUTTON_JUMP);
  
  if (player.y >= GROUND_Y) {
    canJump = true;
  }
  
  if (jumpButtonCurrent == LOW && jumpButtonPrevious == HIGH && canJump) {
    player.velocity_y = JUMP_FORCE;
    canJump = false;
  }
  
  jumpButtonPrevious = jumpButtonCurrent;
}

void resetObstacle(int index) {
  obstacles[index].x = SCREEN_WIDTH + random(115, 150);
  
  int type = random(4);
  
  switch (type) {
    case TREE_SMALL:
      obstacles[index].width = 8;
      obstacles[index].height = 16;
      obstacles[index].y = SCREEN_HEIGHT - 16;
      break;
    case TREE_LARGE:
      obstacles[index].width = 12;
      obstacles[index].height = 16;
      obstacles[index].y = SCREEN_HEIGHT - 16;
      break;
    case KUNAI_LOW:
      obstacles[index].width = 16;
      obstacles[index].height = 8;
      obstacles[index].y = SCREEN_HEIGHT - 15;
      break;
    case KUNAI_HIGH:
      obstacles[index].width = 16;
      obstacles[index].height = 8;
      obstacles[index].y = SCREEN_HEIGHT - 19;
      break;
  }
  
  obstacles[index].type = type;
  obstacles[index].active = true;
  obstacles[index].velocity_x = -3;
  obstacles[index].frame = 0;
}

void gameOver() {
  gameState = GAME_OVER;
  
  if (score > highScore) {
    highScore = score;
    EEPROM.write(EEPROM_HIGH_SCORE, highScore);
    EEPROM.commit();
  }
}

void showSplashScreen() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 5);
  display.print("NINJA RUN");
  display.setTextSize(1);
  display.setCursor(8, 40);
  display.print("Press JUMP to start");
  display.display();
  
  while(digitalRead(BUTTON_JUMP) == HIGH) {
    delay(10);
  }
  
  gameState = PLAYING;
}

void handleMenu() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(8, 5);
  display.print("NINJA RUN");
  
  display.setCursor(20, 45);
  display.print("Press JUMP to start");
  display.display();
  
  if (digitalRead(BUTTON_JUMP) == LOW) {
    delay(100);
    initializeGame();
    gameState = PLAYING;
  }
}

void handleGameOver() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(11, 0);
  display.print("GAME OVER");
  
  display.setTextSize(1);
  display.setCursor(0, 26);
  display.print("Score: ");
  display.print(score);
  
  display.setCursor(0, 45);
  display.print("Press JUMP to restart");
  display.display();
  
  if (digitalRead(BUTTON_JUMP) == LOW) {
    delay(200);
    initializeGame();
    gameState = PLAYING;
  }
}

void initializeGame() {
  player.x = 20;
  player.y = GROUND_Y;
  player.width = 16;
  player.height = 16;
  player.velocity_x = 0;
  player.velocity_y = 0;
  player.active = true;
  player.frame = 0;
  
  for (int i = 0; i < 3; i++) {
    obstacles[i].active = false;
    resetObstacle(i);
    obstacles[i].x = SCREEN_WIDTH + i * 100;
  }
  
  for (int i = 0; i < 2; i++) {
    clouds[i].x = SCREEN_WIDTH + i * 80;
    clouds[i].y = random(0, SCREEN_HEIGHT/2);
    clouds[i].width = 12;
    clouds[i].height = 6;
    clouds[i].velocity_x = -1;
    clouds[i].active = true;
  }
  
  score = 0;
  gameSpeed = 1.0;
  frameCount = 0;
  isNight = false;
  dayNightTimer = millis();
}

void updateDayNightCycle() {
  if (millis() - dayNightTimer >= DAY_NIGHT_DURATION) {
    isNight = !isNight;
    dayNightTimer = millis();
  }
}