#include <SoftwareSerial.h>

#include <Gamer.h>

Gamer gamer;

/**
 * Controls
 * 
 * Direction Mode
 *
 * Left/Right/Up/Down as you would expect.
 * Start button toggles edit mode.
 *
 * Edit Mode
 *
 * Up = Flag mine.
 * Down = Dig.
 * Right = Reveal number of adjacent mines under cursor.
 * Left = Blink all neighbour counts. hehe. E.g. square with 7 neighbours will blink 7 times.  This cycles and is a toggle.
 *
 * With lightMode set to true you can cover the LDR with your finger to switch on editMode, play is quicker this way if ambient light is high enough.
 *
 * Win, correctly flag all mines.
 * Completing increases no. of mines by one up to and above threshold in random to max of 64.  Get this far and you can play forever!
 *
 */
#define DEBUG 1

// Preferences.
bool lightMode = false; // Use light sensor to enable dig/flag, cover sensor then up/down to dig/flag. Much faster in a well lit room.
int lightThreshold = 370; // Above this setting light sensor is deemed to be covered. 

/**
 *
 * TODO
 *
 * - Calibrate LDR to set threshold?
 * - Wright some endgame progression code.
 */

int currentX = 0; // Cursor position.
int currentY = 0; // Cursor position.
int btick = 0; // ticker for blink reveal.
int ctick = -1; // ticker for cursor animation.
int tick = 0; // ticker for flag animation, 0,1,2,3,0,1,2,3 etc.
bool reveal = false;
bool boom = false; // Are we ending the game with a bang at the moment?
bool tomorrow = false; // No boom today, boom tomorrow. (e.g. we won, this time)
int boomCycle = 0; // How far into ending animation are we?
bool editMode = false; // 0, normal, 1, edit.  Toggled by start.
int numMines = random(18,24); // How much boom do we need?
int origMap[8][8]; // The original generated map.
int mineMap[8][8]; // The one the player has futzed with. 0 mud, 1, mudmine, 2, flag, 3 clear
int neighbourMap[8][8]; // A lookup table of adjacent mine counts per square.
int blinkMap[8][8]; // hehehehe
byte numbers[10][8]; // Array containing full width 0-9
byte n1[8],n2[8],n3[8],n4[8],n5[8],n6[8],n7[8],n8[8],n9[8],n0[8];

void setup() {
  Serial.begin(9600);
  setupScore();
  setupStuff();
  gamer.begin();
}

void setupStuff() {
  boom = false;
  tomorrow = false;
  boomCycle = 0;
  editMode = false;
  reveal = false;
  currentX = 0;
  currentY = 0;
  int minesPlaced = 0;
  for (int x=0; x<8; x++) {
    for (int y=0; y<8; y++) {
      origMap[x][y] = 0;
      mineMap[x][y] = 0;
      blinkMap[x][y] = 0;
    } 
  }
  while(minesPlaced < numMines) {
    int x = random(8);
    int y = random(8);
    if (mineMap[x][y] == 0) {
      origMap[x][y] = 1;
      mineMap[x][y] = 1;
      minesPlaced++;
    }
  }

#ifdef DEBUG
  // We're printing across X, then down!
  for(int y=0;y<8;y++) {
    for(int x=0;x<8;x++) {
      Serial.print(mineMap[x][y]);
    }
    Serial.println("");
  }
#endif

  for(int x=0;x<8;x++) {
    for(int y=0;y<8;y++) {
      int rightVal = origMap[(x == 7) ? 0 : x+1][y]; // Check right.
      int leftVal = origMap[(x == 0) ? 7 : x-1][y]; // Check left.
      int downVal = origMap[x][(y == 7) ? 0 : y+1]; // Check down.
      int upVal = origMap[x][(y == 0) ? 7 : y-1]; // Che up.
      int uprightVal = origMap[(x == 7) ? 0 : x+1][(y == 0) ? 7 : y-1];
      int upleftVal = origMap[(x == 0) ? 7 : x-1][(y == 0) ? 7 : y-1];
      int downrightVal = origMap[(x == 7) ? 0 : x+1][(y == 7) ? 0 : y+1];
      int downleftVal = origMap[(x == 0) ? 7 : x-1][(y == 7) ? 0 : y+1];
      neighbourMap[x][y] = rightVal + leftVal + downVal + upVal + uprightVal + upleftVal + downrightVal + downleftVal;      
    } 
  }
}

void spiderOut(int x, int y) {
  // Clear all nearby squares that are also 'empty'.
  if(mineMap[x][y] == 0) {
    mineMap[x][y] = 3;
    spiderOut( (x == 7) ? 0 : x+1, y); // Move right.
    spiderOut( (x == 0) ? 7 : x-1, y); // Move left.
    spiderOut( x, (y == 7) ? 0 : y+1); // Move down.
    spiderOut( x, (y == 0) ? 7 : y-1); // Move up.
  }
  return;
}

void digSquare(int x, int y) {
  if (mineMap[x][y] == 0) {
    spiderOut(x,y);
  } else if (mineMap[x][y] == 1) {
    boom = true;
  }
}

void loop() {
  tick = (tick == 3) ? 0 : tick+1;
  if(lightMode == false && gamer.isPressed(START)) {
    editMode = !editMode;
    const char *message = (editMode) ? "Edit Mode" : "Move Mode";
    Serial.println(message);
  } else if (lightMode == true) {
    if (gamer.ldrValue() >= lightThreshold) {
      Serial.println("Edit Mode");
      editMode = true; 
    } else {
      Serial.println("Move Mode");
      editMode = false;
    }
  }
  
  if (!editMode) { // Move cursor
    if (gamer.isPressed(UP)) {
      Serial.println("Up");
      currentY = (currentY == 0) ? 7 : currentY-1;
    }
    if (gamer.isPressed(RIGHT)) {
      Serial.println("Right");
      currentX = (currentX == 7) ? 0 : currentX+1;
    }
    if (gamer.isPressed(DOWN)) {
      Serial.println("Down");
      currentY = (currentY == 7) ? 0 : currentY+1;      
    }
    if (gamer.isPressed(LEFT)) {
      Serial.println("Left");
      currentX = (currentX == 0) ? 7 : currentX-1;
    }
  } else { // Perform actions.
    // Down is dig!
    if (gamer.isPressed(DOWN)) {
      Serial.print("Digging ");
      Serial.print(currentX);
      Serial.print(" ");
      Serial.print(currentY);
      Serial.println("");
      digSquare(currentX,currentY);
    }
    // Up is flag!
    if (gamer.isPressed(UP)) { // Toggle!
      if(mineMap[currentX][currentY] == 0 || mineMap[currentX][currentY] == 1) {
        mineMap[currentX][currentY] = 2;
      } else {
        mineMap[currentX][currentY] = origMap[currentX][currentY];
      }
    }
    // Right is Query
    if (gamer.isPressed(RIGHT) && mineMap[currentX][currentY] == 3) {
      gamer.clear();
      delay(20);
      byte result[8];
      int dig2 = neighbourMap[currentX][currentY];
      Serial.print("Neighbour ");
      Serial.println(dig2);
      for(int p=0;p<8;p++) {
        result[p]=numbers[dig2][p];
      }
      gamer.printImage(result);
      delay(500);
    }
    // Left toggles Reveal, or 'Eye strain mode.'  this will blink revealed squares on a pulse cycle up to neightbour count.
    if (gamer.isPressed(LEFT)) {
      reveal = !reveal;
      if (reveal) {
        Serial.println("Resetting reveal.");
        btick=0;
        for(int x=0;x<8;x++) {
          for(int y=0;y<8;y++) {
             blinkMap[x][y] = 0;
          }
        }
      }
    }
  }
  
  // Update display.
  if (!boom) {
    for(int x=0; x<8;x++) {
      for(int y=0;y<8;y++) {
        if(mineMap[x][y] == 0 || mineMap[x][y] == 1) { // Mud or unfound mine.
          gamer.display[x][y] = HIGH;
        } else if (mineMap[x][y] == 2) { // Flag, blink occasionally.
          gamer.display[x][y] = (tick == 3) ? LOW : HIGH;
        } else if (mineMap[x][y] == 3 && !reveal) { // Clear, unset.
          gamer.display[x][y] = LOW; 
        } else if (mineMap[x][y] == 3 && reveal) {
          // Blink sequence for count
          if (neighbourMap[x][y] > 0 && blinkMap[x][y] < (neighbourMap[x][y]*2)) {
            int blinkCount = blinkMap[x][y];
            gamer.display[x][y] = (blinkCount % 2 == 0) ? LOW : HIGH; // If even low, if odd high!
            blinkMap[x][y] = ++blinkCount;
          } else {
            gamer.display[x][y] = LOW;
          }
        }
      } 
    }
  } else if (boom) {
    if (boomCycle < 10) {
      for(int x=0;x<8;x++) {
        for(int y=0;y<8;y++) {
          gamer.display[x][y] = (random(0,2) == 1) ? HIGH : LOW;
        }
      }
    } else {
      setupStuff();
    }
    boomCycle++;
  }
  
  // Reset reveal cycle for next pass if necessary.
  if (reveal) {
    Serial.print("Blink tick "); Serial.print(btick); Serial.println(".");
    btick++;
    if (btick > 20) { // Have we gone through a full cycle + a bunch (to create a pause if neighbour = 8)
      Serial.println("Resetting tick and blinkMap.");
      btick = 0;
      for(int x=0;x<8;x++) {
        for(int y=0;y<8;y++) {
          blinkMap[x][y] = 0;
        }
      }
    }
  }
  
  // Blink cursor.
  if (ctick == 0) {
    gamer.display[currentX][currentY] = HIGH;
    ctick = 1;
  } else {
    gamer.display[currentX][currentY] = LOW;
    ctick = 0;
  }
  
  // Compare origMap with mineMap, if clean, e.g. every mine is now a 2 and every dirt is now a 3 then win!
  bool allFlagged = true;
  for(int x=0; x<8; x++) {
    for(int y=0; y<8; y++) {
      if (origMap[x][y] == 0 && mineMap[x][y] != 3) {
        allFlagged = false;
        break;
      } else if (origMap[x][y] == 1 && mineMap[x][y] != 2) {
        allFlagged = false;
        break;
      }
    }
    if (!allFlagged)
      break;
  }
  
  if (allFlagged) {
    delay(500);
    setupStuff();
  }
  
  delay(150);
  gamer.updateDisplay();
}

// Here be dragons, unused dragons!

void setupScore() {
  n1[0] = B00011000;
  n1[1] = B00111000;
  n1[2] = B01011000;
  n1[3] = B00011000;
  n1[4] = B00011000;
  n1[5] = B00011000;
  n1[6] = B00011000;
  n1[7] = B01111110;

  n2[0] = B01111110;
  n2[1] = B00000110;
  n2[2] = B00100110;
  n2[3] = B01111110;
  n2[4] = B01100000;
  n2[5] = B01100000;
  n2[6] = B01100000;
  n2[7] = B01111110;

  n3[0] = B01111110;
  n3[1] = B00000110;
  n3[2] = B00000110;
  n3[3] = B01111110;
  n3[4] = B00000110;
  n3[5] = B00000110;
  n3[6] = B00000110;
  n3[7] = B01111110;

  n4[0] = B01100110;
  n4[1] = B01100110;
  n4[2] = B01100110;
  n4[3] = B01111110;
  n4[4] = B00000110;
  n4[5] = B00000110;
  n4[6] = B00000110;
  n4[7] = B00000110;

  n5[0] = B01111110;
  n5[1] = B01100000;
  n5[2] = B01100000;
  n5[3] = B01111110;
  n5[4] = B00000110;
  n5[5] = B00000110;
  n5[6] = B00000110;
  n5[7] = B01111110;

  n6[0] = B01111110;
  n6[1] = B01100000;
  n6[2] = B01100000;
  n6[3] = B01111110;
  n6[4] = B01100110;
  n6[5] = B01100110;
  n6[6] = B01100110;
  n6[7] = B01111110;

  n7[0] = B01111110;
  n7[1] = B00000110;
  n7[2] = B00000110;
  n7[3] = B00000110;
  n7[4] = B00000110;
  n7[5] = B00000110;
  n7[6] = B00000110;
  n7[7] = B00000110;

  n8[0] = B01111110;
  n8[1] = B01100110;
  n8[2] = B01100110;
  n8[3] = B01111110;
  n8[4] = B01100110;
  n8[5] = B01100110;
  n8[6] = B01100110;
  n8[7] = B01111110;

  n9[0] = B01111110;
  n9[1] = B01100110;
  n9[2] = B01100110;
  n9[3] = B01111110;
  n9[4] = B00000110;
  n9[5] = B00000110;
  n9[6] = B00000110;
  n9[7] = B00000110;

  n0[0] = B01111110;
  n0[1] = B01100110;
  n0[2] = B01100110;
  n0[3] = B01100110;
  n0[4] = B01100110;
  n0[5] = B01100110;
  n0[6] = B01100110;
  n0[7] = B01111110;

  for(int x=0;x<8;x++) {
    numbers[0][x] = n0[x];
    numbers[1][x] = n1[x];
    numbers[2][x] = n2[x];
    numbers[3][x] = n3[x];
    numbers[4][x] = n4[x];
    numbers[5][x] = n5[x];
    numbers[6][x] = n6[x];
    numbers[7][x] = n7[x];
    numbers[8][x] = n8[x];
    numbers[9][x] = n9[x];
  }
}
