#include <avr/io.h>
#define F_CPU 1200000UL
#include <util/delay.h>

/*
DDRx	Настройка разрядов порта x на вход или выход.
PORTx	Управление состоянием выходов порта x (если соответствующий разряд настроен как выход), или подключением внутреннего pull-up резистора (если соответствующий разряд настроен как вход).
PINx	Чтение логических уровней разрядов порта x.
*/

typedef uint8_t byte;

#define OUTPUT true
#define INPUT false
#define HIGH true
#define LOW false

#define MODE 0 // пин настраивается на вход или выход
#define STATE 1 // установка высокого или низкого уровня пина

#define CLK PB1  // pin 5
#define SDA PB0  // pin 6

// tm1637
#define TM1637_SETTINGS 0x44
#define TM1637_BRIGHTNESS 0x8E
const byte tm1637Numbers[10] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
};

/*
 Настройка пина.
 Если 'mode' равен STATE, устанавливается логический уровень пина.
 Если 'mode' равен MODE, пин настраивается на вход или выход.
*/
void changePin(byte pin, bool state, byte mode = STATE) {
  if (mode == STATE) (state ? (PORTB |= (1 << pin)) : (PORTB &= ~(1 << pin)));
  else (state ? (DDRB |= (1 << pin)) : (DDRB &= ~(1 << pin)));
}

void _tm1637Start() {
  changePin(CLK, HIGH);
  changePin(SDA, HIGH);
  changePin(SDA, LOW);
}

void _tm1637WriteByte(byte data) {
  byte i = 0;
  send:
  changePin(CLK, LOW);
  changePin(SDA, (data & (1 << i)));
  changePin(CLK, HIGH);
  i += 1;
  if (i < 8) goto send;  // goto экономит 44 байта по сравнению с for или while
}

bool _tm1637Ask() {
  changePin(SDA, INPUT, MODE);
  changePin(CLK, LOW);
  _delay_us(100);
  changePin(CLK, HIGH);
  bool ask = PINB & (1 << SDA);
  changePin(SDA, OUTPUT, MODE);
  return !ask;
}

void _tm1637Stop() {
  changePin(CLK, LOW);
  changePin(SDA, LOW);
  changePin(CLK, HIGH);
  changePin(SDA, HIGH);
}

bool tm1637SendCommand(byte command) {
  _tm1637Start();
  _tm1637WriteByte(command);
  bool ask = _tm1637Ask();
  _tm1637Stop();
  return ask;
}

bool tm1637PrintSymbol(byte position, byte symbol) {
  // position to address
  if (position < 1) position = 1;
  if (position > 4) position = 4;
  position--;
  position += 0xC0;

  _tm1637Start();
  _tm1637WriteByte(position);
  _tm1637Ask();
  _tm1637WriteByte(symbol);
  bool ask = _tm1637Ask();
  _tm1637Stop();
  return ask;
}
/******************************************************/


int main(void) {
  _delay_ms(1000);

  changePin(CLK, OUTPUT, MODE);
  changePin(SDA, OUTPUT, MODE);

  tm1637SendCommand(TM1637_SETTINGS);
  tm1637SendCommand(TM1637_BRIGHTNESS);

  while (1) {
  }
}
