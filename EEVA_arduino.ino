#include <ESP8266WiFi.h>
#include <FirebaseArduino.h>

#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>

#define FIREBASE_HOST "anywhereadapter-default-rtdb.firebaseio.com" 
#define FIREBASE_AUTH "6KJCttDaaXReHvhIcLzaeGn6dXa2hu8UFpoNXLn5" 
#define WIFI_SSID "yeonghak" // 연결 가능한 wifi의 ssid
#define WIFI_PASSWORD "1q2w3e4r" // wifi 비밀번호

#define SS_PIN 15
#define RST_PIN 0
MFRC522 mfrc522(SS_PIN, RST_PIN);

int con = 0; //릴레이모듈 on, off
int la = 3; // 릴레이모듈 핀
int min_time = 1;
double raw_data_A; // 아날로그데이터
double voltage_A; // 전압값
double current_A; // 전류값 계산용 
int power_A; // db에 보내는 전류값
int sum_power_A = 0;
int ave_power_A = 0;
double charge = 0;
String address = "Users/6b7YDKyExXcV3HVZrhZ6qk8KeAe2/point"; // 주소찾아가기 테스트용 스트링
int getout = 0;

String address_users = "Users/";
String address_nfc = "nfc/";
String pointaddress = "/point";
String uidaddress = "/uid";
String nfcaddress = "/nfc";
String numberaddress = "/number";
int n = 1;
String numberString = "0";
String addaddresssum = "0";
String usersaddress = "0";
String getuid = "0";
int getpoint = 0;
int fromnfc = 0;

void setup() {

  // put your setup code here, to run once:

  SPI.begin();
  mfrc522.PCD_Init();

 
  Serial.begin(57600);

  pinMode(la, OUTPUT);
  digitalWrite(la, LOW); //전원차단 상태

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("connecting");

  

  while (WiFi.status() != WL_CONNECTED) {

    Serial.print(".");

    delay(500);

   }

  Serial.println();

  Serial.print("connected: ");

  Serial.println(WiFi.localIP());

  

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.setInt("current",0); // DB전류값 초기화
  Firebase.setInt("ready",0); 
}

 

void loop() {
  
  //Serial.println(Firebase.getInt(address)); // 하위데이터 빼오기 테스트
  
  if(con == 0) {
    Serial.println("Mode : OFF");
    digitalWrite(la, LOW);
    delay(1000);
    con = 1;
  }

  if(Firebase.getInt("ready")==1) {

    Serial.println("Mode : ON");
    digitalWrite(la, HIGH);
    sum_power_A = 0; // 이전에 측정된 값 초기화
    ave_power_A = 0;
    charge = 0;
    Firebase.setInt("current",sum_power_A); // 이전에 데이터베이스에 입력되었던 값 초기화
    Firebase.setInt("ave_current",ave_power_A); 
    Firebase.setInt("charge",charge);// 이전에 데이터베이스에 입력되었던 값 초기화
    charging();  // 전력측정 시작
  }
  delay(3000); // 재부팅 오류 체크

  
  if(!mfrc522.PICC_IsNewCardPresent()) { // 새카드 읽기 
    return;
  }

  if(!mfrc522.PICC_ReadCardSerial()) { // 카드정보 확인
    return;
  }

  Serial.print("UIP Tag : ");
  String content = "";
  byte letter;
  for(byte i  = 0; i< mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i], HEX); // 카드정보 출력
    content.concat(String(mfrc522.uid.uidByte[i], HEX)); // 카드정보 임시저장
  }
  Serial.println("");

  content.toUpperCase();

  if(n==1) { // 허가된 카드만 전력제어 가능
    // nfc카드 체크 예상 구간 못 찾을시 return
    for(n=1;n<10 ; n++) {
      numberString = String(n);
      addaddresssum = address_nfc + numberString;
      addaddresssum = addaddresssum + nfcaddress;
      Serial.println(addaddresssum);
      Serial.println(Firebase.getString(addaddresssum));
      
      if (content == Firebase.getString(addaddresssum)) {
        addaddresssum = address_nfc + numberString;
        addaddresssum = addaddresssum + uidaddress;
        getuid = Firebase.getString(addaddresssum);
        addaddresssum = address_users + getuid;
        addaddresssum = addaddresssum + numberaddress;
        Serial.print("차량번호: ");
        Serial.println(Firebase.getString(addaddresssum));
        addaddresssum = address_users + getuid;
        addaddresssum = addaddresssum + pointaddress;
        n=1;
        break;
      }
      if(Firebase.getString(addaddresssum) == 0) {
        n=1;
        con = 0; 
        Serial.println("no nfc card");
        return;
      }
    }
  }
    if(con == 1) {
      Serial.println("Mode : ON");
      digitalWrite(la, HIGH);
      sum_power_A = 0; // 이전에 측정된 값 초기화
      ave_power_A = 0;
      charge = 0;
      Firebase.setInt("current",sum_power_A);
      Firebase.setInt("ave_current",ave_power_A);
      Firebase.setInt("charge",charge);// 이전에 데이터베이스에 입력되었던 값 초기화
      Firebase.setInt("ready",1);
      fromnfc = 1;
      charging();  // 전력측정 시작
      delay(1000);
      con = 0;
      Firebase.setInt("ready",0);
    }
  


  if (Firebase.failed()) {// Check for errors  파이어베이스 연동 안될 시 출력

    Serial.print("setting /number failed:");

    Serial.println(Firebase.error());

    return;

  }
  
  delay(1000);

  
}


void charging() { // 전력측정 및 DB입력 모듈
  while(1){
  raw_data_A = (double)analogRead(A0); // 아날로그값 읽기
  voltage_A = raw_data_A/1023*5;  // 전압값 계산
  current_A = (voltage_A - 2.6) / 0.525; // 전류값 계산 실수형
  if (current_A < 0)
    current_A = 0;
  power_A = (int)(220*current_A);  // 220V 환산 전류값 정수형

  Serial.println(raw_data_A);
  Serial.println(voltage_A);
  Serial.println(current_A);
  Serial.println(power_A);
  Serial.println("---------------------");
  
  sum_power_A = sum_power_A + power_A;
  delay(1000);
  
  if(power_A > 20) {
   min_time = min_time + 1;
   ave_power_A = sum_power_A / min_time;
   Serial.println(ave_power_A);
  }
  
  if ((min_time % 3) == 0) {
    charge = charge + (((double)ave_power_A / 1000) * 500);
    Serial.println(charge);
    Firebase.setInt("charge",charge); // 전류값 DB입력
    Firebase.setInt("current",sum_power_A); // 전류값 DB입력
    Firebase.setInt("ave_current",ave_power_A);
    Serial.println(Firebase.getInt(addaddresssum));
    if(fromnfc == 1) {
    getpoint = Firebase.getInt(addaddresssum);
      if (getpoint < charge) {
          getout = 1;
          fromnfc = 0;
          return;
      }
    getpoint = getpoint - charge;
    Firebase.setInt(addaddresssum,getpoint);
    }
    Serial.println("send");
  }

  if (getout == 1) {
    digitalWrite(la, LOW);
    Firebase.setInt("ready",0);
    con = 0;
    min_time = 1;
    getout = 0;
    return;  
  }
  
  if (mfrc522.PICC_IsNewCardPresent()){  // 카드 다시읽힐시 전원차단
    // 전에 읽은 정보를 가지고 포인트 차감
    digitalWrite(la, LOW);
    Firebase.setInt("ready",0);
    con = 0;
    min_time = 1;
    fromnfc = 0;
    return;
  }
  if (Firebase.getInt("ready")==0) {
    digitalWrite(la, LOW);
    min_time = 1;
    return;
  }
 }
}
