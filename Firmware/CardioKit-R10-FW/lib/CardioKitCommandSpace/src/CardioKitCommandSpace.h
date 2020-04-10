/*
    CardioKitCommandSpace.h
    Library for Handling User-Defined Commands from Cloud Host
    Created by Nathan Volman, Feb 24, 2020
*/
#ifdef __cplusplus
extern "C" {
#endif
#ifndef CARDIOKITCOMMANDSPACE_H
#define CARDIOKITCOMMANDSPACE_H
#include <WProgram.h>

// call this in loop to handle incoming commands from cloud host
void HandleCloudCommand(uint32_t cmd);

#endif //CARDIOKITCOMMANDSPACE_H
#ifdef __cplusplus
}
#endif
