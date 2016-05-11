
typedef enum
{
	GYRO_TILT_NULL	=	(0<<0),
	
	GYRO_TILT_X_P	=	(1<<0),
	GYRO_TILT_X_M	=	(1<<1),
	
	GYRO_TILT_Y_P	=	(1<<2),
	GYRO_TILT_Y_M	=	(1<<3),
	
	GYRO_TILT_Z_P	=	(1<<4),
	GYRO_TILT_Z_M	=	(1<<5),
} t_en_TiltSignal;

int Init_Gyro(void);
