// C++ code
//
int Minuten = 0;
int Stunden = 0;
int Sekunden = 0;

int pin0 = 16;
int pin1 = 5;
int pin2 = 4;
int pin3 = 0;
int pin4 = 2;
int pin5 = 14;
int pin6 = 12;
int pin7 = 13;
int pin8 = 15;
int pin9 = 3;
int pin10 = 1;

int counter;

void setup()
{
  pinMode(pin0, OUTPUT);
  pinMode(pin1, OUTPUT);
  pinMode(pin2, OUTPUT);
  pinMode(pin3, OUTPUT);
  pinMode(pin4, OUTPUT);
  pinMode(pin5, OUTPUT);
  pinMode(pin6, OUTPUT);
  pinMode(pin7, OUTPUT);
  pinMode(pin8, OUTPUT);
  pinMode(pin9, OUTPUT);
  pinMode(pin10, OUTPUT);

  Minuten = 0;
  Stunden = 0;
  digitalWrite(pin0, LOW);
  digitalWrite(pin1, LOW);
  digitalWrite(pin2, LOW);
  digitalWrite(pin3, LOW);
  digitalWrite(pin4, LOW);
  digitalWrite(pin5, LOW);
  digitalWrite(pin6, LOW);
  digitalWrite(pin7, LOW);
  digitalWrite(pin8, LOW);
  digitalWrite(pin9, LOW);
  digitalWrite(pin10, LOW);

  for (counter = 0; counter < 500; ++counter) {
    digitalWrite(pin0, HIGH);
    digitalWrite(pin1, HIGH);
    digitalWrite(pin2, HIGH);
    delay(5); // Wait for 5 millisecond(s)
    digitalWrite(pin0, LOW);
    digitalWrite(pin1, LOW);
    digitalWrite(pin2, LOW);
    digitalWrite(pin3, HIGH);
    digitalWrite(pin4, HIGH);
    digitalWrite(pin5, HIGH);
    delay(5); // Wait for 5 millisecond(s)
    digitalWrite(pin3, LOW);
    digitalWrite(pin4, LOW);
    digitalWrite(pin5, LOW);
    digitalWrite(pin6, HIGH);
    digitalWrite(pin7, HIGH);
    digitalWrite(pin8, HIGH);
    delay(5); // Wait for 5 millisecond(s)
    digitalWrite(pin6, LOW);
    digitalWrite(pin7, LOW);
    digitalWrite(pin8, LOW);
    digitalWrite(pin9, HIGH);
    digitalWrite(pin10, HIGH);
    delay(5); // Wait for 5 millisecond(s)
    digitalWrite(pin9, LOW);
    digitalWrite(pin10, LOW);
  }
}

void loop()
{
  if (Sekunden >= 24 * (60 * 60)) {
    Sekunden = 0;
  }
  Minuten = ((Sekunden % 3600) / 60);
  Stunden = (Sekunden / 3600);

  for (int i=0; i<50; i++) {
    if (Minuten % 2 >= 1) {
      digitalWrite(pin0, HIGH);
    } else {
      digitalWrite(pin0, LOW);
    }
    if (Minuten % 4 >= 2) {
      digitalWrite(pin1, HIGH);
    } else {
      digitalWrite(pin1, LOW);
    }
    if (Minuten % 8 >= 4) {
      digitalWrite(pin2, HIGH);
    } else {
      digitalWrite(pin2, LOW);
    }
    delay(5); // Wait for 5 millisecond(s)
    digitalWrite(pin0, LOW);
    digitalWrite(pin1, LOW);
    digitalWrite(pin2, LOW);
    if (Minuten % 16 >= 8) {
      digitalWrite(pin3, HIGH);
    } else {
      digitalWrite(pin3, LOW);
    }
    if (Minuten % 32 >= 16) {
      digitalWrite(pin4, HIGH);
    } else {
      digitalWrite(pin4, LOW);
    }
    if (Minuten % 64 >= 32) {
      digitalWrite(pin5, HIGH);
    } else {
      digitalWrite(pin5, LOW);
    }
    delay(5); // Wait for 5 millisecond(s)
    digitalWrite(pin3, LOW);
    digitalWrite(pin4, LOW);
    digitalWrite(pin5, LOW);
    if (Stunden % 2 >= 1) {
      digitalWrite(pin6, HIGH);
    } else {
      digitalWrite(pin6, LOW);
    }
    if (Stunden % 4 >= 2) {
      digitalWrite(pin7, HIGH);
    } else {
      digitalWrite(pin7, LOW);
    }
    if (Stunden % 8 >= 4) {
      digitalWrite(pin8, HIGH);
    } else {
      digitalWrite(pin8, LOW);
    }
    delay(5); // Wait for 5 millisecond(s)
    digitalWrite(pin6, LOW);
    digitalWrite(pin7, LOW);
    digitalWrite(pin8, LOW);
    if (Stunden % 16 >= 8) {
      digitalWrite(pin9, HIGH);
    } else {
      digitalWrite(pin9, LOW);
    }
    if (Stunden % 32 >= 16) {
      digitalWrite(pin10, HIGH);
    } else {
      digitalWrite(pin10, LOW);
    }
    delay(5); // Wait for 5 millisecond(s)
    digitalWrite(pin9, LOW);
    digitalWrite(pin10, LOW);
  }
  Sekunden += 60;
}