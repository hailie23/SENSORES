#include <SPI.h>
#include <MFRC522.h>

// pines
const int pinreed = 4;
const int pinled = 2;
const int pinbuzzer = 13;

// RFID
#define SS_PIN 5
#define RST_PIN 22
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Nextion Serial
#define nexSerial Serial2

// variables
bool auth = false;
bool puerta = false;
bool armado = false;
const int Tgracia = 20;

// UID válido
byte uidValido[4] = {0x33, 0x79, 0x9F, 0x36};

// --- NEXTION ---
void nexSendCmd(const char* cmd) {
    Serial.print("[NEX] Enviando: ");
    Serial.println(cmd);
    nexSerial.print(cmd);
    nexSerial.write(0xFF);
    nexSerial.write(0xFF);
    nexSerial.write(0xFF);
}

void nexPage(int page) {
    Serial.print("[NEX] Cambiando a page ");
    Serial.println(page);
    char cmd[12];
    sprintf(cmd, "page %d", page);
    nexSendCmd(cmd);
}

String nexLeer() {
    String data = "";
    while (nexSerial.available()) {
        char c = nexSerial.read();
        if (c != 0xFF) {
            data += c;
        }
    }
    if (data.length() > 0) {
        Serial.print("[NEX] Recibido: ");
        Serial.println(data);
    }
    return data;
}

// --- FUNCIONES BASE ---
void STpuerta() {
    if (digitalRead(pinreed) == LOW) {
        puerta = true;
    } else {
        puerta = false;
    }
}

void led1() {
    digitalWrite(pinled, HIGH);
    delay(200);
    digitalWrite(pinled, LOW);
    delay(100);
}

void led2() {
    digitalWrite(pinled, HIGH);
    delay(500);
    digitalWrite(pinled, LOW);
    delay(250);
}

void buzzer1() {
    digitalWrite(pinbuzzer, HIGH);
    delay(100);
    digitalWrite(pinbuzzer, LOW);
    delay(100);
}

void buzzer2() {
    digitalWrite(pinbuzzer, HIGH);
    delay(500);
    digitalWrite(pinbuzzer, LOW);
    delay(250);
}

void beepDesarme() {
    buzzer1();
    buzzer1();
    digitalWrite(pinbuzzer, HIGH);
    delay(300);
    digitalWrite(pinbuzzer, LOW);
    delay(100);
}

void beepError() {
    buzzer1();
    buzzer1();
    buzzer1();
    buzzer1();
}

// --- RFID ---
int rfidLeer() {
    if (!mfrc522.PICC_IsNewCardPresent()) return 0;
    if (!mfrc522.PICC_ReadCardSerial()) return 0;

    buzzer1();

    Serial.print("[RFID] Card UID: ");
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        Serial.print(mfrc522.uid.uidByte[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    bool match = true;
    for (byte i = 0; i < 4; i++) {
        if (mfrc522.uid.uidByte[i] != uidValido[i]) {
            match = false;
            break;
        }
    }

    mfrc522.PICC_HaltA();

    if (match) {
        Serial.println("[RFID] Tarjeta VALIDA");
        return 1;
    }
    Serial.println("[RFID] Tarjeta INVALIDA");
    return 2;
}

// --- FLUJO CON NEXTION ---
void esperarRFID() {
    Serial.println("[SYS] Entrando a esperarRFID()");
    nexPage(1);
    unsigned long inicio = millis();

    while (millis() - inicio < 10000) {
        int resultado = rfidLeer();

        if (resultado == 1) {
            Serial.println("[SYS] Armando sistema");
            armado = true;
            buzzer1();
            buzzer1();
            nexPage(3);
            delay(500);
            return;
        }
        else if (resultado == 2) {
            Serial.println("[SYS] Tarjeta rechazada");
            beepError();
            nexPage(2);
            delay(2000);
            nexPage(0);
            return;
        }
        delay(100);
    }
    Serial.println("[SYS] Timeout esperarRFID");
    nexPage(0);
}

void esperarDesarme() {
    Serial.println("[SYS] Entrando a esperarDesarme()");
    nexPage(1);
    unsigned long inicio = millis();

    while (millis() - inicio < 10000) {
        int resultado = rfidLeer();

        if (resultado == 1) {
            Serial.println("[SYS] Desarmando sistema");
            armado = false;
            beepDesarme();
            nexPage(0);
            delay(500);
            return;
        }
        else if (resultado == 2) {
            Serial.println("[SYS] Tarjeta rechazada en desarme");
            beepError();
            nexPage(2);
            delay(2000);
            nexPage(3);
            return;
        }
        delay(100);
    }
    Serial.println("[SYS] Timeout esperarDesarme");
    nexPage(3);
}

void gracia() {
    Serial.println("[SYS] Entrando a gracia()");
    int i = 0;
    buzzer1();
    nexPage(4);

    while (i < Tgracia) {
        Serial.print("[GRACIA] Ciclo: ");
        Serial.println(i);
        led1();
        int resultado = rfidLeer();

        if (resultado == 1) {
            Serial.println("[GRACIA] Desarmado en gracia");
            armado = false;
            auth = false;
            beepDesarme();
            nexPage(0);
            return;
        }
        delay(700);
        i++;
    }
    Serial.println("[GRACIA] Tiempo agotado");
}

void alarma() {
    Serial.println("[SYS] Entrando a alarma()");
    bool toggle = false;

    while (true) {
        if (toggle) {
            nexPage(5);
        } else {
            nexPage(6);
        }
        toggle = !toggle;

        Serial.println("[ALARMA] Sonando...");
        led2();
        buzzer2();

        int resultado = rfidLeer();
        if (resultado == 1) {
            Serial.println("[ALARMA] Desarmado en alarma");
            armado = false;
            auth = false;
            beepDesarme();
            nexPage(0);
            return;
        }
    }
}

// --- SETUP Y LOOP ---
void setup() {
    pinMode(pinreed, INPUT_PULLUP);
    pinMode(pinled, OUTPUT);
    pinMode(pinbuzzer, OUTPUT);
    Serial.begin(115200);
    Serial.println("[BOOT] Iniciando...");

    Serial.println("[BOOT] Iniciando Nextion...");
    nexSerial.begin(9600, SERIAL_8N1, 16, 17);

    Serial.println("[BOOT] Iniciando SPI...");
    SPI.begin();

    Serial.println("[BOOT] Iniciando RFID...");
    mfrc522.PCD_Init();

    Serial.println("[BOOT] Enviando page 0...");
    nexPage(0);

    Serial.println("[BOOT] Sistema listo!");
}

void loop() {
    String cmd = nexLeer();

    if (cmd.indexOf("LOCK") >= 0 && armado == false) {
        Serial.println("[LOOP] Comando LOCK recibido");
        esperarRFID();
    }

    if (cmd.indexOf("UNLOCK") >= 0 && armado == true) {
        Serial.println("[LOOP] Comando UNLOCK recibido");
        esperarDesarme();
    }

    if (armado == true) {
        STpuerta();
        if (puerta == true) {
            Serial.println("[LOOP] Puerta abierta! Entrando a gracia");
            auth = false;
            gracia();
            if (armado == true) {
                alarma();
            }
        }
    }

    delay(50);
}