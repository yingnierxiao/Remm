/*
 * camera.c
 */

#include <opencv2/core/core_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include "camera.h"

typedef struct {
	CvCapture *capture;
	int image_length;
	unsigned char jpeg_image[MAX_JPEG_IMAGE_SIZE];
	int thread_aborted;
	pthread_t grabbing_thread;
	pthread_mutex_t image_copy_mutex;
} camera_ctx_s;

static camera_ctx_s camera_ctx;

void get_frame(unsigned char frame[MAX_JPEG_IMAGE_SIZE], int *size)
{
	pthread_mutex_lock(&camera_ctx.image_copy_mutex);
	*size = camera_ctx.image_length;
	if (*size > MAX_JPEG_IMAGE_SIZE) {
		printf("ERROR %s() Size of encoded image (%d) exceeded maximum buffer size (%d).\n",
				__FUNCTION__, *size, MAX_JPEG_IMAGE_SIZE);
		pthread_mutex_unlock(&camera_ctx.image_copy_mutex);
		return;
	}
	memcpy(frame, camera_ctx.jpeg_image, *size);
	pthread_mutex_unlock(&camera_ctx.image_copy_mutex);
}

void *grab_pictures(void *arg)
{
	IplImage* frame = 0;
	int counter=0;
	struct timeval tv;
	long long elapsed;
	int jpeg_params[] = { CV_IMWRITE_JPEG_QUALITY, 50, 0 };
	CvMat* encodedMat;

	printf("INFO  %s() Start capturing.\n", __FUNCTION__);
	while (camera_ctx.thread_aborted == 0) {
		cvWaitKey(100);

		gettimeofday(&tv, NULL);
		elapsed = tv.tv_sec * 1000000 + tv.tv_usec;
		frame = cvQueryFrame(camera_ctx.capture);
		if (frame == NULL) {
			printf("ERROR %s() Can't query frame from camera.\n", __FUNCTION__);
			break;
		}
		gettimeofday(&tv, NULL);
		elapsed = tv.tv_sec * 1000000 + tv.tv_usec - elapsed;
//		printf("INFO  %s() Captured %d. Elapsed %lld us.\n", __FUNCTION__, counter, elapsed);

		encodedMat = cvEncodeImage(".jpeg", frame, jpeg_params);
		if (encodedMat == NULL) {
			printf("ERROR %s() Can't encode frame.\n", __FUNCTION__);
			break;
		}
		if (encodedMat->cols > MAX_JPEG_IMAGE_SIZE) {
			printf("ERROR %s() Size of encoded image (%d) exceeded maximum buffer size (%d).\n",
					__FUNCTION__, encodedMat->cols, MAX_JPEG_IMAGE_SIZE);
			break;
		}

		pthread_mutex_lock(&camera_ctx.image_copy_mutex);
		camera_ctx.image_length = encodedMat->cols;
		memcpy(camera_ctx.jpeg_image, encodedMat->data.ptr, camera_ctx.image_length);
		pthread_mutex_unlock(&camera_ctx.image_copy_mutex);
		cvReleaseMat(&encodedMat);

		counter++;
	}
	printf("INFO  %s() Capturing is finished.\n", __FUNCTION__);
	camera_ctx.thread_aborted = 1;
	return 0;
}

int is_capture_aborted()
{
	return camera_ctx.thread_aborted;
}

void stop_capturing()
{
	camera_ctx.thread_aborted = 1;
}

int init_camera()
{
	double width, height;
	int ret;

	memset(&camera_ctx, 0x00, sizeof(camera_ctx));
	camera_ctx.thread_aborted = 1;
	camera_ctx.capture = cvCreateCameraCapture(CV_CAP_ANY); //cvCaptureFromCAM( 0 );
	if (camera_ctx.capture == NULL) {
		printf("ERROR %s() Can't open camera.\n", __FUNCTION__);
		return -1;
	}

	cvSetCaptureProperty(camera_ctx.capture, CV_CAP_PROP_FRAME_WIDTH, 320);	//	640x480, 320x240, 160x120
	cvSetCaptureProperty(camera_ctx.capture, CV_CAP_PROP_FRAME_HEIGHT, 240);
//	cvSetCaptureProperty(capture, CV_CAP_PROP_FPS, 30);

	width = cvGetCaptureProperty(camera_ctx.capture, CV_CAP_PROP_FRAME_WIDTH);
	height = cvGetCaptureProperty(camera_ctx.capture, CV_CAP_PROP_FRAME_HEIGHT);
	printf("INFO  %s() Frame size:%.0f x %.0f.\n", __FUNCTION__, width, height);

	ret = pthread_mutex_init(&camera_ctx.image_copy_mutex, NULL);
	if (ret != 0) {
		perror("Creating mutex on image copy");
		return -1;
	}
	ret = pthread_create(&camera_ctx.grabbing_thread, NULL, grab_pictures, NULL);
	if (ret != 0) {
		perror("Starting grabbing thread");
		return -1;
	}

	printf("INFO  %s() Camera is successfully inited.\n", __FUNCTION__);
	camera_ctx.thread_aborted = 0;
	return 0;
}

void release_camera(int signum)
{
	printf("INFO  %s() Release resources. signum=%d\n", __FUNCTION__, signum);
	stop_capturing();
	if (camera_ctx.capture == NULL)
		return;

	if (pthread_join(camera_ctx.grabbing_thread, NULL) != 0) {
		perror("Joining to grabbing_thread");
	}
	if (pthread_mutex_destroy(&camera_ctx.image_copy_mutex) != 0) {
		perror("Destroying mutex image_copy_mutex");
	}
	if (camera_ctx.capture != NULL) {
		cvReleaseCapture(&camera_ctx.capture);
		camera_ctx.capture = NULL;
	}
	printf("INFO  %s() Resources is released.\n", __FUNCTION__);
}
