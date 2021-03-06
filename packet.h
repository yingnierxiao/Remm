/*
 * packet.h
 */

#ifndef PACKET_H_
#define PACKET_H_

/*  Unique packet type identification   */
typedef enum {
    MAGIC_PICTURE   =   0x50494354,
    MAGIC_STATUS    =   0x434F4D4D,
    MAGIC_COMMAND   =   0x53544154
} magic_e;

#define FRAGMENT_SIZE   (512 - 5 * 4)

/*=================================Raspberry Pi===============================*/
/** Sends from Raspberry Pi host. Contain picture.
 *  Picture size is larger then UDP packet size(512, 548 = 576 - 20 - 8, 8192bytes, or 65507?), so need fragmentation. */
//  http://stackoverflow.com/questions/1098897/what-is-the-largest-safe-udp-packet-size-on-the-internet
//  http://stackoverflow.com/questions/900697/how-to-find-the-largest-udp-packet-i-can-send-without-fragmenting
//  PJSIP_UDP_SIZE_THRESHOLD    1300
#pragma pack(push, 1)

typedef struct {
    magic_e magic;
    int picture_id;
    int picture_size;   //  IplImage::imageSize
    int fragment_id;
    int fragment_size;
    unsigned char data[FRAGMENT_SIZE];  //  piece of IplImage::imageData
} picture_packet_s;

/** Sends from Raspberry Pi host. Contain status info. */
typedef struct {
    magic_e magic;
    int height;     /**<    Height above ground */
    int heading;    /**<    Direction accordingly to North Pole */
    double gps_latitude;
    double gps_longitude;
    int pitch;      /**<    Slope accordingly to horizontal positions up/down   */
    int roll;       /**<    Slope accordingly to horizontal positions left/right */
    float battery_charge; /**<    In voltages */
    char info[100];
} status_packet_s;

/*=================================PC host====================================*/
/** Autopilots commands */
typedef enum {
    AUTOPILOT_OFF       = 0x00,
    AUTOPILOT_LANDING   = 0x03,
    AUTOPILOT_RETURN_TO_BASE    = 0x0F,
    AUTOPILOT_STAY_IN_3G_CELL   = 0x3F,
    AUTOPILOT_KEEP_HEIGHT       = 0xFF
} autopilot_command_e;

/** Camera specific settings */
typedef struct camera_settings {
    unsigned int width;
    unsigned int height;
    unsigned int quality;    /**<    JPEG quality in range [0...100] */
    // see enum v4l2_exposure_auto_type in "linux/videodev2.h"
    unsigned int exposure_type;    /**<    0(Auto), 1(Manual), 2(Shutter), 3(Aperture) */
    unsigned int exposure_value;   /**<    Value of manual exposure */
    float fps;      /**<    0(turn off camera), [0.1 to 2] step=0.1, (2 to 10] step=1 */
} camera_settings_s;

/** Sends from PC host. Contain control commands. */
typedef struct {
    magic_e magic;
    int height;
    int direction;
    double gps_latitude;
    double gps_longitude;
    int slope;
    camera_settings_s camera;
    autopilot_command_e command;
} control_packet_s;

#pragma pack(pop)

#endif /* PACKET_H_ */
