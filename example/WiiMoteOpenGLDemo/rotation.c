#include <stdio.h>
#include <string.h>
#include <math.h>

#define rad M_PI/180.0
#define deg 180.0/M_PI

// define the rest position 
static float restPosition[3] = {1,0,0};

float norm(float *vector, int dim){
    float result = 0;
    int i;
    
    for (i=0; i<dim; i++){
        result+= vector[i]*vector[i];
    }
    
    if (result == 0) result = 1;
    return sqrt(result);
}

void normalizeVector(float *vector, int dim){
    int i;
    float vnorm = norm(vector,dim);
    for (i=0; i<dim; i++){
        vector[i]/=vnorm;
    }
}

/****** START OF TEST FUNCTIONS THAT CREATE ACC_DATA ******/

// define X and Y axes 
static float rotationAxisX[3] = {0,0,-1};
static float rotationAxisY[3] = {1,0,0};

static void quaternionFromAxis(float angle, float axis[3], float quaternion[4]){
    normalizeVector(axis,3);
	
	float qangle = angle * 0.5;
	float sinQAngle = sin(qangle);
 
    quaternion[0] = cos(qangle);
    quaternion[1] = axis[0]*sinQAngle;
    quaternion[2] = axis[1]*sinQAngle;
    quaternion[3] = axis[2]*sinQAngle;
}


static void multiplyQuaternions(float qy[4], float qx[4], float result[4]){
    // p0*q2 - p*q + p0*q + q0*p + pxq
    
    float t0 = (qy[3]-qy[2])*(qx[2]-qx[3]);
    float t1 = (qy[0]+qy[1])*(qx[0]+qx[1]);
    float t2 = (qy[0]-qy[1])*(qx[2]+qx[3]);
    float t3 = (qy[3]+qy[2])*(qx[0]-qx[1]);
    float t4 = (qy[3]-qy[1])*(qx[1]-qx[2]);
    float t5 = (qy[3]+qy[1])*(qx[1]+qx[2]);
    float t6 = (qy[0]+qy[2])*(qx[0]-qx[3]);
    float t7 = (qy[0]-qy[2])*(qx[0]+qx[3]);
    float t8 = t5+t6+t7;
    float t9 = (t4+t8)/2;
     
    result[0] = t0+t9-t5;
    result[1] = t1+t9-t8;
    result[2] = t2+t9-t7;
    result[3] = t3+t9-t6;
}

static void rotateVectorWithQuaternion(float restPosition[3], float qrot[4], float qaxis[3]){
    float result[4] = {0,0,0,0};
    
    float qrot_conj[4] = {qrot[0], -qrot[1], -qrot[2], -qrot[3]};
    float qrestPosition[4] = {0, restPosition[0], restPosition[1], restPosition[2]};
    
    multiplyQuaternions(qrestPosition,qrot_conj,result);
    multiplyQuaternions(qrot,result,result);
    
    qaxis[0] = result[1];
    qaxis[1] = result[2];
    qaxis[2] = result[3];
}
    
static void getAccellerometerData(float radrotationAngleX, float radrotationAngleY, float qaxis[3]){
    float qx[4] = {0,0,0,0};
    float qy[4] = {0,0,0,0};
    float qrot[4] = {0,0,0,0};
    
    quaternionFromAxis(radrotationAngleX,rotationAxisX,qx);
    quaternionFromAxis(radrotationAngleY,rotationAxisY,qy);
    
    multiplyQuaternions(qy,qx,qrot);
    rotateVectorWithQuaternion(restPosition,qrot,qaxis);
} 

/****** END OF TEST FUNCTIONS THAT CREATE ACC_DATA ******/


void getRotationMatrixFromQuartenion(float q[4], float m[4][4]){
    float w = q[0];
    float x = q[1];
    float y = q[2];
    float z = q[3];
    
    float y2 = y*y; 
    float x2 = x*x; 
    float z2 = z*z;
    
    m[0][0] = 1-2*y2-2*z2;
    m[0][1] = 2*x*y-2*w*z;
    m[0][2] = 2*x*z+2*w*y;
    m[0][3] = 0;
    
    m[1][0] = 2*x*y+2*w*z;
    m[1][1] = 1-2*x2-2*z2;
    m[1][2] = 2*y*z-2*w*x;
    m[1][3] = 0;
    
    m[2][0] = 2*x*z-2*w*y;
    m[2][1] = 2*y*z+2*w*x;
    m[2][2] = 1-2*x2-2*y2;
    m[2][3] = 0;
    
    m[3][0] = 0;
    m[3][1] = 0;
    m[3][2] = 0;
    m[3][3] = 1;
}


void getRotationMatrixFromVectors(float vin[3], float vout[3], float matrix[4][4]){
    normalizeVector(vout,3);
    
    float q[4] = {0,0,0,0};
    
    float vin_length = 0;
    float vout_length = 0;
    float dotprod = 0;
    
    int i;
    for (i=0; i<3; i++){
        vin_length += vin[i]*vin[i];
        vout_length+= vout[i]*vout[i];
        dotprod += vin[i]*vout[i];
    }

#if 1 //mila
    q[0] = sqrt(vin_length * vout_length) + dotprod;
    q[1] = -1*(vin[1]*vout[2] - vin[2]*vout[1]);
    q[2] = -1*(vin[2]*vout[0] - vin[0]*vout[2]);
    q[3] = -1*(vin[0]*vout[1] - vin[1]*vout[0]);
#else    
    float axis[3] = {0,0,0};
    axis[0] = -1*(vin[1]*vout[2] - vin[2]*vout[1]);
    axis[1] = -1*(vin[2]*vout[0] - vin[0]*vout[2]);
    axis[2] = -1*(vin[0]*vout[1] - vin[1]*vout[0]);
    normalizeVector(axis,3);
    
    float angle = acos(vin[0]*vout[0]+vin[1]*vout[1]+vin[2]*vout[2]);
    quaternionFromAxis(angle, axis, q);
#endif    
	
	normalizeVector(q,4);
    getRotationMatrixFromQuartenion(q,matrix);

}

float getRotationAngle(float matrix[4][4]){
    return acos( (matrix[0][0]+matrix[1][1]+matrix[2][2]-1) * 0.5);
}

#if 0
int main(void)
{
    float accData[3] = {1,0,0};
    float rotationMatrix[4][4];
    
    normalizeVector(restPosition,3);
    
    int angle;
    for (angle = 0; angle <90; angle+=30){
        getAccellerometerData(angle*rad*1.5,angle*rad,accData);
        
        getRotationMatrixFromVectors(restPosition, accData, rotationMatrix);
    }
    
	return 1;
}
#endif



