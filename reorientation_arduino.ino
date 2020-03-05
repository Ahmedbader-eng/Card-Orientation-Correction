#include <Servo.h>

// Pins selection
int IR_sensorPin = 2;     // IR sensor Pin which will be used to detect when a card is out from the input stack
int Servo_flipPin = 3;      // servo motor pin to flip cards
int Servo_stopperPin = 4;   // servo motor pin to stop cards for the camera to capture images
int soleniod_R_vcc = 5;   // Right solnoide pin to rotate the card
int soleniod_R_gnd = 6;   // Right solnoide pin to rotate the card
int soleniod_L_vcc = 7;   // Left solnoide pin to rotate the card
int soleniod_L_gnd = 8;   // Left solnoide pin to rotate the card
int StackMotor_gnd = 9;   // Stack motor pin to feed the system with cards
int StackMotor_vcc = 10;   // Stack motor pin to feed the system with cards
int BeltMotor_vcc = 11;    // Belt motor pin to move the card in the system
int BeltMotor_gnd = 12;   // Belt motor pin to move the card in the system
int LED = 13; // RED LED to indicate if there are errors

// error tracking varibles
int error_counter = 0; // track errors If the camera sends error 10 times or more then there is a problem or the input stack is empty
                       // in this case the machine will stop all actuators untill the problem is fixed by an authroised person
int error_status = 0;

char visualStudio_message = '0'; // varible to read messages from the COM port. The messages are sent from Visiual Studio 
char start_machine = 'x';  


// Servos angle and speed setup
int Servo_flip_angle = 90;
int Servo_stopper_angle = 90;

int Servo_flip_sweep_time = 25; // lower values is higher speed
int Servo_stopper_sweep_time = 25; // lower values is higher speed

// a varible to be used if there are errors which will allow the machine to let the card go when the operator fix the issue if exist
bool fixing = false;

Servo Servo_flip;  // create servo object to control a servo
Servo Servo_stopper;  // create servo object to control a servo

int pos = 0;    // variable to store the servo position

void setup() {
  // Pins mode setup
  pinMode(IR_sensorPin, INPUT);
  pinMode(soleniod_R_vcc, OUTPUT);
  pinMode(soleniod_R_gnd, OUTPUT);
  pinMode(soleniod_L_vcc, OUTPUT);
  pinMode(soleniod_L_gnd, OUTPUT);
  pinMode(StackMotor_vcc, OUTPUT);
  pinMode(StackMotor_gnd, OUTPUT);
  pinMode(BeltMotor_vcc, OUTPUT);
  pinMode(BeltMotor_gnd, OUTPUT);
  pinMode(LED, OUTPUT);

  // Set COM port baude rate to 9600
  Serial.begin(9600);

//  Wait until the openCV program starts running
  while (start_machine != 'S'){
    if(Serial.available()){
      start_machine = Serial.read();
    }
  }
  start_machine = 'x';

  Servo_flip.attach(Servo_flipPin);
  if (Servo_flip.attached()) {
    Servo_flip.detach();
  }

  Servo_stopper.attach(Servo_stopperPin);
  if (Servo_stopper.attached()) {
    Servo_stopper.detach();
  }

}



void loop() {
//--------------------RISE THE STOPPER SERVO ARM UP---------------------//
// The stopper servo arm must be up after each card pass to ensure no card is skipped from the camera 
  if (error_status == 0) {
  
    digitalWrite(LED, LOW);
    
    Servo_stopper.attach(Servo_stopperPin);
    for (pos = 0; pos <= Servo_stopper_angle; pos += 1) { // goes from 0 to the specified Servo_stopper_angle
      // in steps of 1 degree
      Servo_stopper.write(pos);                           // tell servo to go to position in variable 'pos'
      delay(Servo_stopper_sweep_time);                    // waits for the servo to reach the position
    }
    Servo_stopper.detach();
    
    //--------------------RUN THE STACK MOTOR & BELT MOTOR---------------------//

      
    digitalWrite(BeltMotor_vcc, HIGH);
    digitalWrite(BeltMotor_gnd, LOW);
       
    if (!fixing){
      digitalWrite(StackMotor_vcc, HIGH);
      digitalWrite(StackMotor_gnd, LOW);

      while(digitalRead(IR_sensorPin));
      delay(2000);

      digitalWrite(StackMotor_vcc, LOW);
      digitalWrite(StackMotor_gnd, LOW);
      
    } else {
      fixing = false;
    }
  
  } else {
  // if there are keep the input stack motor off to prevent cards from entering the system
  digitalWrite(StackMotor_vcc, LOW);
  digitalWrite(StackMotor_gnd, LOW);
  
  Servo_stopper.attach(Servo_stopperPin);
  for (pos = 0; pos <= Servo_stopper_angle; pos += 1) { // goes from 0 to the specified Servo_stopper_angle
    // in steps of 1 degree
    Servo_stopper.write(pos);                           // tell servo to go to position in variable 'pos'
    delay(5);                    // waits for the servo to reach the position
  }
  Servo_stopper.detach();
  
  }


  Serial.write('C');

//--------------------WAIT FOR VISUALSTUDIO RESPONSE---------------------//
// VisualStudio responses are:
// '1' this is case one NO flip - rotation direction (RIGHT) (Correct position) ---> Right solenoide UP, Left solenoide DOWN, Servos arms DOWN  
// '2' this is case two No flip - rotation direction (LEFT) ---> Right solenoide DOWN, Left solenoide UP, Servos arms DOWN 
// '3' this is case three DO flip - rotation direction (RIGHT) ---> Right solenoide UP, Left solenoide DOWN, Servos arms UP to flip
// '4' this is case four DO flip - rotation direction (LEFT) ---> Right solenoide DOWN, Left solenoide UP, Servos arms UP to flip
// 'E' camera detection error, in this case a red LED turns on and the system waits for the operator to solve the problem  
// 'e' camera access error, in this case a red LED flashes and the system waits for the operator to solve the problem  
  while(!Serial.available());
  visualStudio_message = Serial.read();

  // re-orient the card depending on visualStudio_message:
  switch (visualStudio_message) {
    case '1':    
      caseOne();
      error_status = 0;
      break;
    case '2':    
      caseTwo();
      error_status = 0;
      break;
    case '3':    
      caseThree();
      error_status = 0;
      break;
    case '4':    
      caseFour();
      error_status = 0;
      break;
    case 'E':
      error_counter++;
      error_status = 1;
      while (error_counter >= 10){
        error_handling('E');
      } 
      break;
    case 'e':
    // in this case the openCV program is not able to access the camera
      error_status = 1;
      error_handling('e');
      break;
  }
}


// error handling turn red LED on or Flash red LED
// the program will wait for user (operator) command ('S') to first let the card with error reading go and 
// a second ('S') command in the next loop to run the system again 
void error_handling(char error){
  if (error == 'e'){
    digitalWrite(StackMotor_vcc, LOW);
    digitalWrite(StackMotor_gnd, LOW);
    
    while (true){
      digitalWrite(LED, HIGH);
      delay(200);
      digitalWrite(LED, LOW);
      delay(200);
      
      if(Serial.available()){
        visualStudio_message = Serial.read();
        if (visualStudio_message == 'S'){
          error_status = 0;
          error_counter = 0;
          visualStudio_message = 'x';
  
          Serial.write('C');
          fixing = true;
          break;  
        }
      }
    }
    
  }else{
    digitalWrite(LED, HIGH);
    // To run the system again the Serial port must recieve the charachter 'S'
    if(Serial.available()){
      visualStudio_message = Serial.read();
      if (visualStudio_message == 'S'){
        error_status = 0;
        error_counter = 0;
        visualStudio_message = 'x';

        Serial.write('C');
        fixing = true; 
      }
    }    
  }
}


// Right solenoide UP, Left solenoide DOWN, Stopper Servo let go
void caseOne(){
  digitalWrite(soleniod_R_vcc, LOW);
  digitalWrite(soleniod_R_gnd, HIGH);

  digitalWrite(soleniod_L_vcc, HIGH);
  digitalWrite(soleniod_L_gnd, LOW);

  Servo_stopper.attach(Servo_stopperPin);
  for (pos = Servo_stopper_angle; pos >= 0; pos -= 1) { // goes from the specified Servo_flip_angle to 0 degree
    // in steps of 1 degree
    Servo_stopper.write(pos);                           // tell servo to go to position in variable 'pos'
    delay(Servo_stopper_sweep_time);                    // waits for the servo to reach the position
  }
  Servo_stopper.detach();

  digitalWrite(soleniod_R_vcc, LOW);
  digitalWrite(soleniod_R_gnd, LOW);

  digitalWrite(soleniod_L_vcc, LOW);
  digitalWrite(soleniod_L_gnd, LOW);
  
}

// Right solenoide DOWN, Left solenoide UP, Stopper Servo let go
void caseTwo(){
  digitalWrite(soleniod_R_vcc, HIGH);
  digitalWrite(soleniod_R_gnd, LOW);
  
  digitalWrite(soleniod_L_vcc, LOW);
  digitalWrite(soleniod_L_gnd, HIGH);

  Servo_stopper.attach(Servo_stopperPin);
  for (pos = Servo_stopper_angle; pos >= 0; pos -= 1) { // goes from the specified Servo_flip_angle to 0 degree
    // in steps of 1 degree
    Servo_stopper.write(pos);                           // tell servo to go to position in variable 'pos'
    delay(Servo_stopper_sweep_time);                    // waits for the servo to reach the position
  }
  Servo_stopper.detach();

  digitalWrite(soleniod_R_vcc, LOW);
  digitalWrite(soleniod_R_gnd, LOW);

  digitalWrite(soleniod_L_vcc, LOW);
  digitalWrite(soleniod_L_gnd, LOW);
}

// Right solenoide UP, Left solenoide DOWN, Flip the card
void caseThree(){
  digitalWrite(soleniod_R_vcc, LOW);
  digitalWrite(soleniod_R_gnd, HIGH);

  digitalWrite(soleniod_L_vcc, HIGH);
  digitalWrite(soleniod_L_gnd, LOW);

  flip();
}

// Right solenoide DOWN, Left solenoide UP, Flip the card
void caseFour(){
  digitalWrite(soleniod_R_vcc, HIGH);
  digitalWrite(soleniod_R_gnd, LOW);
  
  digitalWrite(soleniod_L_vcc, LOW);
  digitalWrite(soleniod_L_gnd, HIGH);

  flip();
}

// Flipping servo arm goes up, then both the flipping servo arm and the stopper servo arm go down
void flip() {
  Servo_flip.attach(Servo_flipPin);
  for (pos = 0; pos <= Servo_flip_angle; pos += 1) { // goes from 0 degrees to the specified Servo_flip_angle
    // in steps of 1 degree
    Servo_flip.write(pos);                          // tell servo to go to position in variable 'pos'
    delay(Servo_flip_sweep_time);                   // waits for the servo to reach the position
  }
  Servo_flip.detach();

  
  Servo_stopper.attach(Servo_stopperPin);
  for (pos = Servo_stopper_angle; pos >= 0; pos -= 1) { // goes from the specified Servo_stopper_angle degrees to 0 degree
    // in steps of 1 degree
    Servo_stopper.write(pos);                           // tell servo to go to position in variable 'pos'
    delay(Servo_stopper_sweep_time);                    // waits for the servo to reach the position
  }
  Servo_stopper.detach();

  digitalWrite(soleniod_R_vcc, LOW);
  digitalWrite(soleniod_R_gnd, LOW);

  digitalWrite(soleniod_L_vcc, LOW);
  digitalWrite(soleniod_L_gnd, LOW);

  Servo_flip.attach(Servo_flipPin);
  for (pos = Servo_flip_angle; pos >= 0; pos -= 1) { // goes from the specified Servo_flip_angle to 0 degree
    // in steps of 1 degree
    Servo_flip.write(pos);                          // tell servo to go to position in variable 'pos'
    delay(Servo_flip_sweep_time);                   // waits for the servo to reach the position
  }
  Servo_flip.detach();
}

