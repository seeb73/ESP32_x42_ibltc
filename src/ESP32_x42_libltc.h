#pragma once

#include <Arduino.h>

extern "C" {
  #include "ltc.h"
}

// Zmieniona nazwa klasy dla lepszej percepcji
class ESP32_x42_libltc {
public:
  ESP32_x42_libltc();
  ~ESP32_x42_libltc();

  // --- Metody Enkodera ---
  void beginEncoder(int fps, uint8_t pwm_pin);
  void setTimecode(const char* tc_string);
  void runEncoderTask();
  void stopEncoderTask();

  // --- Metody Dekodera ---
  void beginDecoder(int fps, uint8_t adc_pin);
  void runDecoderTask();
  void stopDecoderTask();
  bool available();
  String getTimecodeString();

private:
  // Wspólne
  int _fps;
  int _sample_rate = 48000;
  uint8_t _pin;
  TaskHandle_t _task_handle = NULL;
  
  // Zmieniono typ, aby pasował do LTCFrameExt
  LTCFrameExt _decoded_frame;
  volatile bool _new_frame_available = false;
  char _decoded_tc_string[12];

  // Specyficzne dla enkodera
  LTCEncoder* _encoder = nullptr;

  // Specyficzne dla dekodera
  LTCDecoder* _decoder = nullptr;

  // Prywatne metody (wrappery i zadania RTOS)
  static void encoder_task_wrapper(void* pvParameters);
  void encoder_task();
  static void decoder_task_wrapper(void* pvParameters);
  void decoder_task();
};
