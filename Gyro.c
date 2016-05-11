
#include "cmsis_os.h"
#include "stm32f429i_discovery_gyroscope.h"

#include "Gyro.h"

#define	TILT_DET_INTERVAL	(300)						// ms
#define	GYRO_READ_INTERVAL	(10)						// ms
#define	GYRO_FIFO_SIZE		(TILT_DET_INTERVAL / GYRO_READ_INTERVAL)
#define	GYRO_MDPS			(1000)										// mdps
#define	GYRO_DIVIDER		(GYRO_MDPS * (1000 / GYRO_READ_INTERVAL))	// mdps / read frequency

#define	TILT_ANGLE_MOV		(10)	// degree

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

static t_st_axis st_axis[GYRO_FIFO_SIZE];

// prototype for timer callback function
static void GyroTimer_Callback(void const *arg);
static osTimerDef(GyroTimer, GyroTimer_Callback);                                      

static t_en_TiltSignal GyroGetTilt(void);
static int GyroGetAngle(t_en_AXIS en_AXIS);


extern osThreadId GetGUIThreadId(void);
	
// Periodic Timer
static void GyroTimer_Callback  (void const *arg)
{
	static uint16_t u16_index;
	t_st_axis *pst_axis = &st_axis[u16_index];
	
	BSP_GYRO_GetXYZ((float *) pst_axis);
	
	if (++u16_index >= GYRO_FIFO_SIZE)
	{
		static t_en_TiltSignal en_TiltSignal;

		u16_index = 0;
		
		switch (en_TiltSignal)
		{
			case GYRO_TILT_X_P:
				if (GyroGetTilt() == GYRO_TILT_X_M)
				{
					osSignalSet(GetGUIThreadId(), en_TiltSignal);
				}
				en_TiltSignal = GYRO_TILT_NULL;
				break;
			
			case GYRO_TILT_X_M:
				if (GyroGetTilt() == GYRO_TILT_X_P)
				{
					osSignalSet(GetGUIThreadId(), en_TiltSignal);
				}
				en_TiltSignal = GYRO_TILT_NULL;
				break;
			
			case GYRO_TILT_Y_P:
				if (GyroGetTilt() == GYRO_TILT_Y_M)
				{
					osSignalSet(GetGUIThreadId(), en_TiltSignal);
				}
				en_TiltSignal = GYRO_TILT_NULL;
				break;
			
			case GYRO_TILT_Y_M:
				if (GyroGetTilt() == GYRO_TILT_Y_P)
				{
					osSignalSet(GetGUIThreadId(), en_TiltSignal);
				}
				en_TiltSignal = GYRO_TILT_NULL;
				break;
			
			case GYRO_TILT_Z_P:
				if (GyroGetTilt() == GYRO_TILT_Z_M)
				{
					osSignalSet(GetGUIThreadId(), en_TiltSignal);
				}
				en_TiltSignal = GYRO_TILT_NULL;
				break;
			
			case GYRO_TILT_Z_M:
				if (GyroGetTilt() == GYRO_TILT_Z_P)
				{
					osSignalSet(GetGUIThreadId(), en_TiltSignal);
				}
				en_TiltSignal = GYRO_TILT_NULL;
				break;
			
			case GYRO_TILT_NULL:
				en_TiltSignal = GyroGetTilt();
				break;
			
		}
	}
}

static t_en_TiltSignal GyroGetTilt(void)
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

int Init_Gyro(void)
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

	BSP_GYRO_Init();
	BSP_GYRO_Reset();

	return(0);
}

