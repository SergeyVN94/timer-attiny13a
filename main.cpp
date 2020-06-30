#include <avr/io.h>
#define F_CPU 1200000UL
#include <util/delay.h>
#include <avr/interrupt.h>

/*
DDRx	Настройка разрядов порта x на вход или выход.
PORTx	Управление состоянием выходов порта x (если соответствующий разряд настроен как выход), или подключением внутреннего pull-up резистора (если соответствующий разряд настроен как вход).
PINx	Чтение логических уровней разрядов порта x.
*/

typedef uint8_t byte;

// pin configuration
#define OUTPUT true
#define INPUT false
#define HIGH true
#define LOW false
#define MODE true // пин настраивается на вход или выход
#define STATE false // установка высокого или низкого уровня пина

// пины
#define PIN_CLK PB1 // pin 6
#define PIN_SDA PB0 // pin 5
#define PIN_BUTTONS PB3 // pin 2
#define PIN_SIGNAL PB4 // pin 3

// кнопки
#define BTN_SET 0
#define BTN_PLUS 1
#define BTN_MINUS 2
#define BTN_NOT_SELECTED 3

// состояния программы
#define SET_MINUTES 0
#define SET_SECONDS 1
#define WORK 2
#define PAUSE 3
#define WORK_END 4

// другие настройки
#define MAX_COUNTER 75
#define SIGNAL_TIME 5 // in seconds

/*
  Настройка пина.
   Если 'mode' равен STATE, устанавливается логический уровень пина.
  Если 'mode' равен MODE, пин настраивается на вход или выход.
*/
void changePin(byte pin, bool state, bool mode = STATE) {
  if (mode == STATE) (state ? (PORTB |= (1 << pin)) : (PORTB &= ~(1 << pin)));
  else (state ? (DDRB |= (1 << pin)) : (DDRB &= ~(1 << pin)));
}

// tm1637
#define TM1637_SETTINGS 0x44
#define TM1637_BRIGHTNESS 0x8E
const byte tm1637Numbers[10] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
};

void _tm1637Start() {
  changePin(PIN_CLK, HIGH);
  changePin(PIN_SDA, HIGH);
  changePin(PIN_SDA, LOW);
}

void _tm1637WriteByte(byte data) {
  byte i = 0;
  send:
  changePin(PIN_CLK, LOW);
  changePin(PIN_SDA, (data & (1 << i)));
  changePin(PIN_CLK, HIGH);
  i += 1;
  if (i < 8) goto send;  // goto экономит 44 байта по сравнению с for или while
}

bool _tm1637Ask() {
  changePin(PIN_SDA, INPUT, MODE);
  changePin(PIN_CLK, LOW);
  changePin(PIN_CLK, HIGH);
  bool ask = PINB & (1 << PIN_SDA);
  changePin(PIN_SDA, OUTPUT, MODE);
  return !ask;
}

void _tm1637Stop() {
  changePin(PIN_CLK, LOW);
  changePin(PIN_SDA, LOW);
  changePin(PIN_CLK, HIGH);
  changePin(PIN_SDA, HIGH);
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

bool isTimerTick = false;
bool pointsOn = true;
byte PROGRAM_STATE = SET_MINUTES;
byte timerCounter = 0;
uint16_t secondsCounter = 0;

uint16_t getButtonsAnalogValue() {
  // включить ацп, тактирование в 128 раз меньше, начать измерение
  ADCSRA = (1 << ADEN) | (3) | (1 << ADSC); 
  // ждать конца измерения
  while (ADCSRA & (1 << ADSC)); 
  return (ADCL | (ADCH << 8));
}

byte checkButtons() {
  uint16_t analogValue = getButtonsAnalogValue();

  if (analogValue >= 950 && analogValue <= 1023) return BTN_SET;
  else if (analogValue >= 470 && analogValue <= 530) return BTN_PLUS;
  else if (analogValue >= 300 && analogValue <= 400) return BTN_MINUS;
  
  return BTN_NOT_SELECTED;
}

void printTime() {
  tm1637PrintSymbol(1, tm1637Numbers[(secondsCounter / 60) / 10]);
  byte minutesUnits = (secondsCounter / 60) % 10;
  tm1637PrintSymbol(2, pointsOn ? tm1637Numbers[minutesUnits] | (1 << 7) : tm1637Numbers[minutesUnits]);
  tm1637PrintSymbol(3, tm1637Numbers[(secondsCounter % 60) / 10]);
  tm1637PrintSymbol(4, tm1637Numbers[(secondsCounter % 60) % 10]);
}

void setStartState() {
  PROGRAM_STATE = SET_MINUTES;
  secondsCounter = 0;
  pointsOn = true;
  printTime();
}

ISR(TIM0_COMPA_vect) {
  if (timerCounter < MAX_COUNTER) {
    timerCounter += 1;
  } else {
    timerCounter = 0;
    isTimerTick = true;
  }  
}

int main(void) {
  changePin(PIN_CLK, OUTPUT, MODE);
  changePin(PIN_SDA, OUTPUT, MODE);
  changePin(PIN_SIGNAL, OUTPUT, MODE);
  changePin(PIN_BUTTONS, INPUT, MODE);

  tm1637SendCommand(TM1637_SETTINGS);
  tm1637SendCommand(TM1637_BRIGHTNESS);

  // timer
  /* нужно:
    1) включить срабатывание по сравнению с регистром (режим СТС)
    2) утановить предделитель
    3) указать регистр, с которым сравнивается
    4) записать число с которым сравнивать
    5) включить прерывания

    TCCR0B, TCCR0A - регистры управления таймером/счетчиком
    TIMSK0 - регистр настрек

    TCNT0 - таймер/счетчик
    OCR0A и OCR0B - регистры сравнения
  */
  TCCR0A |= (1 << WGM01); // режим СТС (сравнение с регистром)
  TCCR0B |= (1 << CS01) | (1 << CS00); // предделитель частоты на 64
  TIMSK0 |= (1 << OCIE0A); // сравнение с регистрам А включение прерывания
  OCR0A = 250;
  TCNT0 = 0; // сброс таймера
  sei(); // включить прерывания

  // АЦП
  ADMUX = 3; // сравнение с питанием, ацп на пине PB3

  // инициализация
  byte signalCounter = SIGNAL_TIME;
  printTime();

  while (1) {
    const byte selectedBtn = checkButtons();

    switch (PROGRAM_STATE) {
      case SET_MINUTES: {
        if (selectedBtn == BTN_PLUS && secondsCounter / 60 < 99) secondsCounter += 60;
        if (selectedBtn == BTN_MINUS && secondsCounter / 60 > 0) secondsCounter -= 60;
        if (selectedBtn == BTN_SET) PROGRAM_STATE = SET_SECONDS;
        if (selectedBtn != BTN_NOT_SELECTED) printTime();
        break;
      }
      case SET_SECONDS: {
        byte seconds = secondsCounter % 60;
        if (selectedBtn == BTN_PLUS && seconds < 60) secondsCounter += 10;
        if (selectedBtn == BTN_MINUS && secondsCounter > 0) secondsCounter -= 10;
        if (selectedBtn == BTN_SET) PROGRAM_STATE = WORK;
        if (selectedBtn != BTN_NOT_SELECTED) printTime();
        break;
      }
      case WORK: {
        if (selectedBtn == BTN_SET || selectedBtn == BTN_MINUS) setStartState();
        if (selectedBtn == BTN_PLUS) PROGRAM_STATE = PAUSE;
        break;
      }
      case PAUSE: {
        pointsOn = true;
        if (selectedBtn == BTN_PLUS) PROGRAM_STATE = WORK;
        if (selectedBtn == BTN_SET || selectedBtn == BTN_MINUS) setStartState();
        break;
      }
      case WORK_END: {
        if (isTimerTick && signalCounter > 0) signalCounter -= 1;

        byte isSignalDisable = signalCounter == 0 || selectedBtn != BTN_NOT_SELECTED;

        if (isSignalDisable) {
          setStartState();
          signalCounter = SIGNAL_TIME;
        }

        changePin(PIN_SIGNAL, isSignalDisable ? LOW : HIGH);

        break;
      }
      default: {
        setStartState();
        break;
      }
    }
    
    if (isTimerTick) {
      if (PROGRAM_STATE == WORK) {    
        if (secondsCounter == 0) PROGRAM_STATE = WORK_END;
        else secondsCounter -= 1;
        pointsOn = !pointsOn; 
        printTime();
      }

      isTimerTick = false;
    }

    _delay_ms(150);
  }
}
