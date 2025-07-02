#include "touch_scroll_handler.h"
#include "scroll_emulator.h"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <getopt.h>
#include <string>

// Глобальные переменные для обработчика сигналов
static TouchScrollHandler* g_handler = nullptr;
static bool g_running = true;

// Обработчик сигналов для корректного завершения
void signalHandler(int signal) {
    std::cout << std::endl << "Получен сигнал " << signal << ", завершаем работу..." << std::endl;
    g_running = false;
    if (g_handler) {
        g_handler->stop();
    }
}

void printUsage(const char* program_name) {
    std::cout << "Использование: " << program_name << " [опции]" << std::endl;
    std::cout << std::endl;
    std::cout << "Touch Scroll Daemon - плавная прокрутка 3 пальцами на сенсорном экране" << std::endl;
    std::cout << "Адаптирован для Plasma Mobile и других touch-устройств" << std::endl;
    std::cout << std::endl;
    std::cout << "Опции:" << std::endl;
    std::cout << "  -h, --help          Показать эту справку" << std::endl;
    std::cout << "  -v, --verbose       Подробный вывод событий" << std::endl;
    std::cout << "  -d, --daemon        Запуск в фоновом режиме" << std::endl;
    std::cout << "  --delay MS          Задержка между скроллами (по умолчанию 30мс)" << std::endl;
    std::cout << "  --steps N           Количество шагов для плавной прокрутки (по умолчанию 3)" << std::endl;
    std::cout << "  --accel FLOAT       Ускорение прокрутки (по умолчанию 1.2)" << std::endl;
    std::cout << "  --test              Тестовый режим с пробными командами прокрутки" << std::endl;
    std::cout << std::endl;
    std::cout << "Примеры:" << std::endl;
    std::cout << "  " << program_name << " -v                      # С подробным выводом" << std::endl;
    std::cout << "  " << program_name << " --daemon                # В фоновом режиме" << std::endl;
    std::cout << "  " << program_name << " --delay 20 --steps 5    # Настроенная конфигурация" << std::endl;
    std::cout << "  " << program_name << " --test                  # Тестирование системы" << std::endl;
    std::cout << std::endl;
    std::cout << "Жесты:" << std::endl;
    std::cout << "  - Касание 3 пальцами + движение по экрану = плавная прокрутка" << std::endl;
    std::cout << "  - Движение вверх/вниз = вертикальная прокрутка" << std::endl;
    std::cout << "  - Движение влево/вправо = горизонтальная прокрутка" << std::endl;
    std::cout << std::endl;
    std::cout << "Остановка: Ctrl+C" << std::endl;
}

void runTests() {
    std::cout << "=== Тестирование Touch Scroll System ===" << std::endl;
    
    // Тест 1: Инициализация ScrollEmulator
    std::cout << "1. Тестирование ScrollEmulator..." << std::endl;
    ScrollEmulator emulator;
    if (emulator.initialize()) {
        std::cout << "   ✓ ScrollEmulator инициализирован: " << emulator.getMethod() << std::endl;
        
        // Тест базовой прокрутки
        std::cout << "   → Тест прокрутки вниз..." << std::endl;
        emulator.smoothScrollDown(3, 100);
        usleep(200000); // 200мс
        
        std::cout << "   → Тест прокрутки вверх..." << std::endl;
        emulator.smoothScrollUp(3, 100);
        usleep(200000);
        
        std::cout << "   ✓ Базовая прокрутка работает" << std::endl;
    } else {
        std::cout << "   ✗ Ошибка инициализации ScrollEmulator" << std::endl;
        return;
    }
    
    // Тест 2: Инициализация TouchScrollHandler
    std::cout << "2. Тестирование TouchScrollHandler..." << std::endl;
    TouchScrollHandler handler;
    if (handler.initialize()) {
        std::cout << "   ✓ TouchScrollHandler инициализирован" << std::endl;
        handler.cleanup();
    } else {
        std::cout << "   ✗ Ошибка инициализации TouchScrollHandler" << std::endl;
        std::cout << "   Возможные причины:" << std::endl;
        std::cout << "     - Нет прав доступа к touch устройствам" << std::endl;
        std::cout << "     - Отсутствуют сенсорные экраны в системе" << std::endl;
        std::cout << "     - Нужен запуск с sudo или в группе input" << std::endl;
    }
    
    std::cout << "=== Тестирование завершено ===" << std::endl;
}

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool daemon_mode = false;
    bool test_mode = false;
    int delay_ms = 30;
    int steps = 3;
    double acceleration = 1.2;
    
    // Парсинг аргументов командной строки
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"daemon", no_argument, 0, 'd'},
        {"delay", required_argument, 0, 0},
        {"steps", required_argument, 0, 1},
        {"accel", required_argument, 0, 2},
        {"test", no_argument, 0, 3},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    while ((c = getopt_long(argc, argv, "hvd", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                printUsage(argv[0]);
                return 0;
            case 'v':
                verbose = true;
                break;
            case 'd':
                daemon_mode = true;
                break;
            case 0: // --delay
                delay_ms = std::stoi(optarg);
                break;
            case 1: // --steps
                steps = std::stoi(optarg);
                break;
            case 2: // --accel
                acceleration = std::stod(optarg);
                break;
            case 3: // --test
                test_mode = true;
                break;
            case '?':
                std::cerr << "Неизвестная опция. Используйте --help для справки." << std::endl;
                return 1;
            default:
                return 1;
        }
    }
    
    // Тестовый режим
    if (test_mode) {
        runTests();
        return 0;
    }
    
    // Фоновый режим
    if (daemon_mode) {
        std::cout << "Запуск в фоновом режиме..." << std::endl;
        if (daemon(0, 0) != 0) {
            std::cerr << "Ошибка перехода в фоновый режим" << std::endl;
            return 1;
        }
        verbose = false; // Отключаем verbose в daemon режиме
    }
    
    // Настройка обработчиков сигналов
    signal(SIGINT, signalHandler);   // Ctrl+C
    signal(SIGTERM, signalHandler);  // Terminate
    signal(SIGHUP, signalHandler);   // Hangup
    
    if (!daemon_mode) {
        std::cout << "=== Touch Scroll Daemon ===" << std::endl;
        std::cout << "Плавная прокрутка 3 пальцами на сенсорном экране" << std::endl;
        std::cout << "Адаптирован для Plasma Mobile" << std::endl;
        std::cout << "Настройки: задержка=" << delay_ms << "мс, шаги=" << steps 
                  << ", ускорение=" << acceleration << std::endl;
        std::cout << std::endl;
    }
    
    // Инициализация обработчика touch событий
    TouchScrollHandler handler;
    g_handler = &handler;
    
    // Настройка verbose режима
    handler.setVerbose(verbose);
    
    // Настройка параметров прокрутки
    ScrollEmulator::ScrollConfig config;
    config.delay_ms = delay_ms;
    config.smooth_steps = steps;
    config.acceleration = static_cast<float>(acceleration);
    handler.setScrollConfig(config);
    
    // Инициализация
    if (!handler.initialize()) {
        std::cerr << "Ошибка: не удалось инициализировать touch обработчик" << std::endl;
        std::cerr << "Попробуйте:" << std::endl;
        std::cerr << "  1. sudo usermod -a -G input $USER  # Добавить в группу input" << std::endl;
        std::cerr << "  2. sudo " << argv[0] << "  # Или запустить с sudo" << std::endl;
        std::cerr << "  3. " << argv[0] << " --test  # Проверить систему" << std::endl;
        return 1;
    }
    
    if (!daemon_mode) {
        std::cout << "Обработчик touch жестов готов!" << std::endl;
        std::cout << "Используйте 3 пальца на сенсорном экране для прокрутки" << std::endl;
        std::cout << "Нажмите Ctrl+C для выхода" << std::endl;
        std::cout << std::endl;
    }
    
    // Основной цикл обработки событий
    try {
        handler.run();
    } catch (const std::exception& e) {
        std::cerr << "Ошибка во время работы: " << e.what() << std::endl;
        return 1;
    }
    
    if (!daemon_mode) {
        std::cout << "Завершение работы..." << std::endl;
    }
    
    // Очистка
    handler.cleanup();
    g_handler = nullptr;
    
    return 0;
} 