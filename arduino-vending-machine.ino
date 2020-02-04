#include <LiquidCrystalRus.h>
#include <Keypad.h>

const byte KEYPADROWS = 4; // 4 строки
const byte KEYPADCOLS = 4; // 4 столбца

const unsigned int LCDCHARWIDTH = 20;
const unsigned int LCDCHARHEIGHT = 4;

const unsigned int MAXSUCCESSCODES = 30;
const unsigned int MAXCODEPLACES = 8;
const unsigned int places[MAXCODEPLACES][2] = {
  {1, 0}, {1, 1}, {1, 2}, {1, 3},
  {LCDCHARWIDTH / 2, 0}, {LCDCHARWIDTH / 2, 1}, {LCDCHARWIDTH / 2, 2}, {LCDCHARWIDTH / 2, 3}
};
const unsigned int MAXADMINPAGES = ceil(MAXSUCCESSCODES / MAXCODEPLACES) + 1;

// --------------Пины--------------

// Инициализация дисплея
LiquidCrystalRus lcd(6, 7, 8, 9, 10, 11);

// Выводные диджитал пины
const int SUCCESSPIN = 13;

// Входные диджитал пины
const int REMOTECONTROLPIN = 44;

// Пины клавиатуры (строки, колонки)
byte rowPins[KEYPADROWS] = {29, 28, 27, 26};
byte colPins[KEYPADCOLS] = {25, 24, 23, 22};

// --------------Настройки--------------

// Время
const unsigned long BLOCKTIMEOUT = 1800;  // В секундах
const unsigned long SUCCESSTIMEOUT = 3;  // В секундах
const unsigned long SUCCESSPINTIMEOUT = 500;  // В миллисекундах
const unsigned long WRONGCODETIMEOUT = 30;  // В секундах
const unsigned long DEBOUNCEDELAY = 500;  // В миллисекундах

// Попытки на ввод пинкода
const int MAXTRIES = 3;

// Административный пинкод
const String ADMINCODE = "147*0#";

// Коды
String successCodes[MAXSUCCESSCODES] = {
  "30*901", "941114", "*16453", "02#6**", "*79#60",
  "816199", "#17424", "#45*#9", "019*62", "4#77*3",
  "8#97*6", "443872", "*7944*", "496418", "924#95",
  "*438*9", "973233", "239#65", "918455", "86*880",
  "#05973", "0239#9", "994*19", "8#2025", "484871",
  "240513", "066182", "*9#0*2", "0437#4", "557189"
};

// Использования
bool codeUsed[MAXSUCCESSCODES] = {
  false, false, false, false, false,
  false, false, false, false, false,
  false, false, false, false, false,
  false, false, false, false, false,
  false, false, false, false, false,
  false, false, false, false, false,
};

// --------------Состояния--------------

// Состояния выходных пинов
int successPinState = LOW;

// Состояния входных пинов
int remoteControlPinState = LOW;

// Хранилища временных меток для рассчёта задержек
unsigned long blockStartTime = 0;
unsigned long successStartTime = 0;
unsigned long successPinStartTime = 0;
unsigned long wrongCodeStartTime = 0;
unsigned long lastDebounceTime = 0;

// Хранилища переменных среды
unsigned int tries = MAXTRIES;
String currentCode = "";
unsigned int adminPage = 1;
unsigned int setCodeNum = 0;

// Настройки клавиатуры
char keys[KEYPADROWS][KEYPADCOLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// Создаём клавиатуру
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, KEYPADROWS, KEYPADCOLS );

// Возможные состояния автомата
enum States
{
  OPEN_STATE,
  CLOSED_STATE,
  SUCCESS_STATE,
  BLOCKED_STATE,
  ADMIN_STATE,
  SETCODES_STATE
};

// Текущее и предыдущее состояние автомата
States currentState;
States previousState;

void setup() {
  // Устанавливаем колличество колонок и строк дисплея
  lcd.begin(LCDCHARWIDTH, LCDCHARHEIGHT);

  // set the digital pin as output:
  pinMode(SUCCESSPIN, OUTPUT);

  // initialize the digital pin as an input:
  pinMode(REMOTECONTROLPIN, INPUT);

  // Рисуем экран
  notify("", "Добро", "пожаловать", "");

  // Инициализируем текущее состояние
  toOpen();
}

void loop() {
  digitalWrite(SUCCESSPIN, successPinState);
  checkRemoteControl();

  bool charGot = false;
  if (tries < MAXTRIES && (when() - wrongCodeStartTime > WRONGCODETIMEOUT)) {
    tries = MAXTRIES;
  }

  switch (currentState)
  {
    case OPEN_STATE:
      charGot = inputDefault();
      if (charGot) {
        drawOPEN_STATE();
      }
      break;
    case CLOSED_STATE:
      charGot = inputDefault();
      if (charGot) {
        drawCLOSED_STATE();
      }
      break;
    case SUCCESS_STATE:
      charGot = inputDefault();
      if (millis() - successPinStartTime > SUCCESSPINTIMEOUT) {
        successPinState = LOW;
      }
      if (when() - successStartTime > SUCCESSTIMEOUT) {
        for (int i = 0; i < MAXSUCCESSCODES; i++) {
          if (!codeUsed[i]) {
            return toOpen();
          }
        }
        return toClosed();
      }
      break;
    case BLOCKED_STATE:
      drawBLOCKED_STATE();
      charGot = inputDefault();
      if (when() - blockStartTime > BLOCKTIMEOUT) {
        return toOpen();
      }
      break;
    case ADMIN_STATE:
      charGot = inputAdmin();
      if (charGot) {
        drawADMIN_STATE();
      }
      break;
    case SETCODES_STATE:
      charGot = inputSetCodes();
      if (charGot) {
        drawSETCODES_STATE();
      }
      break;
  }
}

// --------------Отображение--------------

void drawState(States state) {
  switch (state)
  {
    case OPEN_STATE:
      drawOPEN_STATE();
      break;
    case CLOSED_STATE:
      drawCLOSED_STATE();
      break;
    case SUCCESS_STATE:
      drawSUCCESS_STATE();
      break;
    case BLOCKED_STATE:
      drawBLOCKED_STATE();
      break;
    case ADMIN_STATE:
      drawADMIN_STATE();
      break;
    case SETCODES_STATE:
      drawSETCODES_STATE();
      break;
  }
}

void drawOPEN_STATE() {
  lcd.setCursor(4, 1);
  lcd.print("Введите код:");
  lcd.setCursor(7, 2);
  String tmp = currentCode;
  for (int i = 0; i < 6 - currentCode.length(); i++) {
    tmp += "_";
  }
  lcd.print(tmp);
  lcd.setCursor(1, 3);
  lcd.print("A)Стереть D)Готово");
  drawFrame();
}

void drawCLOSED_STATE() {
  lcd.setCursor(3, 1);
  lcd.print("Магазин");
  lcd.setCursor(11, 2);
  lcd.print("закрыт");
  drawFrame();
}

void drawSUCCESS_STATE() {
  lcd.setCursor(5, 1);
  lcd.print("Верный код");
  drawFrame();
}

void drawBLOCKED_STATE() {
  lcd.setCursor(4, 1);
  lcd.print("Заблокировано");
  lcd.setCursor(6, 2);
  lcd.print(
    formatTime(
      BLOCKTIMEOUT - (when() - blockStartTime)
    )
  );
  drawFrame();
}

void drawADMIN_STATE() {
  switch (adminPage)
  {
    case 1:
      lcd.setCursor(0, 0);
      lcd.print("A/B)Листать  Админка");
      lcd.setCursor(0, 1);
      lcd.print("С)НовКод  D)Выйти  1");
      lcd.setCursor(0, 2);
      lcd.print("1)кВводу  3)кБлоку /");
      lcd.setCursor(0, 3);
      lcd.print("2)кЗакрыто4)кУспех " + MAXADMINPAGES);
      break;
    default:
      int shift = MAXCODEPLACES * (adminPage - 2);
      for (int i = 0; i < MAXCODEPLACES; i++) {
        int t = i + shift;
        if (t >= MAXSUCCESSCODES) {
          break;
        }
        String r = "";
        if (codeUsed[t]) {
          r = "П";
        }
        lcd.setCursor(places[i][0], places[i][1]);
        lcd.print(successCodes[t] + "-" + r);
      }
      lcd.setCursor(19, 1);
      lcd.print(adminPage);
      lcd.setCursor(19, 2);
      lcd.print("/");
      lcd.setCursor(19, 3);
      lcd.print(String(MAXADMINPAGES));
      break;
  }
}

void drawSETCODES_STATE() {
  unsigned int codeNum = setCodeNum + 1;
  lcd.setCursor(1, 0);
  lcd.print("A)Стереть B)Рандом");
  lcd.setCursor(8, 1);
  lcd.print(formatDyadicSection(codeNum) + "/" + formatDyadicSection(MAXSUCCESSCODES));
  String tmp = currentCode;
  for (int i = 0; i < 6 - currentCode.length(); i++) {
    tmp += "_";
  }
  lcd.setCursor(7, 2);
  lcd.print(tmp);
  lcd.setCursor(1, 3);
  lcd.print("С)Отмена  D)Готово");
}

void drawFrame() {
  lcd.setCursor(0, 0);
  lcd.print("#");
  lcd.setCursor(0, 1);
  lcd.print("#");
  lcd.setCursor(0, 2);
  lcd.print("#");
  lcd.setCursor(0, 3);
  lcd.print("#");
  lcd.setCursor(19, 0);
  lcd.print("#");
  lcd.setCursor(19, 1);
  lcd.print("#");
  lcd.setCursor(19, 2);
  lcd.print("#");
  lcd.setCursor(19, 3);
  lcd.print("#");
  lcd.setCursor(0, 0);
}

void notify(String a, String b, String c, String d) {
  lcd.clear();
  lcd.setCursor(LCDCHARWIDTH / 2 - a.length() / 4, 0);
  lcd.print(a);
  lcd.setCursor(LCDCHARWIDTH / 2 - b.length() / 4, 1);
  lcd.print(b);
  lcd.setCursor(LCDCHARWIDTH / 2 - c.length() / 4, 2);
  lcd.print(c);
  lcd.setCursor(LCDCHARWIDTH / 2 - d.length() / 4, 3);
  lcd.print(d);
  delay(1000);
  lcd.clear();
  drawState(currentState);
}

// --------------Переходы--------------

void toOpen() {
  changeState(OPEN_STATE);
  drawOPEN_STATE();
}

void toClosed() {
  changeState(CLOSED_STATE);
  drawCLOSED_STATE();
}

void toSuccess() {
  successStartTime = when();
  successPinStartTime = millis();
  changeState(SUCCESS_STATE);
  drawSUCCESS_STATE();
}

void toBlock() {
  blockStartTime = when();
  changeState(BLOCKED_STATE);
  drawBLOCKED_STATE();
}

void toAdmin() {
  adminPage = 1;
  States tmp[4] = {OPEN_STATE, CLOSED_STATE, SUCCESS_STATE, BLOCKED_STATE};
  for (int i = 0; i < 4; i++) {
    if (currentState == tmp[i]) {
      previousState = currentState;
      break;
    }
  }
  changeState(ADMIN_STATE);
  drawADMIN_STATE();
}

void toSetCodes() {
  changeState(SETCODES_STATE);
  drawSETCODES_STATE();
}

void changeState(States s) {
  lcd.clear();
  currentCode = "";
  if (s == SUCCESS_STATE) {
    successPinState = HIGH;
  } else {
    successPinState = LOW;
  }
  currentState = s;
}

void switchState() {
  switch (currentState)
  {
    case OPEN_STATE:
      toClosed();
      break;
    case CLOSED_STATE:
      toOpen();
      break;
  }
}

// --------------Утилиты--------------

void checkRemoteControl() {
  if ((millis() - lastDebounceTime) > DEBOUNCEDELAY) {
    int reading = digitalRead(REMOTECONTROLPIN);
    if (reading != remoteControlPinState) {
      remoteControlPinState = reading;
      lastDebounceTime = millis();
      if (remoteControlPinState == HIGH) {
        switchState();
      }
    }
  }
}

bool inputDefault() {
  char key = keypad.getKey();
  if (key) {
    String deleteChars = "ABC";
    String confirmChars = "D";
    if (deleteChars.indexOf(key) != -1) {
      if (currentCode.length() > 0) {
        currentCode.remove(currentCode.length() - 1);
        lcd.clear();
      }
    } else if (confirmChars.indexOf(key) != -1) {
      checkCode();
      return false;
    } else if (currentCode.length() < 6) {
      currentCode += key;
    }
    return true;
  }
  return false;
}

bool inputAdmin() {
  char key = keypad.getKey();
  if (key) {
    String forvardChars = "A";
    String backChars = "B";
    String newCodesChars = "C";
    String exitChars = "D";
    String toOpenChars = "1";
    String toClosedChars = "2";
    String toBlockChars = "3";
    String toSuccessChars = "4";
    if (backChars.indexOf(key) != -1) {
      if (adminPage > 1) {
        adminPage--;
        lcd.clear();
        return true;
      }
    } else if (forvardChars.indexOf(key) != -1) {
      if (adminPage < MAXADMINPAGES) {
        adminPage++;
        lcd.clear();
        return true;
      }
    } else if (newCodesChars.indexOf(key) != -1) {
      toSetCodes();
    } else if (exitChars.indexOf(key) != -1) {
      switch (previousState) {
        case OPEN_STATE:
          toOpen();
          break;
        case CLOSED_STATE:
          toClosed();
          break;
        case SUCCESS_STATE:
          toSuccess();
          break;
        case BLOCKED_STATE:
          toBlock();
          break;
      }
    } else if (toOpenChars.indexOf(key) != -1) {
      toOpen();
    } else if (toClosedChars.indexOf(key) != -1) {
      toClosed();
    } else if (toBlockChars.indexOf(key) != -1) {
      toBlock();
    } else if (toSuccessChars.indexOf(key) != -1) {
      toSuccess();
    }
  }
  return false;
}

bool inputSetCodes() {
  char key = keypad.getKey();
  if (key) {
    String deleteChars = "A";
    String randomChars = "B";
    String exitChars = "C";
    String confirmChars = "D";
    if (deleteChars.indexOf(key) != -1) {
      if (currentCode.length() > 0) {
        currentCode.remove(currentCode.length() - 1);
        lcd.clear();
      }
    } else if (randomChars.indexOf(key) != -1) {
      currentCode = generateRandomCode();
    } else if (exitChars.indexOf(key) != -1) {
      toAdmin();
      return false;
    } else if (confirmChars.indexOf(key) != -1) {
      if (currentCode.length() == 6) {
        successCodes[setCodeNum] = currentCode;
        codeUsed[setCodeNum] = false;
        currentCode = "";
        setCodeNum++;
        if (setCodeNum >= MAXSUCCESSCODES) {
          setCodeNum = 0;
          toOpen();
          return false;
        }
        lcd.clear();
      }
    } else if (currentCode.length() < 6) {
      currentCode += key;
    }
    return true;
  }
  return false;
}

void checkCode() {
  if (currentCode.equals(ADMINCODE)) {
    currentCode = "";
    return toAdmin();
  }
  if (currentState != OPEN_STATE) {
    currentCode = "";
    return;
  }
  for (int i = 0; i < MAXSUCCESSCODES; i++) {
    if (currentCode.equals(successCodes[i]) && !codeUsed[i]) {
      currentCode = "";
      codeUsed[i] = true;
      return toSuccess();
    }
  }
  tries--;
  currentCode = "";
  notify("", "Ошибочный код", "", "");
  if (tries == MAXTRIES - 1) {
    wrongCodeStartTime = when();
  }
  if (tries < 1) {
    tries = MAXTRIES;
    return toBlock();
  }
}

String generateRandomCode() {
  const int MAXALLOWEDCHARS = 12;
  char allowedChars[MAXALLOWEDCHARS] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '*', '#'};
  String r = "";
  for (int i = 0; i < 6; i++) {
    r.concat(allowedChars[random(0, MAXALLOWEDCHARS)]);
  }
  if (r == ADMINCODE) {
    return generateRandomCode();
  }
  return r;
}

unsigned int when() {
  return millis() / 1000;
}

String formatTime(unsigned int secCount) {
  int minutes = secCount / 60;
  int hours = minutes / 60;
  minutes = minutes % 60;
  secCount = secCount % 60;
  return formatDyadicSection(hours) + ":" + formatDyadicSection(minutes) + ":" + formatDyadicSection(secCount);
}

String formatDyadicSection(unsigned int c) {
  if (c < 1) {
    return String("00");
  }
  if (c < 10) {
    return "0" + String(c);
  }
  return String(c);
}
