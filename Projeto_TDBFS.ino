// Includes ------------------------
#include <TDBFS.h>
#include <AT24C256.h>
#include <TConfig.h>
#include <LiquidCrystal.h>
//#include "ds3231.h"
#include <MatrixKeyboard.h>
#include <PN532.h>
//#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <RTC_DS3231.h>
// End Includes -------------------
// Defines ------------------------
#define PASSWORDSIZE 4
#define SHOWDYSPLAYDELAYTIME 1000
#define STUDENTNUMBERSIZE 8
#define SCK 13
#define MOSI 11
#define SS 10
#define MISO 12

// -------------------------------- Defines RTC DS3231 -----------------------------------------
#define SQW_FREQ DS3231_SQW_FREQ_1     //  0b00000000  1Hz
//#define SQW_FREQ DS3231_SQW_FREQ_1024     //0b00001000   1024Hz
//#define SQW_FREQ DS3231_SQW_FREQ_4096  // 0b00010000   4096Hz
//#define SQW_FREQ DS3231_SQW_FREQ_8192 //0b00011000      8192Hz
//----------------------------------------------------------------------------------------------

// End defines --------------------
// Headers ------------------------
void readKeyPAD();
void readMifareID();
void menu();
void writeLCDMain();
void adjustDate();
void adjustHour();
void dumpMem();
void registerTeacherCard();
void classStart();
void classFinish();
void registerStudentNumber();
void rtc_setup();
void mifareOption(uint32_t id);
void menuAvaible(uint32_t card);
void stateDisplayChange(char key);
boolean changePassword();
boolean checkMasterPassword();
boolean isLeapYear(uint8_t year);
boolean checkValidDate(uint8_t day, uint8_t month, uint8_t year);
boolean checkValidHour(uint8_t hour, uint8_t minute, uint8_t seconds);
boolean adjustHourRTC(uint8_t hour, uint8_t minute, uint8_t seconds);
int64_t getLCDNumber (uint8_t size, uint8_t posX, uint8_t posY);
uint32_t readStudentNumber();
uint32_t readTargetID();
// End Headers --------------------

// Start Classes ------------------
AT24C256 mem(0x50);
TDBFS fs(mem);
LiquidCrystal lcd(8, 7, 5, 4, 3, 2);
MatrixKeyboard mk(A1, A2, A3);
RTC_DS3231 RTC;
PN532 nfc(SCK, MISO, MOSI, SS);

// end Classes --------------------
// Variables ----------------------
using namespace std;
int statemachine = 0;
boolean classWasStart = false;
volatile long TOGGLE_COUNT = 0;
// End Variables -------------------

/*------------------------------------------------------------------------------------
//####################################################################################
// INTERRUPT SERVICE ROUTINES
//####################################################################################

//ISR(TIMER0_COMPA_vect) {
//digitalWrite(LED_PIN, !digitalRead(LED_PIN));      // ^ 1);
//}

ISR(TIMER1_COMPA_vect) {
	//digitalWrite(LED_PIN, !digitalRead(LED_PIN));      // ^ 1);
	digitalWrite(LED_PIN, !digitalRead(LED_PIN));
	digitalWrite(LED_ONBAORD, !digitalRead(LED_ONBAORD)); //useful on nano's and some other 'duino's
	TOGGLE_COUNT++;
}

ISR(INT0_vect) {
	// Do something here
	//digitalWrite(LED_PIN, !digitalRead(LED_PIN));      // ^ 1);
	//TOGGLE_COUNT++;
}
------------------------------------------------------------------------------------*/

#define DEBUG

void setup() {
        lcd.begin(16, 2);
	lcd.print("Inicializando");

	Serial.begin(115200);
	Serial.println("TBF v. 0.1 init");

	Wire.begin();
	#ifdef DEBUG
	Serial.println("Wire initialized");
	#endif

	#ifdef DEBUG
	Serial.println("RTC initialized");
	#endif

	if (!TConfig::isValid()) {
		TConfig::clear();
		#ifdef DEBUG
		Serial.println("RTC initialized");
		#endif
	}
	
	#ifdef DEBUG
	Serial.println("LCD initialized");
	#endif
	
	RTC.begin();
        Serial.println("RTC_DS3231 RTC");
        Serial.println("RTC.begin()");
        if (! RTC.isrunning()) {
          Serial.println("RTC is NOT running!");
        } else {
          Serial.println("RTC is running!");
        }
        
        lcd.clear();
	writeLCDBegin();
        //fs.check();
        //mem.writeByte(0, 0x54);
}

void loop() {
	if (mk.longKeyPressed()) {
		while(mk.sample() == '#');
		menuAvaible((uint32_t)0xDEADBEEF);
	}
	readMifareID();
}

int32_t readPassword(){
	int32_t password = 0;
	for (int i = 0; i < PASSWORDSIZE; ++i) {
		lcd.setCursor(i,1);
		int digit = (int)mk.getdigit();
		if (digit == -1) {
			lcd.clear();
			return -1;
		} else {
			lcd.print("*");
			password = (password*10) + digit;
		}
	}
	lcd.clear();
	return password;
}

boolean promptAndCheckMasterPassword() {
	lcd.clear();
	lcd.print("Digite a senha");
	int32_t password = readPassword();
	if(password == -1) {
		return false;
	} else {
		return TConfig::checkAdminPassword(password);
	}
	return false;
}

void writeLCDBegin() {
	lcd.clear();
	lcd.print("Pressione # para");
	lcd.setCursor(0, 1);
	lcd.print("menu (3 seg.)");
	return;
}

void menuAvaible(uint32_t card) {
	boolean checkPass;
	if (TConfig::checkAdminCard(card)) {
		menu();
	} else if ((checkPass = promptAndCheckMasterPassword())) {
		menu();
	} else if (checkPass) {
		lcd.clear();
		lcd.write("Nao confere");
		delay(SHOWDYSPLAYDELAYTIME);
		writeLCDBegin();
		return;
	}
	return;
}

void menu() {
	lcd.print("Digite o modo de");
	lcd.setCursor(1,1);
	lcd.print("Inicializacao");
	boolean verify = 1;
	while(verify) {
	      	char key = mk.getch();
      		switch (key) {
      			case '1':
      			registerStudentNumber(); writeLCDBegin(); verify = 0; break;
      			case '2':
	      		classStart(); writeLCDBegin(); verify = 0; break;
      			case '3':
      			classFinish(); writeLCDBegin(); verify = 0; break;
      			case '4':
      			adjustHour(); writeLCDBegin(); verify = 0; break;
      			case '5':
      			adjustDate(); writeLCDBegin(); verify = 0; break;
      			case '6':
      			dumpMem(); writeLCDBegin(); verify = 0; break;
      			case '7':
      			registerTeacherCard(); writeLCDBegin();  verify = 0; break;
      			case '8':
      			changePassword(); writeLCDBegin(); verify = 0; break;
      			case '*':
      			stateDisplayChange(key); verify = 1; break;
      			case '#':
      			stateDisplayChange(key); verify = 1; break;
      			default:
      			writeLCDBegin(); verify = 0; return;
      		}
	}
	return;
}

void readMifareID() {
	uint32_t id;
	id = readTargetID();
	if (id != 0) {
		lcd.clear();
		lcd.print("Cartao Lido");
		delay(SHOWDYSPLAYDELAYTIME);
		lcd.clear();
		mifareOption(id);
		return;
	}
	return;
}

void adjustDate() {
	boolean valid = false;
	uint8_t day = 0;
	uint8_t month = 0;
	uint8_t year = 0;
	while (!valid) {
		lcd.clear();
		lcd.print("DD:MM:AAAA");
		
		day = (uint8_t)getLCDNumber(2, 0, 1);
		if (day == -1) {
			return;
		}                  
		
		month = (uint8_t)getLCDNumber(2, 3, 1);
		if (month == -1) {
			return;
		}
		
		year = (uint8_t)getLCDNumber(4, 6, 1);
		if (year == -1) {
			return;
		}
		
		if(checkValidDate(day, month, year)){
			adjustDateRTC(day, month, year);
			return;
		}
	}
	return;
}

int64_t getLCDNumber (uint8_t size, uint8_t posX, uint8_t posY) {
	int64_t number = 0;
	for (int i = posX; i < (size + posX); ++i) {
		lcd.setCursor(i,posY);
		int digit = mk.getdigit();
		if (digit == -1) {
			lcd.clear();
			lcd.print("Nao Salvo");
			delay(SHOWDYSPLAYDELAYTIME);
			writeLCDBegin();
			return -1;
		} else {
			lcd.print(digit);
			number = (number*10) + digit;
		}
	}
	return number;
}

boolean adjustDateRTC(uint8_t day, uint8_t month, uint8_t year) {
	DateTime now = RTC.now();
	DateTime newDateTime(year, month, day, now.hour(), now.minute(), now.second());
	RTC.adjust(newDateTime);
	return true;
}

boolean checkValidDate(uint8_t day, uint8_t month, uint8_t year) {
	if ((day > 0) && (day <= 31) && (month > 0) && (month <= 12) && (year > 0)) {
		if (((month == 4) || (month == 6) || (month == 9) || (month == 11)) && (day <= 31)) {
			return true;
		} else if ( (month == 2) && ( (day <= 28) || ( (day <= 29) && ( isLeapYear(year) ) ) ) ) {
			return true;
		} else {
			return false;
		}
		
	} else {
		return false;
	}
}

boolean isLeapYear(uint8_t year) {
	return ( ( year % 4 == 0 && year % 100 != 0 ) || year % 400 == 0 );
}

void adjustHour() {
	boolean valid = false;
	uint8_t hour = 0;
	uint8_t minute = 0;
	while (!valid) {
		lcd.clear();
		lcd.print("HH:MM");
		hour = (uint8_t)getLCDNumber(2, 0, 1);
		if (hour == -1) {
			return;
		}
		minute = (uint8_t)getLCDNumber(2, 3, 1);
		if (minute == -1) {
			return;
		}
		valid = checkValidHour(hour, minute, 0);
	}
	Serial.println("TryAdjust");
	lcd.clear();
	lcd.print("Ajustado");
	delay(SHOWDYSPLAYDELAYTIME);
	writeLCDBegin();
	adjustHourRTC(hour, minute, 0);
	return;
}

boolean adjustHourRTC(uint8_t hour, uint8_t minute, uint8_t seconds) {
	DateTime now = RTC.now();
	DateTime newDateTime(now.year(), now.month(), now.day(), hour, minute, seconds);
	RTC.adjust(newDateTime);
	return true;
}

boolean checkValidHour(uint8_t hour, uint8_t minute, uint8_t seconds) {
	if ((hour >= 0) && (minute >= 0) && (seconds >= 0) && (hour <= 23) && (minute <= 59) && (seconds <= 59)) {
		return true;
	} else {
		return false;
	}
	return false;
}

void dumpMem() {
	fs.memoryDump();
	return;
}

void registerTeacherCard() {
	uint32_t card = 0;
	lcd.clear();
	lcd.print("Aguardando Leitura");
	card = readTargetID();
	if (card == 0) {
		return;
	}
	TConfig::saveAdminCard(card);
	lcd.clear();
	lcd.print("Salvo");
	delay(SHOWDYSPLAYDELAYTIME);
	writeLCDBegin();
	return;
}

void classStart() {
	if (classWasStart) {
		lcd.clear();
		lcd.print("Alread Init");
		delay(SHOWDYSPLAYDELAYTIME);
		writeLCDBegin();
	} else {
		classWasStart = true;
		DateTime now = RTC.now();
		fs.startClass(now.year(), now.month(), now.day(), now.hour(), now.minute(), false);
		lcd.clear();
		lcd.print("Aula iniciada");
		delay(SHOWDYSPLAYDELAYTIME);
		writeLCDBegin();
	}
	return;
}

void classFinish() {
	if (!classWasStart) {
		return;
	}
	DateTime now = RTC.now();
	fs.finishClass(now.year(), now.month(), now.day(), now.hour(), now.minute(), false);
	classWasStart = false;
	lcd.clear();
	lcd.print("Aula finalizada");
	delay(SHOWDYSPLAYDELAYTIME);
	writeLCDBegin();
	return;
}

boolean changePassword() {
	boolean checkPass;
	if((checkPass = promptAndCheckMasterPassword())){
		int32_t password1;
		int32_t password2;
		lcd.clear();
		lcd.print("Digite a senha");
		password1 = readPassword();
		if (password1 == -1) {
			return false;
		}
		lcd.clear();
		lcd.print("Redigite a senha");
		password2 = readPassword();
		if (password1 == password2) {
			TConfig::saveAdminPassword((uint16_t)password1);
			lcd.clear();
			lcd.print("Salvo!");
			delay(SHOWDYSPLAYDELAYTIME);
			writeLCDBegin();
			return true;
		} else {
			lcd.clear();
			lcd.print("Nao Salvo!");
			delay(SHOWDYSPLAYDELAYTIME);
  			writeLCDBegin();
			return false;
		}
	} else if (!checkPass) {
		lcd.clear();
		lcd.print("Senha não confere!");
		delay(SHOWDYSPLAYDELAYTIME);
		writeLCDBegin();
		return false;
	} else {
		lcd.clear();
		lcd.print("Nao Salvo!");
		delay(SHOWDYSPLAYDELAYTIME);
		writeLCDBegin();
		return false;
	}
	return false;
}

uint32_t readStudentNumber() {
	int64_t studentNumber = getLCDNumber(STUDENTNUMBERSIZE, 0, 1);
	if (studentNumber == -1) {
		return 0;
	}
	return (uint32_t)studentNumber;
}

void registerStudentNumber() {
	if(!classWasStart){
		return;
	}
	lcd.clear();
	lcd.print("Digite o RA");
	uint32_t studentNumber = readStudentNumber();
	if (studentNumber == 0) {
		return;
	}
	DateTime now = RTC.now();
	fs.recordPresenceStudentNumber(studentNumber, now.year(), now.month(), now.day(), now.hour(), now.minute(), false);
	lcd.clear();
	lcd.print("Salvo!");
	delay(SHOWDYSPLAYDELAYTIME);
	writeLCDBegin();
	return;
}

void mifareOption(uint32_t id) {
	if (TConfig::checkAdminCard(id)){
		menu();
		return;
	} else if (!fs.findStudent(id)) {
		lcd.clear();
		lcd.print("Digite o RA");
		uint32_t studentNumber = readStudentNumber();
		if (studentNumber != 0) {
			fs.addStudent(id, studentNumber);
			lcd.clear();
			lcd.print("Salvo!");
			delay(SHOWDYSPLAYDELAYTIME);
			writeLCDBegin();
		} else {
			lcd.clear();
			lcd.print("Nao Salvo!");
			delay(SHOWDYSPLAYDELAYTIME);
			writeLCDBegin();
		}
        } else if (classWasStart) {
		DateTime now = RTC.now();
		fs.recordPresenceCard(id, now.year(), now.month(), now.day(), now.hour(), now.minute(), false);
		lcd.clear();
		lcd.print("Salvo!");
		delay(SHOWDYSPLAYDELAYTIME);
		writeLCDBegin();
		return;
	} else {
                lcd.clear();
		lcd.print("Aula nao comecou!");
		delay(SHOWDYSPLAYDELAYTIME);
		writeLCDBegin();
        }
	return;
}

uint32_t readTargetID() {
 	 uint32_t targetID = 0;
	 nfc.enable();
	 nfc.begin();
	 uint32_t versiondata = nfc.getFirmwareVersion();
	 if (! versiondata) {
		 Serial.print("Didn't find PN53x board");
		 while (1); // halt
	 }
	 nfc.SAMConfig();
	 while (targetID == 0) {
		targetID = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A);
		if (mk.sample()=='#') {
			nfc.disable();
			return 0;
		}
	 }
	 nfc.disable();
	 return targetID;
}

void stateShowDisplay() {
    switch (statemachine) {
    	case 0:
	lcd.clear(); lcd.print("Digite o modo de"); lcd.setCursor(0, 1); lcd.print(" Inicializacao"); break;
    	case 1:
    	lcd.clear(); lcd.print("1-Registra"); lcd.setCursor(0, 1); lcd.print("2-Comeca Aula"); break;
    	case 2:
    	lcd.clear(); lcd.print("3-Finaliza Aula"); lcd.setCursor(0, 1); lcd.print("4-Ajusta Hora"); break;
    	case 3:
    	lcd.clear(); lcd.print("5-Ajusta Data"); lcd.setCursor(0, 1); lcd.print("6-dumpMem"); break;
    	case 4:
    	lcd.clear(); lcd.print("7-Registra TCard"); lcd.setCursor(0, 1); lcd.print("8-Troca Senha"); break;
        case 5:
    	lcd.clear(); lcd.print("9-Cancela"); lcd.setCursor(0, 1); lcd.print("0-Cancela"); break;
    	default:
    	return;
    }
}


void stateDisplayChange(char key) {
    if (key == '#'){
      statemachine++;
    } else if (key == '*'){
      statemachine--;
    }
    if (statemachine < 0) {
      statemachine = 4;
    } else if (statemachine > 5) {
      statemachine = 0;
    }
    stateShowDisplay();
 }
