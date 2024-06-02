void loop() {
  float temp = readBME280();
  Serial.println(temp);
  delay(60000);
}