#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include <math.h>

#include <compiler_mcs51.h>

#include "nrfdbg.h"

#include "rf_protocol.h"
#include "proc_packet.h"
//#include "math_cordic.h"
#include "reports.h"
#include "dongle_settings.h"
#include "mdu.h"
#include "mymath.h"

// process_packet function processes the data from the sensor MPU-6050 attached to the user's head,
// and calculates xyz coordinates from the received quaternions.
//
// Almost the entire process_packet function is more-or-less copied from the ED Tracker project.
// 
// ED Tracker can be found here: http://edtracker.org.uk/

#define APPLY_DRIFT_COMP_PACKETS	5

int16_t driftSamples = -2;
float lastX = 0, dX = 0, dY, dZ;
float lX = 0.0;
float dzX = 0.0;
uint8_t ticksInZone = 0;
uint8_t recalibrateSamples = 200;
float cx, cy, cz = 0.0;
bool calibrated = false;
int16_t sampleCount = 0;
uint8_t pckt_cnt = 0;
bool pc_recenter = false;

/*
#define DRIFT_TABLE_SIZE	30

typedef struct
{
	int16_t		cnt;
	float		drift;
} new_drift_data_t;

#define MIN_TEMPERATURE		250

new_drift_data_t drift_all[DRIFT_TABLE_SIZE];
*/

void save_x_drift_comp(void)
{
	// get the current settings
	FeatRep_DongleSettings __xdata new_settings;
	memcpy(&new_settings, get_settings(), sizeof(FeatRep_DongleSettings));
	
	// set the new value
	new_settings.x_drift_comp += get_curr_x_drift_comp();
	
	save_settings(&new_settings);
}

float get_curr_x_drift_comp(void)
{
	if (driftSamples > 0)
		return dX / (float)driftSamples;
		
	return 0;
}

void recenter(void)
{
	pc_recenter = true;
}

float constrain_flt(float val)
{
	if (val < -16383.0)
		return -16384.0;
		
	if (val > 16383.0)
		return 16383.0;
		
	return val;
}

int32_t constrain_16bit(int32_t val)
{
	if (val < -32768)
		return -32768;
		
	if (val > 32767)
		return 32767;
		
	return val;
}

/*
int8_t dcnt = 0;
#define DUMP_CHUNK	2

void dump_drift(void)
{
	int8_t c;
	int8_t found = 0;
	
	for (c = 0; c < DRIFT_TABLE_SIZE  &&  found < DUMP_CHUNK; ++c)
	{
		if (drift_all[dcnt].cnt)
		{
			dprintf("%d %d %f  \n", (dcnt << 1) + MIN_TEMPERATURE, drift_all[dcnt].cnt, drift_all[dcnt].drift);
			found++;
		}
		
		dcnt++;
		if (dcnt == DRIFT_TABLE_SIZE)
		{
			dcnt = 0;
			//dprintf("\x1b[2J");		// clear screen
			dprintf("\x1b[H");		// cursor home
			//dputs("---");
		}
	}
}
*/

bool process_packet(mpu_packet_t* pckt)
{
	float newZ, newY, newX;
	float dzlimit;
	int32_t iX, iY, iZ;

	float qw, qx, qy, qz;
	float qww, qxx, qyy, qzz;
	
	const FeatRep_DongleSettings __xdata * pSettings = get_settings();

	// calculate Yaw/Pitch/Roll
	qw = pckt->quat[0];
	qx = pckt->quat[1];
	qy = pckt->quat[2];
	qz = pckt->quat[3];

	/*
	// the CORDIC trig functions return angles in units already adjusted to the 16
	// bit integer range, so there's no need to scale the results by 10430.06
		
	qww = mul_16x16(qw, qw);
	qxx = mul_16x16(qx, qx);
	qyy = mul_16x16(qy, qy);
	qzz = mul_16x16(qz, qz);

	newZ =  iatan2_cord(2 * (mul_16x16(qy, qz) + mul_16x16(qw, qx)), qww - qxx - qyy + qzz);
	newY = -iasin_cord(-2 * (mul_16x16(qx, qz) - mul_16x16(qw, qy)));
	newX = -iatan2_cord(2 * (mul_16x16(qx, qy) + mul_16x16(qw, qz)), qww + qxx - qyy - qzz);
	*/

	qww = qw * qw;
	qxx = qx * qx;
	qyy = qy * qy;
	qzz = qz * qz;

	newZ =  atan2(2 * (qy*qz + qw*qx), qww - qxx - qyy + qzz);
	newY = -asin(-2 * (qx*qz - qw*qy));
	newX = -atan2(2 * (qx*qy + qw*qz), qww + qxx - qyy - qzz);

	newX *= 10430.06;
	newY *= 10430.06;
	newZ *= 10430.06;
	
	if (!calibrated)
	{
		//memset(drift_all, 0, sizeof(drift_all));	// reset our drift
	
		if (sampleCount < recalibrateSamples)
		{
			// accumulate samples
			cx += newX;
			cy += newY;
			cz += newZ;
			++sampleCount;
		} else {
			calibrated = true;

			cx /= (float)sampleCount;
			cy /= (float)sampleCount;
			cz /= (float)sampleCount;

			dX = dY = dZ = 0.0;
			driftSamples = -2;
			recalibrateSamples = 100;	// reduce calibrate next time around
		}

		return false;
	}

	// has the user pressed the recenter button on the tracker?
	if ((pckt->flags & FLAG_RECENTER)  ||  pc_recenter)
	{
		sampleCount = 0;
		cx = cy = cz = 0.0;
		calibrated = false;

		pc_recenter = false;
		
		return false;
	}

	// apply calibration offsets
	newX = newX - cx;

	// this should take us back to zero BUT we may have wrapped so ..
	if (newX < -32768.0)
		newX += 65536.0;
	else if (newX > 32768.0)
		newX -= 65536.0;

	newY = newY - cy;
	newZ = newZ - cz;

	// clamp at 90 degrees left and right
	newX = constrain_flt(newX);
	newY = constrain_flt(newY);
	newZ = constrain_flt(newZ);

	// dprintf("%6.0f %6.0f %6.0f\n", newX, newY, newZ);
	
	if (pSettings->is_linear)
	{
		iX = newX * pSettings->fact_x;
		iY = newY * pSettings->fact_y;
		iZ = newZ * pSettings->fact_z;
		dzlimit = 35.0 * pSettings->fact_x * pSettings->autocenter;
	} else {
		iX = (0.000122076 * newX * newX * pSettings->fact_x) * (newX / fabs(newX));
		iY = (0.000122076 * newY * newY * pSettings->fact_y) * (newY / fabs(newY));
		iZ = (0.000122076 * newZ * newZ * pSettings->fact_z) * (newZ / fabs(newZ));
		dzlimit = 39.0 * pSettings->fact_x * pSettings->autocenter;
	}

	// clamp after scaling to keep values within 16 bit range
	iX = constrain_16bit(iX);
	iY = constrain_16bit(iY);
	iZ = constrain_16bit(iZ);

	// set the values in the USB report
	usb_joystick_report.x = iX;
	usb_joystick_report.y = iY;
	usb_joystick_report.z = iZ;

	// autocentering
	if (pSettings->autocenter)
	{
		// if we're looking ahead, give or take
		//  and not moving
		//  and pitch is levelish then start to count
		if (fabs(newX) < dzlimit  &&  fabs(newX - lX) < 3  &&  labs(iZ) < 1000)
		{
			//dprintf("I ");
			ticksInZone++;
			dzX += iX;
		} else {
			//dprintf("O ");
			ticksInZone = 0;
			dzX = 0.0;
		}
		
		/*
		dprintf("%f ", fabsf(newX));
		dprintf("%f ", dzlimit);
		dprintf("%f ", fabsf(newX - lX));
		dprintf("%ld ", labs(iY));
		dprintf("%d\n", ticksInZone);
		*/
		
		lX = newX;

		// if we stayed looking ahead-ish long enough then adjust yaw offset
		if (ticksInZone >= 20)
		{
			//dprintf("*");

			// NB this currently causes a small but visible jump in the
			// view. Useful for debugging!
			cx += dzX * 0.05;
			ticksInZone = 0;
			dzX = 0.0;
		}
	}

	// Apply X axis drift compensation every APPLY_DRIFT_COMP_PACKETS packets
	if (++pckt_cnt == APPLY_DRIFT_COMP_PACKETS)
	{
		cx += pSettings->x_drift_comp;

		if (cx > 65536.0)
			cx -= 65536.0;
		else if (cx < -65536.0)
			cx += 65536.0;

		pckt_cnt = 0;

		driftSamples++;

		if (driftSamples > 0)
		{
			dX += newX - lastX;
			
			/*
			float dX_loc = newX - lastX;
			int8_t ndx;
			dX += dX_loc;

			ndx = pckt->temperature - MIN_TEMPERATURE;
			if (ndx >= 0)
			{
				ndx >>= 1;	// drop resolution

				if (ndx < DRIFT_TABLE_SIZE)
				{
					drift_all[ndx].cnt++;
					drift_all[ndx].drift += dX_loc;
				}
			}
			*/
		}

		lastX = newX;
	}

	return true;
}
