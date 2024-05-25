// MAX7219 - using Led Control library to display 8x8 bitmap
#include <LedControl.h>

// Constants for analog joystick and button thresholds
#define NEUTRAL 510
#define MAX 1023
#define ERR 10
#define SWITCH_PLAYER 3
#define RANDOM 3
#define NUM_MAPS 4
#define MAP_SIZE 6
#define DEBOUNCE_DELAY 300
#define CPU_MOVE 300
#define G2P 2
#define GCPU 1
#define HARD 2
#define RESET_TIME 120000

// Pin Definitions for MAX7219 LED matrix, analog joystick, and button
int DIN = 11;
int CS = 10;
int CLK = 13;
LedControl lc = LedControl(DIN, CLK, CS, 0);

int xPin = A0;
int yPin = A1;
int buttonPin = 2;

// Global Variables
// Volatile variable to track button state for interrupt
volatile bool buttonState = 1;
int x = 0;
int y = 0;
int tx = -1;
int ty = -1;

int menu = 1;
int diff_select = 0;
int num_players = 1;
int game_mode;
int num_player_selected = 0;
unsigned long last_debounce_time = 0;
int game = 0;
int nr_map = 0;
int to_move = 0;
int difficulty = 0;
unsigned long  last_move = 0;

// Bitmaps for displaying game elements on LED matrix
byte DIFFICULTY[3][8] = {
    {B00000000, B00111100, B00100000, B00111000, B00100000, B00111100, B00000000, B00000000},
    {B00000000, B00100010, B00110110, B00101010, B00100010, B00100010, B00000000, B00000000},
    {B00000000, B00100010, B00100010, B00111110, B00100010, B00100010, B00000000, B00000000}};
byte WIN[2][8] = {
    {B00001111, B00001111, B00001111, B00001111, B00001111, B00001111, B00001111, B00001111},
    {B11110000, B11110000, B11110000, B11110000, B11110000, B11110000, B11110000, B11110000}};
byte EMPTY[8] = {B00000000, B00000000, B00000000, B00000000, B00000000, B00000000, B00000000, B00000000};
byte P2[8] = {B00000000, B01000111, B10100101, B00100111, B01000100, B10000100, B11100100, B00000000};
byte P1[8] = {B00000000, B00100111, B01100101, B00100111, B00100100, B00100100, B00100100, B00000000};
byte QUESTION_MARK[8] = {B00000000, B00011100, B00100010, B00001100, B00001000, B00000000, B00001000, B00000000};
byte NUM_PLAY[8][3];
byte Disp[8];

// Map Configurations
int map_layout[3][6] = {
    {6, 5, 4, 3, 2, 1},
    {6, 6, 6, 6, 6, 6},
    {5, 3, 6, 4, 4, 2}};

int current_map[6];

// Function to reset the game state
void reset_game() {
  num_player_selected = 0;
  menu = 1;
  x = 0;
  y = 0;
  tx = -1;
  ty = -1;
  num_player_selected = 0;
  to_move = 0;
  diff_select = 0;
}

// Setup function to initialize pins, serial communication, and LED matrix
void setup() {
    pinMode(buttonPin, INPUT);
    Serial.begin(9600);
    lc.shutdown(0, false);
    lc.setIntensity(0, 0);
    lc.clearDisplay(0);
    pinMode(buttonPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(buttonPin), ISR_button_pressed, CHANGE);
    for (int i = 0; i < 8; i++) NUM_PLAY[i][1] = P1[i];
    for (int i = 0; i < 8; i++) NUM_PLAY[i][2] = P2[i];
}

// Function to display current state on LED matrix
void show_display() {
    for (int i = 0; i < 8; i++) lc.setRow(0, i, Disp[i]);
}

// Function for difficulty selection based on joystick input
void difficulty_select() {
    int xValue = analogRead(xPin);
    unsigned long current_time = millis();
    if (current_time > last_debounce_time + DEBOUNCE_DELAY) {
        if (xValue < ERR) {
            difficulty = (difficulty - 1 + 3) % 3;
            last_debounce_time = current_time;
        } else if (xValue > MAX - ERR) {
            difficulty = (difficulty + 1) % 3;
            last_debounce_time = current_time;
        }
    }

    for (int i = 0; i < 8; i++) Disp[i] = DIFFICULTY[difficulty][i];

    if (buttonState == 0) {
        diff_select = 0;
        game_mode = GCPU;
        if (difficulty == HARD) {
            to_move = 1;
        }
        buttonState = 1;
    }
}

// Function for navigating the main menu
void menu_org() {
    if (diff_select > 0) {
        difficulty_select();
        return;
    } else if (num_player_selected > 0) {
        map_select();
        return;
    }

    int xValue = analogRead(xPin);
    unsigned long current_time = millis();
    if (current_time > last_debounce_time + DEBOUNCE_DELAY) {
        if (xValue < NEUTRAL - ERR || xValue > NEUTRAL + ERR) {
            num_players = SWITCH_PLAYER - num_players;
            last_debounce_time = current_time;
        }
    }
    for (int i = 0; i < 8; i++) Disp[i] = NUM_PLAY[i][num_players];
    if (buttonState == 0) {
        num_player_selected = num_players;
        if (num_players == 2) {
            game_mode = G2P;
        } else {
            diff_select = 1;
        }
        long current_time = millis();
        randomSeed(current_time);
        buttonState = 1;
    }
}

// Function to convert map layout to display format
void convert_map_to_disp(int mp[6]) {
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < mp[i]; j++)
            Disp[6 - j] |= (1 << (i + 1));
}

// Function to randomly generate a map layout
void random_map() {
    long randNumber = random(20, 30);
    for (int i = 0; i < MAP_SIZE; i++) {
        long randCol = random(0, MAP_SIZE);
        while (randCol > randNumber && (MAP_SIZE - i - 1) * MAP_SIZE < randNumber - randCol)
            randCol = random(0, MAP_SIZE);
        randNumber -= randCol;
        current_map[i] = randCol;
    }
}

// Function to reset cursor position to first postion available
void reset_cursor() {
    x = 0;
    while (current_map[x] == 0 && x < MAP_SIZE)
        x++;
    y = current_map[x];
}

// Function for map selection based on joystick input
void map_select() {
    int xValue = analogRead(xPin);
    unsigned long current_time = millis();
    if (current_time > last_debounce_time + DEBOUNCE_DELAY) {
        if (xValue < ERR) {
            nr_map = (nr_map - 1 + NUM_MAPS) % NUM_MAPS;
            last_debounce_time = current_time;
        } else if (xValue > MAX - ERR) {
            nr_map = (nr_map + 1) % NUM_MAPS;
            last_debounce_time = current_time;
        }
    }

    if (nr_map == RANDOM)
        for (int i = 0; i < 8; i++) Disp[i] = QUESTION_MARK[i];
    else
        convert_map_to_disp(map_layout[nr_map]);
    if (buttonState == 0) {
        menu = 0;
        if (nr_map != RANDOM)
            for (int i = 0; i < 6; i++) current_map[i] = map_layout[nr_map][i];
        else
            random_map();
        reset_cursor();
        buttonState = 1;
    }
}

// Function for player movement based on joystick input
void player_move() {
    int xValue = analogRead(xPin);
    int yValue = analogRead(yPin);
    unsigned long current_time = millis();
    if (current_time > last_debounce_time + DEBOUNCE_DELAY) {
        if (yValue < ERR) {
            y--;
            if (y == 0) {
                y = 1;
            }
            last_debounce_time = current_time;
        } else if (yValue > MAX - ERR) {
            y++;
            if (y > current_map[x]) {
                y = current_map[x];
            }
            last_debounce_time = current_time;
        }
        if (xValue < ERR) {
            int cp_x = x - 1;
            while (cp_x >= 0 && current_map[cp_x] == 0)
                cp_x--;
            if (cp_x >= 0) {
                x = cp_x;
                if (y > current_map[x])
                    y = current_map[x];
            }
            last_debounce_time = current_time;
        } else if (xValue > MAX - ERR) {
            int cp_x = x + 1;

            while (cp_x < MAP_SIZE && current_map[cp_x] == 0)
                cp_x++;
            if (cp_x < MAP_SIZE) {
                x = cp_x;
                if (y > current_map[x])
                    y = current_map[x];
            }
            last_debounce_time = current_time;
        }
    }
    if (buttonState == 0) {
        to_move = (to_move + 1) % 2;
        current_map[x] = y - 1;
        reset_cursor();
        buttonState = 1;
    }
}

// Function to move cursor for any CPU move
void make_move(int new_x, int new_y) {
  tx = new_x;
  ty = new_y;
  unsigned long current_time = millis();
  if (last_move + CPU_MOVE < current_time) {
    if (x != tx) {
        x++;
        while(current_map[x] == 0) {
          x++;
        }
        y = current_map[x];
        last_move = current_time;
    } else if (y != ty) {
      y--;
      last_move = current_time;
    }
  }
  if (x == tx && y == ty && last_move + CPU_MOVE < current_time) {
    current_map[new_x] = new_y - 1;
    reset_cursor();
    to_move = (to_move + 1) % 2;
    last_move = current_time;
    tx = -1;
    ty = -1;
  }
}

// Function to execute CPU move on hard difficulty
void CPU_h_move() {
    int sum = 0;
    for (int i = 0; i < 6; i++) {
        sum ^= current_map[i];
    }
    if (sum == 0) {
        make_move(x, y);
        return;
    }
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < current_map[i]; j++) {
          if ((sum ^ current_map[i] ^ j) == 0) {
            make_move(i, j + 1);
            return;
          }
        }
    }
}

// Function to execute CPU move on easy difficulty
void CPU_e_move() {
  int nr_col = 0;
  for(int i = 0; i < MAP_SIZE;i++)
    if(current_map[i] > 0)
      nr_col++;

  long randNumber = random(1, nr_col + 1);
  for(int i = 0; i < MAP_SIZE;i++) {
    if(current_map[i] > 0)
      randNumber--;
    if (randNumber == 0) {
      make_move(i, random(1, current_map[i] + 1));
      break;
    }
  }
}

// Function to check win condition and display winner
int win_game() {
  int sum = 0;
  for(int i = 0; i < MAP_SIZE;i++) sum += current_map[i];
  if (sum != 0) {
    return 0;
  }
  for (int i = 0; i < 8;i++) {
    for (int j = 0; j < 8;j++)
      if (i % 2 == 0)
        Disp[j] = WIN[to_move][j];
      else
        Disp[j] = EMPTY[j];
    show_display();
    delay(300);
  }
  return 1;
}

// Function to manage game flow and player/CPU moves
void game_manager() {
    switch (game_mode) {
        case G2P:
            player_move();
            break;
        case GCPU:
            if (to_move == 1) {
              if(tx != -1) {
                make_move(tx, ty);
                break;
              }
              if (difficulty == 0) {
                CPU_e_move();
              } else {
                CPU_h_move();
              }
            } else {
                player_move();
            }
            break;
        default:
            break;
    }
    unsigned long curr_time = millis();
    if (win_game() == 1 || last_debounce_time + RESET_TIME < curr_time) {
      reset_game();
      return;
    }
    for (int i = 0; i < 8; i += 7) {
        if (to_move == 0) {
            Disp[i] |= (1 << 7);
            Disp[i] &= ~(1);
        } else {
            Disp[i] |= 1;
            Disp[i] &= ~(1 << 7);
        }
    }
    if ((curr_time / 200) % 2 == 0) {
        convert_map_to_disp(current_map);
    } else {
        int cp_y = current_map[x];
        current_map[x] = y - 1;
        convert_map_to_disp(current_map);
        current_map[x] = cp_y;
    }
}

// Main loop to continuously update game state and LED display
void loop() {
    if (menu > 0) {
        menu_org();
    } else {
        game_manager();
    }
    show_display();
    for (int i = 0; i < 8; i++) Disp[i] = EMPTY[i];
}

// Interrupt Service Routine for button press detection
void ISR_button_pressed(void) {
    buttonState = digitalRead(buttonPin);
}
