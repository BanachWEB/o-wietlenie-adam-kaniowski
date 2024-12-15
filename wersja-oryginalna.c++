#define BUFFER_SIZE 64
#include <Arduino.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_NeoPixel.h>

// Adresy czujników VL53L0X
#define ADRES_LOX1 0x30
#define ADRES_LOX2 0x31

// Pin do podłączenia diod LED
#define PIN_LED 8
#define ILOSC_LED 950

// Piny do obsługi XSHUT czujników
#define PIN_CZUJNIK1_XSHUT 7
#define PIN_CZUJNIK2_XSHUT 6
#define JASNOSC 30  // Ustaw jasność na wartość od 0 do 255
#define KROK_TECZY 512  // Zwiększony krok tęczy


char buffer[BUFFER_SIZE];
int bufferIndex = 0;

// Inicjalizacja obiektów do obsługi czujników i diod LED
Adafruit_VL53L0X loxCzujnik1;
Adafruit_VL53L0X loxCzujnik2;
Adafruit_NeoPixel strip(ILOSC_LED, PIN_LED, NEO_GRB + NEO_KHZ800);

// Zmienne przechowujące odległości od czujników i aktualne animacje
int odleglosc1 = 0;                    // Odległość od przeszkody czujnika 1
int odleglosc2 = 0;                    // Odległość od przeszkody czujnika 2
int aktualnaAnimacjaCzujnika1 = 0;     // Numer aktualnej animacji dla czujnika 1
int aktualnaAnimacjaCzujnika2 = 0;     // Numer aktualnej animacji dla czujnika 2

// Stałe do konfiguracji programu
const int liczbaAnimacji = 3;          // Liczba dostępnych animacji
const int minimalnaOdleglosc = 150;    // Minimalna odległość do aktywacji czujników
const int maksymalnaOdleglosc = 700;   // Maksymalna odległość do aktywacji czujników
const int szybkoscZmianyJasnosci = 100; // Szybkość zmiany jasności diod LED
const int timingBudget = 83000;        // Czas pomiaru czujników VL53L0X w mikrosekundach

// Flaga aktywacji czujników
bool czujnik1Aktywny = false;          // Flaga aktywacji ruchu dla czujnika 1
bool czujnik2Aktywny = false;          // Flaga aktywacji ruchu dla czujnika 2

// Czasy do kontrolowania zmian animacji i świecenia
const long czasZmianyAnimacji = 4000;          // Okres czasu między zmianami animacji
const long czasSwiecenia = 18000;               // Czas trwania świecenia diod LED po wykryciu ruchu
const long czasZablokowaniaCzujnikow = 3000;   // Czas blokowania czujników po detekcji ruchu

unsigned long ostatniCzas = 0;
unsigned long aktualnyKatTeczy = 0;
unsigned long poprzedniCzasAnimacjiTecza = 0;
unsigned long poprzedniCzasTecza = 0;
const int interwalAnimacjiTecza = 200; // Czas między klatkami animacji w milisekundach

unsigned long poprzedniCzasGwiezdneNiebo = 0;
const int interwalAnimacjiGwiezdneNiebo = 100; // Czas między klatkami animacji w milisekundach


unsigned long czasZmianyAnimacjiCzujnik1 = 0;  // Czas ostatniej zmiany animacji dla czujnika 1
unsigned long czasZmianyAnimacjiCzujnik2 = 0;  // Czas ostatniej zmiany animacji dla czujnika 2
unsigned long czasRozpoczeciaSwiecenia = 0;    // Czas rozpoczęcia świecenia diod LED po detekcji ruchu
unsigned long czasZablokowania = 0;             // Czas blokowania czujników po detekcji ruchu
unsigned long czasTrwaniaNiskiejOdleglosciCzujnik1 = 0;
unsigned long czasTrwaniaNiskiejOdleglosciCzujnik2 = 0;
const unsigned long minimalnyCzasNiskiejOdleglosci = 3000; // 3 sekund
const unsigned long czasOczekiwaniaPoZmianie = 3000; // 3 sekundy


// Funkcja do monitorowania dostępnej pamięci dynamicznej
int freeMemory() {
  char* buf;
  int size = 1024; // Wybierz dowolny rozmiar większy niż oczekiwane zużycie pamięci

  while ((buf = (char*)malloc(--size)) == NULL);

  free(buf);
  return size;
}




// Funkcja inicjalizująca czujniki
void initSensors() {
  Wire.begin();

  // Inicjalizacja czujnika 1
  pinMode(PIN_CZUJNIK1_XSHUT, OUTPUT);
  digitalWrite(PIN_CZUJNIK1_XSHUT, LOW);
  delay(10);
  digitalWrite(PIN_CZUJNIK1_XSHUT, HIGH);
  delay(10);
  if (!loxCzujnik1.begin(ADRES_LOX1)) {
    Serial.println("Błąd inicjalizacji czujnika 1!");
    while (1)
      ;
  }
  loxCzujnik1.setMeasurementTimingBudgetMicroSeconds(timingBudget);

  // Inicjalizacja czujnika 2
  pinMode(PIN_CZUJNIK2_XSHUT, OUTPUT);
  digitalWrite(PIN_CZUJNIK2_XSHUT, LOW);
  delay(10);
  digitalWrite(PIN_CZUJNIK2_XSHUT, HIGH);
  delay(10);
  if (!loxCzujnik2.begin(ADRES_LOX2)) {
    Serial.println("Błąd inicjalizacji czujnika 2!");
    while (1)
      ;
  }
  loxCzujnik2.setMeasurementTimingBudgetMicroSeconds(timingBudget);
}

// Funkcja zmieniająca animację
void zmienAnimacje(int &aktualnaAnimacja) {
  int nowaAnimacja = (aktualnaAnimacja + 1) % liczbaAnimacji;
  aktualnaAnimacja = nowaAnimacja;

 // Serial.print("                  Zmieniono animację. Nowa animacja: ");
 // Serial.println(aktualnaAnimacja);
}

// Funkcja animacji "Gwiezdne Niebo"
void gwiezdneNiebo() {
  static const int czasMigotania = 1000;  // Czas trwania migotania gwiazd (w milisekundach)
  static unsigned long poprzedniCzas = 0;

  unsigned long aktualnyCzas = millis();

  if (aktualnyCzas - poprzedniCzas >= czasMigotania) {
    int indeksGwiazdy = random(ILOSC_LED);
    uint32_t kolorGwiazdy = strip.Color(random(256), random(256), random(256));

    strip.setPixelColor(indeksGwiazdy, kolorGwiazdy);
    strip.show();

    poprzedniCzas = aktualnyCzas;
  }
}



// Funkcje obsługujące animacje dla poszczególnych czujników
void animacjaCzujnika1() {
  // Wyświetlenie informacji o aktualnej animacji dla czujnika 1 w konsoli
 // Serial.println("Aktualna animacja dla czujnika 1: " + String(aktualnaAnimacjaCzujnika1));

  // Wybór animacji w zależności od wartości aktualnaAnimacjaCzujnika1
  switch (aktualnaAnimacjaCzujnika1) {
    case 0:
      // Animacja 0 - Czerwony kolor
        animacjaTeczy();     // zmienie sie bardzo pomału tylko wtedy gdy na czujniku jest przeszkoda, a ma być tecza w ruchoma
      
      break;
    case 1:
      // Animacja 1 - Niebieski kolor
      for (int i = 0; i < ILOSC_LED; i++) {
        strip.setPixelColor(i, strip.Color(50, 50, 255));
      }
      break;
    case 2:
      // Animacja 2 - Tęcza
for (int i = 0; i < ILOSC_LED; i++) {
        strip.setPixelColor(i, strip.Color(255, 50, 0));
}
      break;
  }

  // Ustawienie jasności oświetlenia i aktualizacja wyświetlania
 // strip.setBrightness(30);
  strip.show();
}

void animacjaCzujnika2() {
  // Wyświetlenie informacji o aktualnej animacji dla czujnika 2 w konsoli
 // Serial.println("Aktualna animacja dla czujnika 2: " + String(aktualnaAnimacjaCzujnika2));

  // Wybór animacji w zależności od wartości aktualnaAnimacjaCzujnika2
  switch (aktualnaAnimacjaCzujnika2) {
    case 0:
      // Animacja 0 - Zielony kolor
      for (int i = 0; i < ILOSC_LED; i++) {
        strip.setPixelColor(i, strip.Color(50, 255, 50));
      }
      break;
    case 1:
      // Animacja 1 - Niebo
  gwiezdneNiebo();
      break;
    case 2:
      // Animacja 2 - Cyjan kolor
      for (int i = 0; i < ILOSC_LED; i++) {
        strip.setPixelColor(i, strip.Color(50, 155, 155));
      }
      break;
  }

  // Ustawienie jasności oświetlenia i aktualizacja wyświetlania
 // strip.setBrightness(30);
  strip.show();
}
 




void obslugaBledow(String komunikat) {
  // Obsługa błędów, np. wyświetlenie komunikatu o błędzie w konsoli
  Serial.println("Błąd: " + komunikat);
}

void setup() {
  Serial.begin(9600);
  strip.setBrightness(JASNOSC); // Ustaw jasność diod LED

  strip.begin();
  strip.show();

  initSensors();
}

void loop() {
  readSensors();
  kontrolaSwiecenia();
  zmienAnimacjeCzujnik1();
  zmienAnimacjeCzujnik2();

  



/*
int availableMemory = freeMemory();
  Serial.print("Dostępna pamięć dynamiczna: ");
  Serial.print(availableMemory);
  Serial.println(" bajtów");
*/
  // Sprawdzenie błędów transmisji
  while (Serial.available() > 0) {
    int bytesRead = Serial.readBytes(buffer + bufferIndex, BUFFER_SIZE - bufferIndex);
    bufferIndex += bytesRead;
    buffer[bufferIndex] = '\0';

    if (bufferIndex >= BUFFER_SIZE - 1) {
      Serial.println("Przepełnienie bufora, czyszczenie...");
      clearBuffer();
    }
  }
  
}

void clearBuffer() {
  bufferIndex = 0;
  memset(buffer, 0, BUFFER_SIZE);
}



void zmienAnimacjeCzujnik1() {
  // Sprawdzenie, czy odległość od czujnika 1 jest mniejsza niż 70 mm
  if (odleglosc1 < 70) {
    // Sprawdzenie, czy czujnik 1 nie jest aktywny
    if (!czujnik1Aktywny) {
      // Aktualizacja czasu zmiany animacji i aktywacja czujnika 1
      czasZmianyAnimacjiCzujnik1 = millis();
      czujnik1Aktywny = true;
      Serial.println("Aktywowano czujnik 1");
    }

    // Obliczenie czasu trwania niskiej odległości od czujnika 1
    czasTrwaniaNiskiejOdleglosciCzujnik1 = millis() - czasZmianyAnimacjiCzujnik1;

    // Sprawdzenie, czy czas trwania niskiej odległości przekroczył minimalny czas
    if (czasTrwaniaNiskiejOdleglosciCzujnik1 >= minimalnyCzasNiskiejOdleglosci) {
      // Zmiana aktualnej animacji czujnika 1
      zmienAnimacje(aktualnaAnimacjaCzujnika1);
      Serial.println("Zmieniono animację czujnika 1");
      // Oczekiwanie przez określony czas po zmianie animacji
      //delay(czasOczekiwaniaPoZmianie);
          czujnik1Aktywny = false;

    }
  } else {
    // Wyłączenie aktywacji czujnika 1 i resetowanie czasu trwania niskiej odległości
    czujnik1Aktywny = false;
    czasTrwaniaNiskiejOdleglosciCzujnik1 = 0;
  }
}

void zmienAnimacjeCzujnik2() {
  // Sprawdzenie, czy odległość od czujnika 2 jest mniejsza niż 70 mm
  if (odleglosc2 < 70) {
    // Sprawdzenie, czy czujnik 2 nie jest aktywny
    if (!czujnik2Aktywny) {
      // Aktualizacja czasu zmiany animacji i aktywacja czujnika 2
      czasZmianyAnimacjiCzujnik2 = millis();
      czujnik2Aktywny = true;
      Serial.println("Aktywowano czujnik 2");
    }

    // Obliczenie czasu trwania niskiej odległości od czujnika 2
    czasTrwaniaNiskiejOdleglosciCzujnik2 = millis() - czasZmianyAnimacjiCzujnik2;

    // Sprawdzenie, czy czas trwania niskiej odległości przekroczył minimalny czas
    if (czasTrwaniaNiskiejOdleglosciCzujnik2 >= minimalnyCzasNiskiejOdleglosci) {
      // Zmiana aktualnej animacji czujnika 2
      zmienAnimacje(aktualnaAnimacjaCzujnika2);
      Serial.println("Zmieniono animację czujnika 2");
      // Oczekiwanie przez określony czas po zmianie animacji
      //delay(czasOczekiwaniaPoZmianie);
          czujnik2Aktywny = false;

    }
  } else {
    // Wyłączenie aktywacji czujnika 2 i resetowanie czasu trwania niskiej odległości
    czujnik2Aktywny = false;
    czasTrwaniaNiskiejOdleglosciCzujnik2 = 0;
  }
}


void readSensors() {
  // Deklaracja struktur przechowujących dane pomiarowe z czujników
  VL53L0X_RangingMeasurementData_t daneCzujnika1;
  VL53L0X_RangingMeasurementData_t daneCzujnika2;

  // Wykonanie pomiaru odległości dla czujnika 1
  loxCzujnik1.rangingTest(&daneCzujnika1, false);

  // Wykonanie pomiaru odległości dla czujnika 2
  loxCzujnik2.rangingTest(&daneCzujnika2, false);

  // Przypisanie zmierzonych odległości do zmiennych odleglosc1 i odleglosc2
  odleglosc1 = daneCzujnika1.RangeMilliMeter;
  odleglosc2 = daneCzujnika2.RangeMilliMeter;

  // Wyświetlenie zmierzonych odległości w konsoli
 // Serial.print("1: ");
/// Serial.print(odleglosc1);
//Serial.print(" mm ");

 // Serial.print(" 2: ");
//  Serial.print(odleglosc2);
 // Serial.println(" mm ");

  // Sprawdzenie warunku czasowego, czy można aktywować czujniki
  if (millis() - czasZablokowania >= czasZablokowaniaCzujnikow) {
    // Aktywacja czujnika 1, jeśli spełnione są warunki odległości
    if (!czujnik1Aktywny && odleglosc1 >= minimalnaOdleglosc && odleglosc1 <= maksymalnaOdleglosc) {
      czujnik1Aktywny = true;
      czujnik2Aktywny = false;
      czasZablokowania = millis() + czasZablokowaniaCzujnikow;

      // Wywołanie funkcji animującej dla czujnika 1
      animacjaCzujnika1();


    }

    // Aktywacja czujnika 2, jeśli spełnione są warunki odległości
    if (!czujnik2Aktywny && odleglosc2 >= minimalnaOdleglosc && odleglosc2 <= maksymalnaOdleglosc) {
      czujnik2Aktywny = true;
      czujnik1Aktywny = false;
      czasZablokowania = millis() + czasZablokowaniaCzujnikow;

      // Wywołanie funkcji animującej dla czujnika 2
      animacjaCzujnika2();


    }
  }
}

void animacjaTeczy() {
  unsigned long teraz = millis();

  if (teraz - ostatniCzas >= 0) {
    ostatniCzas = teraz;

    for (int i = 0; i < ILOSC_LED; i++) {
      // Oblicz kąt tęczy dla każdego piksela
      int katTeczy = (aktualnyKatTeczy + (i * KROK_TECZY) / (ILOSC_LED - 1)) % 65536;

      // Konwertuj kąt tęczy na RGB i ustaw kolor piksela
      uint32_t kolor = strip.gamma32(strip.ColorHSV(katTeczy, 255, 255));
      strip.setPixelColor(i, kolor);
    }

        // Ustawienie jasności oświetlenia i aktualizacja wyświetlania
      //strip.setBrightness(30);
      strip.show();

    // Płynny ruch tęczy
    aktualnyKatTeczy = (aktualnyKatTeczy + 256) % 65536;
  }
}



void kontrolaSwiecenia() {
  unsigned long aktualnyCzas = millis();

  // Sprawdzenie aktywacji czujników i rozpoczęcie świecenia    //if ((czujnik1Aktywny || czujnik2Aktywny) && czasRozpoczeciaSwiecenia == 0) {: Warunek sprawdzający, czy któryś z czujników (czujnik1 lub czujnik2) jest aktywny (czujnik1Aktywny lub czujnik2Aktywny), oraz czy czas rozpoczęcia świecenia (czasRozpoczeciaSwiecenia) jest równy 0. Jeśli warunek jest spełniony, ustawia czas rozpoczęcia świecenia na aktualny czas.
  if ((czujnik1Aktywny || czujnik2Aktywny) && czasRozpoczeciaSwiecenia == 0) {
    czasRozpoczeciaSwiecenia = aktualnyCzas;
  }

  // Wyłączenie czujnika1, jeśli przekroczono maksymalną odległość  //if (czujnik1Aktywny && odleglosc1 > maksymalnaOdleglosc) {: Warunek sprawdzający, czy czujnik1 jest aktywny (czujnik1Aktywny) i jednocześnie odległość odleglosc1 jest większa niż maksymalna dopuszczalna odległość (maksymalnaOdleglosc). Jeśli warunek jest spełniony, wyłącza czujnik1.
  if (czujnik1Aktywny && odleglosc1 > maksymalnaOdleglosc) {
    czujnik1Aktywny = false;
  }

  // Wyłączenie czujnika2, jeśli przekroczono maksymalną odległość   //if (czujnik2Aktywny && odleglosc2 > maksymalnaOdleglosc) {: Warunek sprawdzający, czy czujnik2 jest aktywny (czujnik2Aktywny) i jednocześnie odległość odleglosc2 jest większa niż maksymalna dopuszczalna odległość (maksymalnaOdleglosc). Jeśli warunek jest spełniony, wyłącza czujnik2.
  if (czujnik2Aktywny && odleglosc2 > maksymalnaOdleglosc) {
    czujnik2Aktywny = false;
  }

  // Sprawdzenie czasu trwania świecenia      //if (aktualnyCzas - czasRozpoczeciaSwiecenia >= czasSwiecenia) {: Warunek sprawdzający, czy czas od rozpoczęcia świecenia (różnica między aktualnym czasem a czasem rozpoczęcia) jest większy lub równy czasowi trwania świecenia (czasSwiecenia).
  if (aktualnyCzas - czasRozpoczeciaSwiecenia >= czasSwiecenia) {
    // Wyłączenie obu czujników     //Wewnątrz powyższego warunku następuje wyłączenie obu czujników (czujnik1Aktywny = false; i czujnik2Aktywny = false;), resetowanie czasu rozpoczęcia świecenia (czasRozpoczeciaSwiecenia = 0;) oraz wyłączenie oświetlenia poprzez ustawienie wszystkich pikseli na czarny kolor
    czujnik1Aktywny = false;
    czujnik2Aktywny = false;
    czasRozpoczeciaSwiecenia = 0;

    // Wyłączenie oświetlenia (ustawienie pikseli na czarny kolor)
    for (int i = 0; i < ILOSC_LED; i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
    //strip.setBrightness(30);
    strip.show();
  }
}