//---!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!IMPORTANT NOTES!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!---//
/*
-    	Consider only the biggest for get_road_pos -Vikram 
-
-			Moved some declarations into main.h so that order of declaration of functions and variables
-			will not matter
-
-			Changed speed to be dependent on a ratio	
-
-			motor_control PWMx: 1 for PB15, 2 for PB14
-
-			Shuttlecock release --> wait 220ms ---> use the racket (if pneumatic can be fixed)
-
-				How to break out of while loop when: (A: Auto, M: Manual)
-					(X) --> Reached holder zone (A to M)
-					(X) -->	Start line tracing again	(M to A)
-					(X) --> Reached serving zone	(A to M)
-
-				If time allows (but still really important):
-					(X) --> Find a way to reset the whole int main(void){...}
-					(X) --> Still need to know how to actually detect which holder tube is correct
-							(X) SUGGESTED BY HARDWARE:
-								(X) --> Either (1) pass current through gripper and automatically grip if detected current
-								(X) --> Or (2) light up LED if current is detected
-
-				TODO (before Saturday):
-					--> Find the correct magnitude of motor_control() for wheels
-					--> How to use get_angle() to turn 
-					--> When to call wheel_speed_on_arc()
-					(X) --> What to do on a 90deg turn
-					--> What to do on a 135deg turn
-					IF POSSIBLE:	
-						--> Find correct timing for hitting badminton
-						(X) --> Set up basic bluetooth functions
-						--> Setting up PS3 driver to use PS3 controller to emulate keyboard (using Scarlet Crush)
-						--> Try to set up second version of code
-
-				(X) IMPORTANT POINT TO SHARE WITH HARDWARE SECTOR AS WELL:
-					(X) --> When testing the motor driver, motor_control(0, 0, 100); was used. However, for the first
-							(X) argument, only 1 or 2 can be input. If 0 is input, it does not really do anything.
-							--> GPIO pin starts at high voltage (because in GPIO_switch.c, GPIO mode is IPU
*/

#include "main.h"

int main(void){
	all_init();
	
	while(1){		
		if(autoormanual == 0){
			if(get_ms_ticks()%5 == 0){
				tft_prints(0,0,"                     ");
				tft_prints(0,0,"%d",get_road_pos()); 
				tft_update();
				primitive_stabiliser();
			}
		}

		else{
			uart_interrupt(COM3);
			
			if(read_GPIO_switch(GPIOA, GPIO_Pin_9)){ //left gripper
				tft_prints(0,3,"                 ");
				tft_prints(0,3,"LEFT CURRENT YES");
				tft_update();
				LED_ON(GPIOA, GPIO_Pin_15);
				uart_tx(COM3, "%s", "LC0");
			}
			else{
				tft_prints(0,3,"                ");
				tft_prints(0,3,"LEFT CURRENT NO");
				tft_update();
				LED_ON(GPIOA, GPIO_Pin_15);
				uart_tx(COM3, "%s", "LC1");
			}

			if(!read_GPIO_switch(GPIOA, GPIO_Pin_10)){ //right gripper
				tft_prints(0,4,"                  ");
				tft_prints(0,4,"RIGHT CURRENT YES");
				tft_update();
				LED_ON(GPIOA, GPIO_Pin_15);
				uart_tx(COM3, "%s", "RC0");
			}
			else{
				tft_prints(0,4,"                ");
				tft_prints(0,4,"RIGHT CURRENT NO");
				tft_update();
				LED_ON(GPIOB, GPIO_Pin_3);
				uart_tx(COM3, "%s", "RC1");
			}
		}
	}	
	return 0;
}

void all_init(void){
	motor_init();
	pneumatic_init();
	LED_INIT();
	linear_ccd_init();
	tft_init(0, BLACK, WHITE, WHITE);
	ticks_init();
	adc_init();
	button_init();
	GPIO_switch_init();
	buzzer_init();
	uart_init(COM3, 115200);
	uart_interrupt_init(COM3, &bluetooth_listener);
	for(int i = 0; i < 10; i++){
		mean_array[i] = 64;
	}
}

int get_road_pos(void){		
	int sum = 0;
	int count = 0;
	
	//---Clearing TFT---//
	for(int i = 0; i < 128; i++){
	tft_put_pixel(i, linear_ccd_buffer1[i], BLACK);
	}
	
	//---Updating CCD and getting road_pos---//
	linear_ccd_read();
		
	//---Updating TFT and calculating road_pos---//
	for(int i = 0; i < 128; i++){
		if(linear_ccd_buffer1[i] >= 150){
			tft_put_pixel(i, linear_ccd_buffer1[i], WHITE);
			sum = sum + i;
			count++;
		}
		else{
			tft_put_pixel(i, linear_ccd_buffer1[i], BLUE);
		}
	}
			
	if(count < 5){
		road_pos = -1;
	}
	else{
		road_pos = sum/count;		
	}
	
	return road_pos;
}

void primitive_stabiliser(void){
	//turning left (need to fix: not turning left enough)
	if(road_pos != -1 && road_pos < 58){
			rightspeedratio = 0.8;
			leftspeedratio = 0.4;
			rightdirection = 1;
			leftdirection = 0;		
	}
	
	//turning right (can turn smoothly)
	else if(road_pos > 80){ 
			rightspeedratio = 0.4;
			leftspeedratio = 0.8;
			rightdirection = 0;
			leftdirection = 1;
	}
	
	//straight (need to fix: left wheel too fast)
	else if(road_pos >= 58 && road_pos <= 80){
			rightspeedratio = 0.6;
			leftspeedratio = 0.8;
			rightdirection = 1;
			leftdirection = 1;
	}
	
	//90deg turn
	
	//135deg turn
	
	//Check if at holder zone/starting zone
	else{
		int check = 0;
		for(int i = 0; i < 128; i++){
			if(linear_ccd_buffer1[i] >= 150){
				check++;
			}
		}
		
		if(check > 125){
		rightspeedratio = 0;
		leftspeedratio = 0;
		autoormanual = 1;
		}
	}
	
			motor_control(1,rightdirection,rightspeedratio*motormag);
			motor_control(2,leftdirection,leftspeedratio*motormag);
}

//---BLUETOOTH---//
void bluetooth_listener(const uint8_t byte){	
	leftspeedratio = 0.8;
	rightspeedratio = 0.4;
	
	switch(byte){
		//case for left gripper
		case lgrip:
			if(uselgrip%2 == 0){
				pneumatic_control(GPIOB, GPIO_Pin_5, 1);
				tft_prints(0,2,"                     ");
				tft_prints(0,2,"lgrip open");
				tft_update();
				uselgrip++;
			}
			else{
				pneumatic_control(GPIOB, GPIO_Pin_5, 0);
				tft_prints(0,2,"                     ");
				tft_prints(0,2,"lgrip close");
				tft_update();
				uselgrip++;
			}
			
			uart_tx_byte(COM3, 'o');
			break;
		
		//case for right gripper
		case rgrip:
			if(usergrip%2 == 0){
				pneumatic_control(GPIOB, GPIO_Pin_6, 1);
				tft_prints(0,2,"                     ");
				tft_prints(0,2,"rgrip open");
				tft_update();
				usergrip++;
			}
			else{
				pneumatic_control(GPIOB, GPIO_Pin_6, 0);
				tft_prints(0,2,"                     ");
				tft_prints(0,2,"rgrip close");
				tft_update();
				usergrip++;
			}
		
			uart_tx_byte(COM3, 'p');
			break;
		
		//case for pivoting left
		case pivotleft:
			motor_control(1,0,motormag*rightspeedratio); //right wheel forward
			motor_control(2,1,motormag*leftspeedratio); //left wheel backward
			
			tft_prints(0,2,"                     ");
			tft_prints(0,2,"pivot left");
			tft_update();
			uart_tx_byte(COM3, 'a');
			break;
		
		//case for pivoting right
		case pivotright:
			motor_control(1,1,motormag*rightspeedratio); //right wheel backward
			motor_control(2,0,motormag*leftspeedratio); //left wheel forward
		
			tft_prints(0,2,"                     ");
			tft_prints(0,2,"pivot right");
			tft_update();
			uart_tx_byte(COM3, 'd');
			break;
		
		//case for moving forward
		case forward:
			motor_control(1,1,motormag*rightspeedratio);
			motor_control(2,1,motormag*leftspeedratio);
		
			tft_prints(0,2,"                     ");
			tft_prints(0,2,"forward");
			tft_update();
			uart_tx_byte(COM3, 'w');
			break;
		
		//case for moving backward
		case backward:
			motor_control(1,0,motormag*rightspeedratio);
			motor_control(2,0,motormag*leftspeedratio);
		
			tft_prints(0,2,"                     ");
			tft_prints(0,2,"backward");
			tft_update();
			uart_tx_byte(COM3, 's');
			break;
		
		//case for stopping
		case stop:
			motor_control(1,0,0);
			motor_control(2,0,0);
		
			tft_prints(0,2,"                     ");
			tft_prints(0,2,"stop");
			tft_update();
			uart_tx(COM3, "%s","Stop");
			break;
		
		//case for raising flag
		case raiseflag:
			if(flagraise%2 == 0){
				pneumatic_control(GPIOB, GPIO_Pin_6, 1);
				
				tft_prints(0,2,"                     ");
				tft_prints(0,2,"flag up");
				tft_update();
				flagraise++;
			}
			else{
				pneumatic_control(GPIOB, GPIO_Pin_6, 0);
				
				tft_prints(0,2,"                     ");
				tft_prints(0,2,"flag down");
				tft_update();
				flagraise++;
			}
			
			uart_tx_byte(COM3, 'f');
			break;
			
		
		//case for dropping shuttlecock and hitting with racket
		case hit:
			if(racketswing%2 == 0){
				pneumatic_control(GPIOB, GPIO_Pin_7, 1);
				uart_tx(COM3, "%s", "drop");
				tft_prints(0,2,"                     ");
				tft_prints(0,2,"drop");
				tft_update();
				
				_delay_ms(150);
				
				pneumatic_control(GPIOB, GPIO_Pin_5, 1);
				uart_tx(COM3, "%s", "hit");
				tft_prints(0,2,"                     ");
				tft_prints(0,2,"hit");
				tft_update();
				racketswing++;
			}
			else{
				pneumatic_control(GPIOB, GPIO_Pin_5, 0);
				pneumatic_control(GPIOB, GPIO_Pin_7, 0);
				
				
				tft_prints(0,2,"                     ");
				tft_prints(0,2,"close");
				tft_update();
				racketswing++;
			}
			
			uart_tx_byte(COM3, 'h');
			break;
			
		case beginautozone:
			autoormanual = 0;
		break;
	}
}


















//---VVVVVVV---JUNK FOR NOW---VVVVVVVVVVVVV---//
//---ANGLE---//
int get_moving_average(void){
	mai = maf;
	int sum = 0;
	
	for(int i = 0; i < 9; i++){
		mean_array[i] = mean_array[i+1];
	}
	mean_array[9] = get_road_pos();
	
	for(int j = 0; j < 10; j++){
		sum += mean_array[j];
	}
	
	maf = sum/10;
	
	return maf;
}

int get_didt(void){
	/*
	int i = 0;
	while(i<10){
		if(get_ms_ticks()%10 == 0){
			for(i = 0; i < 10; i++){
				if(i == 0){
					mai = get_moving_average();
				}
				else if(i == 9){
					didt = (get_moving_average() - mai)/0.01;
				}
				else{
					get_moving_average();
				}
			}
		}
	}
			*/
		
		didt = (get_moving_average() - mai)/0.01;
	
	return didt;
}

int get_angle(void){	
	double halfccdrange = 0.26; //need to find thru test (from 15cm high, can see 26 cm)
	double speed = 0.5; //need to find thru test
	
	angle = atan((-1*halfccdrange/(speed*64))*get_didt())*1000;

	return angle;
}

int wheel_speed_on_arc(void){
	double wheelbase = 0.4;
	
	if(angle > 0){
	rightspeedratio = leftspeedratio*(1-wheelbase)/(1+wheelbase);
	}
	
	else if(angle < 0){
	leftspeedratio = rightspeedratio*(1-wheelbase)/(1+wheelbase);
	}
}

int area(void){
	Larea = 0;
	Marea = 0;
	Rarea = 0;
	areaflag = 0;
	
	//Calculating left area
	for(int i = 0; i < 42; i++){
		Larea += ccd_refined[i];
	}
	
	//Calculating middle area
	for(int i = 42; i < 85; i++){
		Marea += ccd_refined[i];
	}
	
	//Calculating right area
	for(int i = 85; i < 128; i++){
		Rarea += ccd_refined[i];
	}
	
	//Too right, so turn left
	if(Rarea > Marea && Rarea > Larea){
		areaflag = 0;
	}
	
	//Stay middle
	else if(Marea > Rarea && Marea > Larea){
		areaflag = 1;
	}
	
	//Too left, turn right
	else if(Larea > Marea && Larea > Rarea){
		areaflag = 2;
	}
	
	//Stop when full white line
	else if(Larea >= 42*140 && Rarea >= 43*140  && Marea >= 42*140){
		areaflag = 3;
	}

	tft_prints(0,4,"               ");
	tft_prints(0,4,"%d",areaflag);
	tft_update();
	return areaflag;
}

int stabiliser(void){
	switch(area()){
		case 0:
			tft_prints(0,1,"                     ");
			tft_prints(0,1,"turn left");
			tft_update();
			break;
		
		case 1:
			tft_prints(0,1,"                     ");
			tft_prints(0,1,"go straight");
			tft_update();
			break;
		
		case 2:
			tft_prints(0,1,"                     ");
			tft_prints(0,1,"turn right");
			tft_update();
			break;		
		
		case 3:
			tft_prints(0,1,"                     ");
			tft_prints(0,1,"stop");
			tft_update();
			break;			
	}
}
//---DECASTELJAU ALGORITHM---//
u32 deCasteljau(u32 args[], double t){
	if(sizeof(args)/sizeof(args[0]) == 1){
		return args[0];
	}
	
	u32 newSize = sizeof(args)/sizeof(args[0]) - 1;
	u32 newArgs[newSize];
	
	for(int i = 0; i < sizeof(args)/sizeof(args[0]); i++){
		newArgs[i] = (1-t) * args[i] + t*args[i+1];
	}

	return deCasteljau(newArgs, t);
}

/*
//---RAMER-DOUGLAS-PEUCKER ALGORITHM---//
u32 DouglasPeucker(u32 args[], int startIndex, int lastIndex, double epsilon){
	double dmax = 0;
	int index = startIndex;
	int i = index + 1;
		
	for(i = index + 1; i < lastIndex; i++){
		double d;
		if(d > dmax){
			index = i;
			dmax = d;
		}
	}
	
	if(dmax > epsilon){
		double res1[200];
		double res2[200];
		
		int j = 0;
		while(res1[j] != '\0')
			{
				++j;
			} 
		res1[j] = DouglasPeucker(args, startIndex, index, epsilon);
		
		res2[j] = DouglasPeucker(args, index, lastIndex, epsilon);
		
		j = 0;
		while(res1[j] != '\0')
			{
				++j;
			} 
		double finalRes[1000];
		
		j = 0;
		
		for(int i = 0; i < sizeof(res1)/sizeof(res1[0]);i++){
			while(finalRes[j] != '\0')
			{
				++j;
			}
			finalRes[j] = res1[i]; 
		}
		
		j = 0;
		for(int i = 0; i < sizeof(res2)/sizeof(res2[0]);i++){
			while(finalRes[j] != '\0')
			{
				++j;
			}
			finalRes[j] = res2[i]; 
		}
		
		return finalRes[0];
	}
	else{
		double list[1000];
		list[0] = (args[startIndex], args[lastIndex]);
		return list[0];
	}
}

double PointLineDistance(double pointx, double pointy, double startx, double starty, double endx, double endy){
	if(startx == endx && starty == endy){
		
	}
}
*/
