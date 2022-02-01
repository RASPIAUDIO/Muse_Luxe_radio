  
   extern "C"
{
#include "hal_i2c.h"
//
#include "tinyScreen128x64.h"
}
#include "Arduino.h"
#include <Audio.h>

#include "SPIFFS.h"
#include "IotWebConf.h"
#include "lwip/apps/sntp.h"
#include <NeoPixelBus.h>

///////////////////////////////////////////
//#define SCREEN 0            // without screen
#define SCREEN 1             // with screen
///////////////////////////////////////////


#define I2S_DOUT      26
#define I2S_BCLK      5
#define I2S_LRC       25
#define I2S_DIN       35
#define I2SN (i2s_port_t)0
#define I2CN (i2c_port_t)0
#define SDA 18
#define SCL 23

//Buttons
#define MU GPIO_NUM_12        // ON/OFF
#define VM GPIO_NUM_32        // volume -
#define VP GPIO_NUM_19        // volume +
#define CFG GPIO_NUM_12
#define STOP GPIO_NUM_12

//Amp power enable
#define PA GPIO_NUM_21     
       

#define MAXSTATION 17
#define maxVol 33
#define maxAudio 22

//////////////////////////////
// NeoPixel led control
/////////////////////////////
#define PixelCount 1
#define PixelPin 22
RgbColor RED(255, 0, 0);
RgbColor GREEN(0, 255, 0);
RgbColor BLUE(0, 0, 255);
RgbColor YELLOW(255, 128, 0);
RgbColor WHITE(255, 255, 255);
RgbColor BLACK(0, 0, 0);

RgbColor REDL(64, 0, 0);
RgbColor GREENL(0, 64, 0);
RgbColor BLUEL(0, 0, 64);
RgbColor WHITEL(64, 64, 64);
RgbColor BLACKL(0, 0, 0);
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);



////////////////////////////////////////////////////
const char thingName[] = "muse";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "musemuse";


DNSServer dnsServer;
WebServer server(80);

void playWav(char* n);
char* Rlink(int st);
char* Rname(int st);
int maxStation(void);
int touch_get_level(int t);
bool Baff = false;
int Taff = 0;

time_t now;
struct tm timeinfo;
int previousMin = -1;
char timeStr[10];
char comValue[16];
char newNameValue[16];
char newLinkValue[80];
// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "museRadio V0"


Audio audio;
//httpd_handle_t radio_httpd = NULL;
int b0 = -1,b1 = -1,b2 = -1;
bool mute = false;
int station = 0;
int previousStation;
int vol= maxVol / 2;
int oldVol;
int previousVol = -1;
int selectedVol;
int previousLevel = -1;
int retries = 0;
int vlevel,vmute;

esp_err_t err;
char ssid[32]= "a";
char pwd[32]= "b";
size_t lg;
int MS;

bool muteON = false;
uint32_t sampleRate;
char* linkS;
bool timeON = false;
bool connected = true;
char mes[200];
int iMes ;
bool started = false;

SemaphoreHandle_t buttonsSem = xSemaphoreCreateMutex();




#define ES8388_ADDR 0x10
///////////////////////////////////////////////////////////////////////
// Write ES8388 register (using I2c)
///////////////////////////////////////////////////////////////////////
void ES8388_Write_Reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = val;
    hal_i2c_master_mem_write((i2c_port_t)0, ES8388_ADDR, buf[0], buf + 1, 1);
  // ES8388_REGVAL_TBL[reg]=val;
}

////////////////////////////////////////////////////////////////////////
// Read ES8388 register  (using I2c)
////////////////////////////////////////////////////////////////////////
uint8_t ES8388_Read_Reg( uint8_t reg_add)
{
    uint8_t val;
    hal_i2c_master_mem_read((i2c_port_t)0, ES8388_ADDR, reg_add, &val, 1);
    return val;
}


////////////////////////////////////////////////////////////////////////
//
// manages volume (via vol xOUT1, vol DAC, and vol xIN2)
//
////////////////////////////////////////////////////////////////////////

void ES8388vol_Set(uint8_t volx)
{
#define lowVol 8
 
  if (volx > maxVol) volx = maxVol;
  if (volx == 0)ES8388_Write_Reg(25, 0x04); else ES8388_Write_Reg(25, 0x00);
  if(volx > lowVol)
  {
    audio.setVolume(maxAudio);
    ES8388_Write_Reg(46, volx);
    ES8388_Write_Reg(47, volx);
    ES8388_Write_Reg(26, 0x00);
    ES8388_Write_Reg(27, 0x00); 
  }
  else
  {
    audio.setVolume(maxAudio*volx/lowVol);
    ES8388_Write_Reg(46, lowVol);
    ES8388_Write_Reg(47, lowVol);
    ES8388_Write_Reg(26, 0x00);
    ES8388_Write_Reg(27, 0x00);  
  }
}

//////////////////////////////////////////////////////////////////
//
// init CCODEC chip ES8388 (via I2C)
//
////////////////////////////////////////////////////////////////////
void ES8388_Init(void)
{
// provides MCLK
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
    WRITE_PERI_REG(PIN_CTRL, READ_PERI_REG(PIN_CTRL)& 0xFFFFFFF0);
  
// reset
    ES8388_Write_Reg(0, 0x80);
    ES8388_Write_Reg(0, 0x00);
// mute
    ES8388_Write_Reg(25, 0x04);
    ES8388_Write_Reg(1, 0x50);
//powerup
    ES8388_Write_Reg(2, 0x00);
// slave mode
    ES8388_Write_Reg(8, 0x00);
// DAC powerdown
    ES8388_Write_Reg(4, 0xC0);
// vmidsel/500k ADC/DAC idem
    ES8388_Write_Reg(0, 0x12);
  
    ES8388_Write_Reg(1, 0x00);
// i2s 16 bits
    ES8388_Write_Reg(23, 0x18);
// sample freq 256
    ES8388_Write_Reg(24, 0x02);
// LIN2/RIN2 for mixer
    ES8388_Write_Reg(38, 0x09);
// left DAC to left mixer
    ES8388_Write_Reg(39, 0x90);
// right DAC to right mixer
    ES8388_Write_Reg(42, 0x90);
// DACLRC ADCLRC idem
    ES8388_Write_Reg(43, 0x80);
    ES8388_Write_Reg(45, 0x00);
// DAC volume max
    ES8388_Write_Reg(27, 0x00);
    ES8388_Write_Reg(26, 0x00);
  
    ES8388_Write_Reg(2 , 0xF0);
    ES8388_Write_Reg(2 , 0x00);
    ES8388_Write_Reg(29, 0x1C);
// DAC power-up LOUT1/ROUT1 enabled
    ES8388_Write_Reg(4, 0x30);
// unmute
    ES8388_Write_Reg(25, 0x00);
// amp validation
    gpio_set_level(PA, 1);
    ES8388_Write_Reg(46, 33);
    ES8388_Write_Reg(47, 33);

}

///////////////////////////////////////////////////////////////////////
// task managing the speaker buttons
// normal or long press
/////////://///////////////////////////////////////////////////////////
static void keyb(void* pdata)
{
static int v0, v1, v2;
static int ec0=0, ec1=0, ec2=0;
  while(1)
  {
    if((gpio_get_level(VP) == 1) && (ec0 == 1)){b0 = v0; ec0 = 0;}
    if((gpio_get_level(VP) == 1) && (b0 == -1)) {v0 = 0;ec0 = 0;}
    if(gpio_get_level(VP) == 0) {v0++; ec0 = 1;}
   
    if((gpio_get_level(VM) == 1) && (ec1 == 1)){b1 = v1; ec1 = 0;}
    if((gpio_get_level(VM) == 1) && (b1 == -1)) {v1 = 0;ec1 = 0;}
    if(gpio_get_level(VM) == 0) {v1++; ec1 = 1;}
   
    if((gpio_get_level(MU) == 1) && (ec2 == 1)){b2 = v2; ec2 = 0;}
    if((gpio_get_level(MU) == 1) && (b2 == -1)) {v2 = 0; ec2 = 0;}
    if(gpio_get_level(MU) == 0) {v2++; ec2
    = 1;}
    
  //  printf("%d %d %d %d %d %d\n",b0,b1,b2,v0,v1,v2);
    delay(100);
  }
}
//////////////////////////////////////////////////////////////////////////
// task for battery monitoring
//////////////////////////////////////////////////////////////////////////
#define NGREEN 2300
#define NYELLOW 1800
static void battery(void* pdata)
{
  int val;
  while(1)
  {
     val = adc1_get_raw(ADC1_GPIO33_CHANNEL);
     printf("Battery : %d\n", val);
     if(val < NYELLOW) strip.SetPixelColor(0, RED);
     else if(val > NGREEN) strip.SetPixelColor(0, GREEN);
     else strip.SetPixelColor(0, YELLOW);
     strip.Show();   
     delay(10000);
  }
}


void confErr(void)
{
  drawStrC(26, "Error...");
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  param management (via local server)
//        add     : add a new station
//        del     : delete a station
//        mov     : move a station (change position)
////////////////////////////////////////////////////////////////////////////////////////////////////
void configRadio(void)
{
char com[8];
char comV[16];
char* P;
int n, m;
char lf[] = {0x0A, 0x00};
char li[80];
char na[17];
    started = false;
    clearBuffer();
    drawStrC(16, "initializing...");
    sendBuffer();
  
    strcpy(comV, comValue);
     P = strtok(comV, ",");
     strcpy(com, P);
     P = strtok(NULL, ",");
     if( P != NULL) 
     {
      n = atoi(P);
      P = strtok(NULL, ",");
      if(P != NULL) m = atoi(P);
     }
  
    printf("xxxxxxxxxxxxxxxxxx %s   %d   %d\n",com, n, m);
   
    if(strcmp(com, "add") == 0)
    {
      printf("add\n");
      printf("link ==> %s\n", newLinkValue);
      printf("name ==> %s\n", newNameValue);
  
      
      File ln = SPIFFS.open("/linkS", "r+");
      ln.seek(0, SeekEnd);
      ln.write((const uint8_t*)newLinkValue, strlen(newLinkValue));
      ln.write((const uint8_t*)lf, 1);
      ln.close();
      ln = SPIFFS.open("/nameS", "r+");
      ln.seek(0,SeekEnd);
      ln.write((const uint8_t*)newNameValue, strlen(newNameValue));
      ln.write((const uint8_t*)lf, 1);
      ln.close();    
    }
    else
    {
   
      if(strcmp(com, "del") == 0)
      {
        File trn = SPIFFS.open("/trn", "w");
        File trl = SPIFFS.open("/trl", "w");
        for(int i=0;i<n;i++)
        {
          strcpy(li,Rlink(i));
          strcpy(na, Rname(i));
          trn.write((const uint8_t*)na, strlen(na));
          trn.write((const uint8_t*)lf, 1);
          trl.write((const uint8_t*)li, strlen(li));
          trl.write((const uint8_t*)lf, 1);
        }
        for(int i=n+1;i<=MS;i++)
        {
          strcpy(li,Rlink(i));
          strcpy(na, Rname(i));
          trn.write((const uint8_t*)na, strlen(na));
          trn.write((const uint8_t*)lf, 1);
          trl.write((const uint8_t*)li, strlen(li));
          trl.write((const uint8_t*)lf, 1);
        }
        SPIFFS.remove("/nameS");
        SPIFFS.remove("/linkS");
        SPIFFS.rename("/trn", "/nameS");
        SPIFFS.rename("/trl", "/linkS");
      }
      else if(strcmp(com, "mov") == 0)
      {
        File trn = SPIFFS.open("/trn", "w");
        File trl = SPIFFS.open("/trl", "w");
        if(n > m)
        {
        for(int i=0;i<m;i++)
        {
          strcpy(li,Rlink(i));
          strcpy(na, Rname(i));
          trn.write((const uint8_t*)na, strlen(na));
          trn.write((const uint8_t*)lf, 1);
          trl.write((const uint8_t*)li, strlen(li));
          trl.write((const uint8_t*)lf, 1);
        }
          strcpy(li,Rlink(n));
          strcpy(na, Rname(n));
          trn.write((const uint8_t*)na, strlen(na));
          trn.write((const uint8_t*)lf, 1);
          trl.write((const uint8_t*)li, strlen(li));
          trl.write((const uint8_t*)lf, 1);
        for(int i=m;i<n;i++)
        {
          strcpy(li,Rlink(i));
          strcpy(na, Rname(i));
          trn.write((const uint8_t*)na, strlen(na));
          trn.write((const uint8_t*)lf, 1);
          trl.write((const uint8_t*)li, strlen(li));
          trl.write((const uint8_t*)lf, 1);
        }
         for(int i=n+1;i<=MS;i++)
        {
          strcpy(li,Rlink(i));
          strcpy(na, Rname(i));
          trn.write((const uint8_t*)na, strlen(na));
          trn.write((const uint8_t*)lf, 1);
          trl.write((const uint8_t*)li, strlen(li));
          trl.write((const uint8_t*)lf, 1);
        }
        }
        else
        {
          for(int i=0;i<n;i++)
        {
          strcpy(li,Rlink(i));
          strcpy(na, Rname(i));
          trn.write((const uint8_t*)na, strlen(na));
          trn.write((const uint8_t*)lf, 1);
          trl.write((const uint8_t*)li, strlen(li));
          trl.write((const uint8_t*)lf, 1);
        }
         
        for(int i=n+1;i<m+1;i++)
        {
          strcpy(li,Rlink(i));
          strcpy(na, Rname(i));
          trn.write((const uint8_t*)na, strlen(na));
          trn.write((const uint8_t*)lf, 1);
          trl.write((const uint8_t*)li, strlen(li));
          trl.write((const uint8_t*)lf, 1);
        }
          strcpy(li,Rlink(n));
          strcpy(na, Rname(n));
          trn.write((const uint8_t*)na, strlen(na));
          trn.write((const uint8_t*)lf, 1);
          trl.write((const uint8_t*)li, strlen(li));
          trl.write((const uint8_t*)lf, 1);
         for(int i=m+1;i<=MS;i++)
        {
          strcpy(li,Rlink(i));
          strcpy(na, Rname(i));
          trn.write((const uint8_t*)na, strlen(na));
          trn.write((const uint8_t*)lf, 1);
          trl.write((const uint8_t*)li, strlen(li));
          trl.write((const uint8_t*)lf, 1); 
        }
        }
        SPIFFS.remove("/nameS");
        SPIFFS.remove("/linkS");
        SPIFFS.rename("/trn", "/nameS");
        SPIFFS.rename("/trl", "/linkS");  
    }
    started = true;
}
}

//////////////////////////////////////////////////////////////////
// local
// detects non ASCII chars and converts them (if possible...)
/////////////////////////////////////////////////////////////////
void convToAscii(char *s, char *t)
{
  int j = 0;
  for(int i=0; i<strlen(s);i++)
  {
    if(s[i] < 128) t[j++] = s[i];
    else
    {
      if(s[i] == 0xC2)
      {
        t[j++] = '.';
        i++;
      }
      else if(s[i] == 0xC3)
      {
        i++;
        if((s[i] >= 0x80) && (s[i] < 0x87)) t[j++] = 'A';
        else if((s[i] >= 0x88) && (s[i] < 0x8C)) t[j++] = 'E';
        else if((s[i] >= 0x8C) && (s[i] < 0x90)) t[j++] = 'I'; 
        else if(s[i] == 0x91) t[j++] = 'N';
        else if((s[i] >= 0x92) && (s[i] < 0x97)) t[j++] = 'O'; 
        else if(s[i] == 0x97) t[j++] = 'x';
        else if(s[i] == 0x98) t[j++] = 'O';
        else if((s[i] >= 0x99) && (s[i] < 0x9D)) t[j++] = 'U'; 
        else if((s[i] >= 0xA0) && (s[i] < 0xA7)) t[j++] = 'a'; 
        else if((s[i] == 0xA7) ) t[j++] = 'c'; 
        else if((s[i] >= 0xA8) && (s[i] < 0xAC)) t[j++] = 'e'; 
        else if((s[i] >= 0xAC) && (s[i] < 0xB0)) t[j++] = 'i'; 
        else if(s[i] == 0xB1) t[j++] = 'n';
        else if((s[i] >= 0xB2) && (s[i] < 0xB7)) t[j++] = 'o'; 
        else if(s[i] == 0xB8) t[j++] = 'o';
        else if((s[i] >= 0xB9) && (s[i] < 0xBD)) t[j++] = 'u'; 
        else t[j++] = '.';
      }
    }
  }

  t[j] = 0;
}



/////////////////////////////////////////////////////////////////////
// play station task (core 1)
//
/////////////////////////////////////////////////////////////////////
static void playRadio(void* data)
{
  while (started == false) delay(100);
  while(1)
  {
  // printf("st %d prev %d\n",station,previousStation);
    if((station != previousStation)||(connected == false))
    {     
        printf("station no %d\n",station);    
        i2s_stop(I2SN);
        i2s_zero_dma_buffer(I2SN);
        delay(500);
        i2s_start(I2SN);     
        audio.stopSong();
        connected = false;
        //delay(100);
        linkS = Rlink(station);
        mes[0] = 0;
        audio.connecttohost(linkS);
        previousStation = station;
    }
  // delay(10);
  //if(connected == false) delay(50);

  audio.loop();
  }
}


/////////////////////////////////////////////////////////////////////////////
// gets station link from SPIFFS file "/linkS"
//
/////////////////////////////////////////////////////////////////////////////
char* Rlink(int st)
{
int i;
static char b[80];
  File ln = SPIFFS.open("/linkS", FILE_READ);
  i = 0;
  uint8_t c;
  while(i != st)
  {
    while(c != 0x0a)ln.read(&c, 1);
    c = 0;
    i++;
  }
  i = 0;
  do
  {
    ln.read((uint8_t*)&b[i], 1);
    i++;
  }while(b[i-1] != 0x0a);
  b[i-1] = 0;
  ln.close();
  return b;
}
/////////////////////////////////////////////////////////////////////////////////
//  gets station name from SPIFFS file "/namS"
//
/////////////////////////////////////////////////////////////////////////////////
char* Rname(int st)
{
int i;
static char b[20];
  File ln = SPIFFS.open("/nameS", FILE_READ);
  i = 0;
  uint8_t c;
  while(i != st)
  {
    while(c != 0x0a)ln.read(&c, 1);
    c = 0;
    i++;
  }
  i = 0;
  do
  {
    ln.read((uint8_t*)&b[i], 1);
    i++;
  }while(b[i-1] != 0x0a);
  b[i-1] = 0;
  ln.close();
  return b;
}
/////////////////////////////////////////////////////////////////////////
//  defines how many stations in SPIFFS file "/linkS"
//
////////////////////////////////////////////////////////////////////////
int maxStation(void)
{
  File ln = SPIFFS.open("/linkS", FILE_READ);
  uint8_t c;
  int m = 0;
  int t;
  t = ln.size();
  int i = 0;
  do 
  {
    while(c != 0x0a){ln.read(&c, 1); i++;}
    c = 0;
    m++;
  }while(i < t);
  ln.close();
  return m;  
}

///////////////////////////////////////////////////////////////////////////////////
//
//  init. local server custom parameters
//
///////////////////////////////////////////////////////////////////////////////////

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
IotWebConfParameter comParam = IotWebConfParameter("Action", "actionParam", comValue, 16, "action", "add, del or mov", NULL);
IotWebConfSeparator separator1 = IotWebConfSeparator();
IotWebConfParameter newNameParam = IotWebConfParameter("New Name", "nameParam", newNameValue, 16);
IotWebConfSeparator separator2 = IotWebConfSeparator();
IotWebConfParameter newLinkParam = IotWebConfParameter("New Link", "linkParam", newLinkValue, 80);

void setup() { 
Serial.begin(115200);

if(!SPIFFS.begin())Serial.println("Erreur SPIFFS");
// SPIFFS maintenance   
  
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while(file){
      Serial.print("FILE: ");
      Serial.println(file.name());
      file = root.openNextFile();
    }
    printf("====> %d\n",(int)SPIFFS.totalBytes());
    printf("====> %d\n",(int)SPIFFS.usedBytes());   
    //SPIFFS.format();
    
printf(" SPIFFS used bytes  ====> %d of %d\n",(int)SPIFFS.usedBytes(), (int)SPIFFS.totalBytes());      

////////////////////////////////////////////////////////////////
// init led handle
///////////////////////////////////////////////////////////////
  strip.Begin();  

////////////////////////////////////////////////////////////////
// init ADC interface for battery survey
/////////////////////////////////////////////////////////////////
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_GPIO33_CHANNEL, ADC_ATTEN_DB_11);
////////////////////////////////////////////////////////////////
  hal_i2c_init(0, SDA, SCL);
  
// variables de travail
  previousStation = -1;
  station = 0;
  MS = maxStation()-1;
  printf("max ===> %d\n",MS);

/////////////////////////////////////////////////////////////
// recovers params (station & vol)
///////////////////////////////////////////////////////////////
    char b[4];
    File ln = SPIFFS.open("/station", "r");
    ln.read((uint8_t*)b, 2);
    b[2] = 0;
    station = atoi(b);
    ln.close();
    ln = SPIFFS.open("/volume", "r");
    ln.read((uint8_t*)b, 2);
    b[2] = 0;
    vol = atoi(b);
    ln.close();
    printf("station = %d    vol = %d\n",station, vol);
///////////////////////////////////////////////////////   
// initi gpios
////////////////////////////////////////////////////////////
//gpio_reset_pin
    gpio_reset_pin(MU);
    gpio_reset_pin(VP);
    gpio_reset_pin(VM);

//       gpio_reset_pin(STOP);      
     
//gpio_set_direction
    gpio_set_direction(MU, GPIO_MODE_INPUT);  
    gpio_set_direction(VP, GPIO_MODE_INPUT);  
    gpio_set_direction(VM, GPIO_MODE_INPUT);  
      
//        gpio_set_direction(STOP, GPIO_MODE_INPUT);  

//gpio_set_pull_mode
    gpio_set_pull_mode(MU, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(VP, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(VM, GPIO_PULLUP_ONLY);

//        gpio_set_pull_mode(STOP, GPIO_PULLUP_ONLY);


// power enable
    gpio_reset_pin(PA);
    gpio_set_direction(PA, GPIO_MODE_OUTPUT);   
    gpio_set_pull_mode(PA, GPIO_PULLUP_ONLY);   
    gpio_set_level(PA, 1); 

// init audio
   audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
// init  codec ES8388
   printf("%d %d\n", vol, maxVol);
   ES8388_Init();
// init volume   
   ES8388vol_Set(vol);  


 
// init screen handler   
#if SCREEN == 1
   tinySsd_init(SDA, SCL, 0, 0x3C, 1);  
   clearBuffer();
   sendBuffer();
   drawBigStrC(24,"Ros&Co");
   sendBuffer();
#endif

/////////////////////////////////////////////////////////
//init WiFi  
//////////////////////////////////////////////////////////////
// init. local server main parameters
//////////////////////////////////////////////////////////////
    iotWebConf.addParameter(&comParam);
    iotWebConf.addParameter(&separator1);
    iotWebConf.addParameter(&newNameParam);
    iotWebConf.addParameter(&separator2);
    iotWebConf.addParameter(&newLinkParam);
  //init custom parameters  management callbacks 
    iotWebConf.setConfigSavedCallback(&configRadio);  
    iotWebConf.setFormValidator(&formValidator);

  // -- Initializing the configuration.
  // pin for manual init  
    iotWebConf.setConfigPin(CFG);
    iotWebConf.init();
  
     // iotWebConf.setApTimeoutMs(30000);
  // init web server
  // -- Set up required URL handlers on the web server.
    server.on("/", handleRoot);
    server.on("/config", []{ iotWebConf.handleConfig(); });
    server.onNotFound([](){ iotWebConf.handleNotFound(); });
  
    delay(1000);
    xTaskCreatePinnedToCore(playRadio, "radio", 5000, NULL, 5, NULL,0);
  //task managing the battery
    xTaskCreate(battery, "battery", 5000, NULL, 1, NULL);  
  
    //task managing buttons
    xTaskCreate(keyb, "keyb", 5000, NULL, 5, NULL);

  
}

void loop() {
#define longK 8  
//static int v0, v1, v2;
//static int ec0=0, ec1=0, ec2=0;
   iotWebConf.doLoop();
   if (WiFi.status() != WL_CONNECTED) return; 
   started = true;

   if(timeON == false)
   {
//////////////////////////////////////////////////////////////////
// initialisation temps NTP
//
////////////////////////////////////////////////////////////////////
// time zone init
    setenv("TZ", "CEST-1", 1);
    tzset();
//sntp init
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
    int retry = 0;
    while((timeinfo.tm_year < (2016 - 1900)) && (++retry < 20))
    {
        delay(500);
        time(&now);
        localtime_r(&now, &timeinfo);
     }
     timeON = true;
   }
   
   Baff = false;
   
 
    oldVol = vol;
    if((b0 > 0) && (b0 < longK)) {vol = vol + 1 ;b0 = -1;}
    if((b1 > 0) && (b1 < longK)) {vol = vol - 1 ;b1 = -1;}
  
    if (vol > maxVol) vol = maxVol;
    if (vol < 0) vol = 0;

//changement de volume
     if(vol != oldVol)
     {
        Baff = true;
        muteON = false;
        oldVol = vol;  
        printf("vol = %d\n",vol);
        ES8388vol_Set(vol);
        char b[4];
        sprintf(b,"%02d",vol);     
        File ln = SPIFFS.open("/volume", "w");
        ln.write((uint8_t*)b, 2);
        ln.close();     
     }
  
// changement de station
     if((b0 > 0) && (b0 > longK)) {station++; b0 = -1;}
     if((b1 > 0) && (b1 > longK)) {station--; b1 = -1;}

     if(station > MS) station = 0;
     if(station < 0) station = MS;
     if(station != previousStation)
     {
        delay(500);
        Baff = true;
        char b[4];
        sprintf(b,"%02d",station);
        File ln = SPIFFS.open("/station", "w");
        ln.write((uint8_t*)b, 2);
        ln.close();   
     }

// mute / unmute
    if ((b2 > 0) && (b2 < longK))
    {
      if (mute == false)
      {
        mute = true;
        ES8388_Write_Reg(25, 0x04);
      }
      else
      {
        mute = false;
        ES8388_Write_Reg(25, 0x00);
      }
      b2 = -1;
  } 

// deep sleep
    if((b2 > 0) && (b2 > longK) )
    {
      clearBuffer();
      sendBuffer();
      strip.SetPixelColor(0, BLACK);
      strip.Show();
      delay(1000);
      esp_sleep_enable_ext0_wakeup(STOP,LOW);     
      esp_deep_sleep_start();
    }
// time
    time(&now);
    localtime_r(&now, &timeinfo);

    if(previousMin != timeinfo.tm_min)
    {
      Baff = true;
      previousMin = timeinfo.tm_min;
    }
    if(connected == false) Baff = true;
    if(strlen(mes) != 0) Baff = true;
    Taff++;
    if(Taff > 5)
    {
      Taff = 0;
      Baff = true;
    }

   
#if SCREEN == 1
//////////////////////////////////////////////////////////   
// handling display 
/////////////////////////////////////////////////////////
    if(Baff == true)
     {
        clearBuffer();
      
      //displays station name
        drawStrC(14,Rname(station));
        drawHLine(23, 0, 128);
        drawHLine(52, 0, 128);
       
      //displays time (big chars) 
        sprintf(timeStr,"%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min); 
        drawBigStrC(30, timeStr);
      
        if (connected == false) 
        {
          drawStrC(16,"..........");
        }
       
      // displays sound index (60x10, 10 values)
        drawIndexb(28, 119, 20, 8, 10, vol*10/maxVol);
        
      //displays scrolling messages
       if(strlen(mes) != 0)
       {
        char mesa[17];
        strncpy(mesa, &mes[iMes],16);
        if(strlen(mesa) < 16) iMes = 0; else iMes++;
        mesa[16] = 0;
       
       drawStr(56, 0, mesa);
       }
       sendBuffer();
    
    
     }

   
#endif
 
   delay(200);
}

///////////////////////////////////////////////////////////////////////
//  stuff for  web server intialization
//       wifi credentials and other things...
//
/////////////////////////////////////////////////////////////////////////
void handleRoot()
{
  char b[6];
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>Muse Radio</title></head><body>--- MUSE Radio --- Ros & Co ---";
  s += "<li>---- Stations ----";
  s += "<ul>";

  for(int i=0;i<=MS;i++)
  {
  s += "<li>";
  sprintf(b, "%02d  ",i);
  s += (String)b;
  s += (String) Rname(i);
  }
  s += "</ul>";
  s += "Go to <a href='config'>configure page</a> to change values (wifi credentials, stations names and links...)";
  s += "</body></html>\n";

  server.send(200, "text/html", s);

}
////////////////////////////////////////////////////////////////////////
// custom parameters verification
///////////////////////////////////////////////////////////////////
boolean formValidator()
{
  Serial.println("Validating form.");
  boolean valid = true;
  if(server.arg(comParam.getId()).length() > 0)
  {
    String buf;
    String com;
    String name;
    String link;
    int n,m;
    buf = server.arg(comParam.getId());
    com = server.arg(comParam.getId()).substring(0,3);

    if((com != "add") && (com != "del") && (com != "mov")) 
    {
      comParam.errorMessage = "Action should be add, del or mov";
      valid = false;
      return valid;
    }
    if(com == "add")
    {
      name = server.arg(newNameParam.getId());
      link = server.arg(newLinkParam.getId());
      if((name.length() == 0) || (name.length() > 16))
      {
        newNameParam.errorMessage = "add needs a station name (16 chars max)";
        valid = false;
        return valid;
      }
      if(link.length() == 0)
      {
        newLinkParam.errorMessage = "add needs a valid link";
        valid = false;
        return valid;
      }
    }
    if(com == "del")
    {
      int l = buf.indexOf(',');
      if(l == -1)
      {
        comParam.errorMessage = "incorrect del... del,[station to delete] (ie del,5)";
        valid = false;
        return valid;
      }
      sscanf(&buf[l+1],"%d",&n);
      if((n < 0) || (n >= MS))
      {
        comParam.errorMessage = "incorrect station number";
        valid = false;
        return valid;
      }
      
    }
    if(com == "mov")
    {
      int l = buf.indexOf(',');
      int k = buf.lastIndexOf(',');
      if((l == -1)||(k == -1))
      {
        comParam.errorMessage = "incorrect mov... mov,[old position],[new position] (ie mov,5,7)";
        valid = false;
        return valid;
      }
      sscanf(&buf[l+1],"%d",&n);
      sscanf(&buf[k+1],"%d",&m);
      if((n < 0) || (n > MS)|| (m < 0) || (m > MS) || (m == n))
      {
        comParam.errorMessage = "incorrect station number";
        valid = false;
        return valid;
      }
      
    }  
  }
  return valid;
}

// optional
void audio_info(const char *info){
#define maxRetries 4
   // Serial.print("info        "); Serial.println(info);
    if(strstr(info, "SampleRate=") > 0) 
    {
    sscanf(info,"SampleRate=%d",&sampleRate);
    printf("==================>>>>>>>>>>%d\n", sampleRate);
    }
    connected = true;   
    if(strstr(info, "failed") > 0){connected = false; printf("failed\n");}
}
void audio_id3data(const char *info){  //id3 metadata
    //Serial.print("id3data     ");Serial.println(info);
}
void audio_eof_mp3(const char *info){  //end of file
    //Serial.print("eof_mp3     ");Serial.println(info);
}
void audio_showstation(const char *info){
    //Serial.print("station     ");Serial.println(info);
}
void audio_showstreaminfo(const char *info){
  //  Serial.print("streaminfo  ");Serial.println(info);
}
void audio_showstreamtitle(const char *info){
   Serial.print("streamtitle ");Serial.println(info);
   if(strlen(info) != 0) 
   {
   convToAscii((char*)info, mes);
   iMes = 0;
   }
   else mes[0] = 0;
}
void audio_bitrate(const char *info){
   // Serial.print("bitrate     ");Serial.println(info);
}
void audio_commercial(const char *info){  //duration in sec
   // Serial.print("commercial  ");Serial.println(info);
}
void audio_icyurl(const char *info){  //homepage
   // Serial.print("icyurl      ");Serial.println(info);
}
void audio_lasthost(const char *info){  //stream URL played
    //Serial.print("lasthost    ");Serial.println(info);
}
void audio_eof_speech(const char *info){
    //Serial.print("eof_speech  ");Serial.println(info);
} 
