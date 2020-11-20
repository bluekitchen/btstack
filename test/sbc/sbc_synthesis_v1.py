#!/usr/bin/env python3
import numpy as np
import wave
import struct
import sys
import time
from sbc import *

matrix_R = np.zeros(shape = (16,8))
matrix_C2 = np.zeros(shape = (8,8))
matrix_N = np.zeros(shape = (16,16))
V_SGN = [1,1,1,1, 0, -1,-1,-1,-1, 1,1,1,1,1,1,1]
V_remap = np.zeros(80)

def sbc_init_synthesis_v1(M):
    global matrix_R, matrix_C2, matrix_N
    if M == 4:
        print("SBC V1 init failed, 4-subband version not implemented yet")
    M2 = M << 1
    Mh = M >> 1
    L  = 10 * M
    L2 = L << 1

    matrix_R = np.zeros(shape = (M2,M))
    matrix_C2 = np.zeros(shape = (M,M))
    matrix_N = np.zeros(shape = (M2,M2))
    V_remap  = np.zeros(L)

    R_c1 = 12 
    
    for k in range(Mh):
        matrix_R[k][k+Mh] = 1

    for k in range(Mh+1,M2):
        matrix_R[k][abs(R_c1-k)] = -1

    for k in range(M):
        for i in range(M):
            matrix_C2[k][i] = np.cos((i+0.5)*k*np.pi/M)
    
    matrix_N = np.dot(matrix_R, matrix_C2)

    for i in range(L):
        offset = i%M2
        if offset >= M:
            offset += M2
        V_remap[i] = L2/5 * (i/M2) + offset



def VSGN(i,M2):
    return V_SGN[i%M2]

def remap_V(i):
    global V_remap
    return V_remap[i]

def sbc_frame_synthesis_v1_4subbands(frame, ch, blk):
    print ("sbc_frame_synthesis_v1_4subbands(frame, ch, blk) not implemented yet")
    exit(1)

def sbc_frame_synthesis_v1_8subbands(frame, ch, blk):
    print ("sbc_frame_synthesis_v1_8subbands(frame, ch, blk) not implemented yet")
    exit(1)

def matrix_R():
    global matrix_R
    return matrix_R

def matrix_C2():
    global matrix_C2
    return matrix_C2

def matrix_N():
    global matrix_N
    return matrix_N

def R(k,i):
    global matrix_R
    return matrix_R[k][i]

def C2(k,i):
    global matrix_C2
    return matrix_C2[k][i]