#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <dht11.h>
#include <MQ2.h>
#include <Servo.h>
#include <stdio.h>

//Beeb
#define BeebPin  13

//设置SG90
Servo myservo;
#define ServoPin  14
const char* SubTopic_ControlAppliances = "ljxxxy/Control/Appliances";     //控制电器

//设置MQ2
int pin = A0;
MQ2 mq2(pin);
const char* PubTopic_LPG = "ljxxxy/environment/LPGCurve";           //LPG主题
const char* PubTopic_CO = "ljxxxy/environment/COCurve";             //CO主题
const char* PubTopic_Smoke = "ljxxxy/environment/SmokeCurve";       //Smoke主题

//设置DHT11
dht11 DHT11;//定义传感器类型
#define DHT11PIN 12//定义传感器连接引脚。此处的PIN2在NodeMcu8266开发板上对应的引脚是D6
float temperature = 0;
float humidity = 0;
bool Flag_Data = false;
const char* PubTopic_Temperature = "ljxxxy/environment/temperature";      //温度主题
const char* PubTopic_Humidity = "ljxxxy/environment/humidity";            //湿度主题

//获取环境信息通用主题
const char* SubTopic_GetData = "ljxxxy/environment/getdata";              //获取数据

// 设置wifi接入信息(请根据您的WiFi信息进行修改)
const char* ssid = "Ljxxxy";
const char* password = "987654321";
const char* mqttServer = "iot.ranye-iot.net";
// 如以上MQTT服务器无法正常连接，请前往以下页面寻找解决方案
// http://www.taichi-maker.com/public-mqtt-broker/
 
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
 
// ****************************************************
// 注意！以下需要用户根据然也物联平台信息进行修改！否则无法工作!
// ****************************************************
const char* mqttUserName = "ljxxxy_1";         // 服务端连接用户名(需要修改)
const char* mqttPassword = "16037573";          // 服务端连接密码(需要修改)
const char* clientId = "ljxxxy_1_id";          // 客户端id (需要修改)
const char* subTopic = "ljxxxy/led_kz1";        // 订阅主题(需要修改)
const char* pubTopic = "ljxxxy/led_zt1";        // 订阅主题(需要修改)
const char* willTopic = "ljxxxy/led_yz1";       // 遗嘱主题名称(需要修改)
// ****************************************************

//遗嘱相关信息
const char* willMsg = "esp8266_1 offline";        // 遗嘱主题信息
const int willQos = 0;                          // 遗嘱QoS
const int willRetain = false;                   // 遗嘱保留

const int subQoS = 1;            // 客户端订阅主题时使用的QoS级别（截止2020-10-07，仅支持QoS = 1，不支持QoS = 2）
const bool cleanSession = false; // 清除会话（如QoS>0必须要设为false）

bool ledStatus = HIGH;

//函数声明
void connectWifi(void);
void connectMQTTserver(void);
void subscribeTopic(void);
void receiveCallback(char* topic, byte* payload, unsigned int length);
bool DHT11_GetData(float* temperature, float* humidity);
void pubTemmsg(void);
void pubQuamsg(void);
void Servo_TurnOff(void);
void Servo_TurnOn(void);
void pubMQTTmsg(void);

void setup() {
  // LED
  pinMode(LED_BUILTIN, OUTPUT);          // 设置板上LED引脚为输出模式
  digitalWrite(LED_BUILTIN, ledStatus);  // 启动后关闭板上LED

  //Beeb
  pinMode(BeebPin, OUTPUT);          // 设置板上Beeb引脚为输出模式
  digitalWrite(BeebPin, HIGH);       // 启动后关闭板上Beeb

  Serial.begin(9600);                    // 启动串口通讯
  
  // 设置SG90
  myservo.attach(14);
  myservo.write(0);

  // 设置MQ2
  mq2.begin();

  // 设置ESP8266工作模式为无线终端模式
  WiFi.mode(WIFI_STA);
  
  // 连接WiFi
  connectWifi();
  
  // 设置MQTT服务器和端口号
  mqttClient.setServer(mqttServer, 1883);
  mqttClient.setCallback(receiveCallback);
 
  // 连接MQTT服务器
  connectMQTTserver();
}
 
void loop() {
  // 如果开发板未能成功连接服务器，则尝试连接服务器
  if (!mqttClient.connected()) {
    connectMQTTserver();
  }
 
   // 处理信息以及心跳
   mqttClient.loop();
}
 
// 连接MQTT服务器并订阅信息
void connectMQTTserver(void){
  // 根据ESP8266的MAC地址生成客户端ID（避免与其它ESP8266的客户端ID重名）
  
 
  /* 连接MQTT服务器
  boolean connect(const char* id, const char* user, 
                  const char* pass, const char* willTopic, 
                  uint8_t willQos, boolean willRetain, 
                  const char* willMessage, boolean cleanSession); 
  若让设备在离线时仍然能够让qos1工作，则connect时的cleanSession需要设置为false                
                  */
  if (mqttClient.connect(clientId, mqttUserName, 
                         mqttPassword, willTopic, 
                         willQos, willRetain, willMsg, cleanSession)) { 
    Serial.print("MQTT Server Connected. ClientId: ");
    Serial.println(clientId);
    Serial.print("MQTT Server: ");
    Serial.println(mqttServer);    
    
    subscribeTopic(); // 订阅指定主题
  } else {
    Serial.print("MQTT Server Connect Failed. Client State:");
    Serial.println(mqttClient.state());
    delay(5000);
  }   
}
 
// 收到信息后的回调函数
void receiveCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message Received [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("");
  Serial.print("Message Length(Bytes) ");
  Serial.println(length);
  
  if ((char)payload[0] == 'o' && (char)payload[1] == 'n'){
    Servo_TurnOn();
  }
  if ((char)payload[0] == 'o' && (char)payload[1] == 'f' && (char)payload[2] == 'f'){
    Servo_TurnOff();
  }

  if ((char)payload[0] == 't' && (char)payload[1] == 'e' && (char)payload[2] == 'm'){
    pubTemmsg();
  }

  if ((char)payload[0] == 'q' && (char)payload[1] == 'u' && (char)payload[2] == 'a'){
    pubQuamsg();
  }

  if ((char)payload[0] == '1') {     // 如果收到的信息以“1”为开始
    ledStatus = LOW;
    digitalWrite(BUILTIN_LED, ledStatus);  // 则点亮LED。
    pubMQTTmsg();
  } 

  if ((char)payload[0] == '0') {
    ledStatus = HIGH;                           
    digitalWrite(BUILTIN_LED, ledStatus); // 否则熄灭LED。
    pubMQTTmsg();
  }
 
  // pubMQTTmsg();
}
 
// 订阅指定主题
void subscribeTopic(void){
  // 通过串口监视器输出是否成功订阅主题以及订阅的主题名称
  // 请注意subscribe函数第二个参数数字为QoS级别。这里为QoS = 1
  if(mqttClient.subscribe(subTopic, subQoS)){
    Serial.print("Subscribed Topic: ");
    Serial.println(subTopic);
  } else {
    Serial.print("Subscribe Fail...");
  }  

  if(mqttClient.subscribe(SubTopic_GetData, subQoS)){
    Serial.print("Subscribed Topic: ");
    Serial.println(SubTopic_GetData);
  } else {
    Serial.print("Subscribe Fail...");
  }

  if(mqttClient.subscribe(SubTopic_ControlAppliances, subQoS)){
    Serial.print("Subscribed Topic: ");
    Serial.println(SubTopic_ControlAppliances);
  } else {
    Serial.print("Subscribe Fail...");
  }
}
 
bool DHT11_GetData(float* temperature, float* humidity){
  DHT11.read(DHT11PIN); //更新传感器所有信息
  *temperature = DHT11.temperature;
  *humidity = DHT11.humidity;
  if(temperature != 0 && humidity != 0){
    return true;
  }
  else{
    return false;
  }
}

//发布空气质量
void pubQuamsg(void){
  char LPGMessage[20];
  char COMessage[20];
  char SmokeMessage[20];

  float LPG, CO, Smoke;

  // mq2.begin();

  LPG = mq2.readLPG();        //0.015g超标
  CO = mq2.readCO();          //0.010g超标
  Smoke = mq2.readSmoke();    //0.035g超标

  if (LPG > 5 || CO > 10 || Smoke > 10){
    for(int i = 0; i < 50; i ++){
      digitalWrite(BeebPin, LOW);
      delay(10);
      digitalWrite(BeebPin, HIGH);
      delay(10);
    }
  }

  sprintf(LPGMessage, "%f", LPG);
  sprintf(COMessage, "%f", CO);
  sprintf(SmokeMessage, "%f", Smoke);

  if(mqttClient.publish(PubTopic_LPG, LPGMessage)){
    // Serial.println("Publish Topic:");Serial.println(Topic_Temperature);
    // Serial.println("Publish message:");Serial.println(temperatureMessage);    
  } else {
    Serial.println("LPG Message Publish Failed."); 
  }
  if(mqttClient.publish(PubTopic_CO, COMessage)){
    // Serial.println("Publish Topic:");Serial.println(Topic_Temperature);
    // Serial.println("Publish message:");Serial.println(temperatureMessage);    
  } else {
    Serial.println("CO Message Publish Failed."); 
  }
  if(mqttClient.publish(PubTopic_Smoke, SmokeMessage)){
    // Serial.println("Publish Topic:");Serial.println(Topic_Temperature);
    // Serial.println("Publish message:");Serial.println(temperatureMessage);    
  } else {
    Serial.println("Smoke Message Publish Failed."); 
  }
}

//发布温湿度
void pubTemmsg(void){
  char temperatureMessage[20];
  char humidityMessage[20];
  if(DHT11_GetData(&temperature, &humidity)){
    sprintf(temperatureMessage, "%f", temperature);
    sprintf(humidityMessage, "%f", humidity);

    Serial.print("Temperature: ");
    Serial.println(temperature);

    if(mqttClient.publish(PubTopic_Temperature, temperatureMessage)){
      // Serial.println("Publish Topic:");Serial.println(Topic_Temperature);
      // Serial.println("Publish message:");Serial.println(temperatureMessage);    
    } else {
      Serial.println("Temperature Message Publish Failed."); 
    }
    if(mqttClient.publish(PubTopic_Humidity, humidityMessage)){
      // Serial.println("Publish Topic:");Serial.println(Topic_Temperature);
      // Serial.println("Publish message:");Serial.println(temperatureMessage);    
    } else {
      Serial.println("Humidity Message Publish Failed."); 
    }
  }
}

// 开
void Servo_TurnOff(void){
  myservo.write(0);
}

// 关
void Servo_TurnOn(void){
  myservo.write(180);
}

// 发布LED信息
void pubMQTTmsg(){
  char* pubMessage;
  
  if (ledStatus == LOW){
    pubMessage = "LED ON";
  } else {
    pubMessage = "LED OFF";
  }
 
  // 实现ESP8266向主题发布信息
  if(mqttClient.publish(pubTopic, pubMessage)){
    Serial.println("Publish Topic:");Serial.println(pubTopic);
    Serial.println("Publish message:");Serial.println(pubMessage);    
  } else {
    Serial.println("Message Publish Failed."); 
  }
}
 
// ESP8266连接wifi
void connectWifi(){
 
  WiFi.begin(ssid, password);
 
  //等待WiFi连接,成功连接后输出成功信息
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi Connected!");  
  Serial.println(""); 
}