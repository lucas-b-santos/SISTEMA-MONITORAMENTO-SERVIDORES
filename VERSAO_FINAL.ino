/*                              +-----------------------+
                                | O      | USB |      O |
                                |        -------        |
                            3V3 | [ ]               [ ] | VIN
                            GND | [ ]               [ ] | GND
  Touch3/HSPI_CS0/ADC2_3/GPIO15 | [ ]               [ ] | GPIO13/ADC2_4/HSPI_ID/Touch4
CS/Touch2/HSPI_WP/ADC2_2/GPIO2  | [ ]               [ ] | GPIO12/ADC2_5/HSPI_Q/Touch5
   Touch0/HSPI_HD/ADC2_0/GPIO4  | [ ]               [ ] | GPIO14/ADC2_6/HSPI_CLK/Touch6
                  U2_RXD/GPIO16 | [ ]               [ ] | GPIO27/ADC2_7/Touch7
                  U2_TXD/GPIO17 | [ ]               [ ] | GPIO26/ADC2_9/DAC2
               V_SPI_CS0/GPIO5  | [ ]  ___________  [ ] | GPIO25/ADC2_8/DAC1
           SCK/V_SPI_CLK/GPIO18 | [ ] |           | [ ] | GPIO33/ADC1_5/Touch8/XTAL32
     U0_CTS/MSIO/V_SPI_Q/GPIO19 | [ ] |           | [ ] | GPIO32/ADC1_4/Touch9/XTAL32
            SDA/V_SPI_HD/GPIO21 | [ ] |           | [ ] | GPIO35/ADC1_7
             CLK2/U0_RXD/GPIO3  | [ ] |           | [ ] | GPIO34/ADC1_6
             CLK3/U0_TXD/GPIO1  | [ ] |           | [ ] | GPIO39/ADC1_3/SensVN
     SCL/U0_RTS/V_SPI_WP/GPIO22 | [ ] |           | [ ] | GPIO36/ADC1_0/SensVP
           MOSI/V_SPI_WP/GPIO23 | [ ] |___________| [ ] | EN
                                |                       |
                                |  |  |  ____  ____  |  |
                                |  |  |  |  |  |  |  |  |
                                |  |__|__|  |__|  |__|  |
                                | O                   O |
                                +-----------------------+
  GPIO15                          GPIO13
  GPIO2                           GPIO12
  GPIO4  DS18B20                  GPIO14
  GPIO16                          GPIO27
  GPIO17                          GPIO26
  GPIO5                           GPIO25
  GPIO18  DHT11                   GPIO33
  GPIO19                          GPIO32
  GPIO21  BUTTON                  GPIO35
  GPIO3                           GPIO34
  GPIO1                           GPIO39
  GPIO22                          GPIO36
  GPIO23

  Digitais:
  - DS18B20 Sensor temperatura DallasTemperature
  - DHT11 Sensor temperatura umidade
  - BUTTON KY-004 Module
  
--ESQUEMA FORNECIDO PELO ORIENTADOR DO PROJETO (THIAGO BERTICELLI LÓ). SEU USERNAME NO GITHUB: "losaum"--
*/


// Wi-Fi
#include <WiFi.h>
#include <WiFiClientSecure.h>
#define WIFI_SSID "NOME-DA-REDE"
#define WIFI_PASSWORD "SENHA-DA-REDE"
WiFiClientSecure secured_client;

// Bot do Telegram
#include <UniversalTelegramBot.h>
#include "CTBot.h"
CTBot myBot;
CTBotReplyKeyboard myKbd;
bool isKeyboardActive;
#define BOT_TOKEN "TOKEN-DO-BOT"     // token do perfil-bot, fornecido durante a sua criação
const unsigned long BOT_MTBS = 1000; // intervalo entre a verificação de novas mensagens
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;

// Int64 para string
#include <Int64String.h>

// Sensor ds18b20
#include <OneWire.h>
#include <DallasTemperature.h>
OneWire barramento(4); //pino Ds18b20
DallasTemperature sensor(&barramento);
DeviceAddress sensor1;

// Botão
#include <ezButton.h>
const int buttonPin = 21; // pino do botão
ezButton button(buttonPin);
bool btnState_last;
bool btnState;

// DHT 11
#include "DHT.h"
#define DHTPIN 18 // pino do DHT11
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
float temp_ds18b20;
float temp_DHT11;
float humid_DHT11;

// contadores de tempo
long lastReadSensors;
unsigned long timerLOG;
int countTEMP = 0;
int countHUMID = 0;

// As duas tasks que serão executadas, uma em cada núcleo
TaskHandle_t Task1;
TaskHandle_t Task2;

// Firebase
#include <Firebase_ESP_Client.h>
#define API_KEY "CHAVE-DA-API"
#define FIREBASE_PROJECT_ID "ID-DO-PROJETO"
// email e senha de conta autorizada na Firebase
#define USER_EMAIL "EMAIL"
#define USER_PASSWORD "SENHA"
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
String uid;
String databasePath;
String parentPath;
/* caminho para armazenar os IDS na Firestore - coleção "Dados" e Documento "IDS", 
dentro desse documento existe o campo "Lista_IDS", um array de 15 posições */
String documentPath = "Dados/IDS"; 

// variaveis para pegar o timestamp
const char *ntpServer = "pool.ntp.org"; //"south-america.pool.ntp.org"
const long gmtOffset_sec = -3 * 3600;   // -2*3600 para horário de verão
const int daylightOffset_sec = 0;
String ts;

// aqui se define os limites para temperatura e humidade; quando ultrapassados, os usuários serão notificados
#define LIMITE_TEMP 23
#define LIMITE_HUMIDITY 60

#define INTERVALO_LEITURA 10000 //intervalo de tempo entre a leitura dos sensores
#define INTERVALO_ARMAZENAR 300000 //intervalo de tempo entre o armazenamento das medições

// variaveis para verificar se o usuário já foi notificado
bool notifiedTEMP = false;
bool notifiedHUMIDITY = false;

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// ==============================FUNÇÕES PARA PEGAR TIMESTAMP===========================

String getTimeString()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time - (getTimeString)");
    return String("");
  }
  char timeStringBuff[30];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%S-03:00", &timeinfo);
  String timeString = String(timeStringBuff);
  return timeString;
}

long getEpochTime()
{
  time_t now;
  time(&now);
  return now;
}

String getTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    return "Failed to obtain time";
  }
  char timeStringBuff[30];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S-03:00", &timeinfo);
  return String(timeStringBuff);
}

// ===================================================================================================

// ==============================FUNÇÕES PARA INICIALIZAR COMPONENTES===========================

void initDs18b20()
{
  DeviceAddress tempDeviceAddress;
  sensor.begin();

  Serial.print(sensor.getDeviceCount(), DEC);
  Serial.println(" sensores.");
  if (!sensor.getAddress(sensor1, 0))
    Serial.println("Sensores nao encontrados !");
  // Mostra o endereco do sensor encontrado no barramento
  Serial.print("Endereco sensor: ");

  sensor.getAddress(tempDeviceAddress, 0);
  const int resolution = 12;
  sensor.setResolution(tempDeviceAddress, resolution);
  sensor.setWaitForConversion(false);
}

void initDHT11()
{
  dht.begin();
  humid_DHT11 = dht.readHumidity();
  temp_DHT11 = dht.readTemperature();
  if (isnan(temp_DHT11) || isnan(humid_DHT11))
  {
    Serial.println(F("Falha ao ler o sensor DHT11!"));
    return;
  }
}

void initWIFI()
{
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println(getTime());
}

void initFirestore()
{
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  fbdo.setResponseSize(2048);
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "")
  {
    Serial.print('.');
    delay(1000);
  }
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);
  Serial.println("Esperando a Firestore estar pronta");
  while (!Firebase.ready())
  {
    Serial.print('.');
    delay(1000);
  }
}

// ===================================================================================================

void lerSensores()
{
  humid_DHT11 = dht.readHumidity();
  temp_DHT11 = dht.readTemperature();
  temp_ds18b20 = sensor.getTempCByIndex(0);
  sensor.requestTemperatures();
  doDataString();
}

// converter string para int64
int64_t str2int64(String s)
{
  int64_t x = 0;
  for (int i = 0; i < s.length(); i++)
  {
    char c = s.charAt(i);
    x *= 10;
    x += (c - '0');
  }
  return x;
}

/*objeto responsável por pegar e adicionar os IDS na Firestore,
além de notificar os usuários cadastrados com a mensagem escolhida*/
class IdsList
{
private:
  String listIds[15];

public:
  IdsList()
  {
    for (int i = 0; i < 15; i++)
    {
      listIds[i] = "";
    }
  }

  void getIDS()
  {

    if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str()))
      Serial.println("Pegando IDS da Firestore");
    else
      Serial.println(fbdo.errorReason());

    FirebaseJson payload;
    payload.setJsonData(fbdo.payload().c_str());

    for (int i = 0; i < 15; i++)
    {
      FirebaseJsonData jsonData;
      payload.get(jsonData, "fields/Lista_IDS/arrayValue/values/[" + String(i) + "]/stringValue", true);
      listIds[i] = jsonData.stringValue;
    }
  }

  void atualizarIDS()
  {
    FirebaseJson content;
    for (int i = 0; i < 15; i++)
    {
      content.set("fields/Lista_IDS/arrayValue/values/[" + String(i) + "]/stringValue", listIds[i]);
    }

    if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "Lista_IDS"))
      Serial.println("Atualizando os IDS na Firestore");
    else
      Serial.println(fbdo.errorReason() + " atualizarIDS");
  }

  bool addId(int64_t id)
  {
    for (int i = 0; i < 15; i++)
    {
      if (listIds[i] == "")
      {
        listIds[i] = int64String(id);
        atualizarIDS();
        return true;
      }
    }
    return false;
  }

  bool removeId(int64_t id)
  {
    for (int i = 0; i < 15; i++)
    {
      if (listIds[i] == int64String(id))
      {
        listIds[i] = "";
        atualizarIDS();
        return true;
      }
    }
    return false;
  }

  bool isListed(int64_t id)
  {
    getIDS();
    for (int i = 0; i < 15; i++)
    {
      if (listIds[i] == int64String(id))
      {
        return true;
      }
    }
    return false;
  }

  void notify(CTBot myBot, String mensagen)
  {
    for (int i = 0; i < 15; i++)
    {
      if (listIds[i] != "")
      {
        myBot.sendMessage(str2int64(listIds[i]), mensagen);
      }
    }
  }
};

IdsList idsList;

// faz uma string para enviar quando o usuário requisita as informações (Pressiona o botão Info!)
char temp[300];
void doDataString()
{
  snprintf(temp, 400, "\
-----------------------------\n\
- DS18B20 ----\n\
Temperature = %.2f\n\
- DHT11 ----\n\
Humidity = %.2f%\n\
Temperature = %.2f *C\n",
           temp_ds18b20, humid_DHT11, temp_DHT11);
}

// rotina de verificação e tratamento das mensagens
void loopTelegram()
{
  // a variable to store telegram message data
  TBMessage msg;

  // if there is an incoming message...
  if (myBot.getNewMessage(msg))
  {
    long telegramDelay = millis();
    // check what kind of message I received
    if (msg.messageType == CTBotMessageText)
    {
      Serial.print("\nMensagem: ");
      Serial.println(msg.text);
      Serial.println();

      // received a text message
      if (msg.text.equalsIgnoreCase("show keyboard"))
      {
        // the user is asking to show the reply keyboard --> show it
        myBot.sendMessage(msg.sender.id, "Reply Keyboard enable. You can send a simple text, your contact, your location or hide the keyboard", myKbd);
        isKeyboardActive = true;
      }
      else if (msg.text.equalsIgnoreCase("Add Id"))
      {
        bool result = idsList.isListed(msg.sender.id);
        if (result)
        {
          myBot.sendMessage(msg.sender.id, "ID já foi adicionado!");
        }
        else
        {
          bool result = idsList.addId(msg.sender.id);
          if (result)
          {
            myBot.sendMessage(msg.sender.id, "ID adicionado!");
          }
          else
          {
            myBot.sendMessage(msg.sender.id, "Não foi possível adicionar!");
          }
        }
      }
      else if (msg.text.equalsIgnoreCase("Remove Id"))
      {
        bool result = idsList.isListed(msg.sender.id);
        if (result)
        {
          bool result = idsList.removeId(msg.sender.id);
          if (result)
          {
            myBot.sendMessage(msg.sender.id, "ID removido!");
          }
          else
          {
            myBot.sendMessage(msg.sender.id, "Não foi possível remover!");
          }
        }
        else
        {
          myBot.sendMessage(msg.sender.id, "ID não listado!");
        }
      }
      else if (msg.text.equalsIgnoreCase("Info!"))
      {
        myBot.sendMessage(msg.sender.id, temp);
      }

      // check if the reply keyboard is active
      else if (isKeyboardActive)
      {
        // is active -> manage the text messages sent by pressing the reply keyboard buttons
        if (msg.text.equalsIgnoreCase("Hide replyKeyboard"))
        {
          // sent the "hide keyboard" message --> hide the reply keyboard
          myBot.removeReplyKeyboard(msg.sender.id, "Reply keyboard removed");
          isKeyboardActive = false;
        }
        else
        {
          // print every others messages received
          myBot.sendMessage(msg.sender.id, msg.text);
        }
      }
      else
      {
        // the user write anything else and the reply keyboard is not active --> show a hint message
        myBot.sendMessage(msg.sender.id, "Try 'show keyboard'");
        Serial.print("Id:");
        Serial.println(msg.sender.id);
      }
    }
    Serial.print("Telegram delay: ");
    Serial.println(millis() - telegramDelay);
  }
}

void createKeyboards()
{
  myKbd.addButton("Info!");
  myKbd.addRow();
  myKbd.addButton("Add Id");
  myKbd.addButton("Remove Id");
  myKbd.addRow();
  myKbd.addButton("Hide replyKeyboard");
  myKbd.enableResize();
}

// task que será executada no core 0
void taskTelegram(void *pvParameters)
{
  delay(10);
  Serial.print("TaskTelegram running on core ");
  Serial.println(xPortGetCoreID());

  // Telegram bot
  myBot.wifiConnect(WIFI_SSID, WIFI_PASSWORD);
  myBot.setTelegramToken(BOT_TOKEN);

  // check if all things are ok
  if (myBot.testConnection())
  {
    Serial.println("\ntestConnection OK");
  }
  else
  {
    Serial.println("\ntestConnection NOK");
  }
  // Create keyboards
  createKeyboards();
  isKeyboardActive = false;

  for (;;)
  {
    loopTelegram();
    vTaskDelay(1200 / portTICK_PERIOD_MS);
  }
}

void lerBtn()
{
  btnState = button.getState();

  if (btnState_last != btnState)
  {

    if (!btnState)
    {
      idsList.notify(myBot, "Faltou luz!");
      atualizarLOG_Alertas("Queda_de_Luz_");
      Serial.println("Apertado!");
    }
    else
    {
      idsList.notify(myBot, "Voltou a luz!");
      Serial.println("Solto!");
    }
  }
  btnState_last = btnState;
}

bool verificarTemp(bool notified)
{
  if (notified)
  {
    countTEMP++;
    if (temp_ds18b20 <= LIMITE_TEMP)
    {
      idsList.notify(myBot, "Temperatura normalizou.");
      countTEMP = 0;
      return false;
    }
    else if (countTEMP == 6)
    {
      countTEMP = 0;
      return false;
    }
    return notified;
  }
  if (temp_ds18b20 > LIMITE_TEMP)
  {
    idsList.notify(myBot, "ALERTA: temperatura em nível crítico!");
    atualizarLOG_Alertas("Temperatura_Crítica_");

    return true;
  }
  return false;
}

bool verificarUMIDADE(bool notified)
{
  if (notified)
  {
    countHUMID++;
    if (humid_DHT11 <= LIMITE_HUMIDITY)
    {
      idsList.notify(myBot, "Umidade normalizou.");
      countHUMID = 0;
      return false;
    }
    else if (countHUMID == 6)
    {
      countHUMID = 0;
      return false;
    }
    return notified;
  }
  if (humid_DHT11 > LIMITE_HUMIDITY)
  {
    idsList.notify(myBot, "ALERTA: umidade em nível crítico!");
    atualizarLOG_Alertas("Umidade_Crítica_");
    return true;
  }
  return false;
}

// função que atualiza o log de alertas
void atualizarLOG_Alertas(String tipo)
{
  FirebaseJson content;
  //essa coleção já foi criada previamente na Firestore
  String LOG_Path = "LOG_Alertas/";
  String ts = getTimeString();
  // coloca o valor da funçao epoch como nome do documento criado, sucedendo o tipo de evento ocorrido
  LOG_Path += tipo + String(getEpochTime());

  content.set("fields/Temp. DS18B20/doubleValue", temp_ds18b20);
  content.set("fields/Temp. DHT11/doubleValue", temp_DHT11);
  content.set("fields/Humidity DHT11/doubleValue", humid_DHT11);
  content.set("fields/Timestamp/timestampValue", ts.c_str());

  if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", LOG_Path.c_str(), content.raw()))
    Serial.println("Armazenando");
  else
    Serial.println(fbdo.errorReason());
}

// função que salva as medições na Firestore
void atualizarLOG_Medicoes()
{
  FirebaseJson content;
  //essa coleção já foi criada previamente na Firestore
  String LOG_Path = "LOG_Medicoes/";
  String ts = getTimeString();
  LOG_Path += String(getEpochTime()); //coloca o valor da funçao epoch como nome do documento criado

  content.set("fields/Temp. DS18B20/doubleValue", temp_ds18b20);
  content.set("fields/Temp. DHT11/doubleValue", temp_DHT11);
  content.set("fields/Humidity DHT11/doubleValue", humid_DHT11);
  content.set("fields/Timestamp/timestampValue", ts.c_str());

  if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", LOG_Path.c_str(), content.raw()))
    Serial.println("Armazenando");
  else
    Serial.println(fbdo.errorReason());
}

void setup()
{

  Serial.begin(115200);

  Serial.println();
  initWIFI();
  initFirestore();
  idsList.getIDS();
  initDs18b20();
  initDHT11();
  lastReadSensors = millis();
  xTaskCreatePinnedToCore(
      taskTelegram, /* Task function. */
      "Task1",      /* name of task. */
      10000,        /* Stack size of task */
      NULL,         /* parameter of the task */
      1,            /* priority of the task */
      &Task1,       /* Task handle to keep track of created task */
      0);           /* pin task to core 0 */
  Serial.print("\nsetup() running on core ");
  Serial.println(xPortGetCoreID());
}

void loop()
{
  button.loop();
  lerBtn();
  if (millis() - lastReadSensors > INTERVALO_LEITURA) 
  {
    lerSensores();
    notifiedTEMP = verificarTemp(notifiedTEMP);
    notifiedHUMIDITY = verificarUMIDADE(notifiedHUMIDITY);
    lastReadSensors = millis();
  }

  if (millis() - timerLOG > INTERVALO_ARMAZENAR)
  {
    atualizarLOG_Medicoes();
    timerLOG = millis();
  }
}