#pragma once

#include <Arduino.h>

// Dołączamy oryginalny nagłówek libltc, opakowany w extern "C"
// To jest klucz do poprawnego linkowania kodu C z C++
extern "C" {
  #include "libltc/ltc.h"
}

class ESP32_LTC {
public:
  ESP32_LTC();
  ~ESP32_LTC();

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
  SMPTETimecode _smpte;

  // Specyficzne dla enkodera
  LTCEncoder* _encoder = nullptr;

  // Specyficzne dla dekodera
  LTCDecoder* _decoder = nullptr;
  volatile bool _new_frame_available = false;
  char _decoded_tc_string[12]; // "hh:mm:ss:ff" + null

  // Prywatne metody (wrappery i zadania RTOS)
  static void encoder_task_wrapper(void* pvParameters);
  void encoder_task();
  static void decoder_task_wrapper(void* pvParameters);
  void decoder_task();
};
