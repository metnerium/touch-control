#include "touch_scroll_handler.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <cstring>  // для strerror
#include <cerrno>   // для errno

TouchScrollHandler::TouchScrollHandler() 
    : li_(nullptr), udev_(nullptr), fd_(-1), running_(false), verbose_(false) {
    scroll_emulator_.reset(new ScrollEmulator());  // Используем reset вместо make_unique для C++11
}

TouchScrollHandler::~TouchScrollHandler() {
    cleanup();
}

bool TouchScrollHandler::initialize() {
    if (verbose_) {
        std::cout << "=== Инициализация TouchScrollHandler ===" << std::endl;
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
        udev_unref(udev_);
        udev_ = nullptr;
        return false;
    }
    
    fd_ = libinput_get_fd(li_);
    if (fd_ < 0) {
        std::cerr << "Ошибка: не удалось получить файловый дескриптор libinput" << std::endl;
        libinput_unref(li_);
        li_ = nullptr;
        udev_unref(udev_);
        udev_ = nullptr;
        return false;
    }
    
    if (verbose_) {
        std::cout << "✓ libinput инициализирован для сенсорных экранов" << std::endl;
    }
    
    return true;
}

void TouchScrollHandler::cleanup() {
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

void TouchScrollHandler::setScrollConfig(const ScrollEmulator::ScrollConfig& config) {
    if (scroll_emulator_) {
        scroll_emulator_->setConfig(config);
    }
}

void TouchScrollHandler::run() {
    if (!li_) {
        std::cerr << "Ошибка: обработчик не инициализирован" << std::endl;
        return;
    }
    
    running_ = true;
    
    if (verbose_) {
        std::cout << "Начинаем обработку touch жестов..." << std::endl;
        std::cout << "Используйте 3 пальца на сенсорном экране для скролла (Ctrl+C для выхода)" << std::endl;
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
        std::cout << "Обработка touch жестов завершена" << std::endl;
    }
}

void TouchScrollHandler::stop() {
    running_ = false;
}

void TouchScrollHandler::processEvents() {
    libinput_dispatch(li_);
    
    struct libinput_event *event;
    while ((event = libinput_get_event(li_))) {
        enum libinput_event_type type = libinput_event_get_type(event);
        
        switch (type) {
            case LIBINPUT_EVENT_TOUCH_DOWN: {
                struct libinput_event_touch *touch = 
                    libinput_event_get_touch_event(event);
                handleTouchDown(touch);
                break;
            }
            
            case LIBINPUT_EVENT_TOUCH_MOTION: {
                struct libinput_event_touch *touch = 
                    libinput_event_get_touch_event(event);
                handleTouchMotion(touch);
                break;
            }
            
            case LIBINPUT_EVENT_TOUCH_UP:
            case LIBINPUT_EVENT_TOUCH_CANCEL: {
                struct libinput_event_touch *touch = 
                    libinput_event_get_touch_event(event);
                handleTouchUp(touch);
                break;
            }
            
            default:
                // Игнорируем другие события
                break;
        }
        
        libinput_event_destroy(event);
    }
}

void TouchScrollHandler::handleTouchDown(struct libinput_event_touch* touch) {
    touch_state_.current_fingers++;
    
    int32_t slot = libinput_event_touch_get_slot(touch);
    double x = libinput_event_touch_get_x(touch);
    double y = libinput_event_touch_get_y(touch);
    
    touch_state_.start_x[slot] = x;
    touch_state_.start_y[slot] = y;
    touch_state_.current_x[slot] = x;
    touch_state_.current_y[slot] = y;
    
    // Сохраняем количество пальцев при начале жеста
    if (touch_state_.current_fingers == 1) {
        touch_state_.gesture_start_time = std::chrono::steady_clock::now();
        touch_state_.total_delta_x = 0.0;
        touch_state_.total_delta_y = 0.0;
    }
    
    if (verbose_ && touch_state_.current_fingers == 3) {
        std::cout << "Началось касание 3 пальцами на экране" << std::endl;
    }
}

void TouchScrollHandler::handleTouchMotion(struct libinput_event_touch* touch) {
    // Обрабатываем только жесты с 3 пальцами
    if (touch_state_.current_fingers != 3) {
        return;
    }
    
    int32_t slot = libinput_event_touch_get_slot(touch);
    double x = libinput_event_touch_get_x(touch);
    double y = libinput_event_touch_get_y(touch);
    
    // Сохраняем предыдущую позицию
    if (touch_state_.current_x.count(slot)) {
        touch_state_.previous_x[slot] = touch_state_.current_x[slot];
    } else {
        touch_state_.previous_x[slot] = x; // При первом движении
    }
    
    if (touch_state_.current_y.count(slot)) {
        touch_state_.previous_y[slot] = touch_state_.current_y[slot];
    } else {
        touch_state_.previous_y[slot] = y; // При первом движении
    }
    
    // Обновляем текущую позицию
    touch_state_.current_x[slot] = x;
    touch_state_.current_y[slot] = y;
    
    // Вычисляем инкрементальную дельту движения для этого слота
    double incremental_delta_x = x - touch_state_.previous_x[slot];
    double incremental_delta_y = y - touch_state_.previous_y[slot];
    
    // Накапливаем общее движение (ИСПРАВЛЕНИЕ: += вместо =)
    touch_state_.total_delta_x += incremental_delta_x;
    touch_state_.total_delta_y += incremental_delta_y;
    
    if (!touch_state_.active) {
        // Проверяем, достигли ли мы порога для начала жеста
        double total_movement = std::sqrt(
            touch_state_.total_delta_x * touch_state_.total_delta_x +
            touch_state_.total_delta_y * touch_state_.total_delta_y
        );
        
        if (total_movement > TouchScrollState::START_THRESHOLD) {
            touch_state_.active = true;
            touch_state_.start_fingers = touch_state_.current_fingers;
            touch_state_.last_scroll_time = std::chrono::steady_clock::now();
            
            if (verbose_) {
                TouchDirection dir = calculateDirection(
                    touch_state_.total_delta_x, touch_state_.total_delta_y);
                std::cout << "Touch жест активирован, направление: " << static_cast<int>(dir) << std::endl;
            }
        }
    }
    
    if (touch_state_.active && shouldScroll()) {
        // ИСПРАВЛЕНИЕ: используем инкрементальные дельты для плавного скролла
        // Вычисляем среднее инкрементальное движение всех активных пальцев
        std::pair<double, double> avg_delta_pair = getIncrementalAverageDelta();
        double avg_incremental_x = avg_delta_pair.first;
        double avg_incremental_y = avg_delta_pair.second;
        
        // Для touch экранов инвертируем Y-координату (touch обычно инвертирован относительно скролла)
        performSmoothScroll(avg_incremental_x / 10.0, -avg_incremental_y / 10.0); // Инвертируем Y
        touch_state_.last_scroll_time = std::chrono::steady_clock::now();
    }
}

void TouchScrollHandler::handleTouchUp(struct libinput_event_touch* touch) {
    touch_state_.current_fingers--;
    
    int32_t slot = libinput_event_touch_get_slot(touch);
    
    if (touch_state_.current_fingers == 0) {
        if (touch_state_.active && verbose_) {
            std::cout << "Touch жест завершен" << std::endl;
        }
        touch_state_.reset();
    }
    
    // Удаляем данные о этом слоте
    touch_state_.start_x.erase(slot);
    touch_state_.start_y.erase(slot);
    touch_state_.current_x.erase(slot);
    touch_state_.current_y.erase(slot);
    touch_state_.previous_x.erase(slot);
    touch_state_.previous_y.erase(slot);
}

TouchDirection TouchScrollHandler::calculateDirection(double delta_x, double delta_y) {
    double abs_x = std::abs(delta_x);
    double abs_y = std::abs(delta_y);
    
    if (abs_x > abs_y) {
        return (delta_x > 0) ? TouchDirection::RIGHT : TouchDirection::LEFT;
    } else {
        return (delta_y > 0) ? TouchDirection::DOWN : TouchDirection::UP;
    }
}

void TouchScrollHandler::performSmoothScroll(double delta_x, double delta_y) {
    // Определяем основное направление движения
    double abs_x = std::abs(delta_x);
    double abs_y = std::abs(delta_y);
    
    // Вычисляем временную разность для адаптации скорости
    auto now = std::chrono::steady_clock::now();
    auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - touch_state_.last_scroll_time).count();
    
    if (time_diff == 0) time_diff = 1; // Избегаем деления на ноль
    
    // Вертикальная прокрутка (приоритет)
    if (abs_y > TouchScrollState::SCROLL_THRESHOLD) {
        int intensity = calculateScrollIntensity(abs_y, time_diff);
        
        if (delta_y < 0) {
            // Движение вверх = скролл вверх
            scroll_emulator_->smoothScrollUp(intensity, 30);
            if (verbose_) {
                std::cout << "↑ Touch скролл вверх: " << intensity << std::endl;
            }
        } else {
            // Движение вниз = скролл вниз  
            scroll_emulator_->smoothScrollDown(intensity, 30);
            if (verbose_) {
                std::cout << "↓ Touch скролл вниз: " << intensity << std::endl;
            }
        }
    }
    // Горизонтальная прокрутка (если вертикальное движение меньше)
    else if (abs_x > TouchScrollState::SCROLL_THRESHOLD) {
        int intensity = calculateScrollIntensity(abs_x, time_diff);
        
        if (delta_x < 0) {
            // Движение влево = скролл влево
            scroll_emulator_->smoothScrollLeft(intensity, 30);
            if (verbose_) {
                std::cout << "← Touch скролл влево: " << intensity << std::endl;
            }
        } else {
            // Движение вправо = скролл вправо
            scroll_emulator_->smoothScrollRight(intensity, 30);
            if (verbose_) {
                std::cout << "→ Touch скролл вправо: " << intensity << std::endl;
            }
        }
    }
}

bool TouchScrollHandler::shouldScroll() {
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - touch_state_.last_scroll_time).count();
    
    return time_since_last >= TouchScrollState::MIN_SCROLL_INTERVAL_MS;
}

int TouchScrollHandler::calculateScrollIntensity(double delta, double time_diff_ms) {
    // Базовая интенсивность на основе размера движения (для touch экранов другая шкала)
    double base_intensity = std::abs(delta) / 5.0;
    
    // Корректировка на основе времени (чем быстрее, тем интенсивнее)
    double time_factor = 40.0 / std::max(1.0, static_cast<double>(time_diff_ms));
    
    // Комбинируем факторы
    double intensity = base_intensity * time_factor;
    
    // Ограничиваем диапазон для touch экранов
    return std::max(1, std::min(15, static_cast<int>(intensity)));
}

std::pair<double, double> TouchScrollHandler::getIncrementalAverageDelta() {
    if (touch_state_.current_x.empty() || touch_state_.previous_x.empty()) {
        return std::make_pair(0.0, 0.0);
    }
    
    double total_delta_x = 0.0;
    double total_delta_y = 0.0;
    int count = 0;
    
    // Вычисляем инкрементальную дельту для каждого активного пальца
    for (const auto& pair : touch_state_.current_x) {
        int32_t slot = pair.first;
        if (touch_state_.previous_x.count(slot) && 
            touch_state_.current_y.count(slot) && 
            touch_state_.previous_y.count(slot)) {
            
            double delta_x = touch_state_.current_x[slot] - touch_state_.previous_x[slot];
            double delta_y = touch_state_.current_y[slot] - touch_state_.previous_y[slot];
            
            total_delta_x += delta_x;
            total_delta_y += delta_y;
            count++;
        }
    }
    
    if (count > 0) {
        total_delta_x /= count;
        total_delta_y /= count;
    }
    
    return std::make_pair(total_delta_x, total_delta_y);
}

int TouchScrollHandler::openRestricted(const char* path, int flags, void* user_data) {
    int fd = open(path, flags);
    if (fd < 0) {
        TouchScrollHandler* handler = static_cast<TouchScrollHandler*>(user_data);
        if (handler && handler->verbose_) {
            std::cerr << "Не удалось открыть " << path << ": " << strerror(errno) << std::endl;
        }
    }
    return fd;
}

void TouchScrollHandler::closeRestricted(int fd, void* user_data) {
    (void)user_data;  // Подавляем предупреждение о неиспользованном параметре
    close(fd);
} 