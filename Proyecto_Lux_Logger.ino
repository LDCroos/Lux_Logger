/********************************************************************  
*  Firmware final CAD - Embebidos II                                *
*                                                                   *
*  Por:                                                             *
*  - Christian Cardona Sánchez                                      *
*  - Laura Daniela Cruz Ducuara                                     *
**********************************************************************/ 
//mosquitto -c "C:\Program Files\mosquitto\mosquitto.conf" -v
/********************************************************************
*                          LIBRERIAS                                *
*********************************************************************/
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_VEML7700.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/********************************************************************
*               CREDENCIALES COMUNICACIÓN WI-FI  Y MQTT              *
*********************************************************************/

const char* ssid = "LDCroos";
const char* password = "02276lau";
const char* mqtt_server = "192.168.81.63";
const int mqtt_port = 1883;
const char* mqtt_user = "LDCroos";        
const char* mqtt_password = "123456";  

/**************************************************************
*               CONSTANTES, VARIABLES Y OBJETOS               *
***************************************************************/
#define VEML7700_ADDRESS 0x10
#define RTC_ADDRESS 0x68
#define chipSelect 5
#define redPin 27
#define greenPin 26
#define bluePin 25

RTC_DS3231 rtc;
Adafruit_VEML7700 lightMeter;
WiFiClient espClient;
PubSubClient client(espClient);

const unsigned long interval = 4000;                                   // Intervalo de tiempo entre lecturas (en milisegundos)
const int num_readings = 5;                                            // Cantidad de muestras
unsigned long previousMillis = 0;
float readings[num_readings];
int readings_count = 0;                                                //Contador de muestras
const char* clientId = "esp32_client";                                 // Identificador único para este cliente MQTT
Adafruit_SSD1306 oled(128,64,&Wire,-1);


/**************************************************************
*                          FUNCIONES                          *
***************************************************************/
void setup_wifi();
void takeReadings();
void writeDataToSD();
void sendMQTTData();
void LED_RGB(int red, int green, int blue);
void OLED_print(String mensaje);

/**************************************************************
*                          CODIGO                             *
***************************************************************/

void setup() {
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  pinMode(redPin,   OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin,  OUTPUT);
  LED_RGB(LOW, LOW, LOW);
  OLED_print("Encendido");
  Wire.begin();
  Serial.begin(9600);
  rtc.begin();
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  SPI.begin();
  if (!lightMeter.begin()) {
    Serial.println(" Error al inicializar sensor de luz");
    return;
  }
  OLED_print("Sensor de luz activo");
  if (!SD.begin(chipSelect)) {
    Serial.println(" Error al inicializar la tarjeta SD");
    return;
  }
  OLED_print("Tarjeta SD activa");
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    OLED_print(" Tomando datos");
    takeReadings();

    if (readings_count == num_readings) {
      
      OLED_print(" Muestras tomadas");
      writeDataToSD();
      readings_count = 0;
      if (client.connect(clientId, mqtt_user, mqtt_password)) {                         // Autenticación MQTT
        OLED_print("Conexion MQTT establecida");                                        // Conexión exitosa
        Serial.println("Conexión al servidor MQTT establecida");
        OLED_print("Conexion NODE-RED activa");
        LED_RGB(LOW, HIGH, LOW);
        Serial.println("Conexión y suscripción completadas. Enviando datos...");
        sendMQTTData();
        Serial.println(" ");
        client.disconnect();
        LED_RGB(HIGH, LOW, LOW);
        OLED_print("Conexion MQTT inactiva");
        LED_RGB(LOW, LOW, LOW);
      } else {
        LED_RGB(HIGH, LOW, LOW);                                                          // Conexión fallida
        OLED_print("Error conexion MQTT");
        OLED_print("Se seguira tomando datos");
        LED_RGB(LOW, LOW, LOW);
        Serial.println("Error al conectar al servidor MQTT");
        Serial.println("Se seguirán tomando datos");
      }
    }
  }
  client.loop();
}

void LED_RGB(int red, int green, int blue){
  digitalWrite(redPin, red);
  digitalWrite(greenPin, green);
  digitalWrite(bluePin, blue);
}

void takeReadings() {
  DateTime now = rtc.now();                                                               // Obtener la fecha y hora actual del RTC
  float lux = lightMeter.readLux();                                                       // Leer el valor de lux del sensor
  Serial.print(" Fecha y hora: ");                                                        // Imprimir la fecha y hora junto con el valor de lectura
  Serial.print(now.timestamp());
  Serial.print(", Luz: ");
  Serial.println(lux);
  readings[readings_count] = lux;                                                         // Almacenar la lectura en el arreglo
  readings_count++;
}

void setup_wifi() {
  delay(10);
  Serial.println(" Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println(" WiFi conectado");
  OLED_print("WiFi conectado");
  Serial.println(" Direccion IP:");
  Serial.println(WiFi.localIP());
}

void writeDataToSD() {
  Serial.println(" - Escribiendo datos en la tarjeta SD -");
  File dataFile = SD.open("/data.txt", FILE_APPEND);                                        // Abrir o crear el archivo en modo de escritura
  if (dataFile) {
    OLED_print("Escribiendo datos SD");
    DateTime now = rtc.now();                                                               // Obtener la fecha y hora actual del RTC
    for (int i = 0; i < num_readings; i++) {                                                // Escribir los datos en el archivo
      // Escribir la fecha
      dataFile.print(now.year(), DEC);
      dataFile.print("/");
      dataFile.print(now.month(), DEC);
      dataFile.print("/");
      dataFile.print(now.day(), DEC);
      dataFile.print(",");
      // Escribir la hora
      dataFile.print(now.hour(), DEC);
      dataFile.print(":");
      dataFile.print(now.minute(), DEC);
      dataFile.print(":");
      dataFile.print(now.second(), DEC);
      dataFile.print(",");
      // Escribir el valor de la lectura de luz
      dataFile.println(readings[i]);
    }
    dataFile.close();                                                                       // Cerrar el archivo después de escribir
    Serial.println(" Datos escritos en la tarjeta SD correctamente");
    OLED_print("Datos almacenados");
  } else {
    Serial.println(" Error al abrir el archivo en la tarjeta SD");
    OLED_print("Falla archivo SD");
  }
}

void sendMQTTData() {
  Serial.println(" ");
  Serial.println("Enviando datos al servidor MQTT...");
  // Abrir el archivo de datos en modo lectura
  File dataFile = SD.open("/data.txt");
  // Contar las líneas del archivo
  int contador = 0;
  while (dataFile.available()) {
    dataFile.readStringUntil('\n');
    contador++;
  }
  dataFile.close();
  Serial.print(" El archivo tiene'");                                                      // Imprimir la cantidad de datos
  Serial.print(contador);
  Serial.println(" datos");
  OLED_print("Cantidad de datos");
  String aux = String(contador);
  OLED_print(aux);
  dataFile = SD.open("/data.txt");
  if (dataFile) {
    OLED_print("Enviando datos a NODE-RED");
    // Leer y enviar línea por línea hasta el final del archivo
    String line;
    while (dataFile.available()) {
      line = dataFile.readStringUntil('\n');
      line.trim();                                                                         // Eliminar espacios en blanco al inicio y al final
      // Dividir la línea en partes utilizando la coma como delimitador
      int commaIndex1 = line.indexOf(',');
      int commaIndex2 = line.indexOf(',', commaIndex1 + 1);
      
      if (commaIndex1 != -1 && commaIndex2 != -1) {
        String date = line.substring(0, commaIndex1);
        String time = line.substring(commaIndex1 + 1, commaIndex2);
        String lux = line.substring(commaIndex2 + 1);

        // Enviar cada parte al servidor MQTT
        client.publish("/date", date.c_str());
        client.publish("/time", time.c_str());
        client.publish("/lux", lux.c_str());

        // Imprimir mensajes de depuración
        Serial.println("Fecha enviada: " + date);
        Serial.println("Hora enviada: " + time);
        Serial.println("Luz enviada: " + lux);
      }
      delay(1000);                                                                            // Pequeña pausa para evitar congestión en el envío MQTT
    }
    // Cerrar el archivo después de leer
    dataFile.close();
    OLED_print("Datos enviados");
    Serial.println("Todos los datos han sido enviados al servidor MQTT");

    if (SD.remove("/data.txt")) {                                                             // Borrar el contenido del archivo después de enviar los datos
      Serial.println("Contenido del archivo borrado");
      Serial.println(" ");
      OLED_print("Datos borrados del almacenamiento");
    } else {
      Serial.println("Error al borrar el archivo");
    }
  } else {
    Serial.println("Error al abrir el archivo de datos en la tarjeta SD");
  }
}

void OLED_print(String mensaje){
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0, 0);
  oled.println("L U X    L O G G E R");
  oled.setCursor(0, 30);
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.println(mensaje);
  oled.display(); 
  delay(4000);
}

