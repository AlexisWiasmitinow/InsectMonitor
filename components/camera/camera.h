#ifndef _CAMERA_H_
#define _CAMERA_H_

#define IMAGE_FRAMESIZE FRAMESIZE_VGA

#if IMAGE_FRAMESIZE == FRAMESIZE_VGA

#define IMAGE_WIDTH 640
#define IMAGE_HEIGHT 480

#elif IMAGE_FRAMESIZE == FRAMESIZE_QVGA

#define IMAGE_WIDTH 320
#define IMAGE_HEIGHT 240

#endif

#define IMAGE_SIZE (IMAGE_WIDTH * IMAGE_HEIGHT)  // VGA

#include <esp_err.h>
#include "esp_camera.h"

esp_err_t camera_init(void);
void camera_release_buf(camera_fb_t *buf);
camera_fb_t *camera_take_pic(void);
esp_err_t camera_save_pic(const camera_fb_t *pic, char *path);
int camera_get_prev(camera_fb_t *pic);
int camera_get_diff_prev(const camera_fb_t *pic, int difference_threshold);
esp_err_t camera_crop_image(const camera_fb_t *pic, camera_fb_t *croppedPic,
                            uint16_t startX, uint16_t startY, uint16_t width,
                            uint16_t height);
void camera_image_free_buf(camera_fb_t *pic);

#endif  // _CAMERA_H_