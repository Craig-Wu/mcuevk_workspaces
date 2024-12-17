/*
* Copyright (c) 2020 - 2024 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#ifndef __HDMI_DIS__
#define __HDMI_DIS__


// #ifdef __LINUX__
	#include "sii902_linux.h"
// #else
// 	#include "sil9024.h"
// #endif



void hdmi_init(void);
void hdmi_loop(void);

#endif
