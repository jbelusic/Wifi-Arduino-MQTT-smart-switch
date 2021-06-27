
// Ovaj program sluzi za ukljucivanje/iskljucivanje sijalice
// Sijalica se inače pali/gasi preko GPIO3 pina (Svijetlo)
//     [ 1- pali  (GPIO3-HIGH)        0 - gasi  (GPIO3-LOW ) ]
// Pritiskom na "Taster" (GPIO0) palimo/gasimo sijalicu Svijetlo(GPIO3)
// čak i kada nismo spojeni na WiFi mrežu ni na WEB server (interrupt)

/**************************************************/


#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Bounce2.h>
#include <Ticker.h>     //Ticker Library
#define NUM_BUTTONS 1   // Broj debansiranih prekidača  (Taster)

const char *ssid          = "MyWifiSSID";
const char *password      = "MySSIDPassword";
const char *mqtt_server   = "MyMQTT";
const int mqtt_port       = 1883;
const char* mqtt_user     = "MQTTusername";
const char* mqtt_password = "MQTTpassword";
const char *device_id     = "Some_random_id";

const char *subpoint_1    = "PUBPOINT/SWITCH";
const char *avlb          = "PUBPOINT/AVLB";
const char *stat          = "SUBPOINT/RES";
const char *willTopic     = "SUBPOINT/RES";
const int   willQoS       = 0;
const int   willRetain    = 1;
const char *willMessage   = "OFFLINE";


WiFiClient espClient;
PubSubClient client(espClient);

//   Realizacija s ESP01 chipom (Esp8266 s malim brojem pinova(8 pina))
const int Taster                       = 0;   // GPIO 0
const int Svjetlo                      = 3;   // Samo Rx pin moze sluziti kao izlaz

const uint8_t BUTTON_PINS[NUM_BUTTONS] = {0};  // Krajnji prekidači odnosno tipkala
char          message_buff[100];
char          msg[50];
int           Switchevi[NUM_BUTTONS]   = { 0 };
bool          LedState                 = 0;    // Varijabla za stanje Led-a odnosno Svjetla
bool          TogleSvjetlo             = 0;    // Varijabla kojom Toglujemo odnosno mijenjamo stanje Led-a
bool          Primio                   = LOW;  // Primljen signal od MQTT brokera
bool          Konektiran               = 0;   // varijabla koja nam govori da li je Esp8266 konektiran na server
int           brojac_0                 = 0;
Bounce * buttons = new Bounce[NUM_BUTTONS];   // poziv klase u program
Ticker timer;


void callback(char* pTopic, byte* payload, unsigned int length)
{
  Serial.print("Message [");
  Serial.print(pTopic);
  Serial.println("] ");
  int i;

  for (i = 0; i < length; i++)
    message_buff[i] = payload[i];
  message_buff[i] = '\0';
  String msgString = String(message_buff);
  Serial.println("Message: ");
  Serial.println(msgString);

  if (strcmp(pTopic, subpoint_1) <= 1)
  {
    if (msgString == "0"){   // Na pr..  ako sam na web UI poslao SklopkaEsp==0 onda palimo sijalicu
      snprintf (msg, 50, "%ld", 0);
      LedState = 0;       // Palimo sijalicu
    } else {
      snprintf (msg, 50, "%ld", 1);
      LedState = 1;       // Gasimo sijalicu
    }
    Primio = 1;
    client.publish(stat, msg);
  }
  if (strcmp(pTopic, avlb) == 0) {
    snprintf (msg, 50, "%ld", 1);
    client.publish(stat, "AVLB");
  }
}

// Interrupt program kada dodje do owerflow Timer1
void ICACHE_RAM_ATTR Vrijeme() {
  static int brojac_0 = 0;

  //   Detekcija tipkala za svijetlo   GPIO4
  if (digitalRead(Taster) == 0) // detekcija log. "0"
    brojac_0++;
  else
    brojac_0 = 0;


  if (brojac_0 == 50 & !Konektiran) // Debounciran prekidac
  {
    //Serial.println("T1");
    LedState = !LedState;
    digitalWrite(Svjetlo, LedState);
    TogleSvjetlo = 0;
  }
  timer1_write(500);      //100 us
}

void Opsluzi_Svjetlo(void)
{
  Switchevi[NUM_BUTTONS] = { 0 };
  for (int i = 0; i < NUM_BUTTONS; i++)  {
    // Update Bounce instance
    buttons[i].update();
    if ( buttons[i].fell() ) {
      Switchevi[i] = 1;
    }
  }
  if ( Switchevi[0] && Konektiran)  {
    TogleSvjetlo = 1;                     // On light or Off light
    Serial.print("Push taster: ");
    Serial.println("GPIO0");
    Switchevi[0] = 0;
  }
  if (Primio)
  {
    digitalWrite(Svjetlo, LedState);
    snprintf (msg, 50, "%ld", LedState);
    client.publish(stat, msg);
    Primio = 0;
    TogleSvjetlo = 0;
  }

  if (TogleSvjetlo)
  {
    // Toggle LED state :
    LedState = !LedState;
    digitalWrite(Svjetlo, LedState);

    snprintf (msg, 50, "%ld", LedState); 
    client.publish(stat, msg); 
    Primio = 0;
    TogleSvjetlo = 0;
  }

}

void reconnect()
{

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.println("Connecting to WiFi..");
    Konektiran = 0;
  }
  while (!client.connected()) {

    Serial.print("Connecting to MQTT server...");
    if (client.connect(device_id, mqtt_user, mqtt_password, willTopic, willQoS, willRetain, willMessage)) {
      Serial.println("spojen");
      LedState = digitalRead(Svjetlo);
      client.subscribe(subpoint_1);
      client.subscribe(avlb);
      Konektiran = 1;
      snprintf (msg, 50, "%ld", LedState);
      client.publish(stat, "AVLB");
      digitalWrite(Svjetlo, LedState);
      TogleSvjetlo = 0;
      client.publish(stat, msg);
    }
    else
    {
      Konektiran = 0;
      Serial.print("Error, rc=");
      Serial.print(client.state());
      Serial.println(" connecting again... ");
    }
  }
}

  void setup() {
    Serial.begin(115200);
    pinMode(Svjetlo, OUTPUT);
    pinMode(Taster, INPUT_PULLUP);

    timer1_attachInterrupt(Vrijeme);    //    ISR Function
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
    /* Djeljitelj:
      TIM_DIV1 = 0,   //80MHz (80 tik/us - 104857.588 us maks.)
      TIM_DIV16 = 1,  //5MHz (5 tik/us - 1677721.4 us maks.)
      TIM_DIV256 = 3  //312.5Khz (1 tik = 3.2us - 26843542.4 us maks.)
      Napuni:
      TIM_SINGLE  0   //na interrupt rutinu potrebno je upisati novu vrijednost za ponovni start timera
      TIM_LOOP  1     //na interrupt brojac ce startati opet s istom vrijednoscu
    */
    // Napuni Timer1 za 0.5s interval
    timer1_write(500);   // 500 / 5 tik po us TIM_DIV16 == 100 us interval



    for (int i = 0; i < NUM_BUTTONS; i++) {
      buttons[i].attach( BUTTON_PINS[i] , INPUT_PULLUP  );  // debounciranje in switch - PULLUP
      buttons[i].interval(50);                              // interval debaunce - ms
    }
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      Serial.println("Connecting to WiFi..");
    }
    Serial.println("Connected to WiFi network!");
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
  }

  void loop() {
    //
    Opsluzi_Svjetlo();
    if (!client.connected())
    {
      reconnect();
    }
    client.loop();
  }
