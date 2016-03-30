void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(A0,INPUT);
  pinMode(5,OUTPUT);
  pinMode(6,OUTPUT);
  pinMode(7,OUTPUT);
  pinMode(8,OUTPUT);
  pinMode(9,OUTPUT);
  pinMode(10,OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  int analogValue = analogRead(A0);
  Serial.println(analogValue);
  if(analogValue > 200) {
    digitalWrite(6,HIGH);
    delay(200);
  }
  else if(analogValue > 1023){
    digitalWrite(6,HIGH);
    delay(500);
  }
  else if(analogValue > 1023){
    digitalWrite(7,HIGH);
    delay(500);
  }
  else if(analogValue > 1023){
    digitalWrite(8,HIGH);
    delay(500);
  }
  else if(analogValue > 1023){
    digitalWrite(9,HIGH);
    delay(500);
  }
  else if(analogValue > 1023){
    digitalWrite(10,HIGH);
    delay(500);
  }  
  else{
    digitalWrite(5,LOW);
    digitalWrite(6,LOW);
    digitalWrite(7,LOW);
    digitalWrite(8,LOW);
    digitalWrite(9,LOW);
    digitalWrite(10,LOW);
  }
  delay(10);
}



