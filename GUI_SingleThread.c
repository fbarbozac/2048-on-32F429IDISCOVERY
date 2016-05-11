#include <stdio.h>

#include "cmsis_os.h"                               // CMSIS RTOS header file
#include "GUI.h"
#include "DIALOG.h"

#include "stm32f429i_discovery_gyroscope.h"

#define	LCD_UPDATE_INTERVAL	(500)						// ms
#define	TILT_DETEC_INTERVAL	(200)						// ms
#define	GYRO_READ_INTERVAL	(10)						// ms
#define	GYRO_FIFO_SIZE		(TILT_DETEC_INTERVAL / GYRO_READ_INTERVAL)
#define	GYRO_MDPS			(1000)										// mdps
#define	GYRO_DIVIDER		(GYRO_MDPS * (1000 / GYRO_READ_INTERVAL))	// mdps / read frequency

#define	TILT_ANGLE_MOV		(15)	// degree

typedef struct
{
	float x, y, z;
} t_st_axis;

typedef enum
{
	GYRO_X,
	GYRO_Y,
	GYRO_Z,
} t_en_AXIS;

typedef enum
{
	GYRO_TILT_NULL	=	(0<<0),
	
	GYRO_TILT_X_P	=	(1<<0),
	GYRO_TILT_X_M	=	(1<<1),
	
	GYRO_TILT_Y_P	=	(1<<2),
	GYRO_TILT_Y_M	=	(1<<3),
	
	GYRO_TILT_Z_P	=	(1<<4),
	GYRO_TILT_Z_M	=	(1<<5),
} t_en_Tilt;

static t_st_axis st_axis[GYRO_FIFO_SIZE];

/*----------------------------------------------------------------------------
 *      GUIThread: GUI Thread for Single-Task Execution Model
 *---------------------------------------------------------------------------*/
 

void GUIThread (void const *argument);              // thread function
osThreadId id_GUIThread;                           // thread id
osThreadDef (GUIThread, osPriorityIdle, 1, 4096);   // thread object

static void GyroTimer_Callback(void const *arg);           // prototype for timer callback function
static osTimerDef(GyroTimer, GyroTimer_Callback);                                      

static t_en_Tilt GyroGetTilt(void);
static int GyroGetAngle(t_en_AXIS en_AXIS);

// Periodic Timer
static void GyroTimer_Callback  (void const *arg)
{
	static uint16_t u16_index;
	t_st_axis *pst_axis = &st_axis[u16_index];
	
	BSP_GYRO_GetXYZ((float *) pst_axis);
	
	if (++u16_index >= GYRO_FIFO_SIZE)
	{
		static t_en_Tilt en_Tilt;

		u16_index = 0;
		
		switch (en_Tilt)
		{
			case GYRO_TILT_X_P:
				if (GyroGetAngle(GYRO_X) < -TILT_ANGLE_MOV)
				{
					osSignalSet(id_GUIThread, en_Tilt);
				}
				en_Tilt = GYRO_TILT_NULL;
				break;
			
			case GYRO_TILT_X_M:
				if (GyroGetAngle(GYRO_X) > TILT_ANGLE_MOV)
				{
					osSignalSet(id_GUIThread, en_Tilt);
				}
				en_Tilt = GYRO_TILT_NULL;
				break;
			
			case GYRO_TILT_Y_P:
				if (GyroGetAngle(GYRO_Y) < -TILT_ANGLE_MOV)
				{
					osSignalSet(id_GUIThread, en_Tilt);
				}
				en_Tilt = GYRO_TILT_NULL;
				break;
			
			case GYRO_TILT_Y_M:
				if (GyroGetAngle(GYRO_Y) > TILT_ANGLE_MOV)
				{
					osSignalSet(id_GUIThread, en_Tilt);
				}
				en_Tilt = GYRO_TILT_NULL;
				break;
			
			case GYRO_TILT_Z_P:
				if (GyroGetAngle(GYRO_Z) < -TILT_ANGLE_MOV)
				{
					osSignalSet(id_GUIThread, en_Tilt);
				}
				en_Tilt = GYRO_TILT_NULL;
				break;
			
			case GYRO_TILT_Z_M:
				if (GyroGetAngle(GYRO_Z) > TILT_ANGLE_MOV)
				{
					osSignalSet(id_GUIThread, en_Tilt);
				}
				en_Tilt = GYRO_TILT_NULL;
				break;
			
			case GYRO_TILT_NULL:
				en_Tilt = GyroGetTilt();
				break;
			
		}
	}
}

static t_en_Tilt GyroGetTilt(void)
{
	int AngleX = GyroGetAngle(GYRO_X);
	
	if (AngleX < -TILT_ANGLE_MOV)
	{
		return GYRO_TILT_X_M;
	}
	else if (AngleX > TILT_ANGLE_MOV)
	{
		return GYRO_TILT_X_P;
	}
	else
	{
		int AngleY = GyroGetAngle(GYRO_Y);

		if (AngleY < -TILT_ANGLE_MOV)
		{
			return GYRO_TILT_Y_M;
		}
		else if (AngleY > TILT_ANGLE_MOV)
		{
			return GYRO_TILT_Y_P;
		}
		else
		{
			int AngleZ = GyroGetAngle(GYRO_Z);
			
			if (AngleZ < -TILT_ANGLE_MOV)
			{
				return GYRO_TILT_Z_M;
			}
			else if (AngleZ > TILT_ANGLE_MOV)
			{
				return GYRO_TILT_Z_P;
			}
		}
	}

	return GYRO_TILT_NULL;
}

static int GyroGetAngle(t_en_AXIS en_AXIS)
{
	uint16_t u16_index;
	float Angle = 0;
	
	if (en_AXIS == GYRO_X)
	{
		for (u16_index = 0; u16_index < GYRO_FIFO_SIZE; u16_index++)
		{
			Angle += st_axis[u16_index].x;
		}
	}
	else if (en_AXIS == GYRO_Y)
	{
		for (u16_index = 0; u16_index < GYRO_FIFO_SIZE; u16_index++)
		{
			Angle += st_axis[u16_index].y;
		}
	}
	else
	{
		for (u16_index = 0; u16_index < GYRO_FIFO_SIZE; u16_index++)
		{
			Angle += st_axis[u16_index].z;
		}
	}

	// Keep our angle between 0-359 degrees
	return ((int) (Angle / GYRO_DIVIDER) % 360);
}

int Init_GUIThread (void)
{
	// Create periodic timer
	osTimerId idGyro = osTimerCreate(osTimer(GyroTimer), osTimerPeriodic, NULL);

	// Periodic timer created
	// Start timer with periodic 10ms interval
	if ((idGyro == NULL) || (osTimerStart(idGyro, GYRO_READ_INTERVAL) != osOK))
	{
		// Timer could not be started
		return (-1);
	}
	else if ((id_GUIThread = osThreadCreate(osThread(GUIThread), NULL)) == NULL)
	{
		return (-1);
	}
	else
	{
		BSP_GYRO_Init();
		BSP_GYRO_Reset();
	}

	return(0);
}

void GUIThread (void const *argument)
{
	#define	X0_STR			(10)

	#define	R_CIRCLES_XY	(20)
	#define	CIRCLE_DIST		(180)
	
	#define	X0_CIRCLE_UP	(120)
	#define	Y0_CIRCLE_UP	(100)
	
	#define	X0_CIRCLE_DOWN	(X0_CIRCLE_UP)
	#define	Y0_CIRCLE_DOWN	(Y0_CIRCLE_UP + CIRCLE_DIST)
	
	#define	X0_CIRCLE_LEFT	(X0_CIRCLE_UP - (CIRCLE_DIST / 2))
	#define	Y0_CIRCLE_LEFT	(Y0_CIRCLE_UP + (CIRCLE_DIST / 2))
	
	#define	X0_CIRCLE_RIGHT	(X0_CIRCLE_UP + (CIRCLE_DIST / 2))
	#define	Y0_CIRCLE_RIGHT	(Y0_CIRCLE_LEFT)

	#define	X0_CIRCLE_CENTER	(X0_CIRCLE_UP)
	#define	Y0_CIRCLE_CENTER	(Y0_CIRCLE_LEFT)
	
	GUI_Init(); /* Initialize the Graphics Component */
	
	while (1)
	{
		char Str[30];
		osEvent evt;
		
		GUI_SetColor(GUI_WHITE);
		sprintf(Str, "st_axis[0].x = %05.1f", st_axis[0].x);
		GUI_DispStringAt(Str, 0, 0);

		sprintf(Str, "st_axis[0].y = %05.1f", st_axis[0].y);
		GUI_DispNextLine();
		GUI_DispString(Str);

		sprintf(Str, "st_axis[0].z = %05.1f", st_axis[0].z);
		GUI_DispNextLine();
		GUI_DispString(Str);

		sprintf(Str, "st_angle.x = %03dº", GyroGetAngle(GYRO_X));
		GUI_DispNextLine();
		GUI_DispString(Str);
		
		sprintf(Str, "st_angle.y = %03dº", GyroGetAngle(GYRO_Y));
		GUI_DispNextLine();
		GUI_DispString(Str);
		
		sprintf(Str, "st_angle.z = %03dº", GyroGetAngle(GYRO_Z));
		GUI_DispNextLine();
		GUI_DispString(Str);
		
		evt = osSignalWait (0, LCD_UPDATE_INTERVAL);
	
		GUI_SetColor(GUI_GREEN);
		GUI_FillCircle(X0_CIRCLE_UP, Y0_CIRCLE_UP, R_CIRCLES_XY);
		GUI_FillCircle(X0_CIRCLE_DOWN, Y0_CIRCLE_DOWN, R_CIRCLES_XY);
		GUI_FillCircle(X0_CIRCLE_LEFT, Y0_CIRCLE_LEFT, R_CIRCLES_XY);
		GUI_FillCircle(X0_CIRCLE_RIGHT, Y0_CIRCLE_RIGHT, R_CIRCLES_XY);

		if (evt.status == osEventSignal)
		{
			GUI_SetColor(GUI_RED);
			
			// handle event status
			switch (evt.value.signals)
			{
				case GYRO_TILT_X_P:
					GUI_FillCircle(X0_CIRCLE_DOWN, Y0_CIRCLE_DOWN, R_CIRCLES_XY);
					break;
				
				case GYRO_TILT_X_M:
					GUI_FillCircle(X0_CIRCLE_UP, Y0_CIRCLE_UP, R_CIRCLES_XY);
					break;
				
				case GYRO_TILT_Y_P:
					GUI_FillCircle(X0_CIRCLE_RIGHT, Y0_CIRCLE_RIGHT, R_CIRCLES_XY);
					break;
				
				case GYRO_TILT_Y_M:
					GUI_FillCircle(X0_CIRCLE_LEFT, Y0_CIRCLE_LEFT, R_CIRCLES_XY);
					break;
				
				case GYRO_TILT_Z_P:
					break;
				
				case GYRO_TILT_Z_M:
					break;
				
				case GYRO_TILT_NULL:
					break;
				
			}
		}
	
//		if (GyroGetAngle(GYRO_X) < -TILT_ANGLE_MOV)
//		{
//			GUI_SetColor(GUI_RED);
//			GUI_FillCircle(X0_CIRCLE_UP, Y0_CIRCLE_UP, R_CIRCLES_XY);
//		}
//		else
//		{
//			GUI_SetColor(GUI_GREEN);
//			GUI_FillCircle(X0_CIRCLE_UP, Y0_CIRCLE_UP, R_CIRCLES_XY);
//		}
//		
//		if (GyroGetAngle(GYRO_X) > TILT_ANGLE_MOV)
//		{
//			GUI_SetColor(GUI_RED);
//			GUI_FillCircle(X0_CIRCLE_DOWN, Y0_CIRCLE_DOWN, R_CIRCLES_XY);
//		}
//		else
//		{
//			GUI_SetColor(GUI_GREEN);
//			GUI_FillCircle(X0_CIRCLE_DOWN, Y0_CIRCLE_DOWN, R_CIRCLES_XY);
//		}
//		
//		if (GyroGetAngle(GYRO_Y) < -TILT_ANGLE_MOV)
//		{
//			GUI_SetColor(GUI_RED);
//			GUI_FillCircle(X0_CIRCLE_LEFT, Y0_CIRCLE_LEFT, R_CIRCLES_XY);
//		}
//		else
//		{
//			GUI_SetColor(GUI_GREEN);
//			GUI_FillCircle(X0_CIRCLE_LEFT, Y0_CIRCLE_LEFT, R_CIRCLES_XY);
//		}
//		
//		if (GyroGetAngle(GYRO_Y) > TILT_ANGLE_MOV)
//		{
//			GUI_SetColor(GUI_RED);
//			GUI_FillCircle(X0_CIRCLE_RIGHT, Y0_CIRCLE_RIGHT, R_CIRCLES_XY);
//		}
//		else
//		{
//			GUI_SetColor(GUI_GREEN);
//			GUI_FillCircle(X0_CIRCLE_RIGHT, Y0_CIRCLE_RIGHT, R_CIRCLES_XY);
//		}

		/* All GUI related activities might only be called from here */
		GUI_TOUCH_Exec();     /* Optional Touch support, uncomment if required */
		GUI_Exec();             /* Execute all GUI jobs ... Return 0 if nothing was done. */
		GUI_X_ExecIdle();       /* Nothing left to do for the moment ... Idle processing */
	}
}

