/*
   Project name: Mini przejazd kolejowy
   Functions: Podnoszenie/opuszczanie szlabanu,miganie 2 diodami,buczenie buzzerem gdy podjezdza pociag aż do jego odjazdu
   Features: Sterowanie janością diód, określenie długości czasu przejazdu pociągu
   Upgrades: Zamiana na program obiektowy, zastosowanie buzzera bez generatora [użycie funkcji tune()]
   Warnings: Maksymalny czas ciągłej pracy to 50dni (zmienna unsigned int przechwouje czas)
   Authors: Kamil Jamros
*/

#include <Servo.h> //Biblioteka odpowiedzialna za serwomechanizm
/*
   Wybór czy używamy PWM
*/

//Czy chcesz sterować janością diód ? Jeżeli nie, zakomentuj linijkę poniżej
#define USEPWMLED

/*
    PINs definitions
*/

// Wybierz jeden z pinów obsługujących PWM : 11,10,9,6,5,3
#define SG90PIN 9
//W ardunio UNO, detektor może być tylko na pinie 2 lub 3 (tylko na tych pinach można obsługiwać przerwania zewnętrzne)
#define DETEKTORPIN 2
#define BUZZERPIN 11

// Jeżeli chcesz sterować janością diód i głośnocią buzzera wybierz piny, które to obsługują (PWM) :
// 11,6,5,3 [piny 9 i 10 są używane w bibliotece "servo.h")
#define LED1PIN 6
#define LED2PIN 5

/*
   Enumeration definition
*/

// Typ wyliczeniowy określający możliwe akcje szlabanu
enum akcjeSzlabanu {
  opuszczanie,
  wznoszenie,
  oczekiwanieNaPrzejazd,
  oczekiwanieNaPociag
};

/*
   Variables definitions
*/

//Tworzenie obiektu typu wyliczeniowego zawierającego aktualnie wykonywana akcję
volatile akcjeSzlabanu aktualnaAkcjaSzlabanu = oczekiwanieNaPociag; //Volatile, ponieważ zmienna jest używana w przerwaniu onStep()
volatile unsigned long czasPierwszegoZbocza = 0; //Zmienna przechowujaca czas wykrycia pociągu
volatile unsigned long czasOstatniegoZbocza = 0; //Zmienna przechowujaca czas końca wykrywania pociągu
const int dodatkowyCzas = 500UL; //Określenie dodatkowego czasu wymaganego do przejazdu pociągu od punktu wykrywania do szlabanu

unsigned long aktualnyCzas = 0; //Zmienna wykorzystywana do przechowania czasu od uruchomienia płytki

Servo SG90;  //Tworzymy obiekt, dzięki któremu możemy odwołać się do serwomechanizmu
const int pozycjaPoczatkowa = 90; //Początkowa pozycja serwa 0-180
const int zmiana = 12; //Co ile ma się zmieniać pozycja serwa?
const int opoznieniePomiedzySkokami = 100UL; //co ile ms ma być nowa pozycja seromechanizmu
//const int czasPrzejazdu = 5000UL; //Czas pomiędzy otwarciem, a zamykaniem szlabanu
const int maksWychylenie = 180; //Maksymalne wychylenie 0-180
unsigned long zapamietanyCzasSzlaban = 0; //Wykorzystywane przy obliczaniu czasu opóżnien
int aktualnaPozycja = maksWychylenie; //Pozycja wyjściowa serwomechanizmu

int stanLED1 = LOW;
unsigned long zapamietanyCzasLED = 0; //Analogicznie do zapamietanyCzasSzlaban
unsigned long czasSwiecieniaDiod = 500UL;  // Czas świecenia i nieświecenia się diód

// Sprawdzenie czy zdefiniowano definicje USEPWMLED
#ifdef USEPWMLED
const int janoscLED1 = 255; //Okreslenie jasności LED 1 0-255
const int janoscLED2 = 15; //Okreslenie jasności LED 2 0-255
#endif

int stanBuzzer = LOW;
unsigned long zapamietanyCzasBuzzer = 0; //Analogicznie do zapamietanyCzasSzlaban
unsigned long czasWlaczeniaBuzzera = 500UL;  // Czas piszczenie i niepiszczenia buzzera

void setup()
{
  pinMode(DETEKTORPIN, INPUT_PULLUP); //Ustawiamy kierunek pinu detektora na wejście z podciąganiem do Vcc
  attachInterrupt(digitalPinToInterrupt(DETEKTORPIN), onStep, CHANGE ); //Włączamy obsługę przerwań na pinie 2
  //Funkcja attachInterrupt przyjmuje jako argument numer przerwania,
  //więc używamy funkcji digitalPinToInterrupt do zamiany numeru pinu na numer przerwania,
  //drugi argument to funkcja, która ma się wykonać po wykryciu zdarzenia,
  //trzeci argument to typ zbocza, na który ma zaaragować program

  SG90.attach(SG90PIN);  //Serwomechanizm podłączony do pinu 9

  pinMode(LED1PIN, OUTPUT);      // ustawiamy LED1PIN jako wyjście
  pinMode(LED2PIN, OUTPUT);      // ustawiamy LED2PIN jako wyjście

  pinMode(BUZZERPIN, OUTPUT);      // ustawiamy BUZZERPIN jako wyjście

  ustawieniaPoczatkowe();       // Procedura odpowiedzialna za ustawienia początkowe dla serwa, diód oraz buzzera
}

void ustawieniaPoczatkowe() {
  SG90.write(maksWychylenie); //Ustawienie pozycji początkowej serwomechanizmu na maksymalne wychylenie
  digitalWrite(LED1PIN, LOW); // Zgaszenie diody 1
  digitalWrite(LED2PIN, LOW); // Zgaszenie diody 2
  digitalWrite(BUZZERPIN, LOW); // Wyłączenie buzzera
}

void loop()
{
  //Sprawdzenie czy nadjeżdza pociąg
  if (aktualnaAkcjaSzlabanu != oczekiwanieNaPociag) {
    aktualnyCzas = millis(); //Pobierz liczbe milisekund od startu płytki
    obslugaDiod(aktualnyCzas); //Procedura obsługi migania diód
    obslugaBuzzera(aktualnyCzas); //Procedura obsługi buzzera
    obslugaSzlabanu(aktualnyCzas); //Procedura obsługi szlabanu
  }
}

void onStep()
{
  //Wykrycie zbliżającego się pociągu
  czasOstatniegoZbocza = millis(); //Zapisanie czasu końca wykrycia pociągu w celu obliczenia wymaganego czasu przejazdu
  if (aktualnaAkcjaSzlabanu == oczekiwanieNaPociag) {
    aktualnaAkcjaSzlabanu = opuszczanie; //Rozpoczęcie algorytmu (zmiana akcji szlabanu na opuszczanie)
    czasPierwszegoZbocza = millis(); //Zapisanie czasu wykrycia pociągu w celu obliczenia wymaganego czasu przejazdu
  }
}

void obslugaDiod(unsigned long aktualnyCzas) {
  //Sprawdzenie czy mineło ponad "czasSwiecieniaDiod" ms
  if (aktualnyCzas - zapamietanyCzasLED >= czasSwiecieniaDiod) {

    zapamietanyCzasLED = aktualnyCzas; //Zapamietanie aktualnego czasu

    stanLED1 = !stanLED1; // Zmiana stanu diody na przeciwny (operacja NOT logiczna)

    // Sprawdzenie czy zdefiniowano definicje USEPWMLED
#ifdef USEPWMLED
    if (stanLED1 == LOW) {
      digitalWrite(LED1PIN, stanLED1); //Wyłączenie diody LED1 (analogWrite(LED1PIN,0) - nie gwarantuję stanu niskiego dla pinów 5 i 6)
      analogWrite(LED2PIN, janoscLED2); //Włączenie PWM na pinie LED2PIN z wypełnieniem "jasnoscLED2"
    }
    else {
      analogWrite(LED1PIN, janoscLED1);
      digitalWrite(LED2PIN, !stanLED1); // Wyłączenie diody LED2
    }
#else
    digitalWrite(LED1PIN, stanLED1);
    digitalWrite(LED2PIN, !stanLED1);
#endif
  }
}

void obslugaBuzzera(unsigned long aktualnyCzas) {
  if (aktualnyCzas - zapamietanyCzasBuzzer >= czasWlaczeniaBuzzera) {
    zapamietanyCzasBuzzer = aktualnyCzas;
    stanBuzzer = !stanBuzzer;
    digitalWrite(BUZZERPIN, stanBuzzer);
  }
}

void obslugaSzlabanu(unsigned long aktualnyCzas) {
  // Switch obslugujacy "maszyne stanów"
  switch (aktualnaAkcjaSzlabanu) {
    case opuszczanie:
      opuszczanieSzlabanu(aktualnyCzas);
      break;
    case wznoszenie:
      wznoszenieSzlabanu(aktualnyCzas);
      break;
    case oczekiwanieNaPrzejazd:
      oczekiwanieNaPrzejazdPociagu(aktualnyCzas);
      break;
  }
}

void opuszczanieSzlabanu(unsigned long aktualnyCzas) {
  //Podnoś dopóki nie osiągnie określone maksimum
  if (aktualnaPozycja > pozycjaPoczatkowa) {
    //Oczekiwanie na upłyniecię "opoznieniePomiedzySkokami" ms
    if (aktualnyCzas - zapamietanyCzasSzlaban >= opoznieniePomiedzySkokami) {
      zapamietanyCzasSzlaban = aktualnyCzas; //Zapamiętanie czasu ostatniej akcji
      aktualnaPozycja = aktualnaPozycja - zmiana; //Zwiększenie aktualnej pozycji serwamechanizmu
      SG90.write(aktualnaPozycja); //Wykonaj ruch
    }
  }
  else {
    aktualnaAkcjaSzlabanu = oczekiwanieNaPrzejazd; //Zmiana akacji na oczekiwanie
  }
}

void oczekiwanieNaPrzejazdPociagu(unsigned long aktualnyCzas) {
  //Sprawdzeni czy szlaban jest zamknięty przez czas wymagany do przejazdu pociągu + dodatkowyCzas
  if (aktualnyCzas - zapamietanyCzasSzlaban >= czasOstatniegoZbocza - czasPierwszegoZbocza + 2 * dodatkowyCzas) {
    zapamietanyCzasSzlaban = aktualnyCzas; //Zapamiętanie czasu, w celu jego użycia w stanie "wznoszenie szlabanu"
    aktualnaAkcjaSzlabanu = wznoszenie;  //Zmiana akcji na wznoszenie szlabanu
  }
}

void wznoszenieSzlabanu(unsigned long aktualnyCzas) {
  //Opuszczaj szlaban dopóki nie osiągnie okreslonego minimum
  if (aktualnaPozycja < maksWychylenie) {
    //Oczekiwanie na upłyniecię "opoznieniePomiedzySkokami" ms
    if (aktualnyCzas - zapamietanyCzasSzlaban >= opoznieniePomiedzySkokami) {
      zapamietanyCzasSzlaban = aktualnyCzas;
      aktualnaPozycja = aktualnaPozycja + zmiana; //Zwiększenie aktualnej pozycji serwa
      SG90.write(aktualnaPozycja); //Wykonaj ruch
    }
  }
  else {
    aktualnaAkcjaSzlabanu = oczekiwanieNaPociag; //Zmiana akcji na oczekiwanie na nowy pociag
    ustawieniaPoczatkowe();
  }
}
