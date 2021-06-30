/*
 * Arduino logic generator and monitor.
 *
 * Copyright (C) 2021 Eirik Haustveit <ehau@hvl.no>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * The source is kept in a single file for easy distribution, but
 * ideally several different libs. could probably be extracted from the
 * source, and kept in separate files.
 * 
 * The pins of the microcontroller have been given different names,
 * this should probably have been stored in a HAL lib to make stuff more clean.
 * 
 * The terminal() function has room for improvement, possibly by replasing it with
 * a state machine.
 * 
 */

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <errno.h>


/* Uncomment for color support, if your terminal supports ANSI escape codes */
//#define USE_ANSI_ESCAPE_CODE 

const char* help_str = R"(
====================================
Arduino logic generator and monitor
====================================
Supported commands:
help - Show this command overview
clear - Clear screen (only on supported terminal)
c - Send clock pulse
p - Output 3-bit on D8 - D10
d - Output 8-bit word on D0 - D7
ra - Read analog voltage on A0 - A5
rd - Read digital status on A0 - A5
)";

const uint8_t c_backspace = 8;
const uint8_t c_space = 32;

const uint8_t c_max_cmd_len = 32;

const uint8_t clk_pin = 13;

char cmd_history[c_max_cmd_len];

typedef enum {
  BLACK = 0,
  RED,
  GREEN,
  YELLOW,
  BLUE,
  MAGENTA,
  CYAN,
  WHITE,
  RESET
} color_t;

void terminal();
void command_interpreter(const uint8_t * cmd);
void to_upper(char * str);

void clock_pulse();

void clear_terminal();

void set_terminal_color(color_t color);

//void plot_inputs();

void print_io_status();
void read_digital_inputs_a0_a5();
void read_digital_inputs_d2_d12();
void read_analog_inputs_a0_a5();
void write_digital_outputs_d2_d9(uint8_t value);
void write_digital_outputs_d10_d12(uint8_t value);

void serial_printf(HardwareSerial& serial, const char* fmt, ...);

void setup() {


  Serial.begin(115200);

  pinMode(13, OUTPUT);
}

void loop() {

  //delay(10);


  terminal();
}

void terminal(){

  uint8_t rx_byte = 0;
  uint8_t rx_buffer[c_max_cmd_len];
  uint8_t i = 0;

  set_terminal_color(RED);
  Serial.print("MON>");
  set_terminal_color(WHITE);
  do{

    if (Serial.available() > 0) {

      rx_byte = Serial.read();
      delay(1);

      //Serial.print(rx_byte);

      if(rx_byte == c_backspace){

        if(i > 0){
          Serial.write(c_backspace);
          Serial.write(c_space);
          Serial.write(c_backspace);
          i--;
        }

      }
      else if( (rx_byte >= 'a' && rx_byte <= 'z') || (rx_byte >= 'A' && rx_byte <= 'Z' ) || (rx_byte >= '0' && rx_byte <= '9')){
        if(i < (c_max_cmd_len - 1)){

          Serial.write(rx_byte);
          rx_buffer[i++] = rx_byte;
          rx_buffer[i] = '\0';
        }
      }
      else if (rx_byte == '\n' || rx_byte == '\r'){
        while (Serial.read() >= 0){} /* Clear remaining data in UART rx buffer */
        Serial.println();
      }
    }

  } while (!(rx_byte == '\n' || rx_byte == '\r'));

    if(i > 0){
      command_interpreter(rx_buffer);
    }

}

/* TODO: Find a more flexible way to implement
 * the command parsing.
 */
void command_interpreter(const uint8_t * cmd){

  uint8_t param = 0;
  char *endptr = NULL;
  const char *nptr = NULL;

  /* Reset errno before calls to strtol */
  errno = 0;

  to_upper((char*)cmd); /* Convert command to upper case */
  //Serial.println((char*)cmd);

  if(!strcmp_P((const char*)cmd, PSTR("HELP"))){
    Serial.print(help_str);
    //Serial.println("Hjelp!!!");
  }
  else if (!strcmp_P((const char*)cmd, PSTR("CLEAR"))){
    clear_terminal();
  }
  else if (!strcmp_P((const char*)cmd, PSTR("C"))){
    clock_pulse();
  }
  else if (!strcmp_P((const char*)cmd, PSTR("CH"))){
    Serial.println("Clock high.");
    digitalWrite(clk_pin, HIGH);
  }
  else if (!strcmp_P((const char*)cmd, PSTR("CL"))){
    Serial.println("Clock low.");
    digitalWrite(clk_pin, LOW);
  }
  else if (!strcmp_P((const char*)cmd, PSTR("CT"))){
    Serial.println("Clock toggle.");
    if(digitalRead(clk_pin)){
      digitalWrite(clk_pin, LOW);
    }
    else{
      digitalWrite(clk_pin, HIGH);
    }
  }
  else if (!strncmp_P((const char*)cmd, PSTR("D"),1)){


    if (!strncmp_P((const char*)(cmd + 1), PSTR("0X"),2)){
      param = strtol((const char*)(cmd + 3),NULL,16);
    }
    else if(!strncmp_P((const char*)(cmd + 1), PSTR("0B"),2)){
      param = strtol((const char*)(cmd + 3),NULL,2);
    }
    else if(!strncmp_P((const char*)(cmd + 1), PSTR("0D"),2)){
      param = strtol((const char*)(cmd + 3),NULL,10);
    }
    else{
      Serial.println("Error invalid number format specifier. Shoud be 0x, 0b, or 0d.");
      return;
    }

    write_digital_outputs_d2_d9(param);
  }
  else if (!strncmp_P((const char*)cmd, PSTR("P"),1)){


    if (!strncmp_P((const char*)(cmd + 1), PSTR("0X"),2)){
      param = strtol((const char*)(cmd + 3),NULL,16);
    }
    else if(!strncmp_P((const char*)(cmd + 1), PSTR("0B"),2)){
      param = strtol((const char*)(cmd + 3),NULL,2);
    }
    else if(!strncmp_P((const char*)(cmd + 1), PSTR("0D"),2)){
      param = strtol((const char*)(cmd + 3),NULL,10);
    }
    else{
      Serial.println("Error invalid number format specifier. Shoud be 0x, 0b, or 0d.");
      return;
    }

    write_digital_outputs_d10_d12(param);
  }
  else if (!strncmp_P((const char*)cmd, PSTR("BR"),2)){ /* Bit reset */

    nptr = (const char*)(cmd + 2);

    param = strtol(nptr, &endptr,10);
    
    if (nptr == endptr){
      Serial.println("Invalid parameter.");
    }
    else{
      if(param >= 0 && param <= 10){
        serial_printf(Serial, "Setting bit: %i low.\n", param);
        //pinMode(param, OUTPUT);
        digitalWrite(param + 2, LOW);
      }
      else{
        Serial.println("Bit number is not a valid output port.");
      }
    }

  }
  else if (!strncmp_P((const char*)cmd, PSTR("BS"),2)){ /* Bit set*/

    nptr = (const char*)(cmd + 2);
    param = strtol(nptr, &endptr,10);
    uint8_t pin = 0;
    
    if (nptr == endptr){
      Serial.println("Invalid parameter.");
    }
    else{
      if(param >= 0 && param <= 10){
        serial_printf(Serial, "Setting bit: %i high.\n", param);
        pin = param + 2;
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
      }
      else{
        Serial.println(PSTR("Bit number is not a valid output port."));
      }
    }

  }
  else if (!strncmp_P((const char*)cmd, PSTR("READ"),4)){

    read_digital_inputs_a0_a5();
    read_digital_inputs_d2_d12();

  }
  else if (!strncmp_P((const char*)cmd, PSTR("AREAD"),4)){

    read_analog_inputs_a0_a5();

  }
  else if (!strncmp_P((const char*)cmd, PSTR("STATUS"),6)){

    print_io_status();

  }
  else if (!strncmp_P((const char*)cmd, PSTR("HVL"),3)){

    Serial.println("IDER.");

  }
  else if (!strncmp_P((const char*)cmd, PSTR("PLOT"),4)){

    Serial.println("Plot not supported (yet).");
    //plot_inputs();
  }
  else{
    Serial.println("ERROR! Command not found");
  }  

}

void print_io_status(){

  
  #ifdef USE_ANSI_ESCAPE_CODE
    Serial.println("I/O status display.");
  #else
    Serial.println(PSTR("Coloroed I/O status display is disabled because not all terminals support it."));
  #endif

}

void clock_pulse(){

  Serial.println("CLK pulse");
  digitalWrite(clk_pin, HIGH);
  delay(50);
  digitalWrite(clk_pin, LOW);
}

void read_digital_inputs_a0_a5(){

  for(uint8_t i = 0; i <= 5; i++){

    pinMode(A0 + i, INPUT);

    serial_printf(Serial, "A%i:%i\t",i,digitalRead(A0 + i));
  }

  Serial.println();
}


void read_digital_inputs_d2_d12(){

  /* 
   * Read the digital inputs without configuring them as inputs
   * Useful to check your current output configuration.
   */

  uint8_t pin = 0;

  for(uint8_t i = 0; i < 11; i++){
    pin = i + 2;
    serial_printf(Serial, "D%i:%i\t",i,digitalRead(pin));
  }

  Serial.println();
}

void write_digital_outputs_d2_d9(uint8_t value){

  /* Outputs D2 - D9 are renamed D0 to D7 for this applicaiton. */

  serial_printf(Serial, "DEC: %i \t HEX: %X\n", value, value);

  uint8_t bit;

//  for(uint8_t i = 0; i <= 7; i++){

  // TODO: This is a bit messy, fix it.

  for(uint8_t i = 8; i > 0; i--){
    bit = (value >> (i - 1))&0x01;

    pinMode(1 + i, OUTPUT);
    serial_printf(Serial, "D%i:%i\t",i - 1, bit);
    digitalWrite(1 + i, bit);

  }

  Serial.println();

}

void write_digital_outputs_d10_d12(uint8_t value){

  /* Outputs D10 - D12 are renamed D8 to D10 for this applicaiton. */

  serial_printf(Serial, "DEC: %i \t HEX: %X \t BIN: %B\n", value, value, value);

  uint8_t bit = 0;
  uint8_t i = 0;

  for(i = 0; i < 50; i++)
    Serial.print("-");

  Serial.println();

  for(i = 8; i > 0; i--){

    bit = (value >> (i-1))&0x01;
    serial_printf(Serial, "Bit%i:%i\t",i-1, bit);
  }
 
  Serial.println();
  
  for(uint8_t i = 8; i > 0; i--){

    bit = (value >> (i-1))&0x01;

    if(i < 4){
      serial_printf(Serial, "D%i:%i\t",i + 7, bit);
      pinMode(i + 9, OUTPUT);
      digitalWrite(i + 9,bit);

    }
    else{
      serial_printf(Serial, "X\t");
    }

  }

  Serial.println();
}

void read_analog_inputs_a0_a5(){

  for(uint8_t i = 0; i <= 5; i++){
    serial_printf(Serial, "A%i:%i\t\t",i,analogRead(A0 + i));
  }

  Serial.println();
  for(uint8_t i = 0; i <= 5; i++){
    serial_printf(Serial, "A%i:%2f[V]\t",i,(analogRead(A0 + i)*5.0)/1023.0);
  }
  Serial.println();

  //TODO: Add display of digital status ref. TLL thresholds
}

void to_upper(char * str){

    while(*str) 
    {
        *str = (*str >= 'a' && *str <= 'z') ? *str-32 : *str;
        str++;
    }
}

void set_terminal_color(color_t color){

  #ifdef USE_ANSI_ESCAPE_CODE

  switch (color)
  {
  case BLACK:
    Serial.print("\x1b[30m");
    break;
  case RED:
    Serial.print("\x1b[31m");
    break;
  case GREEN:
    Serial.print("\x1b[32m");
    break;
  case YELLOW:
    Serial.print("\x1b[33m");
    break;
  case BLUE:
    Serial.print("\x1b[34m");
    break;
  case MAGENTA:
    Serial.print("\x1b[35m");
    break;
  case CYAN:
    Serial.print("\x1b[36m");
    break;
  case WHITE:
      Serial.print("\x1b[37m");
    break;
  case RESET:
    Serial.print("\x1b[0m");
    break;
  default:
    Serial.print("\x1b[0m");
    break;
  }
//
//  if(color == RED){
//  }
//  else if (color == WHITE){
//  }
//
  #endif
}

void clear_terminal(){

  #ifdef USE_ANSI_ESCAPE_CODE
    Serial.print("\x1b[2J");
  #else
    Serial.println(PSTR("Clear function is disabled since not all displays support it."));
  #endif
}
//
//typedef struct {
//  uint8_t width;
//  uint8_t height;
//} plot_window_t;
//
//#define TERMINAL_WIDTH 80
//#define TERMINAL_HEIGHT 40
//
//uint8_t terminal_buffer[TERMINAL_WIDTH*TERMINAL_HEIGHT];
//
//
//void buffer_init(plot_window_t *window, uint8_t *buf){
//
//  uint8_t i = 0;
//  uint8_t j = 0;
//
//  for(i = 0; i < window->height; i++){
//    for(j = 0; j < window->width - 2; j++){
//      *(buf + i + j) = 'x';
//    }
//    *(buf + i + j + 1) = '\r';
//    *(buf + i + j + 2) = '\n';
//  }
//
//}
//
//void fill_line(plot_window_t *window){
//
//  for(uint8_t i = 0; i < window->width; i++){
//    
//  }
//}
//
//void show_plot(plot_window_t *window, uint8_t *buf){
//
//  for(uint8_t i = 0; i < window->height; i++){
//    for(uint8_t j = 0; j < window->width; j++){
//      Serial.write(*(buf + i + j));
//    }
//  }
//
//}
//
//void plot_inputs(){
//
//  plot_window_t window;
//  window.width = TERMINAL_WIDTH;
//  window.height = TERMINAL_HEIGHT;
//
//  buffer_init(&window, terminal_buffer);
//
//  show_plot(&window, terminal_buffer);
//
//
//}


void serial_printf(HardwareSerial& serial, const char* fmt, ...) { 
    va_list argv;
    va_start(argv, fmt);

    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%') {
            // Look for specification of number of decimal places
            int places = 2;
            if (fmt[i+1] == '.') i++;  // alw1746: Allows %.4f precision like in stdio printf (%4f will still work).
            if (fmt[i+1] >= '0' && fmt[i+1] <= '9') {
                places = fmt[i+1] - '0';
                i++;
            }
            
            switch (fmt[++i]) {
                case 'B':
                    serial.print("0b"); // Fall through intended
                case 'b':
                    serial.print(va_arg(argv, int), BIN);
                    break;
                case 'c': 
                    serial.print((char) va_arg(argv, int));
                    break;
                case 'd': 
                case 'i':
                    serial.print(va_arg(argv, int), DEC);
                    break;
                case 'f': 
                    serial.print(va_arg(argv, double), places);
                    break;
                case 'l': 
                    serial.print(va_arg(argv, long), DEC);
                    break;
                case 'o':
                    serial.print(va_arg(argv, int) == 0 ? "off" : "on");
                    break;
                case 's': 
                    serial.print(va_arg(argv, const char*));
                    break;
                case 'X':
                    serial.print("0x"); // Fall through intended
                case 'x':
                    serial.print(va_arg(argv, int), HEX);
                    break;
                case '%': 
                    serial.print(fmt[i]);
                    break;
                default:
                    serial.print("?");
                    break;
            }
        } else {
            serial.print(fmt[i]);
        }
    }
    va_end(argv);
}