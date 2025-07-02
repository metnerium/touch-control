#include "gesture_scroll_handler.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <cstring>  // для strerror
#include <cerrno>   // для errno

GestureScrollHandler::GestureScrollHandler() 
    : li_(nullptr), udev_(nullptr), fd_(-1), running_(false), verbose_(false) {
    scroll_emulator_.reset(new ScrollEmulator());  // Используем reset вместо make_unique для C++11
}

GestureScrollHandler::~GestureScrollHandler() {
    cleanup();
}

bool GestureScrollHandler::initialize() {
    if (verbose_) {
        std::cout << "=== Инициализация GestureScrollHandler ===" << std::endl;
    }
    
    // Инициализируем scroll emulator
    if (!scroll_emulator_->initialize()) {
        std::cerr << "Ошибка: не удалось инициализировать ScrollEmulator" << std::endl;
        return false;
    }
    
    if (verbose_) {
        std::cout << "✓ ScrollEmulator инициализирован: " << scroll_emulator_->getMethod() << std::endl;
    }
    
    // Создаем udev контекст
    udev_ = udev_new();
    if (!udev_) {
        std::cerr << "Ошибка: не удалось создать udev контекст" << std::endl;
        return false;
    }
    
    // Настройка интерфейса libinput
    static const struct libinput_interface interface = {
        .open_restricted = openRestricted,
        .close_restricted = closeRestricted,
    };
    
    // Создаем контекст libinput для udev
    li_ = libinput_udev_create_context(&interface, this, udev_);
    if (!li_) {
        std::cerr << "Ошибка: не удалось создать контекст libinput" << std::endl;
        std::cerr << "Возможные причины:" << std::endl;
        std::cerr << "  - Нет прав доступа к /dev/input/*" << std::endl;
        std::cerr << "  - Нужно добавить пользователя в группу input" << std::endl;
        std::cerr << "  - Или запустить с sudo" << std::endl;
        udev_unref(udev_);
        udev_ = nullptr;
        return false;
    }
    
    // Добавляем все устройства из текущего сеанса
    if (libinput_udev_assign_seat(li_, "seat0") != 0) {
        std::cerr << "Ошибка: не удалось назначить seat0" << std::endl;
        libinput_unref(li_);
        li_ = nullptr;
        return false;
    }
    
    fd_ = libinput_get_fd(li_);
    if (fd_ < 0) {
        std::cerr << "Ошибка: не удалось получить файловый дескриптор libinput" << std::endl;
        libinput_unref(li_);
        li_ = nullptr;
        return false;
    }
    
    if (verbose_) {
        std::cout << "✓ libinput инициализирован" << std::endl;
    }
    
    return true;
}

void GestureScrollHandler::cleanup() {
    running_ = false;
    
    if (li_) {
        libinput_unref(li_);
        li_ = nullptr;
    }
    
    if (udev_) {
        udev_unref(udev_);
        udev_ = nullptr;
    }
    
    if (scroll_emulator_) {
        scroll_emulator_.reset();
    }
}

void GestureScrollHandler::setScrollConfig(const ScrollEmulator::ScrollConfig& config) {
    if (scroll_emulator_) {
        scroll_emulator_->setConfig(config);
    }
}

void GestureScrollHandler::run() {
    if (!li_) {
        std::cerr << "Ошибка: обработчик не инициализирован" << std::endl;
        return;
    }
    
    running_ = true;
    
    if (verbose_) {
        std::cout << "Начинаем обработку жестов..." << std::endl;
        std::cout << "Используйте 3 пальца для скролла (Ctrl+C для выхода)" << std::endl;
    }
    
    struct pollfd fds;
    fds.fd = fd_;
    fds.events = POLLIN;
    
    while (running_) {
        int ret = poll(&fds, 1, 100); // Таймаут 100мс
        
        if (ret < 0) {
            if (errno == EINTR) {
                continue; // Прерывание сигналом
            }
            std::cerr << "Ошибка poll: " << strerror(errno) << std::endl;
            break;
        }
        
        if (ret > 0 && (fds.revents & POLLIN)) {
            processEvents();
        }
    }
    
    if (verbose_) {
        std::cout << "Обработка жестов завершена" << std::endl;
    }
}

void GestureScrollHandler::stop() {
    running_ = false;
}

void GestureScrollHandler::processEvents() {
    libinput_dispatch(li_);
    
    struct libinput_event *event;
    while ((event = libinput_get_event(li_))) {
        enum libinput_event_type type = libinput_event_get_type(event);
        
        switch (type) {
            case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN: {
                struct libinput_event_gesture *gesture = 
                    libinput_event_get_gesture_event(event);
                handleSwipeBegin(gesture);
                break;
            }
            
            case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE: {
                struct libinput_event_gesture *gesture = 
                    libinput_event_get_gesture_event(event);
                handleSwipeUpdate(gesture);
                break;
            }
            
            case LIBINPUT_EVENT_GESTURE_SWIPE_END: {
                struct libinput_event_gesture *gesture = 
                    libinput_event_get_gesture_event(event);
                handleSwipeEnd(gesture);
                break;
            }
            
            default:
                // Игнорируем другие события
                break;
        }
        
        libinput_event_destroy(event);
    }
}

void GestureScrollHandler::handleSwipeBegin(struct libinput_event_gesture* gesture) {
    gesture_state_.reset();
    gesture_state_.finger_count = libinput_event_gesture_get_finger_count(gesture);
    gesture_state_.gesture_start_time = std::chrono::steady_clock::now();
    
    // Обрабатываем только жесты с 3 пальцами
    if (gesture_state_.finger_count != 3) {
        return;
    }
    
    if (verbose_) {
        std::cout << "Начало жеста с " << gesture_state_.finger_count << " пальцами" << std::endl;
    }
}

void GestureScrollHandler::handleSwipeUpdate(struct libinput_event_gesture* gesture) {
    // Обрабатываем только жесты с 3 пальцами
    if (gesture_state_.finger_count != 3) {
        return;
    }
    
    // Получаем дельту движения (неускоренную)
    double delta_x = libinput_event_gesture_get_dx_unaccelerated(gesture);
    double delta_y = libinput_event_gesture_get_dy_unaccelerated(gesture);
    
    // Накапливаем общее движение
    gesture_state_.total_delta_x += delta_x;
    gesture_state_.total_delta_y += delta_y;
    
    // Сохраняем текущие дельты
    gesture_state_.last_delta_x = delta_x;
    gesture_state_.last_delta_y = delta_y;
    
    if (!gesture_state_.active) {
        // Проверяем, достигли ли мы порога для начала жеста
        double total_movement = std::sqrt(
            gesture_state_.total_delta_x * gesture_state_.total_delta_x +
            gesture_state_.total_delta_y * gesture_state_.total_delta_y
        );
        
        if (total_movement > GestureScrollState::START_THRESHOLD) {
            gesture_state_.active = true;
            gesture_state_.last_scroll_time = std::chrono::steady_clock::now();
            
            if (verbose_) {
                SwipeDirection dir = calculateDirection(
                    gesture_state_.total_delta_x, gesture_state_.total_delta_y);
                std::cout << "Жест активирован, направление: " << static_cast<int>(dir) << std::endl;
            }
        }
    }
    
    if (gesture_state_.active && shouldScroll()) {
        performSmoothScroll(delta_x, delta_y);
        gesture_state_.last_scroll_time = std::chrono::steady_clock::now();
    }
}

void GestureScrollHandler::handleSwipeEnd(struct libinput_event_gesture* gesture) {
    (void)gesture;  // Подавляем предупреждение о неиспользованном параметре
    
    if (gesture_state_.finger_count == 3 && gesture_state_.active) {
        if (verbose_) {
            std::cout << "Жест завершен" << std::endl;
        }
    }
    
    gesture_state_.reset();
}

SwipeDirection GestureScrollHandler::calculateDirection(double delta_x, double delta_y) {
    double abs_x = std::abs(delta_x);
    double abs_y = std::abs(delta_y);
    
    if (abs_x > abs_y) {
        return (delta_x > 0) ? SwipeDirection::RIGHT : SwipeDirection::LEFT;
    } else {
        return (delta_y > 0) ? SwipeDirection::DOWN : SwipeDirection::UP;
    }
}

void GestureScrollHandler::performSmoothScroll(double delta_x, double delta_y) {
    // Определяем основное направление движения
    double abs_x = std::abs(delta_x);
    double abs_y = std::abs(delta_y);
    
    // Вычисляем временную разность для адаптации скорости
    auto now = std::chrono::steady_clock::now();
    auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - gesture_state_.last_scroll_time).count();
    
    if (time_diff == 0) time_diff = 1; // Избегаем деления на ноль
    
    // Вертикальная прокрутка (приоритет)
    if (abs_y > GestureScrollState::SCROLL_THRESHOLD) {
        int intensity = calculateScrollIntensity(abs_y, time_diff);
        
        if (delta_y < 0) {
            // Движение вверх = скролл вверх
            scroll_emulator_->smoothScrollUp(intensity, 50);
            if (verbose_) {
                std::cout << "↑ Скролл вверх: " << intensity << std::endl;
            }
        } else {
            // Движение вниз = скролл вниз  
            scroll_emulator_->smoothScrollDown(intensity, 50);
            if (verbose_) {
                std::cout << "↓ Скролл вниз: " << intensity << std::endl;
            }
        }
    }
    // Горизонтальная прокрутка (если вертикальное движение меньше)
    else if (abs_x > GestureScrollState::SCROLL_THRESHOLD) {
        int intensity = calculateScrollIntensity(abs_x, time_diff);
        
        if (delta_x < 0) {
            // Движение влево = скролл влево
            scroll_emulator_->smoothScrollLeft(intensity, 50);
            if (verbose_) {
                std::cout << "← Скролл влево: " << intensity << std::endl;
            }
        } else {
            // Движение вправо = скролл вправо
            scroll_emulator_->smoothScrollRight(intensity, 50);
            if (verbose_) {
                std::cout << "→ Скролл вправо: " << intensity << std::endl;
            }
        }
    }
}

bool GestureScrollHandler::shouldScroll() {
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - gesture_state_.last_scroll_time).count();
    
    return time_since_last >= GestureScrollState::MIN_SCROLL_INTERVAL_MS;
}

int GestureScrollHandler::calculateScrollIntensity(double delta, double time_diff_ms) {
    // Базовая интенсивность на основе размера движения
    double base_intensity = std::abs(delta) / 10.0;
    
    // Корректировка на основе времени (чем быстрее, тем интенсивнее)
    double time_factor = 50.0 / std::max(1.0, static_cast<double>(time_diff_ms));
    
    // Комбинируем факторы
    double intensity = base_intensity * time_factor;
    
    // Ограничиваем диапазон
    return std::max(1, std::min(20, static_cast<int>(intensity)));
}

int GestureScrollHandler::openRestricted(const char* path, int flags, void* user_data) {
    int fd = open(path, flags);
    if (fd < 0) {
        GestureScrollHandler* handler = static_cast<GestureScrollHandler*>(user_data);
        if (handler && handler->verbose_) {
            std::cerr << "Не удалось открыть " << path << ": " << strerror(errno) << std::endl;
        }
    }
    return fd;
}

void GestureScrollHandler::closeRestricted(int fd, void* user_data) {
    (void)user_data;  // Подавляем предупреждение о неиспользованном параметре
    close(fd);
} 