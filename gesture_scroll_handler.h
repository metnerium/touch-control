#ifndef GESTURE_SCROLL_HANDLER_H
#define GESTURE_SCROLL_HANDLER_H

#include <libinput.h>
#include <libudev.h>
#include <memory>
#include <chrono>
#include "scroll_emulator.h"

/**
 * Состояние жеста для отслеживания swipe с 3 пальцами
 */
struct GestureScrollState {
    bool active = false;
    double total_delta_x = 0.0;
    double total_delta_y = 0.0;
    double last_delta_x = 0.0;
    double last_delta_y = 0.0;
    int finger_count = 0;
    std::chrono::steady_clock::time_point last_scroll_time;
    std::chrono::steady_clock::time_point gesture_start_time;
    
    // Пороги для определения направления и начала скролла
    static constexpr double START_THRESHOLD = 10.0;  // Минимальное движение для начала
    static constexpr double SCROLL_THRESHOLD = 2.0;  // Минимальное движение для продолжения скролла
    static constexpr int MIN_SCROLL_INTERVAL_MS = 16; // Минимальный интервал между скроллами (60 FPS)
    
    void reset() {
        active = false;
        total_delta_x = 0.0;
        total_delta_y = 0.0;
        last_delta_x = 0.0;
        last_delta_y = 0.0;
        finger_count = 0;
    }
};

/**
 * Направления жестов
 */
enum class SwipeDirection {
    UNKNOWN = 0,
    UP = 1,
    DOWN = 2,
    LEFT = 3,
    RIGHT = 4
};

/**
 * Обработчик жестов для плавной прокрутки
 * Основан на libinput swipe handler из touchegg, но адаптирован для непрерывной прокрутки
 */
class GestureScrollHandler {
public:
    GestureScrollHandler();
    ~GestureScrollHandler();
    
    /**
     * Инициализация libinput и scroll emulator
     */
    bool initialize();
    
    /**
     * Очистка ресурсов
     */
    void cleanup();
    
    /**
     * Основной цикл обработки событий
     */
    void run();
    
    /**
     * Остановка обработки
     */
    void stop();
    
    /**
     * Настройки прокрутки
     */
    void setScrollConfig(const ScrollEmulator::ScrollConfig& config);
    
    /**
     * Включить/отключить подробный вывод
     */
    void setVerbose(bool verbose) { verbose_ = verbose; }

private:
    struct libinput* li_;
    struct udev* udev_;
    int fd_;
    bool running_;
    bool verbose_;
    
    std::unique_ptr<ScrollEmulator> scroll_emulator_;
    GestureScrollState gesture_state_;
    
    /**
     * Обработка событий libinput
     */
    void processEvents();
    
    /**
     * Обработка начала swipe жеста
     */
    void handleSwipeBegin(struct libinput_event_gesture* gesture);
    
    /**
     * Обработка обновления swipe жеста
     */
    void handleSwipeUpdate(struct libinput_event_gesture* gesture);
    
    /**
     * Обработка завершения swipe жеста
     */
    void handleSwipeEnd(struct libinput_event_gesture* gesture);
    
    /**
     * Определение направления жеста
     */
    SwipeDirection calculateDirection(double delta_x, double delta_y);
    
    /**
     * Выполнение плавной прокрутки на основе дельты движения
     */
    void performSmoothScroll(double delta_x, double delta_y);
    
    /**
     * Проверка, прошло ли достаточно времени для следующего скролла
     */
    bool shouldScroll();
    
    /**
     * Вычисление интенсивности скролла на основе скорости жеста
     */
    int calculateScrollIntensity(double delta, double time_diff_ms);
    
    /**
     * Открытие libinput устройства
     */
    static int openRestricted(const char* path, int flags, void* user_data);
    
    /**
     * Закрытие libinput устройства
     */
    static void closeRestricted(int fd, void* user_data);
};

#endif // GESTURE_SCROLL_HANDLER_H 