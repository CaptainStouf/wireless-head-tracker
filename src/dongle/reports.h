#ifndef REPORTS_H
#define REPORTS_H

#ifdef _MSC_VER
# pragma pack(push)
# pragma pack(1)
#endif

// This is the main HID joystick report.
// It contains the axis data we're sending to the PC.

#define JOYSTICK_REPORT_ID				1

typedef struct
{
	uint8_t	report_id;		// == JOYSTICK_REPORT_ID

	int16_t	x;
	int16_t	y;
	int16_t	z;

} hid_joystick_report_t;

// the HID keyboard report
extern hid_joystick_report_t	usb_joystick_report;

void reset_joystick_report(void);

// *****************************************************************
// *****************************************************************
// *****************************************************************

#define DONGLE_SETTINGS_REPORT_ID			2

// data direction: dongle <-> PC
typedef struct
{
	uint8_t		report_id;		// == AXIS_CONFIG_REPORT_ID

	uint8_t		autocenter;		// 0 to 3 for autocenter values
	uint8_t		is_linear;

	// axis factors
	float		fact_x;
	float		fact_y;
	float		fact_z;

	float		x_drift_comp;

} FeatRep_DongleSettings;

// *****************************************************************
// *****************************************************************
// *****************************************************************

#define COMMAND_REPORT_ID		3

enum head_tracker_commands_t
{
	// the first two are sent to the head tracker over radio
	CMD_CALIBRATE			= 1,
	CMD_SEND_CALIB_DATA		= 2,
	
	// these are send from the PC to the dongle
	CMD_RECENTER			= 3,
	CMD_SAVE_DRIFT			= 4,
	CMD_INC_DRIFT_COMP		= 5,
	CMD_DEC_DRIFT_COMP		= 6,
};

// direction: PC -> dongle
typedef struct
{
	uint8_t		report_id;		// COMMAND_REPORT_ID
	uint8_t		command;
} FeatRep_Command;

// *****************************************************************
// *****************************************************************
// *****************************************************************

#define CALIBRATION_DATA_REPORT_ID		4

// direction: dongle -> PC
typedef struct
{
	uint8_t		report_id;		// CALIBRATE_REPORT_ID

	uint8_t		has_tracker_responded;
	
	uint8_t		is_calibrated;

	int16_t		gyro_bias[3];
	int16_t		accel_bias[3];
} FeatRep_CalibrationData;

// *****************************************************************
// *****************************************************************
// *****************************************************************

#define STATUS_REPORT_ID				5

// direction: dongle -> PC
typedef struct
{
	uint8_t		report_id;		// STATUS_REPORT_ID

	uint8_t		num_packets;	// number of packets received in the last second
	
	float		new_drift_comp;	// the calculated drift compensation
	int32_t		driftSamples;
	float		dX;
	
	uint16_t	battery_voltage;
	uint16_t	temperature;

} FeatRep_Status;

// *****************************************************************
// *****************************************************************
// *****************************************************************

#define STATUS_REPORT_ID				5

// direction: dongle -> PC
typedef struct
{
	uint8_t		report_id;		// STATUS_REPORT_ID

	uint8_t		flags;			// 0x01 - increase drift comp by 0.1
								// 0x02 - decrease drift comp by 0.1

} FeatRep_ManualDrift;

#ifdef _MSC_VER
# pragma pack(pop)
#endif

#endif	// REPORTS_H