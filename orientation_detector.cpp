#include "orientation_detector.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <vector>

// Заголовки для X11 (при наличии)
#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#endif

OrientationDetector::OrientationDetector() 
    : current_orientation_(ScreenOrientation::UNKNOWN)
    , monitoring_(false)
    , should_stop_(false)
    , verbose_(false)
    , screen_width_(0)
    , screen_height_(0) {
    last_update_ = std::chrono::steady_clock::now();
}

OrientationDetector::~OrientationDetector() {
    cleanup();
}

bool OrientationDetector::initialize() {
    // Получаем размеры экрана
    auto resolution = getScreenResolution();
    screen_width_ = resolution.first;
    screen_height_ = resolution.second;
    
    // Определяем текущую ориентацию
    current_orientation_ = updateOrientation();
    
    if (verbose_) {
        std::cout << "OrientationDetector инициализирован" << std::endl;
        std::cout << "Размер экрана: " << screen_width_ << "x" << screen_height_ << std::endl;
        std::cout << "Текущая ориентация: " << orientationToString(current_orientation_) << std::endl;
    }
    
    return current_orientation_ != ScreenOrientation::UNKNOWN;
}

void OrientationDetector::cleanup() {
    stopMonitoring();
}

ScreenOrientation OrientationDetector::getCurrentOrientation() {
    return current_orientation_;
}

ScreenOrientation OrientationDetector::updateOrientation() {
    ScreenOrientation new_orientation = ScreenOrientation::UNKNOWN;
    
    // Попытка определения через различные методы
    if (getenv("WAYLAND_DISPLAY")) {
        new_orientation = detectWaylandOrientation();
    }
    
    if (new_orientation == ScreenOrientation::UNKNOWN && getenv("DISPLAY")) {
        new_orientation = detectX11Orientation();
    }
    
    if (new_orientation == ScreenOrientation::UNKNOWN) {
        new_orientation = detectFromSysfs();
    }
    
    if (new_orientation == ScreenOrientation::UNKNOWN) {
        new_orientation = detectFromResolution();
    }
    
    // Обновляем время последнего обновления
    last_update_ = std::chrono::steady_clock::now();
    
    return new_orientation;
}

void OrientationDetector::setOrientationCallback(OrientationCallback callback) {
    callback_ = callback;
}

void OrientationDetector::startMonitoring() {
    if (monitoring_) {
        return;
    }
    
    monitoring_ = true;
    should_stop_ = false;
    
    monitor_thread_ = std::thread(&OrientationDetector::monitorOrientationChanges, this);
    
    if (verbose_) {
        std::cout << "Мониторинг ориентации запущен" << std::endl;
    }
}

void OrientationDetector::stopMonitoring() {
    if (!monitoring_) {
        return;
    }
    
    should_stop_ = true;
    monitoring_ = false;
    
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    
    if (verbose_) {
        std::cout << "Мониторинг ориентации остановлен" << std::endl;
    }
}

std::string OrientationDetector::orientationToString(ScreenOrientation orientation) {
    switch(orientation) {
        case ScreenOrientation::LANDSCAPE_0:   return "альбомная (0°)";
        case ScreenOrientation::PORTRAIT_90:   return "портретная (90°)";
        case ScreenOrientation::LANDSCAPE_180: return "альбомная перевернутая (180°)";
        case ScreenOrientation::PORTRAIT_270:  return "портретная перевернутая (270°)";
        default: return "неизвестная";
    }
}

int OrientationDetector::orientationToDegrees(ScreenOrientation orientation) {
    switch(orientation) {
        case ScreenOrientation::LANDSCAPE_0:   return 0;
        case ScreenOrientation::PORTRAIT_90:   return 90;
        case ScreenOrientation::LANDSCAPE_180: return 180;
        case ScreenOrientation::PORTRAIT_270:  return 270;
        default: return -1;
    }
}

bool OrientationDetector::isPortrait(ScreenOrientation orientation) {
    return orientation == ScreenOrientation::PORTRAIT_90 || 
           orientation == ScreenOrientation::PORTRAIT_270;
}

void OrientationDetector::transformCoordinates(ScreenOrientation orientation, 
                                              double& delta_x, double& delta_y,
                                              int screen_width, int screen_height) {
    // Сохраняем оригинальные значения
    double orig_x = delta_x;
    double orig_y = delta_y;
    
    // Применяем трансформацию в зависимости от ориентации
    switch(orientation) {
        case ScreenOrientation::LANDSCAPE_0:
            // 0° - координаты остаются как есть
            break;
            
        case ScreenOrientation::PORTRAIT_90:
            // 90° поворот по часовой: X становится Y, Y становится -X
            delta_x = orig_y;
            delta_y = -orig_x;
            break;
            
        case ScreenOrientation::LANDSCAPE_180:
            // 180° поворот: инвертируем обе координаты
            delta_x = -orig_x;
            delta_y = -orig_y;
            break;
            
        case ScreenOrientation::PORTRAIT_270:
            // 270° поворот по часовой: X становится -Y, Y становится X
            delta_x = -orig_y;
            delta_y = orig_x;
            break;
            
        default:
            // Неизвестная ориентация - оставляем как есть
            break;
    }
}

ScreenOrientation OrientationDetector::detectX11Orientation() {
#ifdef HAVE_X11
    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        if (verbose_) {
            std::cerr << "Не удалось открыть X11 дисплей" << std::endl;
        }
        return ScreenOrientation::UNKNOWN;
    }
    
    Window root = DefaultRootWindow(display);
    XRRScreenConfiguration *config = XRRGetScreenInfo(display, root);
    
    if (!config) {
        XCloseDisplay(display);
        return ScreenOrientation::UNKNOWN;
    }
    
    Rotation rotation;
    XRRConfigCurrentConfiguration(config, &rotation);
    
    XRRFreeScreenConfigInfo(config);
    XCloseDisplay(display);
    
    // Получаем также текущие размеры для определения базовой ориентации
    auto resolution = getScreenResolution();
    bool current_portrait = resolution.second > resolution.first;
    
    // Определяем базовую ориентацию (какая была бы при 0°)
    bool base_portrait = false;
    if (screen_width_ > 0 && screen_height_ > 0) {
        // Используем сохраненные размеры экрана
        base_portrait = screen_height_ > screen_width_;
    } else {
        // Предполагаем, что обычно экраны альбомные
        base_portrait = current_portrait;
    }
    
    // Преобразуем XRandR rotation в нашу систему
    switch(rotation & 0xf) {
        case RR_Rotate_0:   
            return base_portrait ? ScreenOrientation::PORTRAIT_90 : ScreenOrientation::LANDSCAPE_0;
        case RR_Rotate_90:  
            return base_portrait ? ScreenOrientation::LANDSCAPE_0 : ScreenOrientation::PORTRAIT_90;
        case RR_Rotate_180: 
            return base_portrait ? ScreenOrientation::PORTRAIT_270 : ScreenOrientation::LANDSCAPE_180;
        case RR_Rotate_270: 
            return base_portrait ? ScreenOrientation::LANDSCAPE_180 : ScreenOrientation::PORTRAIT_270;
        default:
            return ScreenOrientation::UNKNOWN;
    }
#else
    return ScreenOrientation::UNKNOWN;
#endif
}

ScreenOrientation OrientationDetector::detectWaylandOrientation() {
    // В Wayland ориентация сложнее определяется
    // Пока используем простое определение по размерам экрана
    return detectFromResolution();
}

ScreenOrientation OrientationDetector::detectFromSysfs() {
    // Попытка чтения ориентации из системных файлов
    int orientation = readOrientationFromSys();
    
    if (orientation >= 0 && orientation <= 3) {
        return static_cast<ScreenOrientation>(orientation);
    }
    
    return ScreenOrientation::UNKNOWN;
}

ScreenOrientation OrientationDetector::detectFromResolution() {
    auto resolution = getScreenResolution();
    
    if (resolution.first == 0 || resolution.second == 0) {
        return ScreenOrientation::UNKNOWN;
    }
    
    // Простое определение: если высота больше ширины, то портретная
    if (resolution.second > resolution.first) {
        return ScreenOrientation::PORTRAIT_90;
    } else {
        return ScreenOrientation::LANDSCAPE_0;
    }
}

std::pair<int, int> OrientationDetector::getScreenResolution() {
    // Попытка получить разрешение из /sys/class/drm
    std::vector<std::string> drm_paths = {
        "/sys/class/drm/card0/card0-eDP-1/modes",
        "/sys/class/drm/card0/card0-LVDS-1/modes", 
        "/sys/class/drm/card0/card0-DSI-1/modes",
        "/sys/class/drm/card0/card0-DP-1/modes",
        "/sys/class/drm/card0/card0-HDMI-A-1/modes"
    };
    
    for (const auto& path : drm_paths) {
        std::ifstream file(path);
        if (file.is_open()) {
            std::string mode;
            std::getline(file, mode);
            file.close();
            
            size_t x_pos = mode.find('x');
            if (x_pos != std::string::npos) {
                try {
                    int width = std::stoi(mode.substr(0, x_pos));
                    int height = std::stoi(mode.substr(x_pos + 1));
                    return std::make_pair(width, height);
                } catch (...) {
                    continue;
                }
            }
        }
    }
    
    // Fallback: через xrandr команду
    if (getenv("DISPLAY")) {
        FILE* pipe = popen("xrandr | grep '*' | awk '{print $1}' | head -1 2>/dev/null", "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe)) {
                std::string resolution_str(buffer);
                // Убираем символ новой строки
                resolution_str.erase(std::remove(resolution_str.begin(), resolution_str.end(), '\n'), resolution_str.end());
                
                size_t x_pos = resolution_str.find('x');
                if (x_pos != std::string::npos) {
                    try {
                        int width = std::stoi(resolution_str.substr(0, x_pos));
                        int height = std::stoi(resolution_str.substr(x_pos + 1));
                        pclose(pipe);
                        return std::make_pair(width, height);
                    } catch (...) {
                        // Ignore parse errors
                    }
                }
            }
            pclose(pipe);
        }
    }
    
    // Последний fallback: предположим стандартные размеры
    return std::make_pair(1920, 1080);
}

int OrientationDetector::readOrientationFromSys() {
    // Пути к файлам ориентации в sysfs
    std::vector<std::string> orientation_paths = {
        "/sys/class/input/input*/device/position",
        "/sys/devices/platform/lis3lv02d/position",
        "/sys/bus/iio/devices/iio:device*/in_accel_mount_matrix",
        "/sys/class/hwmon/hwmon*/device/position"
    };
    
    for (const auto& path_pattern : orientation_paths) {
        // Простое расширение wildcard
        if (path_pattern.find('*') != std::string::npos) {
            std::string base_path = path_pattern.substr(0, path_pattern.find('*'));
            // Попытка чтения из первого найденного файла
            std::string cmd = "find " + base_path + " -name \"" + 
                             path_pattern.substr(path_pattern.find('*') + 1) + "\" 2>/dev/null | head -1";
            
            FILE* pipe = popen(cmd.c_str(), "r");
            if (pipe) {
                char buffer[256];
                if (fgets(buffer, sizeof(buffer), pipe)) {
                    std::string found_path(buffer);
                    found_path.erase(std::remove(found_path.begin(), found_path.end(), '\n'), found_path.end());
                    
                    std::ifstream file(found_path);
                    if (file.is_open()) {
                        std::string content;
                        std::getline(file, content);
                        file.close();
                        pclose(pipe);
                        
                        try {
                            return std::stoi(content);
                        } catch (...) {
                            // Ignore parse errors
                        }
                    }
                }
                pclose(pipe);
            }
        } else {
            // Прямое чтение файла
            std::ifstream file(path_pattern);
            if (file.is_open()) {
                std::string content;
                std::getline(file, content);
                file.close();
                
                try {
                    return std::stoi(content);
                } catch (...) {
                    continue;
                }
            }
        }
    }
    
    return -1; // Не найдено
}

void OrientationDetector::monitorOrientationChanges() {
    while (!should_stop_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        if (should_stop_) break;
        
        ScreenOrientation new_orientation = updateOrientation();
        
        if (new_orientation != ScreenOrientation::UNKNOWN && 
            new_orientation != current_orientation_) {
            
            ScreenOrientation old_orientation = current_orientation_;
            current_orientation_ = new_orientation;
            
            if (verbose_) {
                std::cout << "Ориентация изменена с " << orientationToString(old_orientation) 
                          << " на " << orientationToString(new_orientation) << std::endl;
            }
            
            if (callback_) {
                callback_(old_orientation, new_orientation);
            }
        }
    }
} 