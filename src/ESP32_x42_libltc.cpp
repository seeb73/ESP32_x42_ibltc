#include "ESP32_x42_libltc.h"
#include "driver/ledc.h"
#include "driver/adc.h"

// --- Definicje konfiguracyjne ---
#define LTC_PWM_CHANNEL          LEDC_CHANNEL_0
#define LTC_PWM_TIMER            LEDC_TIMER_0
#define LTC_PWM_RESOLUTION       LEDC_TIMER_8_BIT
#define LTC_TASK_STACK_SIZE      4096
#define LTC_TASK_PRIORITY        2
#define LTC_BUFFER_SIZE          512

ESP32_x42_libltc::ESP32_x42_libltc() {}

ESP32_x42_libltc::~ESP32_x42_libltc() {
  stopEncoderTask();
  stopDecoderTask();
  if (_encoder) ltc_encoder_free(_encoder);
  if (_decoder) ltc_decoder_free(_decoder);
}

// =================================================================
//   IMPLEMENTACJA ENKODERA
// =================================================================

void ESP32_x42_libltc::beginEncoder(int fps, uint8_t pwm_pin) {
  _fps = fps;
  _pin = pwm_pin;
  if (_encoder) ltc_encoder_free(_encoder);
  stopEncoderTask();
  
  _encoder = ltc_encoder_create(_sample_rate, fps, LTC_TV_625_50, 0);
  if (!_encoder) {
    Serial.println("ERROR: Failed to create LTC encoder!");
    return;
  }

  const int pwm_freq = 187500;
  ledc_timer_config_t ledc_timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = LTC_PWM_RESOLUTION,
      .timer_num = LTC_PWM_TIMER,
      .freq_hz = pwm_freq,
      .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel = {
      .gpio_num = _pin,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = LTC_PWM_CHANNEL,
      .timer_sel = LTC_PWM_TIMER,
      .duty = 0,
      .hpoint = 0
  };
  ledc_channel_config(&ledc_channel);
  
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LTC_PWM_CHANNEL, 128);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LTC_PWM_CHANNEL);

  Serial.println("LTC Encoder initialized.");
}

void ESP32_x42_libltc::setTimecode(const char* tc_string) {
  if (!_encoder) return;
  
  SMPTETimecode tc;
  // Poprawione nazwy pól
  sscanf(tc_string, "%d:%d:%d:%d", &tc.hours, &tc.mins, &tc.secs, &tc.frame);
  
  ltc_encoder_set_timecode(_encoder, &tc);
}

void ESP32_x42_libltc::runEncoderTask() {
  if (!_encoder || _task_handle != NULL) return;
  xTaskCreate(encoder_task_wrapper, "ltcEncoderTask", LTC_TASK_STACK_SIZE, this, LTC_TASK_PRIORITY, &_task_handle);
  Serial.println("LTC Encoder task started.");
}

void ESP32_x42_libltc::stopEncoderTask() {
  if (_task_handle != NULL) {
    vTaskDelete(_task_handle);
    _task_handle = NULL;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LTC_PWM_CHANNEL, 128);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LTC_PWM_CHANNEL);
    Serial.println("LTC Encoder task stopped.");
  }
}

void ESP32_x42_libltc::encoder_task_wrapper(void* pvParameters) {
  static_cast<ESP32_x42_libltc*>(pvParameters)->encoder_task();
}

void ESP32_x42_libltc::encoder_task() {
  size_t buffer_len = (_sample_rate / _fps) * 80;
  ltcsnd_sample_t* ltc_buffer = (ltcsnd_sample_t*) malloc(buffer_len * sizeof(ltcsnd_sample_t));
  if (!ltc_buffer) {
    Serial.println("FATAL: Failed to allocate LTC encoder buffer!");
    vTaskDelete(NULL);
    return;
  }

  const long micros_per_sample = 1000000L / _sample_rate;
  unsigned long last_sample_time = micros();

  while (true) {
    ltc_encoder_get_buffer(_encoder, ltc_buffer);
    for (size_t i = 0; i < buffer_len; i++) {
      uint8_t pwm_duty = (uint8_t)(ltc_buffer[i] + 128);
      ledc_set_duty(LEDC_LOW_SPEED_MODE, LTC_PWM_CHANNEL, pwm_duty);
      ledc_update_duty(LEDC_LOW_SPEED_MODE, LTC_PWM_CHANNEL);
      while (micros() - last_sample_time < micros_per_sample) {}
      last_sample_time += micros_per_sample;
    }
    ltc_encoder_inc_timecode(_encoder);
  }
  free(ltc_buffer);
  vTaskDelete(NULL);
}

// =================================================================
//   IMPLEMENTACJA DEKODERA
// =================================================================

void ESP32_x42_libltc::beginDecoder(int fps, uint8_t adc_pin) {
  _fps = fps;
  _pin = adc_pin;
  if (_decoder) ltc_decoder_free(_decoder);
  stopDecoderTask();
  
  _decoder = ltc_decoder_create(_sample_rate / fps, 30);
  if (!_decoder) {
    Serial.println("ERROR: Failed to create LTC decoder!");
    return;
  }
  adc1_channel_t channel = (adc1_channel_t)digitalPinToAnalogChannel(_pin);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(channel, ADC_ATTEN_DB_11);
  Serial.println("LTC Decoder initialized.");
}

void ESP32_x42_libltc::runDecoderTask() {
  if (!_decoder || _task_handle != NULL) return;
  xTaskCreate(decoder_task_wrapper, "ltcDecoderTask", LTC_TASK_STACK_SIZE, this, LTC_TASK_PRIORITY, &_task_handle);
  Serial.println("LTC Decoder task started.");
}

void ESP32_x42_libltc::stopDecoderTask() {
  if (_task_handle != NULL) {
    vTaskDelete(_task_handle);
    _task_handle = NULL;
    Serial.println("LTC Decoder task stopped.");
  }
}

bool ESP32_x42_libltc::available() {
  if (_new_frame_available) {
    _new_frame_available = false;
    return true;
  }
  return false;
}

String ESP32_x42_libltc::getTimecodeString() {
  return String(_decoded_tc_string);
}

void ESP32_x42_libltc::decoder_task_wrapper(void* pvParameters) {
  static_cast<ESP32_x42_libltc*>(pvParameters)->decoder_task();
}

void ESP32_x42_libltc::decoder_task() {
  const long micros_per_sample = 1000000L / _sample_rate;
  unsigned long last_sample_time = micros();
  adc1_channel_t channel = (adc1_channel_t)digitalPinToAnalogChannel(_pin);

  while (true) {
    int adc_val = adc1_get_raw(channel);
    int16_t sample_s16 = (int16_t)((adc_val - 2048) * 16);

    ltc_decoder_write_s16(_decoder, &sample_s16, 1, 0);

    // BŁĄD W LINII PONIŻEJ ZOSTAŁ NAPRAWIONY
    // Dostęp do pól bezpośrednio przez _decoded_frame
    while (ltc_decoder_read(_decoder, &_decoded_frame)) {
      sprintf(_decoded_tc_string, "%02d:%02d:%02d:%02d",
              _decoded_frame.hours,
              _decoded_frame.minutes,
              _decoded_frame.seconds,
              _decoded_frame.frames);
      _new_frame_available = true;
    }

    while (micros() - last_sample_time < micros_per_sample) {}
    last_sample_time += micros_per_sample;
  }
  vTaskDelete(NULL);
}
