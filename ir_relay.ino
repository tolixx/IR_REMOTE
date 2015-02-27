
#include <IRremote.h>
#include <IRremoteInt.h>

#include <EEPROM.h>

/* Номер пина, к котором подключен инфракрасный датчик */
int RECV_PIN = 6;

/* Количество пультов для управления
 * Это значение можно изменять, но учтите, что размер EEPROM ограничен 
 */
 
#define NUM_OF_SETS 3

/*
 * Количество каналов для управления
 *   У меня 4-х канальное реле 
 */
 
#define NUM_OUTS   4

/* Дополнительные комманды - включить все, выключить все, инвертировать включение */
#define ON_ALL      0
#define OFF_ALL     1
#define INVERSE_ALL 2

/* Общее количество комманд - 4 канала + 3 комманды в нашем случае */
#define COMM_COUNT 7

/* Время ожидания в режиме программирования, если 8 секунд не нажимать кнопок, то мы выйдем из режима
 * При если Вы не назначите все 7 кнопок, программирование произведено не будет
 */
#define PROG_TIMEOUT 8000


/*
 * Время задедржи между опросом ИК датчика
 */
#define DELAY_IR_SCAN 100
/* 
 * Мой модуль реле замкнут при 0 и разомкнут при 1 
 * Если у Вас все наоборот, просто поменяйте эти 2 константы 
 */

#define RELAY_ON  LOW
#define RELAY_OFF HIGH

/* Этот код кнопки программирования, он прошит намертво
 * У вас он точно будет другой
 */
 
#define PROG_BUTTON 0xE0E09E61

IRrecv irrecv(RECV_PIN);
decode_results results;

/* Номера пинов, к которым подключено реле */
int outs[NUM_OUTS] = { 2, 3, 4, 5 };

/* Адреса EEPROM, в которые записываются коды комманд
 * У меня выделяется 64 байта на один пульт, тоесть максимум может быть 16 комманд
 * Если надо больше, просто раздвиньте адреса, например 0, 128, 256 - выйдет по 32 комманды
 */

int addresess[NUM_OF_SETS] = { 64, 128, 192 };

void setup() {
  irrecv.enableIRIn(); // включить приемник
  for ( int i = 0; i < NUM_OUTS; i++ ) {
     pinMode(outs[i], OUTPUT);
  }
  
  setAllPins ( RELAY_OFF );
  Serial.begin(9600);
  Serial.println("Setup done");
}

void loop() {
   processIR();  
}

void programIR (int set = 0) {
    
   int num = 0;
   unsigned long buffer[COMM_COUNT];
   
   bool already = false;
   unsigned long tick = millis();
   
   Serial.println ( "IR programMode" );
   delay ( DELAY_IR_SCAN );
   irrecv.resume();
   
   do {
     already = false;
     if (irrecv.decode(&results)) {
       Serial.println ( results.value, HEX );
       if  ( results.value == PROG_BUTTON ) {
          set = ( set + 1 ) %  NUM_OF_SETS; //--- По нажатию кнопки программирования переходим к следующему пульту 
          Serial.print ( "Programming PULT: " );
          Serial.println ( set );
       } else if ( results.value != 0xFFFFFFFF ) {
          
          tick = millis();
         
          Serial.print ( "DETECTED " );
          Serial.print ( results.value, HEX );
          Serial.print ( " FOr " );
          Serial.println ( num );
          
          for ( int i = 0; i < num; i++ ) {
             if ( buffer[i] == results.value ) {
                 Serial.print ( "Button used, choose another one " );
                 Serial.println ( i );
                 already = true;
             }
          }
          
          if ( !already ) {
             buffer[num] = results.value;
             ++num;
          }
       }
       
       irrecv.resume();
     }
     
     delay(DELAY_IR_SCAN);
          
     if ( millis() - tick > PROG_TIMEOUT ) {
        Serial.println ( "Terminating programming" );
        return;
     }
     
   } while ( num <  COMM_COUNT );
 
   //--- Запись кодов в EEPROM ---
   for ( int i = 0; i <  COMM_COUNT; i++ ) {
     writeLong ( addresess[set] + i*sizeof(long), buffer[i] );   
   }
  
   Serial.print ( "Program completed for PULT " ); 
   Serial.println ( set );
}

void processIR() {
   unsigned long value;
  
  if (irrecv.decode(&results)) {
     Serial.println(results.value, HEX);
 
     if ( results.value != 0xFFFFFFFF ) {
        int command;
        
        if ( results.value == PROG_BUTTON ) {
            programIR ();
        } else if ( ( command = getCommandCode(results.value) ) != -1 ) {
            Serial.print ( "Command detected:");
            Serial.println ( command );
   
            runCommand ( command );           
        }
     }
          

     delay(DELAY_IR_SCAN);
     irrecv.resume();
  }
}

/* Выполнение комманды по коду */
void runCommand ( int command ) {
  if ( command < NUM_OUTS ) {
      digitalWrite ( outs[command], !digitalRead(outs[command]));
  } else if ( command == NUM_OUTS + ON_ALL ) {
      setAllPins ( RELAY_ON ); 
  } else if ( command == NUM_OUTS + OFF_ALL ) {
      setAllPins ( RELAY_OFF );
  } else if ( command == NUM_OUTS + INVERSE_ALL ) {
       inverseAllPins ();  
  }
}


void setAllPins ( int value ) {
   for ( int i = 0; i < NUM_OUTS; i++ ) {
      digitalWrite ( outs[i], value );
   }
}

void inverseAllPins ( ) {
  for ( int i = 0; i < NUM_OUTS; i++ ) {
     digitalWrite ( outs[i], !digitalRead ( outs[i] ) );
  }
}

int getCommandCode ( unsigned long value ) {
  for ( int j = 0; j < NUM_OF_SETS; j++ ) {
    for ( int i = 0; i < COMM_COUNT; i++ ) {
        if ( readLong ( addresess[j] + i*sizeof(long) ) == value ) {
           return i;
        }
    }
  }
  return -1;
}

unsigned long readLong ( int address ) {
   unsigned long result = 0;   
   
   for ( int i = 0; i < sizeof(long); i++  ) {
       byte val = EEPROM.read(address + i);
       result = result >> 8;
       result = result | ( (unsigned long)val << 24 );
      
   }
   
   return result;
}

void writeLong ( int address, unsigned long value ) {

  for ( int i = 0; i < sizeof(long); i++ ) {
       byte val = value & 0xFF;
       EEPROM.write(address + i, val);
       value = value >> 8;
  }  
}


