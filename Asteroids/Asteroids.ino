/*
  Asteroids
  Copyright (C) 2011 nootropic design, LLC
  All ights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
*/
//#include <avr/pgmspace.h>
//#include <TVout.h>
//#include <video_gen.h>
//#include <EEPROM.h>
//#include <Controllers.h>
#include "Asteroids.h"
#include "title_bitmap.h"
#include "hackvision_logo_bitmap.h"
#include "asteroid_bitmaps.h"
#include "asteroid_vertices.h"
#include "ship_bitmaps.h"
#include "ship_vertices.h"
#include "saucer_bitmaps.h"
#include "saucer_vertices.h"
#include "explosion_bitmaps.h"
#include "ship_explosion_bitmaps.h"
#include "explosion_frequencies.h"
#include "asteroids_font.h"
#include <MicroGamer.h>
#include "MicroGamerTones.h"
#include <MicroGamerMemoryCard.h>

#define W (WIDTH)
#define H (HEIGHT)

#define BASE_FRAME_RATE (15)
#define MENU_FRAME_RATE (8)

#define NBR_HIGH_SCORES (7)

MicroGamer arduboy;
MicroGamerTones audio(arduboy.audio.enabled);
MicroGamerMemoryCard mem(64/4);

//#define DEBUG

char s[16]; // general string buffer
byte x, y;
float shipX, shipY;
float shipDX, shipDY;
byte shipHeading;
byte oldShipHeading;
byte oldShipX, oldShipY;
byte thrustX, thrustY;
unsigned long clock;
unsigned long nextHeartbeat;

// hyperspaceCount has to be a signed integer
int hyperspaceCount = -1;

byte level = 1;
byte nAsteroids;
volatile unsigned int freq;
volatile byte sound;
byte explosionFreqIndex = 0;
byte heartbeatFreq;
uint32_t LOW_FREQ_OCR = F_CPU / 2 / 1024;

Asteroid asteroids[MAX_ASTEROIDS];
Shot shots[MAX_SHOTS];
Shot saucerShot;
Explosion explosions[MAX_EXPLOSIONS];
Explosion shipExplosion;
byte saucerX, saucerY, endSaucerX;
byte saucerHeading;
byte saucerType;
unsigned int score;
char remainingShips;
char initials[3];
boolean fired = false;
boolean scored = false;
byte currentTonePriority = 0;
byte explosionIndex = 0;

unsigned long debugTime = 2000;

// Allow the overall speed of the game to be adjusted.
// Higher number (like 1.5) slow the game down.  Lower numbers (like 0.6) speed it up.
float speedAdjust = 1.0;

// const char s0[] = "ASTEROIDS";
// const char s1[] = "HIGH SCORES";
// const char s2[] = "GAME";
// const char s3[] = "OVER";

//  const char* const strings[] PROGMEM = {s0, s1, s2, s3};

void setup()  {

  // If pin 12 is pulled LOW, then the PAL jumper is shorted.
  //pinMode(12, INPUT);
  //digitalWrite(12, HIGH);

  //if (digitalRead(12) == LOW) {
  //  tv.begin(_PAL, W, H);
  //  // Since PAL processing is faster, we need to slow the game play down.
  //  speedAdjust = 1.1;
  //} else {
  //  tv.begin(_NTSC, W, H);
  //}

  arduboy.boot();
  arduboy.setFrameRate(BASE_FRAME_RATE);
  arduboy.setTextSize(1);
  arduboy.setTextColor(WHITE);
  arduboy.setTextColor(BLACK);
  // tv.select_font(asteroids_font);
  randomSeed(analogRead(0));

  // tv.set_vbi_hook(&soundISR);

  initGame(false);
}

int downPressed() {
  return arduboy.pressed(DOWN_BUTTON);
}

int upPressed() {
  return arduboy.pressed(UP_BUTTON);
}

int leftPressed() {
  return arduboy.pressed(LEFT_BUTTON);
}

int rightPressed() {
  return arduboy.pressed(RIGHT_BUTTON);
}

int firePressed() {
  return arduboy.pressed(Y_BUTTON);
}

void drawAsteroid(Asteroid a) {
  overlaybitmap(a.x, a.y, asteroid_bitmaps + (((a.info >> 4) & 0x7) * SIZEOF_ASTEROID_BITMAP_RECORD), 0, 0, 0);
}

void eraseAsteroid(Asteroid a) {
  erasebitmap(a.x, a.y, asteroid_bitmaps + (((a.info >> 4) & 0x7) * SIZEOF_ASTEROID_BITMAP_RECORD), 0, 0, 0);
}

void initVars() {
  byte heading;
  byte type;

  clock = 0;
  nextHeartbeat = 1;
  hyperspaceCount = -1;
  nAsteroids = 3 + ((level - 1) / 2);
  if (nAsteroids > 5) {
    nAsteroids = 5;
  }
  explosionIndex = 0;
  shipX = W / 2 - 2;
  shipY = H / 2 - 2;
  oldShipX = (byte)(shipX + 0.5);
  oldShipY = (byte)(shipY + 0.5);
  shipDX = 0.0;
  shipDY = 0.0;
  shipHeading = 0;
  oldShipHeading = 1;
  thrustX = 3;
  thrustY = 6;
  scored = true;
  saucerX = 255;
  saucerY = 255;
  saucerHeading = 0;
  saucerShot.ttl = 0xFF;
  saucerType = 0;
  sound = 0;
  heartbeatFreq = 95;

  x = W / 2;
  y = H / 2;

  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    asteroids[i].info = 0x80; // unused asteroid structure has this value
    asteroids[i].x = 0;
    asteroids[i].y = 0;
  }
  for (int i = 0; i < nAsteroids; i++) {
    type = random(0, 4);
    heading = random(0, 16);
    asteroids[i].info = (type << 4) | heading;
    if (random(0, 2) == 0) {
      asteroids[i].x = random(0, W / 4);
    } else {
      asteroids[i].x = random(W * 3 / 4, W - 1);
    }
    if (random(0, 2) == 0) {
      asteroids[i].y = random(0, H / 4);
    } else {
      asteroids[i].y = random(H * 3 / 4, H - 1);
    }
  }
  for (int i = 0; i < MAX_EXPLOSIONS; i++) {
    explosions[i].index = 255;
  }
  shipExplosion.index = 255;
}

void moveAsteroids() {
  byte type, fast, heading;
  byte div;
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (asteroids[i].info == 0x80) {
      // this is an unused Asteroid structure.
      continue;
    }
    type = (asteroids[i].info >> 4) & 0x7;
    fast = (asteroids[i].info >> 7);
    heading = asteroids[i].info & 0x0F;
    div = 2;
    if ((type < 4) || (fast == 0)) {
      if (clock % 2 == 1) {
        continue;
      } else {
        div = 4;
      }
    }
    eraseAsteroid(asteroids[i]);
    switch (heading) {
      case 0:
        asteroids[i].y--;
        break;
      case 1:
        asteroids[i].y--;
        if (clock % div > 0) {
          // move to the right one pixel every other loop we are moving this asteroid
          // For fast asteroids, this is odd loops because div == 2.
          // For slow asteroids, this is loops divisible by 2, but not 4.  Every other even loop.
          asteroids[i].x++;
        }
        break;
      case 2: // 45 degrees
        asteroids[i].y--;
        asteroids[i].x++;
        break;
      case 3:
        asteroids[i].x++;
        if (clock % div > 0) {
          asteroids[i].y--;
        }
        break;
      case 4:
        asteroids[i].x++;
        break;
      case 5:
        asteroids[i].x++;
        if (clock % div > 0) {
          asteroids[i].y++;
        }
        break;
      case 6:
        asteroids[i].x++;
        asteroids[i].y++;
        break;
      case 7:
        asteroids[i].y++;
        if (clock % div > 0) {
          asteroids[i].x++;
        }
        break;
      case 8:
        asteroids[i].y++;
        break;
      case 9:
        asteroids[i].y++;
        if (clock % div > 0) {
          asteroids[i].x--;
        }
        break;
      case 10:
        asteroids[i].x--;
        asteroids[i].y++;
        break;
      case 11:
        asteroids[i].x--;
        if (clock % div > 0) {
          asteroids[i].y++;
        }
        break;
      case 12:
        asteroids[i].x--;
        break;
      case 13:
        asteroids[i].x--;
        if (clock % div > 0) {
          asteroids[i].y--;
        }
        break;
      case 14:
        asteroids[i].x--;
        asteroids[i].y--;
        break;
      case 15:
        asteroids[i].y--;
        if (clock % div > 0) {
          asteroids[i].x--;
        }
        break;
    }
    if (asteroids[i].x == 255) {
      asteroids[i].x = W - 1;
    } else {
      if (asteroids[i].x > W - 1) {
        asteroids[i].x = 0;
      }
    }
    if (asteroids[i].y == 255) {
      asteroids[i].y = H - 1;
    } else {
      if (asteroids[i].y > H - 1) {
        asteroids[i].y = 0;
      }
    }
    drawAsteroid(asteroids[i]);
  }
}

void moveShot(Shot *s) {
  switch (s->heading) {
    case 0:
      s->y -= SHOT_SPEED * 2;
      break;
    case 1:
      s->y -= SHOT_SPEED * 2;
      s->x += SHOT_SPEED;
      break;
    case 2: // 45 degrees
      s->y -= SHOT_SPEED * 2;
      s->x += SHOT_SPEED * 2;
      break;
    case 3:
      s->x += SHOT_SPEED * 2;
      s->y -= SHOT_SPEED;
      break;
    case 4:
      s->x += SHOT_SPEED * 2;
      break;
    case 5:
      s->x += SHOT_SPEED * 2;
      s->y += SHOT_SPEED;
      break;
    case 6:
      s->x += SHOT_SPEED * 2;
      s->y += SHOT_SPEED * 2;
      break;
    case 7:
      s->y += SHOT_SPEED * 2;
      s->x += SHOT_SPEED;
      break;
    case 8:
      s->y += SHOT_SPEED * 2;
      break;
    case 9:
      s->y += SHOT_SPEED * 2;
      s->x -= SHOT_SPEED;
      break;
    case 10:
      s->x -= SHOT_SPEED * 2;
      s->y += SHOT_SPEED * 2;
      break;
    case 11:
      s->x -= SHOT_SPEED * 2;
      s->y += SHOT_SPEED;
      break;
    case 12:
      s->x -= SHOT_SPEED * 2;
      break;
    case 13:
      s->x -= SHOT_SPEED * 2;
      s->y -= SHOT_SPEED;
      break;
    case 14:
      s->x -= SHOT_SPEED * 2;
      s->y -= SHOT_SPEED * 2;
      break;
    case 15:
      s->y -= SHOT_SPEED * 2;
      s->x -= SHOT_SPEED;
      break;
  }
  if (s->x > 250) {
    s->x = W - 1;
  } else {
    if (s->x > W - 1) {
      s->x = 0;
    }
  }
  if (s->y > 250) {
    s->y = H - 1;
  } else {
    if (s->y > H - 1) {
      s->y = 0;
    }
  }
}

void moveShots() {
  for (int i = 0; i < MAX_SHOTS; i++) {
    if (shots[i].ttl == 0xFF) {
      // this is an unused Shot structure.
      continue;
    }

    if (shots[i].ttl < SHOT_TTL) {
      arduboy.drawPixel(shots[i].x, shots[i].y, BLACK);
      // display.drawPixel(shots[i].x, shots[i].y, 0);
    }

    if (shots[i].ttl == 0) {
      // time to live is 0.  time for this shot to disappear
      shots[i].ttl = 0xFF; // set to unused structure
      continue;
    }
    shots[i].ttl--;
    moveShot(&shots[i]);
    arduboy.drawPixel(shots[i].x, shots[i].y, WHITE);
    // display.drawPixel(shots[i].x, shots[i].y, 1);
  }

  if (saucerShot.ttl != 0xFF) {
    arduboy.drawPixel(saucerShot.x, saucerShot.y, BLACK);
    // display.drawPixel(saucerShot.x, saucerShot.y, 0);
    if (saucerShot.ttl == 0) {
      saucerShot.ttl = 0xFF;
      return;
    }
    saucerShot.ttl--;
    moveShot(&saucerShot);
    arduboy.drawPixel(saucerShot.x, saucerShot.y, WHITE);
    // display.drawPixel(saucerShot.x, saucerShot.y, 1);
  }
}

void heartbeat() {
  if (heartbeatFreq == 85) {
    heartbeatFreq = 95;
  } else {
    heartbeatFreq = 85;
  }
  // This is kinda anoying: playTone(heartbeatFreq, 60, 5);
  //nextHeartbeat = clock + 20;
  nextHeartbeat = max(clock + 10, (clock + 10 + (10 - clock / 100)));
}

void loop() {
  char d;
  boolean killed = false;

  if (!arduboy.nextFrame()) {
    return; // go back to the start of the loop
  }

  clock++;

  if (clock == nextHeartbeat) {
    heartbeat();
  }

  d = 40;
  for (byte i = 0; i < MAX_ASTEROIDS; i++) {
    if (asteroids[i].info == 0x80) {
      continue;
    }
    byte type = (asteroids[i].info >> 4) & 0x7;
    switch (type) {
      case 0:
      case 1:
      case 2:
      case 3:
        d -= 3;
        break;
      case 4:
      case 5:
        d -= 2;
        break;
      case 6:
      case 7:
        d -= 1;
        break;
    }
  }
  d *= speedAdjust;
  if (d > 0) {
    delay(d);
    // tv.delay(d);
  }

  /*
    if ((clock % 10) == 0) {
    debug(d);
    }
  */

  /*
    if ((clock % 10) == 0) {
    debug(getMemory());
    }
  */

  moveAsteroids();

  moveShots();

  getInput();

  moveShip();
  drawShip();

  drawSaucer();

  killed = detectCollisions();
  if (killed) {
    // We were killed.  Pause for a moment.
    for (byte i = 0; i < 30; i++) {
      clock++;
      if (clock % 2 == 0) {
        drawExplosions();
        drawSaucer();
        arduboy.display();
      }
      if (d > 0) {
        delay(d + 5);
        // tv.delay(d + 5);
      }
      moveAsteroids();
    }

    // reset the ship
    shipX = W / 2 - 2;
    shipY = H / 2 - 2;
    oldShipX = (byte)(shipX + 0.5);
    oldShipY = (byte)(shipY + 0.5);
    shipDX = 0.0;
    shipDY = 0.0;
    shipHeading = 0;
    oldShipHeading = 1;
    thrustX = 3;
    thrustY = 6;
    boolean tooCrowded = true;
    while (tooCrowded) {
      tooCrowded = false;
      for (byte i = 0; i < MAX_ASTEROIDS; i++) {
        if (asteroids[i].info == 0x80) {
          continue;
        }
        int dx = asteroids[i].x - oldShipX;
        int dy = asteroids[i].y - oldShipY;
        int distance = sqrt((dx * dx) + (dy * dy));
        //debug(distance);
        if (distance < 15) {
          tooCrowded = true;
          break;
        }
      }
      clock++;
      moveAsteroids();
      if (d > 0) {
        delay(d);
        // tv.delay(d);
      }
    }
    nextHeartbeat = clock + 1;
  }

  drawExplosions();

  displayScore();

  if (nAsteroids == 0) {
    for (byte k = 0; k < MAX_SHOTS; k++) {
      if (shots[k].ttl != 0xFF) {
        arduboy.drawPixel(shots[k].x, shots[k].y, BLACK);
        // display.drawPixel(shots[k].x, shots[k].y, 0);
        shots[k].ttl = 0xFF;
      }
    }
    if (saucerShot.ttl != 0xFF) {
      arduboy.drawPixel(saucerShot.x, saucerShot.y, BLACK);
      // display.drawPixel(saucerShot.x, saucerShot.y, 0);
      saucerShot.ttl = 0xFF;
    }

    for (byte i = 0; i < 10; i++) {
      drawExplosions();
      moveShip();
      drawShip();
      drawSaucer();
      if (d > 0) {
        delay(d + 10);
        // tv.delay(d + 10);
      }
    }
    newLevel();
  }

  if (remainingShips == 0) {
    gameOver();
    if (score > 0) {
      enterHighScore(1);
    }
    initGame(displayHighScores(1));
  }
  arduboy.display();
}

void incrementScore(int n) {
  unsigned int oldScore;
  oldScore = score;
  score += n;
  if ((score / 10000) > (oldScore / 10000)) {
    // we crossed a 10000 point threshold, so get a new ship
    remainingShips++;
  }
}

void displayScore() {
  byte i;
  sprintf(s, "%u", score);
  arduboy.setTextColor(WHITE);
  arduboy.setCursor(0,0);
  arduboy.print(s);
  // tv.print(0, 0, s);
  for (i = 0; i < remainingShips; i++) {
    overlaybitmap(i * 6, 7, ship_bitmaps, 0, 0, 0);
  }

}

void drawExplosions() {
  for (byte i = 0; i < MAX_EXPLOSIONS; i++) {
    if (explosions[i].index != 255) {
      if (explosions[i].index > 0) {
        erasebitmap(explosions[i].x, explosions[i].y, explosion_bitmaps + ((explosions[i].index - 1) * SIZEOF_EXPLOSION_BITMAP_RECORD), 0, 0, 0);
      }
      if (explosions[i].index > 4) {
        explosions[i].index = 255;
      } else {
        overlaybitmap(explosions[i].x, explosions[i].y, explosion_bitmaps + (explosions[i].index++ * SIZEOF_EXPLOSION_BITMAP_RECORD), 0, 0, 0);
      }
    }
  }

  if (shipExplosion.index != 255) {
    if ((clock % 4) == 0) {
      if (shipExplosion.index > 0) {
        erasebitmap(shipExplosion.x, shipExplosion.y, ship_explosion_bitmaps + ((shipExplosion.index - 1) * SIZEOF_EXPLOSION_BITMAP_RECORD), 0, 0, 0);
      }
      if (shipExplosion.index > 5) {
        shipExplosion.index = 255;
      } else {
        overlaybitmap(shipExplosion.x, shipExplosion.y, ship_explosion_bitmaps + (shipExplosion.index++ * SIZEOF_EXPLOSION_BITMAP_RECORD), 0, 0, 0);
      }
    }
  }
}


void drawSaucer() {
  if (((saucerX == 255) && (saucerY == 255)) && (random(0, 500) == 0)) {
    // time to start a new saucer
    saucerY = random(20, H - 20);
    saucerHeading = 0;
    if (random(0, 2) == 0) {
      saucerX = 0;
      saucerHeading = random(1, 4) * 2; // heading 2, 4, or 6
      endSaucerX = W - 10;
    } else {
      saucerX = W - 10;
      saucerHeading = 10 + (random(0, 3) * 2); // heading 10, 12, or 14
      endSaucerX = 0;
    }
  }

  if ((saucerX != 255) && (saucerY != 255)) {
    // saucer is active
    erasebitmap(saucerX, saucerY, saucer_bitmaps, 0, 0, 0);
    if (random(0, 100) < 3) {
      byte newHeading = saucerHeading + ((random(0, 2) - 1) * 2);
      if (((newHeading > 1) && (newHeading < 7)) || ((newHeading > 9) && (newHeading < 15))) {
        saucerHeading = newHeading;
      }
    }
    switch (saucerHeading) {
      case 0:
        saucerY--;
        break;
      case 1:
        saucerY--;
        if (clock % 2 > 0) {
          saucerX++;
        }
        break;
      case 2: // 45 degrees
        saucerY--;
        saucerX++;
        break;
      case 3:
        saucerX++;
        if (clock % 2 > 0) {
          saucerY--;
        }
        break;
      case 4:
        saucerX++;
        break;
      case 5:
        saucerX++;
        if (clock % 2 > 0) {
          saucerY++;
        }
        break;
      case 6:
        saucerX++;
        saucerY++;
        break;
      case 7:
        saucerY++;
        if (clock % 2 > 0) {
          saucerX++;
        }
        break;
      case 8:
        saucerY++;
        break;
      case 9:
        saucerY++;
        if (clock % 2 > 0) {
          saucerX--;
        }
        break;
      case 10:
        saucerX--;
        saucerY++;
        break;
      case 11:
        saucerX--;
        if (clock % 2 > 0) {
          saucerY++;
        }
        break;
      case 12:
        saucerX--;
        break;
      case 13:
        saucerX--;
        if (clock % 2 > 0) {
          saucerY--;
        }
        break;
      case 14:
        saucerX--;
        saucerY--;
        break;
      case 15:
        saucerY--;
        if (clock % 2 > 0) {
          saucerX--;
        }
        break;
    }
    if (saucerX == 255) {
      saucerX = W - 1;
    } else {
      if (saucerX > W - 1) {
        saucerX = 0;
      }
    }
    if (saucerY == 255) {
      saucerY = H - 1;
    } else {
      if (saucerY > H - 1) {
        saucerY = 0;
      }
    }

    overlaybitmap(saucerX, saucerY, saucer_bitmaps, 0, 0, 0);
    if (saucerX == endSaucerX) {
      erasebitmap(saucerX, saucerY, saucer_bitmaps, 0, 0, 0);
      saucerX = 255;
      saucerY = 255;
      return;
    }

    if (saucerShot.ttl == 0xFF) {
      // time to shoot!
      saucerShot.ttl = SHOT_TTL;
      saucerShot.heading = random(0, 16);
      saucerShot.x = saucerX + 3;
      saucerShot.y = saucerY + 3;
    }
  }
}

void moveShip() {
  if (hyperspaceCount > 0) {
    hyperspaceCount--;
    return;
  } else {
    if (hyperspaceCount == 0) {
      oldShipX = random(10, W - 10);
      oldShipY = random(10, H - 10);
      shipX = oldShipX;
      shipY = oldShipY;
      oldShipHeading = shipHeading + 1;
      hyperspaceCount = -1;
    }
  }
  shipX += shipDX;
  shipY += shipDY;
  if (shipDX > 0) {
    shipDX -= DRAG;
  } else {
    if (shipDX < 0) {
      shipDX += DRAG;
    }
  }
  if (shipDY > 0) {
    shipDY -= DRAG;
  } else {
    if (shipDY < 0) {
      shipDY += DRAG;
    }
  }
}

void drawShip() {
  if (hyperspaceCount >= 0) {
    return;
  }
  // If the ship has moved in some way
  if ((oldShipHeading != shipHeading) || (oldShipX != (byte)(shipX + 0.5)) || (oldShipY != (byte)(shipY + 0.5))) {
    erasebitmap(oldShipX, oldShipY, ship_bitmaps + (oldShipHeading * SIZEOF_SHIP_BITMAP_RECORD), 0, 0, 0);
    arduboy.drawPixel(oldShipX + thrustX, oldShipY + thrustY, BLACK);
    // display.drawPixel(oldShipX + thrustX, oldShipY + thrustY, 0);
    oldShipHeading = shipHeading;
    oldShipX = (byte)(shipX + 0.5);
    oldShipY = (byte)(shipY + 0.5);
    if ((shipDX > 0) && (oldShipX > W - 1)) {
      shipX = 0;
      oldShipX = 0;
    } else {
      if ((shipDX < 0) && ((oldShipX == 0) || (oldShipX > 200))) {
        shipX = W - 1;
        oldShipX = W - 1;
      }
    }
    if ((shipDY > 0) && (oldShipY > H - 1)) {
      shipY = 0;
      oldShipY = 0;
    } else {
      if ((shipDY < 0) && ((oldShipY == 0) || (oldShipY > 200))) {
        shipY = H - 1;
        oldShipY = H - 1;
      }
    }

    overlaybitmap(oldShipX, oldShipY, ship_bitmaps + (oldShipHeading * SIZEOF_SHIP_BITMAP_RECORD), 0, 0, 0);
    if (upPressed()) {
      arduboy.drawPixel(oldShipX + thrustX, oldShipY + thrustY, clock % 2);
      // display.drawPixel(oldShipX + thrustX, oldShipY + thrustY, clock % 2);
    }
  }
}

/*
   This is the point-in-polygon algorithm adapted from
   http://www.ecse.rpi.edu/Homepages/wrf/Research/Short_Notes/pnpoly.html
*/
boolean inPolygon(byte nvert, const uint8_t *xvert, const uint8_t *yvert, int x, int y) {
  char i, j;
  byte xvi, xvj, yvi, yvj;
  boolean inside = false;
  for (i = 0, j = nvert - 1; i < nvert; j = i++) {
    xvi = pgm_read_byte(xvert + i);
    xvj = pgm_read_byte(xvert + j);
    yvi = pgm_read_byte(yvert + i);
    yvj = pgm_read_byte(yvert + j);
    if ( ((yvi > y) != (yvj > y)) &&
         (x < (xvj - xvi) * (y - yvi) / (yvj - yvi) + xvi) )
      inside = !inside;
  }
  return inside;
}

boolean detectCollisions() {
  byte type;
  byte nverts;
  const uint8_t *xvert, *yvert;
  const uint8_t *shipxvert, *shipyvert;

  // For each asteroid
  for (byte i = 0; i < MAX_ASTEROIDS; i++) {
    if (asteroids[i].info == 0x80) {
      continue;
    }
    type = (asteroids[i].info >> 4) & 0x7;
    nverts = pgm_read_byte(asteroid_vertices + (type * SIZEOF_ASTEROID_VERTICES_RECORD));
    xvert = asteroid_vertices + (type * SIZEOF_ASTEROID_VERTICES_RECORD) + 1;
    yvert = asteroid_vertices + (type * SIZEOF_ASTEROID_VERTICES_RECORD) + 13;

    // Check collsions with ship
    shipxvert = ship_vertices + (shipHeading * SIZEOF_SHIP_VERTICES_RECORD);
    shipyvert = ship_vertices + (shipHeading * SIZEOF_SHIP_VERTICES_RECORD) + 3;

    for (byte j = 0; j < 3; j++) {
      // Check each of the 3 vertices on the ship
      int sx = oldShipX + pgm_read_byte(shipxvert + j);
      int sy = oldShipY + pgm_read_byte(shipyvert + j);
      if (sx < asteroids[i].x) {
        sx += W;
      }
      if (sy < asteroids[i].y) {
        sy += H;
      }
      if (inPolygon(nverts, xvert, yvert, sx - asteroids[i].x, sy - asteroids[i].y)) {
        // Hit by asteroid!
        die();
        for (byte k = 0; k < MAX_SHOTS; k++) {
          if (shots[k].ttl != 0xFF) {
            arduboy.drawPixel(shots[k].x, shots[k].y, BLACK);
            // display.drawPixel(shots[k].x, shots[k].y, 0);
            shots[k].ttl = 0xFF;
          }
        }
        if (saucerShot.ttl != 0xFF) {
          arduboy.drawPixel(saucerShot.x, saucerShot.y, BLACK);
          // display.drawPixel(saucerShot.x, saucerShot.y, 0);
          saucerShot.ttl = 0xFF;
        }
        displayScore();
        return true;
      }
    }

    // Check collisions with shots
    for (byte j = 0; j < MAX_SHOTS; j++) {
      if (shots[j].ttl == 0xFF) {
        // this is an unused Shot structure.
        continue;
      }
      if (inPolygon(nverts, xvert, yvert, shots[j].x - asteroids[i].x, shots[j].y - asteroids[i].y)) {
        // hit an asteroid!
        playExplosion();
        scored = true;
        eraseAsteroid(asteroids[i]);
        asteroids[i].info = 0x80;  // make the asteroid structure unused
        nAsteroids--;
        // erase the shot and disable it
        arduboy.drawPixel(shots[j].x, shots[j].y, BLACK);
        // display.drawPixel(shots[j].x, shots[j].y, 0);
        shots[j].ttl = 0xFF;

        if (explosions[explosionIndex].index != 255) {
          erasebitmap(explosions[explosionIndex].x, explosions[explosionIndex].y, explosion_bitmaps + ((explosions[explosionIndex].index - 1) * SIZEOF_EXPLOSION_BITMAP_RECORD), 0, 0, 0);
        }
        explosions[explosionIndex].x = asteroids[i].x;
        explosions[explosionIndex].y = asteroids[i].y;
        explosions[explosionIndex].index = 0;
        if (++explosionIndex == MAX_EXPLOSIONS) {
          explosionIndex = 0;
        }

        if (type < 6) {
          // The asteroids breaks into two smaller asteroids.
          byte newAsteroid1 = 255;
          byte newAsteroid2 = 255;
          byte newType1, newType2;
          byte newHeading1, newHeading2;
          for (byte k = 0; k < MAX_ASTEROIDS; k++) {
            // find an unused Asteroids structure
            if (asteroids[k].info == 0x80) {
              newAsteroid1 = k;
              break;
            }
          }
          if (newAsteroid1 != 255) {
            for (byte k = newAsteroid1 + 1; k < MAX_ASTEROIDS; k++) {
              // find an unused Asteroids structure
              if (asteroids[k].info == 0x80) {
                newAsteroid2 = k;
                break;
              }
            }
          }

          switch (type) {
            case 0:
            case 1:
            case 2:
            case 3:
              incrementScore(20);
              newType1 = random(4, 6);
              if (random(0, 100) < 33) {
                // one third of the medium asteroids are fast.
                newType1 |= 0x8;
              }
              newType2 = random(4, 6);
              if (random(0, 100) < 33) {
                // one third of the medium asteroids are fast.
                newType2 |= 0x8;
              }
              break;
            case 4:
            case 5:
              incrementScore(50);
              newType1 = random(6, 8);
              if (random(0, 100) < 67) {
                // two thirds of the medium asteroids are fast.
                newType1 |= 0x8;
              }
              newType2 = random(6, 8);
              if (random(0, 100) < 67) {
                // two thirds of the medium asteroids are fast.
                newType2 |= 0x8;
              }
              break;
          }
          if (newAsteroid1 != 255) {
            newHeading1 = random(0, 16);
            asteroids[newAsteroid1].info = (newType1 << 4) | newHeading1;
            asteroids[newAsteroid1].x = asteroids[i].x;
            asteroids[newAsteroid1].y = asteroids[i].y;
            nAsteroids++;
          }
          if (newAsteroid2 != 255) {
            newHeading2 = random(0, 16);
            while (newHeading2 == newHeading1) {
              newHeading2 = random(0, 16);
            }
            asteroids[newAsteroid2].info = (newType2 << 4) | newHeading2;
            asteroids[newAsteroid2].x = asteroids[i].x;
            asteroids[newAsteroid2].y = asteroids[i].y;
            nAsteroids++;
          }
        } else {
          incrementScore(100);
        }
        break;
      }
    } // for each shot
  } // for each asteroid

  // Detect saucer collisions
  if ((saucerX != 255) && (saucerY != 255)) {
    xvert = saucer_vertices;
    yvert = saucer_vertices + 6;
    for (byte j = 0; j < 3; j++) {
      // Check each of the 3 vertices on the ship
      int sx = oldShipX + pgm_read_byte(shipxvert + j);
      int sy = oldShipY + pgm_read_byte(shipyvert + j);
      if (sx < saucerX) {
        sx += W;
      }
      if (sy < saucerY) {
        sy += H;
      }
      if (inPolygon(6, xvert, yvert, sx - saucerX, sy - saucerY)) {
        // Hit by saucer!
        die();
        erasebitmap(saucerX, saucerY, saucer_bitmaps, 0, 0, 0);
        saucerX = 255;
        saucerY = 255;
        for (byte k = 0; k < MAX_SHOTS; k++) {
          if (shots[k].ttl != 0xFF) {
            arduboy.drawPixel(shots[k].x, shots[k].y, BLACK);
            // display.drawPixel(shots[k].x, shots[k].y, 0);
            shots[k].ttl = 0xFF;
          }
        }
        if (saucerShot.ttl != 0xFF) {
          arduboy.drawPixel(saucerShot.x, saucerShot.y, BLACK);
          // display.drawPixel(saucerShot.x, saucerShot.y, 0);
          saucerShot.ttl = 0xFF;
        }
        displayScore();
        return true;
      }
    } // for each ship vertex

    for (byte j = 0; j < MAX_SHOTS; j++) {
      if (shots[j].ttl == 0xFF) {
        // this is an unused Shot structure.
        continue;
      }
      if (inPolygon(6, xvert, yvert, shots[j].x - saucerX, shots[j].y - saucerY)) {
        // hit the saucer
        playExplosion();
        scored = true;
        incrementScore(500);
        erasebitmap(saucerX, saucerY, saucer_bitmaps, 0, 0, 0);
        arduboy.drawPixel(shots[j].x, shots[j].y, BLACK);
        // display.drawPixel(shots[j].x, shots[j].y, 0);
        shots[j].ttl = 0xFF;
        if (explosions[explosionIndex].index != 255) {
          erasebitmap(explosions[explosionIndex].x, explosions[explosionIndex].y, explosion_bitmaps + ((explosions[explosionIndex].index - 1) * SIZEOF_EXPLOSION_BITMAP_RECORD), 0, 0, 0);
        }
        explosions[explosionIndex].x = saucerX;
        explosions[explosionIndex].y = saucerY;
        explosions[explosionIndex].index = 0;
        if (++explosionIndex == MAX_EXPLOSIONS) {
          explosionIndex = 0;
        }
        saucerX = 255;
        saucerY = 255;
      }
    }
  }

  if (saucerShot.ttl != 0xFF) {
    shipxvert = ship_vertices + (shipHeading * SIZEOF_SHIP_VERTICES_RECORD);
    shipyvert = ship_vertices + (shipHeading * SIZEOF_SHIP_VERTICES_RECORD) + 3;
    if (inPolygon(3, shipxvert, shipyvert, saucerShot.x - oldShipX, saucerShot.y - oldShipY)) {
      // Hit by saucer shot!
      die();
      for (byte k = 0; k < MAX_SHOTS; k++) {
        if (shots[k].ttl != 0xFF) {
          arduboy.drawPixel(shots[k].x, shots[k].y, BLACK);
          // display.drawPixel(shots[k].x, shots[k].y, 0);
          shots[k].ttl = 0xFF;
        }
      }
      if (saucerShot.ttl != 0xFF) {
        arduboy.drawPixel(saucerShot.x, saucerShot.y, BLACK);
        // display.drawPixel(saucerShot.x, saucerShot.y, 0);
        saucerShot.ttl = 0xFF;
      }
      displayScore();
      return true;
    }
  }

  return false;
}

void die() {
  playExplosion();
  remainingShips--;
  erasebitmap(remainingShips * 6, 6, ship_bitmaps, 0, 0, 0);

  erasebitmap(oldShipX, oldShipY, ship_bitmaps + (oldShipHeading * SIZEOF_SHIP_BITMAP_RECORD), 0, 0, 0);

  arduboy.drawPixel(oldShipX + thrustX, oldShipY + thrustY, BLACK);
  // display.drawPixel(oldShipX + thrustX, oldShipY + thrustY, 0);

  shipExplosion.x = oldShipX - 4;
  if (shipExplosion.x > 250) {
    shipExplosion.x = W - 1;
  }
  shipExplosion.y = oldShipY - 4;
  if (shipExplosion.y > 250) {
    shipExplosion.x = H - 1;
  }
  shipExplosion.index = 0;
}

boolean titleScreen() {
  playTitleTune();
  arduboy.fillScreen(BLACK);
  // tv.fill(0);

  overlaybitmap(0, 10, asteroid_bitmaps + (0 * SIZEOF_ASTEROID_BITMAP_RECORD), 0, 0, 0);
  overlaybitmap(35, 0, asteroid_bitmaps + (1 * SIZEOF_ASTEROID_BITMAP_RECORD), 0, 0, 0);
  overlaybitmap(10, 76, asteroid_bitmaps + (2 * SIZEOF_ASTEROID_BITMAP_RECORD), 0, 0, 0);
  overlaybitmap(100, 80, asteroid_bitmaps + (4 * SIZEOF_ASTEROID_BITMAP_RECORD), 0, 0, 0);
  overlaybitmap(80, 8, asteroid_bitmaps + (5 * SIZEOF_ASTEROID_BITMAP_RECORD), 0, 0, 0);
  overlaybitmap(50, 70, asteroid_bitmaps + (6 * SIZEOF_ASTEROID_BITMAP_RECORD), 0, 0, 0);
  overlaybitmap(110, 50, asteroid_bitmaps + (7 * SIZEOF_ASTEROID_BITMAP_RECORD), 0, 0, 0);
  overlaybitmap(52, 24, ship_bitmaps + (2 * SIZEOF_SHIP_BITMAP_RECORD), 0, 0, 0);
  arduboy.drawPixel(63, 19, WHITE);
  // display.drawPixel(63, 19, 1);
  overlaybitmap(70, 0, explosion_bitmaps + (0 * SIZEOF_EXPLOSION_BITMAP_RECORD), 0, 0, 0);
  overlaybitmap(70, 0, explosion_bitmaps + (3 * SIZEOF_EXPLOSION_BITMAP_RECORD), 0, 0, 0);

  overlaybitmap(0, 20, title_bitmap, 0, 0, 0);

  arduboy.display();

  if (pollFireButton(100)) {
    return true;
  }
  
  overlaybitmap(70, 66, hackvision_logo_bitmap, 0, 0, 0);

  arduboy.display();

  if (pollFireButton(400)) {
    return true;
  }
  return false;
}


void newLevel() {
  delay(500);
  //tv.delay(500);

  arduboy.fillScreen(BLACK);
  // tv.fill(0);
  level++;
  initVars();
  drawShip();
}

void gameOver() {
  playGameOver();
  delay(500);
  //tv.delay(500);

  arduboy.fillScreen(BLACK);
  //tv.fill(0);
  //tv.select_font(font6x8);
  // strcpy_P(s, (char *)pgm_read_word(&(strings[2])));

  arduboy.setTextColor(WHITE);
  arduboy.setCursor(35, 40);
  arduboy.print("GAME OVER");

  sprintf(s, "%u", score);
  arduboy.setCursor(40,20);
  arduboy.print(s);

  arduboy.display();

  pollFireButton(100000);
  //tv.delay(2000);
}

void enterInitials() {
  int index = 0;

  // Lower the frame rate otherwise the intials input is too fast
  arduboy.setFrameRate(MENU_FRAME_RATE);

  arduboy.fillScreen(BLACK);
  // tv.fill(0);

  // strcpy_P(s, (char *)pgm_read_word(&(strings[1])));
  // s[10] = '\0'; // hack: truncate the final 'S' off of the string "HIGH SCORES"

  arduboy.setTextColor(WHITE);
  arduboy.setCursor(16, 0);
  arduboy.print("HIGH SCORE");
  // tv.print(16, 0, s);
  sprintf(s, "%u", score);
  arduboy.setCursor(88, 0);
  arduboy.print(s);
  // tv.print(88, 0, s);

  initials[0] = ' ';
  initials[1] = ' ';
  initials[2] = ' ';

  delay(300);

  while (true) {
    if (!arduboy.nextFrame()) {
      continue; // go back to the start of the loop
    }

    arduboy.setCursor(56, 20);
    arduboy.print(initials[0]);
    // tv.print_char(56, 20, initials[0]);
    arduboy.setCursor(64, 20);
    arduboy.print(initials[1]);
    // tv.print_char(64, 20, initials[1]);
    arduboy.setCursor(72, 20);
    arduboy.print(initials[2]);
    // tv.print_char(72, 20, initials[2]);
    for (byte i = 0; i < 3; i++) {
      arduboy.drawLine(56 + (i * 8), 27, 56 + (i * 8) + 6, 27, 1);
      //tv.draw_line(56 + (i * 8), 27, 56 + (i * 8) + 6, 27, 1);
    }
    arduboy.drawLine(56, 28, 88, 28, 0);
    // tv.draw_line(56, 28, 88, 28, 0);
    arduboy.drawLine(56 + (index * 8), 28, 56 + (index * 8) + 6, 28, 1);
    // tv.draw_line(56 + (index * 8), 28, 56 + (index * 8) + 6, 28, 1);
    
    delay(50);
    // tv.delay(150);
    if (leftPressed()) {
      index--;
      if (index < 0) {
        index = 0;
      } else {
        playTone(1046, 20);
      }
    }
    if (rightPressed()) {
      index++;
      if (index > 2) {
        index = 2;
      } else {
        playTone(1046, 20);
      }
    }
    if (upPressed()) {
      initials[index]++;
      playTone(523, 20);
      // A-Z 0-9 :-? !-/ ' '
      if (initials[index] == '0') {
        initials[index] = ' ';
      }
      if (initials[index] == '!') {
        initials[index] = 'A';
      }
      if (initials[index] == '[') {
        initials[index] = '0';
      }
      if (initials[index] == '@') {
        initials[index] = '!';
      }
    }
    if (downPressed()) {
      initials[index]--;
      playTone(523, 20);
      if (initials[index] == ' ') {
        initials[index] = '?';
      }
      if (initials[index] == '/') {
        initials[index] = 'Z';
      }
      if (initials[index] == 31) {
        initials[index] = '/';
      }
      if (initials[index] == '@') {
        initials[index] = ' ';
      }
    }
    if (firePressed()) {
      if (index < 2) {
        index++;
        playTone(1046, 20);
      } else {
        playTone(1046, 20);

        // Go back to standard frame rate
        arduboy.setFrameRate(BASE_FRAME_RATE);
        return;
      }
    }
    arduboy.display();
  }
}

void enterHighScore(byte file) {

  // Each block of EEPROM has 10 high scores, and each high score entry
  // is 5 bytes long:  3 bytes for initials and two bytes for score.
  int address = 0;
  uint8_t hi, lo;
  char tmpInitials[3];
  unsigned int tmpScore = 0;

  mem.load();

  // High score processing
  for (byte i = 0; i < NBR_HIGH_SCORES; i++) {
    hi = mem.read(address + (5 * i));
    lo = mem.read(address + (5 * i) + 1);
    if ((hi == 0xFF) && (lo == 0xFF)) {
      // The values are uninitialized, so treat this entry
      // as a score of 0.
      tmpScore = 0;
    } else {
      tmpScore = (hi << 8) | lo;
    }
    if (score > tmpScore) {
      enterInitials();
      for (byte j = i; j < 10; j++) {
        hi = mem.read(address + (5 * j));
        lo = mem.read(address + (5 * j) + 1);
        if ((hi == 0xFF) && (lo == 0xFF)) {
          tmpScore = 0;
        } else {
          tmpScore = (hi << 8) | lo;
        }
        tmpInitials[0] = (char)mem.read(address + (5 * j) + 2);
        tmpInitials[1] = (char)mem.read(address + (5 * j) + 3);
        tmpInitials[2] = (char)mem.read(address + (5 * j) + 4);

        // tmpScore and tmpInitials now hold what we want to write in the next slot.

        // write score and initials to current slot
        mem.write(address + (5 * j), ((score >> 8) & 0xFF));
        mem.write(address + (5 * j) + 1, (score & 0xFF));
        mem.write(address + (5 * j) + 2, initials[0]);
        mem.write(address + (5 * j) + 3, initials[1]);
        mem.write(address + (5 * j) + 4, initials[2]);
        score = tmpScore;
        initials[0] = tmpInitials[0];
        initials[1] = tmpInitials[1];
        initials[2] = tmpInitials[2];
      }
      score = 0;
      initials[0] = ' ';
      initials[1] = ' ';
      initials[2] = ' ';
      mem.save();
      return;
    }
  }
}


boolean displayHighScores(byte file) {
  
  byte y = 8;
  byte x = 24;
  // Each block of EEPROM has 10 high scores, and each high score entry
  // is 5 bytes long:  3 bytes for initials and two bytes for score.
  int address = 0;
  byte hi, lo;
  arduboy.fillScreen(BLACK);
  //tv.fill(0);
  //tv.select_font(font6x8);
  //strcpy_P(s, (char *)pgm_read_word(&(strings[1])));
  arduboy.setTextColor(WHITE);
  arduboy.setCursor(32,0);
  arduboy.print("HIGH SCORES");
  // tv.print(32, 0, s);

  mem.load();
  
  for (int i = 0; i < NBR_HIGH_SCORES; i++) {
    sprintf(s, "%2d", i + 1);

    arduboy.setCursor(x, y + (i * 8));
    arduboy.print(s);
    //tv.print(x, y + (i * 8), s);

    hi = mem.read(address + (5 * i));
    lo = mem.read(address + (5 * i) + 1);
    if ((hi == 0xFF) && (lo == 0xFF)) {
      score = 0;
    } else {
      score = (hi << 8) | lo;
    }
    initials[0] = (char)mem.read(address + (5 * i) + 2);
    initials[1] = (char)mem.read(address + (5 * i) + 3);
    initials[2] = (char)mem.read(address + (5 * i) + 4);

    if (score > 0) {
      sprintf(s, "%c%c%c %u", initials[0], initials[1], initials[2], score);
      arduboy.setCursor(x + 24, y + (i * 8));
      arduboy.print(s);
      // tv.print(x + 24, y + (i * 8), s);
    }
  }

  arduboy.display();

  delay(300);

  if (pollFireButton(300)) {
    return true;
  }
  return false;
}


boolean pollFireButton(int n) {
  for (int i = 0; i < n; i++) {
    //tv.delay(15);
    delay(15);
    if (firePressed()) {
      return true;
    }
  }
  return false;
}

void initGame(boolean start) {
  arduboy.fillScreen(BLACK);
  // tv.fill(0);

  while (!start) {
    start = titleScreen();
    if (!start) {
      start = displayHighScores(1);
    }
  }

  arduboy.fillScreen(BLACK);
  // tv.fill(0);

  delay(100);
  // tv.delay(100);

  level = 1;
  remainingShips = 3;
  score = 0;
  initVars();
  drawShip();
}



void debug(int m) {
  sprintf(s, "       ");
  arduboy.setCursor(64, FOOTER_Y);
  arduboy.print(s);
  // tv.print(64, FOOTER_Y, s);

  sprintf(s, "%d", m);
  arduboy.setCursor(64, FOOTER_Y);
  arduboy.print(s);
  // tv.print(64, FOOTER_Y, s);
}

void debug(char *ss) {
  for (byte y = FOOTER_Y; y < H; y++) {
    arduboy.drawLine(48, y, 90, y, 0);
    // tv.draw_line(48, y, 90, y, 0);
  }
  arduboy.setTextSize(1);
  arduboy.setTextColor(WHITE);
  arduboy.setTextColor(BLACK);
  arduboy.setCursor(64, FOOTER_Y);
  arduboy.print(ss);

  // tv.print(64, FOOTER_Y, ss);
}




int getMemory() {
  int size = 512;
  byte *buf;
  while ((buf = (byte *) malloc(--size)) == NULL);
  free(buf);
  return size;
}

boolean getInput() {
  boolean input = false;

  if (firePressed()) {
    if (!fired) {
      fired = true;
      input = true;
      freq = F2;
//      playTone(freq, 100, 9);
      playFire();

      byte ttl;
      byte oldestShot = 0;
      byte smallestTTL = 255;
      byte shot = 255;
      // find a shot structure to use.  Either find an unused one or use the oldest one.
      for (byte i = 0; i < MAX_SHOTS; i++) {
        if (shots[i].ttl == 0xFF) {
          shot = i;
          break;
        }
      }
      if (shot != 255) {
        shots[shot].ttl = SHOT_TTL;
        shots[shot].heading = shipHeading;
        shots[shot].x = oldShipX + 3;
        shots[shot].y = oldShipY + 3;
      }
    } else {
      // uncomment for "autofire"
      //fired = false;
    }
  } else {
    fired = false;
  }

  if (leftPressed()) {
    shipHeading--;
    if (shipHeading == 255) {
      shipHeading = 15;
    }
    input = true;
  } else {
    if (rightPressed()) {
      shipHeading++;
      if (shipHeading > 15) {
        shipHeading = 0;
      }
      input = true;
    }
  }


  if (upPressed()) {
    arduboy.drawPixel(oldShipX + thrustX, oldShipY + thrustY, BLACK);
    // display.drawPixel(oldShipX + thrustX, oldShipY + thrustY, 0);
    switch (shipHeading) {
      case 0: // straight up
        shipDY -= THRUST;
        thrustX = 3;
        thrustY = 6;
        break;
      case 1: // 22.5 degrees
        shipDX += THRUST * SIN22_5;
        shipDY -= THRUST * SIN67_5;
        thrustX = 2;
        thrustY = 6;
        break;
      case 2: // 45 degrees
        shipDX += THRUST * SIN45;
        shipDY -= THRUST * SIN45;
        thrustX = 1;
        thrustY = 5;
        break;
      case 3:
        shipDX += THRUST * SIN67_5;
        shipDY -= THRUST * SIN22_5;
        thrustX = 1;
        thrustY = 3;
        break;
      case 4:
        shipDX += THRUST;
        thrustX = 0;
        thrustY = 3;
        break;
      case 5:
        shipDX += THRUST * SIN67_5;
        shipDY += THRUST * SIN22_5;
        thrustX = 1;
        thrustY = 2;
        break;
      case 6:
        shipDX += THRUST * SIN45;
        shipDY += THRUST * SIN45;
        thrustX = 1;
        thrustY = 1;
        break;
      case 7:
        shipDX += THRUST * SIN22_5;
        shipDY += THRUST * SIN67_5;
        thrustX = 2;
        thrustY = 1;
        break;
      case 8:
        shipDY += THRUST;
        thrustX = 3;
        thrustY = 0;
        break;
      case 9:
        shipDX -= THRUST * SIN22_5;
        shipDY += THRUST * SIN67_5;
        thrustX = 3;
        thrustY = 1;
        break;
      case 10:
        shipDX -= THRUST * SIN45;
        shipDY += THRUST * SIN45;
        thrustX = 5;
        thrustY = 1;
        break;
      case 11:
        shipDX -= THRUST * SIN67_5;
        shipDY += THRUST * SIN22_5;
        thrustX = 6;
        thrustY = 2;
        break;
      case 12:
        shipDX -= THRUST;
        thrustX = 6;
        thrustY = 3;
        break;
      case 13:
        shipDX -= THRUST * SIN67_5;
        shipDY -= THRUST * SIN22_5;
        thrustX = 6;
        thrustY = 3;
        break;
      case 14:
        shipDX -= THRUST * SIN45;
        shipDY -= THRUST * SIN45;
        thrustX = 5;
        thrustY = 5;
        break;
      case 15:
        shipDX -= THRUST * SIN22_5;
        shipDY -= THRUST * SIN67_5;
        thrustX = 3;
        thrustY = 6;
        break;
    }
    shipDX = min(shipDX, 10.0);
    shipDY = min(shipDY, 10.0);
    input = true;
  } else {
    if ((downPressed()) && (hyperspaceCount == -1)) {
      // Hyperspace!
      // or as my son Paul calls it, "Galaxy Warp"
      erasebitmap(oldShipX, oldShipY, ship_bitmaps + (oldShipHeading * SIZEOF_SHIP_BITMAP_RECORD), 0, 0, 0);
      hyperspaceCount = 10;
      input = true;
    }
  }

  return input;
}

void playTone(unsigned int frequency, unsigned long duration_ms) {
  // Default is to play tone with highest priority.
  playTone(frequency, duration_ms, 9);
}

void playTone(unsigned int frequency, unsigned long duration_ms, byte priority) {
  // priority is value 0-9, 9 being highest priority
  if (audio.playing() > 0) {
    // If a tone is currently playing, check priority
    if (priority < currentTonePriority) {
      return;
    }
  }
  currentTonePriority = priority;
  audio.tone(frequency, duration_ms);
  //  tv.tone(frequency, duration_ms);
}

void playTones(const uint16_t *tones, byte priority)
{
  // priority is value 0-9, 9 being highest priority
  if (audio.playing() > 0) {
    // If a tone is currently playing, check priority
    if (priority < currentTonePriority) {
      return;
    }
  }
  currentTonePriority = priority;
  audio.tones(tones);
}

void playExplosion(void)
{
#define EXPLOSION_TONES_COUNT 8
  // Explosion tones are replaced with random values for each explosion
  static uint16_t explosionTones[EXPLOSION_TONES_COUNT + 1] = {0, 0, 0, 0,
                                                               0, 0, 0, 0,
                                                               TONES_END};

  for (int t = 0; t < EXPLOSION_TONES_COUNT / 2; t++) {
     explosionTones[t * 2] = random(25, 70);
     explosionTones[t * 2 + 1] = random(50, 100);
  }
  playTones(explosionTones, 9);
  sound = EXPLOSION;
}

const uint16_t fireTones[] = {400, 20,
                              350, 20,
                              300, 20,
                              250, 20,
                              150, 20,
                              100, 20,
                              TONES_END};

void playFire(void)
{
  playTones(fireTones, 9);
  sound = FIRE;  
}

const uint16_t gameOverTones[] = {NOTE_B2,   300,
                                  NOTE_C3,   150,
                                  NOTE_REST, 150,
                                  NOTE_G2,   150,
                                  NOTE_FS2,  150,
                                  NOTE_F2,   150,
                                  NOTE_REST, 300,
                                  NOTE_G2,   150,
                                  NOTE_REST, 150,
                                  NOTE_F2,   150,
                                  NOTE_G2,   150,
                                  NOTE_F2,   150,
                                  NOTE_REST, 150,
                                  NOTE_E2,   600,
                                 TONES_END};

void playGameOver(void)
{
  playTones(gameOverTones, 9);  
}


const uint16_t titleTones[] = {NOTE_B4,   150,
                               NOTE_REST, 150,
                               NOTE_B4,   150, 
                               NOTE_REST, 150,
                               NOTE_C5,   150, 
                               NOTE_REST, 150,
                               NOTE_B4,   150, 
                               NOTE_REST, 30,
                               NOTE_E5,   150, 
                               NOTE_REST, 150,
                               NOTE_B4,   150, 
                               NOTE_C5,   150, 
                               NOTE_B4,   150, 
                               NOTE_C5,   150, 


                               NOTE_A4,   150,
                               NOTE_REST, 150,
                               NOTE_A4,   150, 
                               NOTE_REST, 150,
                               NOTE_B4,   150, 
                               NOTE_REST, 150,
                               NOTE_A4,   150, 
                               NOTE_REST, 30,
                               NOTE_D4,   150, 
                               NOTE_REST, 150,
                               NOTE_A4,   150, 
                               NOTE_B4,   150, 
                               NOTE_A4,   150, 
                               NOTE_B4,   150, 
                               TONES_REPEAT};

void playTitleTune(void)
{
  playTones(titleTones, 9);
}

void overlaybitmap(uint8_t x, uint8_t y, const unsigned char * bmp,
                   uint16_t i, uint8_t width, uint8_t lines) {

  uint8_t temp, lshift, rshift, save, xtra;
  uint16_t si = 0;

  rshift = x & 7;
  lshift = 8 - rshift;
  if (width == 0) {
    width = pgm_read_byte((uint32_t)(bmp) + i);
    i++;
  }
  if (lines == 0) {
    lines = pgm_read_byte((uint32_t)(bmp) + i);
    i++;
  }

//  if (width & 7) {
//    xtra = width & 7;
//    width = width / 8;
//    width++;
//  }
//  else {
//    xtra = 8;
//    width = width / 8;
//  }

  uint8_t screen_h = HEIGHT;
  uint8_t screen_w = WIDTH;
  uint8_t byte_cnt;
  uint8_t bitmap = 0;

  for (uint8_t b = 0 ; b < lines; b++) {
    byte_cnt = 9;
    for (uint8_t a = 0; a < width; a++) {
      if (byte_cnt == 9) {
        bitmap = pgm_read_byte((uint32_t)(bmp) + i);
        i++;
        byte_cnt = 1;
      }
      if ((bitmap & (1 << (8 - byte_cnt))) != 0) {
        arduboy.drawPixel((x + a) % screen_w, (y + b) % screen_h, WHITE);
      }
      byte_cnt++;
    }
  }

//  for (uint8_t l = 0; l < lines; l++) {
//    si = ((y + l) % display.height()) * display.width() + x / 8;
//    //si = (y + l)*display.width() + x/8;
//
//    if (width == 1)
//      temp = 0xff >> rshift + xtra;
//    else
//      temp = 0;
//    save = display.screen[si];
//    temp = pgm_read_byte((uint32_t)(bmp) + i++);
//    display.screen[si++] |= temp >> rshift;
//    for ( uint16_t b = i + width - 1; i < b; i++) {
//      if (si % display.hres == 0) {
//        // wrapped around to the left side
//        si -= display.hres;
//      }
//      save = display.screen[si];
//      display.screen[si] |= temp << lshift;
//      temp = pgm_read_byte((uint32_t)(bmp) + i);
//      display.screen[si++] |= temp >> rshift;
//    }
//    if (si % display.hres == 0) {
//      // wrapped around to the left side
//      si -= display.hres;
//    }
//    if (rshift + xtra < 8)
//      display.screen[si - 1] |= (save & (0xff >> rshift + xtra));	//test me!!!
//    display.screen[si] |= temp << lshift;
//  }
} // end of bitmap

void erasebitmap(uint8_t x, uint8_t y, const unsigned char * bmp,
                 uint16_t i, uint8_t width, uint8_t lines) {

  uint8_t temp, lshift, rshift, save, xtra;
  uint16_t si = 0;

  rshift = x & 7;
  lshift = 8 - rshift;
  if (width == 0) {
    width = pgm_read_byte((uint32_t)(bmp) + i);
    i++;
  }
  if (lines == 0) {
    lines = pgm_read_byte((uint32_t)(bmp) + i);
    i++;
  }

//  if (width & 7) {
//    xtra = width & 7;
//    width = width / 8;
//    width++;
//  }
//  else {
//    xtra = 8;
//    width = width / 8;
//  }

  uint8_t screen_h = HEIGHT;
  uint8_t screen_w = WIDTH;
  uint8_t byte_cnt;
  uint8_t bitmap = 0;

  for (uint8_t b = 0 ; b < lines; b++) {
    byte_cnt = 9;
    for (uint8_t a = 0; a < width; a++) {
      if (byte_cnt == 9) {
        bitmap = pgm_read_byte((uint32_t)(bmp) + i);
        i++;
        byte_cnt = 1;
      }
      if ((bitmap & (1 << (8 - byte_cnt))) != 0) {
        arduboy.drawPixel((x + a) % screen_w, (y + b) % screen_h, BLACK);
      }
      byte_cnt++;
    }
  }

//  for (uint8_t l = 0; l < lines; l++) {
//    si = ((y + l) % display.height()) * display.width() + x / 8;
//    //si = (y + l)*display.width() + x/8;
//    if (width == 1)
//      temp = 0xff >> rshift + xtra;
//    else
//      temp = 0;
//    save = display.screen[si];
//    temp = pgm_read_byte((uint32_t)(bmp) + i++);
//    display.screen[si++] &= ~(temp >> rshift);
//    for ( uint16_t b = i + width - 1; i < b; i++) {
//      if (si % display.hres == 0) {
//        // wrapped around to the left side
//        si -= display.hres;
//      }
//      save = display.screen[si];
//      display.screen[si] &= ~(temp << lshift);
//      temp = pgm_read_byte((uint32_t)(bmp) + i);
//      display.screen[si++] &= ~(temp >> rshift);
//    }
//    if (si % display.hres == 0) {
//      // wrapped around to the left side
//      si -= display.hres;
//    }
//
//    if (rshift + xtra < 8)
//      display.screen[si - 1] &= ~(save & (0xff >> rshift + xtra));	//test me!!!
//    if (rshift + xtra - 8 > 0)
//      display.screen[si] &= ~(temp << lshift);
//  }
} // end of bitmap


//void soundISR() {
//  if (TCCR2B != 0) {
//    if (sound == FIRE) {
//      freq = freq - 100;
//      setPWMFreq(freq);
//    } else {
//      if (sound == EXPLOSION) {
//        explosionFreqIndex++;
//        if (explosionFreqIndex >= 100) {
//          explosionFreqIndex = 0;
//        }
//        freq = pgm_read_byte(explosionFreq + explosionFreqIndex);
//        OCR2A = (LOW_FREQ_OCR / freq) - 1;
//        TCCR2B = 0b111;
//      }
//    }
//  } else {
//    sound = 0;
//  }
//}

//void setPWMFreq(unsigned int f) {
//  uint32_t ocr;
//  uint8_t prescalarbits;
//
//  ocr = F_CPU / f / 2 / 64 - 1;
//  prescalarbits = 0b100;
//  if (ocr > 255) {
//    ocr = F_CPU / f / 2 / 128 - 1;
//    prescalarbits = 0b101;
//
//    if (ocr > 255) {
//      ocr = F_CPU / f / 2 / 256 - 1;
//      prescalarbits = 0b110;
//
//      if (ocr > 255) {
//        ocr = F_CPU / f / 2 / 1024 - 1;
//        prescalarbits = 0b111;
//      }
//    }
//  }
//  TCCR2B = prescalarbits;
//  OCR2A = ocr;
//}
