#include <math.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

static struct timeval init_tv;

union {
    int i;
    float x;
} u;

// taken from http://www.codeproject.com/Articles/69941/Best-Square-Root-Method-Algorithm-Function-Precisi
// Algorithm: Babylonian Method + some manipulations on IEEE 32 bit floating point representation
float sqrt1(const float x){
    u.x = x;
    u.i = (1<<29) + (u.i >> 1) - (1<<22); 

    // Two Babylonian Steps (simplified from:)
    // u.x = 0.5f * (u.x + x/u.x);
    // u.x = 0.5f * (u.x + x/u.x);
    u.x =       u.x + x/u.x;
    u.x = 0.25f*u.x + x/u.x;

    return u.x;
}  

// Algorithm: Log base 2 approximation and Newton's Method
float sqrt3(const float x){
    u.x = x;
    u.i = (1<<29) + (u.i >> 1) - (1<<22); 
    return u.x;
}

float sqrt2(const float n) {
    /*using n itself as initial approximation => improve */
    float x = n;
    float y = 1;
    float e = 0.001; /* e decides the accuracy level*/
    while(x - y > e){
        x = (x + y)/2;
        y = n/x;
    }
    return x;
}

static uint32_t get_time_ms(void){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint32_t time_ms = (uint32_t)((tv.tv_sec  - init_tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
    return time_ms;
}

static int values_len = 100000;

TEST_GROUP(SqrtTest){
    void setup(void){
    }

    void test_method(float (*my_sqrt)(const float x)){
        int i, j;
        float precision = 0;
        int ta = 0;
        int te = 0;

        for (j=0; j<100; j++){
            for (i=0; i<values_len; i++){
                int t1 = get_time_ms();
                float expected = sqrt(i);
                int t2 = get_time_ms();
                float actual = my_sqrt(i);
                int t3 = get_time_ms();
                te += t2 - t1;
                ta += t3 - t2;    
                precision += fabs(expected - actual);
            }
        }
        printf("Precision: %f, Time: (%d, %d)ms\n", precision/values_len, te, ta);
    }
};

TEST(SqrtTest, Sqrt1){
    printf("\nsqrt1: ");
    test_method(sqrt1);
}

TEST(SqrtTest, Sqrt2){
    printf("\nsqrt2: ");
    test_method(sqrt2);
}

TEST(SqrtTest, Sqrt3){
    printf("\nsqrt3: ");
    test_method(sqrt3);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

// TODO: check http://www.embedded.com/electronics-blogs/programmer-s-toolbox/4219659/Integer-Square-Roots