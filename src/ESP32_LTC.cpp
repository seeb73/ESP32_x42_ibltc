#include "ESP32_x42_libltc.h"
#include "driver/ledc.h"
#include "driver/adc.h"

// --- Definicje konfiguracyjne ---
#define LTC_PWM_CHANNEL             LEDC_CHANNEL_0
#define LTC_PWM_TIMER               LEDC_TIMER_0
#define LTC_PWM_RESOLUTION          LEDC_TIMER_8_BIT
#define LTC_ADC_CHANNEL             ADC1_CHANNEL_0 // Domyślnie, zostanie zmienione w beginDecoder
#define LTC_TASK_STACK_SIZE         4096
#define LTC_TASK_PRIORITY           2

ESP32_LTC::ESP32_LTC() {}

ESP32_LTC::~ESP32_LTC() {
  stopEncoderTask();
  stopDecoderTask();
  if (_encoder) ltc_encoder_free(_encoder);
  if (_decoder) ltc_decoder_free(_decoder);
}

// =================================================================
//   IMPLEMENTACJA ENKODERA
// =================================================================

void ESP32_LTC::beginEncoder(int fps, uint8_t pwm_pin) {
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

void ESP32_LTC::setTimecode(const char* tc_string) {
  if (!_encoder) return;
  ltc_encoder_set_timecode_string(_encoder, tc_string);
}

void ESP32_LTC::runEncoderTask() {
  if (!_encoder || _task_handle != NULL) return;
  xTaskCreate(encoder_task_wrapper, "ltcEncoderTask", LTC_TASK_STACK_SIZE, this, LTC_TASK_PRIORITY, &_task_handle);
  Serial.println("LTC Encoder task started.");
}

void ESP32_LTC::stopEncoderTask() {
  if (_task_handle != NULL) {
    vTaskDelete(_task_handle);
    _task_handle = NULL;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LTC_PWM_CHANNEL, 128);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LTC_PWM_CHANNEL);
    Serial.println("LTC Encoder task stopped.");
  }
}

void ESP32_LTC::encoder_task_wrapper(void* pvParameters) {
  static_cast<ESP32_LTC*>(pvParameters)->encoder_task();
}

void ESP32_LTC::encoder_task() {
  size_t buffer_len = ltc_encoder_get_buf_len(_sample_rate, _fps);
  ltcsnd_sample_t* ltc_buffer = (ltcsnd_sample_t*) malloc(buffer_len);
  if (!ltc_buffer) {
    Serial.println("FATAL: Failed to allocate LTC encoder buffer!");
    vTaskDelete(NULL);
    return;
  }

  const long micros_per_sample = 1000000L / _sample_rate;
  unsigned long last_sample_time = micros();

  while (true) {
    ltc_encoder_buffer_frame(_encoder);
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

void ESP32_LTC::beginDecoder(int fps, uint8_t adc_pin) {
  _fps = fps;
  _pin = adc_pin;

  if (_decoder) ltc_decoder_free(_decoder);
  stopDecoderTask();

  // libltc zaleca kolejkę na kilka klatek, aby znaleźć synchronizację
  _decoder = ltc_decoder_create(_sample_rate, fps, 30.0);
  if (!_decoder) {
    Serial.println("ERROR: Failed to create LTC decoder!");
    return;
  }

  // Konfiguracja ADC
  adc1_channel_t channel = (adc1_channel_t)digitalPinToAnalogChannel(_pin);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(channel, ADC_ATTEN_DB_11);

  Serial.println("LTC Decoder initialized.");
}

void ESP32_LTC::runDecoderTask() {
  if (!_decoder || _task_handle != NULL) return;
  xTaskCreate(decoder_task_wrapper, "ltcDecoderTask", LTC_TASK_STACK_SIZE, this, LTC_TASK_PRIORITY, &_task_handle);
  Serial.println("LTC Decoder task started.");
}

void ESP32_LTC::stopDecoderTask() {
  if (_task_handle != NULL) {
    vTaskDelete(_task_handle);
    _task_handle = NULL;
    Serial.println("LTC Decoder task stopped.");
  }
}

bool ESP32_LTC::available() {
  if (_new_frame_available) {
    _new_frame_available = false;
    return true;
  }
  return false;
}

String ESP32_LTC::getTimecodeString() {
  return String(_decoded_tc_string);
}

void ESP32_LTC::decoder_task_wrapper(void* pvParameters) {
  static_cast<ESP32_LTC*>(pvParameters)->decoder_task();
}

void ESP32_LTC::decoder_task() {
  const long micros_per_sample = 1000000L / _sample_rate;
  unsigned long last_sample_time = micros();
  adc1_channel_t channel = (adc1_channel_t)digitalPinToAnalogChannel(_pin);

  while (true) {
    // 1. Odczyt próbki z ADC
    int adc_val = adc1_get_raw(channel);

    // 2. Konwersja próbki 12-bit (0-4095) na 8-bit signed (-128 do 127)
    // Zakładamy, że sygnał jest zbiasowany do 1.65V, czyli środek ADC to ~2048
    ltcsnd_sample_t sample = (ltcsnd_sample_t)((adc_val - 2048) / 16);

    // 3. Przekazanie próbki do dekodera libltc
    ltc_decoder_write_s8(_decoder, &sample, 1);

    // 4. Sprawdzenie, czy dekoder ma gotową ramkę (lub ramki)
    while (ltc_decoder_read(_decoder, &_smpte)) {
      smpte_timecode_to_string(&_smpte, _decoded_tc_string, 12);
      _new_frame_available = true;
    }

    // 5. Utrzymanie stałej częstotliwości próbkowania
    while (micros() - last_sample_time < micros_per_sample) {}
    last_sample_time += micros_per_sample;
  }
  vTaskDelete(NULL);
}
