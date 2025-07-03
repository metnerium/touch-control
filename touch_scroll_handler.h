#ifndef TOUCH_SCROLL_HANDLER_H
#define TOUCH_SCROLL_HANDLER_H

#include <libinput.h>
#include <libudev.h>
#include <memory>
#include <chrono>
#include <unordered_map>
#include "scroll_emulator.h"

/**
 * Состояние touch жеста для отслеживания 3-пальцевого скролла на сенсорном экране
 */
struct TouchScrollState {
    bool active = false;
    int current_fingers = 0;
    int start_fingers = 0;
    std::chrono::steady_clock::time_point last_scroll_time;
    std::chrono::steady_clock::time_point gesture_start_time;
    
    // Позиции пальцев (slot -> координата)
    std::unordered_map<int32_t, double> start_x;
    std::unordered_map<int32_t, double> start_y;
    std::unordered_map<int32_t, double> current_x;
    std::unordered_map<int32_t, double> current_y;
    std::unordered_map<int32_t, double> previous_x;
    std::unordered_map<int32_t, double> previous_y;
    
    // Накопленные дельты для определения направления
    double total_delta_x = 0.0;
    double total_delta_y = 0.0;
    
    // Пороги для определения направления и начала скролла
    static constexpr double START_THRESHOLD = 15.0;  // Минимальное движение для начала (больше для touch)
    static constexpr double SCROLL_THRESHOLD = 3.0;  // Минимальное движение для продолжения скролла
    static constexpr int MIN_SCROLL_INTERVAL_MS = 20; // Минимальный интервал между скроллами
    
    void reset() {
        active = false;
        current_fingers = 0;
        start_fingers = 0;
        start_x.clear();
        start_y.clear();
        current_x.clear();
        current_y.clear();
        previous_x.clear();
        previous_y.clear();
        total_delta_x = 0.0;
        total_delta_y = 0.0;
    }
    
    // Вычисление среднего движения всех пальцев
    std::pair<double, double> getAverageDelta() const {
        if (start_x.empty() || current_x.empty()) {
            return std::make_pair(0.0, 0.0);
        }
        
        double delta_x = 0.0;
        double delta_y = 0.0;
        int count = 0;
        
        for (const auto& pair : start_x) {
            int32_t slot = pair.first;
            if (current_x.count(slot) && current_y.count(slot) && start_y.count(slot)) {
                delta_x += current_x.at(slot) - start_x.at(slot);
                delta_y += current_y.at(slot) - start_y.at(slot);
                count++;
            }
        }
        
        if (count > 0) {
            delta_x /= count;
            delta_y /= count;
        }
        
        return std::make_pair(delta_x, delta_y);
    }
};

/**
 * Направления touch жестов
 */
enum class TouchDirection {
    UNKNOWN = 0,
    UP = 1,
    DOWN = 2,
    LEFT = 3,
    RIGHT = 4
};

/**
 * Обработчик touch событий для плавной прокрутки на сенсорных экранах
 * Адаптирован для Plasma Mobile и других touch-устройств
 */
class TouchScrollHandler {
public:
    TouchScrollHandler();
    ~TouchScrollHandler();
    
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
    TouchScrollState touch_state_;
    
    /**
     * Обработка событий libinput
     */
    void processEvents();
    
    /**
     * Обработка нажатия пальца на экран
     */
    void handleTouchDown(struct libinput_event_touch* touch);
    
    /**
     * Обработка движения пальца по экрану
     */
    void handleTouchMotion(struct libinput_event_touch* touch);
    
    /**
     * Обработка отрыва пальца от экрана
     */
    void handleTouchUp(struct libinput_event_touch* touch);
    
    /**
     * Определение направления жеста
     */
    TouchDirection calculateDirection(double delta_x, double delta_y);
    
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
     * Вычисление среднего инкрементального движения всех активных пальцев
     */
    std::pair<double, double> getIncrementalAverageDelta();
    
    /**
     * Открытие libinput устройства
     */
    static int openRestricted(const char* path, int flags, void* user_data);
    
    /**
     * Закрытие libinput устройства
     */
    static void closeRestricted(int fd, void* user_data);
};

#endif // TOUCH_SCROLL_HANDLER_H 