#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h> 
#include <TinyGPS++.h>  

//*********************
//***   GLOBALES     **
//*********************
#define ID_MQTT     "ESP32_0"    //identificador mqtt para la sesion
#define TOPICO_PUB1 "test/temp1"  //tópico para temperatura que se publica en el Broker
#define TOPICO_PUB2 "test/temp2"  //tópico para temperatura que se publica en el Broker
#define TOPICO_SUB1 "test/led1"   //tópico al que se subscribe
#define TOPICO_SUB2 "test/led2"   //tópico al que se subscribe

// Define the RX and TX pins for Serial 2
#define RXD2 16
#define TXD2 17

#define GPS_BAUD 9600

#define led1 22
#define led2 23
#define buzz 18
#define ONE_WIRE_BUS 19
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire); 

unsigned int intervalo = 1000; //1 segundo
unsigned long valorActual = 0;
unsigned long previousMillis = 0; //para la secuencia
const int interval = 500;
bool buzzState = false;
WiFiClient espClient;           //se declara el objeto espClient de la clase WiFiClient
PubSubClient client(espClient); //se declara el objeto client de la clase PubSubClient
TinyGPSPlus gps;

// GPS

// Create an instance of the HardwareSerial class for Serial 2
HardwareSerial gpsSerial(2);


//***********************
//***   WIFI CONFIG    **
//***********************
const char* SSID          = "Simple"; //FLIA-VILLCA
const char* PASSWORD      = "contra123"; //4722ab+32m

//***********************
//***   MQTT CONFIG    **
//***********************
const char* BROKER_MQTT   = "144.22.56.85"; //192.168.1.111
const int BROKER_PORT     = 1883;
//const char* mqttUser      = "cbba";
//const char* mqttPassword  = "bolivia";

//*********************
//***   FUNCIONES    **
//*********************
//Prototipos
void InitOutput();
void initSerial();
void initWiFi();
void initMQTT();
void reconectWiFi(); 
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void VerificaConexWiFiyMQTT();
void EnviaGPSMQTT();

void InitOutput() {
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(buzz, OUTPUT);
  digitalWrite(buzz, LOW);
  digitalWrite(led1, LOW);
  digitalWrite(led2, LOW);
}

void initSerial() {
  Serial.begin(115200);

  // Start Serial 2 with the defined RX and TX pins and a baud rate of 9600
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Serial 2 started at 9600 baud rate");

  delay(100);
}

void initWiFi() {
  reconectWiFi();
}

void initMQTT() {
  client.setServer(BROKER_MQTT, BROKER_PORT);   //IP y puerto del Broker
  client.setCallback(mqtt_callback);            //se ejecuta cada vez que se recibe un mensaje           
}

//********************
//***   CALLBACK    **
//********************
//esta función es llamada cuando llega información de uno de los tópicos subscritos

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
//Si se publica en el TOPICO_SUB1
  if(strcmp(topic,TOPICO_SUB1) == 0) {
    String msg;
    //obtiene la cadena del payload recibido
    for(unsigned int i = 0; i < length; i++) {
      char c = (char)payload[i];
        msg += c;
    }
    //toma acción dependiendo de la cadena recibida:
    if (msg.equals("on")) {
      digitalWrite(led1, HIGH);
    }
    if (msg.equals("off")) {
      digitalWrite(led1, LOW);
     }
  }
//Si se publica en el TOPICO_SUB2
  if(strcmp(topic,TOPICO_SUB2) == 0) {
    String msg;
    //obtiene la cadena del payload recibido
    for(unsigned int i = 0; i < length; i++) {
      char c = (char)payload[i];
        msg += c;
        Serial.println(c);
    }
    //toma acción dependiendo de la cadena recibida:
    if (msg.equals("on")) {
      digitalWrite(led2, HIGH);
    }
    if (msg.equals("off")) {
      digitalWrite(led2, LOW);
     }
  }  
}

//************************
//***   CONEXIÓN MQTT   **
//************************
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("* Intentando conectar al Broker MQTT: ");
    Serial.println(BROKER_MQTT);
    // if (client.connect(ID_MQTT, mqttUser, mqttPassword)) {
    if (client.connect(ID_MQTT)) {
        Serial.println("Conectado con éxito al Broker MQTT!");
        client.subscribe(TOPICO_SUB1); 
        client.subscribe(TOPICO_SUB2);         
    } 
    else {
        Serial.println("Falla al reconectar al Broker.");
        Serial.println("Habra nueva tentativa de conexion en 2s");
        delay(2000);
    }
  }
}

//************************
//***   CONEXIÓN WIFI   **
//************************ 
void reconectWiFi() {
  if (WiFi.status() == WL_CONNECTED)
    return;
  WiFi.begin(SSID, PASSWORD); //se conecta a la red WiFi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Conectado con éxito a la red WiFi ");
  Serial.print(SSID);
  Serial.println("IP obtenido: ");
  Serial.println(WiFi.localIP());
}

void VerificaConexWiFiyMQTT() {
  reconectWiFi();     //si se pierde la conexión con la red WiFi, se reinicia
  if (!client.connected()) 
    reconnectMQTT();  //si se pierde la conexión con el Broker, se reinicia
}

void BuzzerAlarma(float temperatura, float alarma) {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval && temperatura > alarma) {
    previousMillis = currentMillis;
    buzzState = !buzzState;
    digitalWrite(buzz, buzzState ? HIGH : LOW);
  } else {
    digitalWrite(buzz, LOW);
  }
  
}

//*********************
//***  PUBLICACIÓN   **
//*********************
void EnviaTempMQTT() {
  if(millis() - valorActual >= intervalo) {
    valorActual = millis();
    sensors.requestTemperatures();
    float celsius1 = sensors.getTempCByIndex(0);
    Serial.println(sensors.getTempCByIndex(0),1); //1 decimal
    char tempstring1 [4];
    dtostrf(celsius1,4, 1, tempstring1);            //convierte float a char 
    client.publish(TOPICO_PUB1, tempstring1);       //envía char  

    float celsius2 = sensors.getTempCByIndex(1);
    Serial.println(sensors.getTempCByIndex(1),1); //1 decimal
    char tempstring2 [4];
    dtostrf(celsius2,4, 1, tempstring2);            //convierte float a char 
    client.publish(TOPICO_PUB2, tempstring2);       //envía char

    float temperatura = (celsius1 + celsius2)/2;
    BuzzerAlarma(temperatura, 30.0);
  }
}

void EnviaGPSMQTT() {
  while (gpsSerial.available() > 0) {
        char gpsData = gpsSerial.read();
        gps.encode(gpsData); // Procesar datos con TinyGPS++
    }

    // Imprimir datos si son válidos
    if (gps.location.isValid()) {
        Serial.print("Latitud: "); Serial.println(gps.location.lat(), 6);
        Serial.print("Longitud: "); Serial.println(gps.location.lng(), 6);
    } else {
        Serial.println("Esperando señal GPS...");
    }

    Serial.print("Satélites: ");
    Serial.println(gps.satellites.value());

    delay(1000);
    Serial.println("-------------------------------");
}

void setup() {
  initSerial();
  InitOutput();
  initWiFi();
  initMQTT();
}

void loop() {
  VerificaConexWiFiyMQTT();
  EnviaTempMQTT();
  EnviaGPSMQTT();
  client.loop();
}
