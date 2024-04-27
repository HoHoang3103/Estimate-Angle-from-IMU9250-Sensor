/*
 * imu.c
 *
 *  Created on: Mar 27, 2024
 *      Author: Computer
 */
#include <math.h>
#include "imu.h"

#define RAD_TO_DEG 57.295779513082320876798154814105f
#define DEG_TO_RAD 0.017453292519943295769236907684886f
#define PI	3.14159265359f
#define g_accel 9.80665

#define WHO_AM_I_REG 0x75
#define PWR_MGMT_1_REG 0x6B
#define SMPLRT_DIV_REG 0x19
#define ACCEL_CONFIG_REG 0x1C
#define ACCEL_XOUT_H_REG 0x3B
#define TEMP_OUT_H_REG 0x41
#define GYRO_CONFIG_REG 0x1B
#define GYRO_XOUT_H_REG 0x43

//Magnetometer Registers
#define AK8963_ADDRESS   0x0C<<1
#define AK8963_WHO_AM_I  0x00 // should return 0x48
#define AK8963_INFO      0x01
#define AK8963_ST1       0x02  // data ready status bit 0
#define AK8963_XOUT_L    0x03  // data
#define AK8963_XOUT_H    0x04
#define AK8963_YOUT_L    0x05
#define AK8963_YOUT_H    0x06
#define AK8963_ZOUT_L    0x07
#define AK8963_ZOUT_H    0x08
#define AK8963_ST2       0x09  // Data overflow bit 3 and data read error status bit 2
#define AK8963_CNTL      0x0A  // Power down (0000), single-measurement (0001), self-test (1000) and Fuse ROM (1111) modes on bits 3:0
#define AK8963_ASTC      0x0C  // Self test control
#define AK8963_I2CDIS    0x0F  // I2C disable
#define AK8963_ASAX      0x10  // Fuse ROM x-axis sensitivity adjustment value
#define AK8963_ASAY      0x11  // Fuse ROM y-axis sensitivity adjustment value
#define AK8963_ASAZ      0x12  // Fuse ROM z-axis sensitivity adjustment value

// Setup MPU6050
#define MPU6050_ADDR 0xD0
#define INT_PIN_CFG      0x37

#define GX_OFFSET      0.0355
#define GY_OFFSET      -0.031
#define GZ_OFFSET      -0.041

#define AX_OFFSET      0.03
#define AY_OFFSET      -0.15
#define AZ_OFFSET      -1.25

const uint16_t i2c_timeout = 100;
const double Accel_Z_corrector = 14418.0;

uint32_t timer;

Kalman_t KalmanX = {
    .Q_angle = 0.001f,
    .Q_bias = 0.003f,
    .R_measure = 0.03f};

Kalman_t KalmanY = {
    .Q_angle = 0.001f,
    .Q_bias = 0.003f,
    .R_measure = 0.03f,
};
Kalman_t KalmanZ = {
    .Q_angle = 0.001f,
    .Q_bias = 0.003f,
    .R_measure = 0.03f,
};
enum Ascale {
  AFS_2G = 0,
  AFS_4G,
  AFS_8G,
  AFS_16G
};
enum Gscale {
  GFS_250DPS = 0,
  GFS_500DPS,
  GFS_1000DPS,
  GFS_2000DPS
};
enum Mscale {
  MFS_14BITS = 0, // 0.6 mG per LSB
  MFS_16BITS      // 0.15 mG per LSB
};
float aRes, gRes, mRes;      // scale resolutions per LSB for the sensors
float magCalibration[3] = {0, 0, 0};
float gyroBias[3] = {0, 0, 0}, accelBias[3] = {0, 0, 0}, mag_b[3] = {426.7462, 243.5876, 740.4463}, mag_A[3][3]  = { {0.9791, -0.0251, -0.0318},{-0.0251, 0.972, 0.1029},{-0.0318, 0.1019, 1.063}} ; // row,row,row
uint8_t Gscale = GFS_250DPS;
uint8_t Ascale = AFS_2G;
uint8_t Mscale = MFS_16BITS; // Choose either 14-bit or 16-bit magnetometer resolution
uint8_t Mmode = 0x02; // 2 for 8 Hz, 6 for 100 Hz continuous magnetometer data read

uint8_t MPU6050_Init(I2C_HandleTypeDef *I2Cx)
{
	uint8_t check ;

	uint8_t Data;
    // check device ID WHO_AM_I

    HAL_I2C_Mem_Read(I2Cx, MPU6050_ADDR, WHO_AM_I_REG, 1, &check, 1, i2c_timeout);

    if (check == 113) // 0x68 will be returned by the sensor if everything goes well
    {
        // power management register 0X6B we should write all 0's to wake the sensor up
        Data = 0;
        HAL_I2C_Mem_Write(I2Cx, MPU6050_ADDR, PWR_MGMT_1_REG, 1, &Data, 1, i2c_timeout);

        // Set DATA RATE of 1KHz by writing SMPLRT_DIV register
        Data = 0x07;
        HAL_I2C_Mem_Write(I2Cx, MPU6050_ADDR, SMPLRT_DIV_REG, 1, &Data, 1, i2c_timeout);

        // Set accelerometer configuration in ACCEL_CONFIG Register
        // XA_ST=0,YA_ST=0,ZA_ST=0, FS_SEL=0 -> � 2g
        Data = 0x00;
        HAL_I2C_Mem_Write(I2Cx, MPU6050_ADDR, ACCEL_CONFIG_REG, 1, &Data, 1, i2c_timeout);

        // Set Gyroscopic configuration in GYRO_CONFIG Register
        // XG_ST=0,YG_ST=0,ZG_ST=0, FS_SEL=0 -> � 250 �/s
        Data = 0x00;
        HAL_I2C_Mem_Write(I2Cx, MPU6050_ADDR, GYRO_CONFIG_REG, 1, &Data, 1, i2c_timeout);

        // Set Magnetic configuration
    	uint8_t check1 ;
        Data = 0x22;
        HAL_I2C_Mem_Write(I2Cx, MPU6050_ADDR, INT_PIN_CFG, 1, &Data, 1, i2c_timeout);
        HAL_I2C_Mem_Read(I2Cx, AK8963_ADDRESS, AK8963_WHO_AM_I, 1, &check1, 1, i2c_timeout);
        if (check1 == 72) {
        	//AK8963_Init(I2Cx , magCalibration);

        	 uint8_t rawMagCalData[3];
        			uint8_t Data;
        		//Power down magnetometer
        		Data = 0x00;
        		 HAL_I2C_Mem_Write(I2Cx, AK8963_ADDRESS, AK8963_CNTL, 1, &Data, 1, i2c_timeout);
        		//HAL_Delay(100);
        		 //Enter Fuse ROM access mode
        		 Data = 0x0F;
        		HAL_I2C_Mem_Write(I2Cx, AK8963_ADDRESS, AK8963_CNTL, 1, &Data, 1, i2c_timeout);
        		// HAL_Delay(100);
        		Data = Mscale << 4 | Mmode;
        			HAL_I2C_Mem_Write(I2Cx, AK8963_ADDRESS, AK8963_CNTL, 1, &Data, 1, i2c_timeout);
            return 0;
            }

        return 1;
    }
    return 1;
}

void MPU6050_Read_Accel(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct)
{
    uint8_t Rec_Data[6];

    // Read 6 BYTES of data starting from ACCEL_XOUT_H register

    HAL_I2C_Mem_Read(I2Cx, MPU6050_ADDR, ACCEL_XOUT_H_REG, 1, Rec_Data, 6, i2c_timeout);

    DataStruct->Accel_X_RAW = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    DataStruct->Accel_Y_RAW = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
    DataStruct->Accel_Z_RAW = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);

    /*** convert the RAW values into acceleration in 'g'
         we have to divide according to the Full scale value set in FS_SEL
         I have configured FS_SEL = 0. So I am dividing by 16384.0
         for more details check ACCEL_CONFIG Register              ****/

    DataStruct->Ax = DataStruct->Accel_X_RAW / 16384.0;
    DataStruct->Ay = DataStruct->Accel_Y_RAW / 16384.0;
    DataStruct->Az = DataStruct->Accel_Z_RAW / Accel_Z_corrector;
}

void MPU6050_Read_Gyro(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct)
{
    uint8_t Rec_Data[6];

    // Read 6 BYTES of data starting from GYRO_XOUT_H register

    HAL_I2C_Mem_Read(I2Cx, MPU6050_ADDR, GYRO_XOUT_H_REG, 1, Rec_Data, 6, i2c_timeout);

    DataStruct->Gyro_X_RAW = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    DataStruct->Gyro_Y_RAW = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
    DataStruct->Gyro_Z_RAW = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);

    /*** convert the RAW values into dps (�/s)
         we have to divide according to the Full scale value set in FS_SEL
         I have configured FS_SEL = 0. So I am dividing by 131.0
         for more details check GYRO_CONFIG Register              ****/

    DataStruct->Gx = DataStruct->Gyro_X_RAW / 131.0;
    DataStruct->Gy = DataStruct->Gyro_Y_RAW / 131.0;
    DataStruct->Gz = DataStruct->Gyro_Z_RAW / 131.0;
}
void MPU6050_Read_Mag(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct)
{
	uint8_t data[7];
	uint8_t check;

	/* Check Mag Data Ready Status */
	HAL_I2C_Mem_Read(I2Cx, AK8963_ADDRESS, AK8963_ST1, 1, &check, 1, i2c_timeout);

	if ((check & 0x01) == 0x01 )
	{
		HAL_I2C_Mem_Read(I2Cx, AK8963_ADDRESS, AK8963_XOUT_L, 1, data, 7, i2c_timeout);  // Read the six raw data and ST2 registers sequentially into data array
		uint8_t c = data[6];
		/* Check (ST2 Register) If Magnetic Sensor Overflow Occured */
		if(!(c & 0x08)){

			DataStruct->Mag_X_RAW= ((int16_t)data[1] << 8) | (uint8_t)data[0];
			DataStruct->Mag_Y_RAW = ((int16_t)data[3] << 8) | (uint8_t)data[2];
			DataStruct->Mag_Z_RAW = ((int16_t)data[5] << 8) | (uint8_t)data[4];

			getMres();
			DataStruct->Mx = (float)(DataStruct->Mag_X_RAW * mRes);
			DataStruct->My = (float)(DataStruct->Mag_Y_RAW * mRes);
			DataStruct->Mz = (float)(DataStruct->Mag_Z_RAW * mRes);
		}

	}
}
void MPU6050_Read_Temp(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct)
{
    uint8_t Rec_Data[2];
    int16_t temp;

    // Read 2 BYTES of data starting from TEMP_OUT_H_REG register

    HAL_I2C_Mem_Read(I2Cx, MPU6050_ADDR, TEMP_OUT_H_REG, 1, Rec_Data, 2, i2c_timeout);

    temp = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    DataStruct->Temperature = (float)((int16_t)temp / (float)340.0 + (float)36.53);
}

void MPU6050_Read_All(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct)
{
    uint8_t Rec_Data[14];
    uint8_t data[7];
    uint8_t check;
    int16_t temp;

    // Read 14 BYTES of data starting from ACCEL_XOUT_H register

    HAL_I2C_Mem_Read(I2Cx, MPU6050_ADDR, ACCEL_XOUT_H_REG, 1, Rec_Data, 14, i2c_timeout);

    DataStruct->Accel_X_RAW = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    DataStruct->Accel_Y_RAW = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
    DataStruct->Accel_Z_RAW = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);
    temp = (int16_t)(Rec_Data[6] << 8 | Rec_Data[7]);
    DataStruct->Gyro_X_RAW = (int16_t)(Rec_Data[8] << 8 | Rec_Data[9]);
    DataStruct->Gyro_Y_RAW = (int16_t)(Rec_Data[10] << 8 | Rec_Data[11]);
    DataStruct->Gyro_Z_RAW = (int16_t)(Rec_Data[12] << 8 | Rec_Data[13]);

	getAres();
    DataStruct->Ax = g_accel*(DataStruct->Accel_X_RAW * aRes) -AX_OFFSET;
    DataStruct->Ay = g_accel*(DataStruct->Accel_Y_RAW * aRes)-AY_OFFSET;
    DataStruct->Az = g_accel*(DataStruct->Accel_Z_RAW / Accel_Z_corrector) - AZ_OFFSET;
    DataStruct->Temperature = (float)((int16_t)temp / (float)340.0 + (float)36.53);
	getGres();
    DataStruct->Gx = DEG_TO_RAD*(DataStruct->Gyro_X_RAW * gRes) - GX_OFFSET;
    DataStruct->Gy = DEG_TO_RAD*(DataStruct->Gyro_Y_RAW * gRes)- GY_OFFSET;
    DataStruct->Gz = DEG_TO_RAD*(DataStruct->Gyro_Z_RAW * gRes) - GZ_OFFSET;

	HAL_I2C_Mem_Read(I2Cx, AK8963_ADDRESS, AK8963_ST1, 1, &check, 1, i2c_timeout);
    if ((check & 0x01) == 0x01 ){
    	HAL_I2C_Mem_Read(I2Cx, AK8963_ADDRESS, AK8963_XOUT_L, 1, data, 7, i2c_timeout);  // Read the six raw data and ST2 registers sequentially into data array
    	uint8_t c = data[6];
    	/* Check (ST2 Register) If Magnetic Sensor Overflow Occured */
    	if(!(c & 0x08)){
    		DataStruct->Mag_X_RAW= ((int16_t)data[1] << 8) | data[0];
    		DataStruct->Mag_Y_RAW = ((int16_t)data[3] << 8) | data[2];
    		DataStruct->Mag_Z_RAW = ((int16_t)data[5] << 8) | data[4];
    		getMres();
    		/*
    		DataStruct->Mx = (float)(DataStruct->Mag_X_RAW * mRes *magCalibration[0]);
    		DataStruct->My = (float)(DataStruct->Mag_Y_RAW * mRes*magCalibration[1]);
    		DataStruct->Mz = (float)(DataStruct->Mag_Z_RAW * mRes*magCalibration[2]); */

/*
    		    		DataStruct->Mx = (float)(DataStruct->Mag_X_RAW * mRes );
    		    		DataStruct->My = (float)(DataStruct->Mag_Y_RAW * mRes);
 	    		DataStruct->Mz = (float)(DataStruct->Mag_Z_RAW * mRes);
*/
    		DataStruct->Mx = (float)(DataStruct->Mag_X_RAW * mRes )- mag_b[0];
    		DataStruct->My = (float)(DataStruct->Mag_Y_RAW * mRes)- mag_b[1];
    		DataStruct->Mz = (float)(DataStruct->Mag_Z_RAW * mRes)- mag_b[2];

    		DataStruct->Mx = mag_A[0][0]*DataStruct->Mx + mag_A[0][1]*DataStruct->My + mag_A[0][2]*DataStruct->Mz;
    		DataStruct->My = mag_A[1][0]*DataStruct->Mx + mag_A[1][1]*DataStruct->My + mag_A[1][2]*DataStruct->Mz;
    		DataStruct->Mz = mag_A[2][0]*DataStruct->Mx + mag_A[2][1]*DataStruct->My + mag_A[2][2]*DataStruct->Mz;

    		}
    	}

    // Kalman angle solve
    double dt = (double)(HAL_GetTick() - timer) / 1000;
    timer = HAL_GetTick();
    //double roll = atan2(DataStruct->Ay,DataStruct->Az);
    double roll;
    double roll_sqrt = sqrt(
        DataStruct->Accel_X_RAW * DataStruct->Accel_X_RAW + DataStruct->Accel_Z_RAW * DataStruct->Accel_Z_RAW);
    if (roll_sqrt != 0.0)
    {
    	roll = atan(DataStruct->Accel_Y_RAW / roll_sqrt) ;
        //roll = atan(DataStruct->Accel_Y_RAW / roll_sqrt) * RAD_TO_DEG;
    }
    else
    {
        roll = 0.0;
    }

   //double pitch = asin(-DataStruct->Ax/norm(DataStruct->Ax,DataStruct->Ay,DataStruct->Az));
    double pitch = atan2(-DataStruct->Accel_X_RAW, DataStruct->Accel_Z_RAW);
    //double pitch = atan2(-DataStruct->Accel_X_RAW, DataStruct->Accel_Z_RAW) * RAD_TO_DEG;
    if ((pitch < -90*DEG_TO_RAD && DataStruct->KalmanAngleY > 90*DEG_TO_RAD) || (pitch > 90*DEG_TO_RAD && DataStruct->KalmanAngleY < -90*DEG_TO_RAD))
    {
        KalmanY.angle = pitch;
        DataStruct->KalmanAngleY = pitch;
    }
    else
    {
        DataStruct->KalmanAngleY = Kalman_getAngle(&KalmanY, pitch, DataStruct->Gy, dt);
    }

    if (fabs(DataStruct->KalmanAngleY) > 90*DEG_TO_RAD)
        DataStruct->Gx = -DataStruct->Gx;
    DataStruct->KalmanAngleX = Kalman_getAngle(&KalmanX, roll, DataStruct->Gx, dt);

     float mx = (DataStruct->Mx )/ norm(DataStruct->Mx,DataStruct->My,DataStruct->Mz);
     float my = - (DataStruct->My )/ norm(DataStruct->Mx,DataStruct->My,DataStruct->Mz);
     float mz = (DataStruct->Mz )/ norm(DataStruct->Mx,DataStruct->My,DataStruct->Mz);

     float Mx = mx * cos(pitch) + mz * sin(pitch);
     float My = mx * sin(roll) * sin(pitch) + my * cos(roll) - mz * sin(roll) * cos(pitch);
     float yaw = atan2(-My,Mx);
/*

     float Mx = mx * cos(pitch) + my*sin(pitch)*sin(roll) + mz * sin(pitch)*cos(roll);
     float My =  my * cos(roll) - mz * sin(roll);
     float yaw = atan2(-My,Mx);
*/
     if (yaw > PI) {
      yaw -= 2*PI;
      }
     else if (yaw < -PI) {
      yaw += 2*PI;
      }
     DataStruct->KalmanAngleZ = Kalman_getAngle(&KalmanZ, yaw, DataStruct->Gz, dt);
}

double Kalman_getAngle(Kalman_t *Kalman, double newAngle, double newRate, double dt)
{
    double rate = newRate - Kalman->bias;
    Kalman->angle += dt * rate; // predict goc , dua vao rate cua gyro (van toc goc)

    Kalman->P[0][0] += dt * (dt * Kalman->P[1][1] - Kalman->P[0][1] - Kalman->P[1][0] + Kalman->Q_angle);
    Kalman->P[0][1] -= dt * Kalman->P[1][1];
    Kalman->P[1][0] -= dt * Kalman->P[1][1];
    Kalman->P[1][1] += Kalman->Q_bias * dt;

    double S = Kalman->P[0][0] + Kalman->R_measure;
    double K[2];
    K[0] = Kalman->P[0][0] / S;
    K[1] = Kalman->P[1][0] / S;

    double y = newAngle - Kalman->angle;
    Kalman->angle += K[0] * y;  // update tu roll/pitch cua accel
    Kalman->bias += K[1] * y;

    double P00_temp = Kalman->P[0][0];
    double P01_temp = Kalman->P[0][1];

    Kalman->P[0][0] -= K[0] * P00_temp;
    Kalman->P[0][1] -= K[0] * P01_temp;
    Kalman->P[1][0] -= K[1] * P00_temp;
    Kalman->P[1][1] -= K[1] * P01_temp;

    return Kalman->angle;
};

void AK8963_Init(I2C_HandleTypeDef *I2Cx, float * destination){
	  uint8_t rawMagCalData[3];
		uint8_t Data;
	//Power down magnetometer
	Data = 0x00;
	 HAL_I2C_Mem_Write(I2Cx, AK8963_ADDRESS, AK8963_CNTL, 1, &Data, 1, i2c_timeout);
	//HAL_Delay(100);
	 //Enter Fuse ROM access mode
	 Data = 0x0F;
	HAL_I2C_Mem_Write(I2Cx, AK8963_ADDRESS, AK8963_CNTL, 1, &Data, 1, i2c_timeout);
	// HAL_Delay(100);
	HAL_I2C_Mem_Read(I2Cx, AK8963_ADDRESS, AK8963_ASAX, 1, &rawMagCalData[0], 3, i2c_timeout);// Read the x-, y-, and z-axis calibration values
	destination[0] =  (float)(rawMagCalData[0] - 128)/256. + 1.;   // Return x-axis sensitivity adjustment values, etc.
	 destination[1] =  (float)(rawMagCalData[1] - 128)/256. + 1.;
	destination[2] =  (float)(rawMagCalData[2] - 128)/256. + 1.;
	// Set magnetometer data resolution and sample ODR
	Data = Mscale << 4 | Mmode;
	HAL_I2C_Mem_Write(I2Cx, AK8963_ADDRESS, AK8963_CNTL, 1, &Data, 1, i2c_timeout);
}
void getMres() {
  switch (Mscale)
  {
  // Possible magnetometer scales (and their register bit settings) are:
  // 14 bit resolution (0) and 16 bit resolution (1)
    case MFS_14BITS:
          mRes = 10.*4912./8190.; // Proper scale to return milliGauss
          break;
    case MFS_16BITS:
          mRes = 10.*4912./32760.0; // Proper scale to return milliGauss
          break;
  }
}
void getGres() {
  switch (Gscale)
  {
  // Possible gyro scales (and their register bit settings) are:
  // 250 DPS (00), 500 DPS (01), 1000 DPS (10), and 2000 DPS  (11).
        // Here's a bit of an algorith to calculate DPS/(ADC tick) based on that 2-bit value:
    case GFS_250DPS:
          gRes = 250.0/32768.0;
          break;
    case GFS_500DPS:
          gRes = 500.0/32768.0;
          break;
    case GFS_1000DPS:
          gRes = 1000.0/32768.0;
          break;
    case GFS_2000DPS:
          gRes = 2000.0/32768.0;
          break;
  }
}
void getAres() {
  switch (Ascale)
  {
  // Possible accelerometer scales (and their register bit settings) are:
  // 2 Gs (00), 4 Gs (01), 8 Gs (10), and 16 Gs  (11).
        // Here's a bit of an algorith to calculate DPS/(ADC tick) based on that 2-bit value:
    case AFS_2G:
          aRes = 2.0/32768.0;
          break;
    case AFS_4G:
          aRes = 4.0/32768.0;
          break;
    case AFS_8G:
          aRes = 8.0/32768.0;
          break;
    case AFS_16G:
          aRes = 16.0/32768.0;
          break;
  }
}
float norm(float a, float b, float c) {
 return sqrt(a*a + b*b + c*c);
}
