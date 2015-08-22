//
//  types.h
//  xlayer-module
//
//  Created by Yuping Dong on 7/8/15.
//  Copyright (c) 2015 Yuping Dong. All rights reserved.
//

#ifndef xlayer_module_types_h
#define xlayer_module_types_h

#ifndef pi
#define pi 3.14159265358979323846
#endif

#ifndef epsilon
#define epsilon 0.05
#endif



typedef struct APPST {
    int x;		// # of packets waiting to be transmitted that have 1 stage remaining lifetime.
    int y;		// # of packets waiting to be transmitted that have 2 stage remaining lifetime.
} APPST;

typedef struct QOS {
    float x;		// packet loss rate
    float y;		// transmission time per packet
    float z;		// transmission cost per packet
    int b1;		// internal action at PHY layer
    struct QOS *next;
} QOS;

typedef struct RETLY3 {	// Layer 3 return struct
    float x;		// val_12
    int a3;
    int b1;
    float zx;
    float zy;
    float zz;
} RETLY3;

typedef struct RETLY2 {
    float x;		// val_1
    int a2;
    int a3;
    int b1;
    float zx;
    float zy;
    float zz;
} RETLY2;

typedef struct RETLY1 {
    float x;		// Val_curr
    float a1;
    int a2;
    int a3;
    int b1;
    float zx;
    float zy;
    float zz;
} RETLY1;
