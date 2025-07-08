#ifndef ORIENTATION_DETECTOR_H
#define ORIENTATION_DETECTOR_H

#include <string>
#include <functional>
#include <chrono>
#include <thread>
#include <atomic>

/**
 * Ориентация экрана в четыре стороны
 */
enum class ScreenOrientation {
    LANDSCAPE_0 = 0,    // 0° - обычная альбомная
    PORTRAIT_90 = 1,    // 90° - портретная (поворот по часовой)
    LANDSCAPE_180 = 2,  // 180° - перевернутая альбомная
    PORTRAIT_270 = 3,   // 270° - портретная (поворот против часовой)
    UNKNOWN = 255       // Неизвестная ориентация
};

/**
 * Детектор ориентации экрана с мониторингом изменений
 */
class OrientationDetector {
public:
    // Callback для уведомления об изменении ориентации
    using OrientationCallback = std::function<void(ScreenOrientation, ScreenOrientation)>;
    
    OrientationDetector();
    ~OrientationDetector();
    
    /**
     * Инициализация детектора
     */
    bool initialize();
    
    /**
     * Очистка ресурсов
     */
    void cleanup();
    
    /**
     * Получение текущей ориентации
     */
    ScreenOrientation getCurrentOrientation();
    
    /**
     * Принудительное обновление ориентации
     */
    ScreenOrientation updateOrientation();
    
    /**
     * Установка callback для отслеживания изменений
     */
    void setOrientationCallback(OrientationCallback callback);
    
    /**
     * Запуск мониторинга изменений ориентации
     */
    void startMonitoring();
    
    /**
     * Остановка мониторинга
     */
    void stopMonitoring();
    
    /**
     * Включить/отключить подробный вывод
     */
    void setVerbose(bool verbose) { verbose_ = verbose; }
    
    /**
     * Преобразование ориентации в строку
     */
    static std::string orientationToString(ScreenOrientation orientation);
    
    /**
     * Получение градусов поворота
     */
    static int orientationToDegrees(ScreenOrientation orientation);
    
    /**
     * Проверка, является ли ориентация портретной
     */
    static bool isPortrait(ScreenOrientation orientation);
    
    /**
     * Применение трансформации координат для данной ориентации
     */
    static void transformCoordinates(ScreenOrientation orientation, 
                                   double& delta_x, double& delta_y,
                                   int screen_width = 0, int screen_height = 0);

private:
    ScreenOrientation current_orientation_;
    OrientationCallback callback_;
    bool monitoring_;
    std::atomic<bool> should_stop_;
    std::thread monitor_thread_;
    bool verbose_;
    
    // Кэшированные размеры экрана
    int screen_width_;
    int screen_height_;
    std::chrono::steady_clock::time_point last_update_;
    
    // Методы определения ориентации для разных систем
    ScreenOrientation detectX11Orientation();
    ScreenOrientation detectWaylandOrientation();
    ScreenOrientation detectFromResolution();
    ScreenOrientation detectFromSysfs();
    
    // Мониторинг изменений в отдельном потоке
    void monitorOrientationChanges();
    
    // Получение разрешения экрана
    std::pair<int, int> getScreenResolution();
    
    // Чтение ориентации из системных файлов
    int readOrientationFromSys();
    
    // Проверка изменений через периодический опрос
    void pollOrientationChanges();
};

#endif // ORIENTATION_DETECTOR_H 