#include "gesture_scroll_handler.h"
#include <iostream>
#include <csignal>
#include <getopt.h>
#include <cstdlib>
#include <unistd.h>

// Глобальная переменная для обработчика жестов
GestureScrollHandler* g_handler = nullptr;

void signalHandler(int signal) {
    if (g_handler) {
        std::cout << "\nПолучен сигнал " << signal << ", завершаем работу..." << std::endl;
        g_handler->stop();
    }
}

void printUsage(const char* program_name) {
    std::cout << "Использование: " << program_name << " [OPTIONS]\n\n";
    
    std::cout << "ОПИСАНИЕ:\n";
    std::cout << "  Демон для обработки 3-пальцевых swipe жестов с плавной прокруткой\n";
    std::cout << "  Основан на интеграции touchegg (libinput) + touch-control (scroll emulator)\n\n";
    
    std::cout << "ЖЕСТЫ:\n";
    std::cout << "  3 пальца вверх      Плавная прокрутка вверх\n";
    std::cout << "  3 пальца вниз       Плавная прокрутка вниз\n";
    std::cout << "  3 пальца влево      Плавная прокрутка влево (горизонтальная)\n";
    std::cout << "  3 пальца вправо     Плавная прокрутка вправо (горизонтальная)\n\n";
    
    std::cout << "ОПЦИИ:\n";
    std::cout << "  -d, --delay DELAY        Задержка между шагами скролла в мс (по умолчанию 50)\n";
    std::cout << "  -s, --smooth STEPS       Количество промежуточных шагов для плавности (по умолчанию 1)\n";
    std::cout << "  -a, --accel FACTOR       Ускорение для плавного скролла (1.0 = постоянная скорость)\n";
    std::cout << "  -v, --verbose            Подробный вывод (показывать обнаруженные жесты)\n";
    std::cout << "  -q, --quiet              Тихий режим (минимальный вывод)\n";
    std::cout << "  -h, --help               Показать эту справку\n";
    std::cout << "      --test               Режим тестирования (показать информацию о системе)\n";
    std::cout << "      --daemon             Запустить как демон (фоновый процесс)\n\n";
    
    std::cout << "ПРИМЕРЫ:\n";
    std::cout << "  " << program_name << "                              # Запуск с настройками по умолчанию\n";
    std::cout << "  " << program_name << " -v                           # Запуск с подробным выводом\n";
    std::cout << "  " << program_name << " -d 30 -s 3                   # Быстрый и плавный скролл\n";
    std::cout << "  " << program_name << " -a 1.5 --verbose             # С ускорением и отладкой\n";
    std::cout << "  " << program_name << " --test                       # Проверить совместимость системы\n";
    std::cout << "  " << program_name << " --daemon -q                  # Запуск в фоне\n\n";
    
    std::cout << "ТРЕБОВАНИЯ:\n";
    std::cout << "  - Linux с поддержкой libinput\n";
    std::cout << "  - Доступ к /dev/input/* (обычно требует группу input)\n";
    std::cout << "  - Wayland или X11 для эмуляции скролла\n";
    std::cout << "  - Тачпад с поддержкой 3+ пальцев\n\n";
    
    std::cout << "УПРАВЛЕНИЕ:\n";
    std::cout << "  Ctrl+C или SIGTERM для остановки\n";
    std::cout << "  В режиме демона используйте: killall " << program_name << "\n\n";
}

void printSystemInfo() {
    std::cout << "=== Информация о системе ===" << std::endl;
    
    // Проверяем окружение
    const char* wayland_display = getenv("WAYLAND_DISPLAY");
    const char* x11_display = getenv("DISPLAY");
    
    std::cout << "Графическое окружение:" << std::endl;
    if (wayland_display) {
        std::cout << "  ✓ Wayland: " << wayland_display << std::endl;
    }
    if (x11_display) {
        std::cout << "  ✓ X11: " << x11_display << std::endl;
    }
    if (!wayland_display && !x11_display) {
        std::cout << "  ✗ Не обнаружено графическое окружение" << std::endl;
    }
    
    // Проверяем права доступа к input устройствам
    std::cout << "\nДоступ к устройствам ввода:" << std::endl;
    int ret = system("ls -la /dev/input/event* 2>/dev/null | head -3");
    if (ret != 0) {
        std::cout << "  ✗ Нет доступа к /dev/input/*" << std::endl;
        std::cout << "    Попробуйте: sudo usermod -a -G input $USER" << std::endl;
        std::cout << "    Затем перелогиньтесь" << std::endl;
    }
    
    // Проверяем libinput
    std::cout << "\nБиблиотеки:" << std::endl;
    ret = system("pkg-config --exists libinput 2>/dev/null");
    if (ret == 0) {
        std::cout << "  ✓ libinput доступен" << std::endl;
        system("pkg-config --modversion libinput 2>/dev/null | sed 's/^/    Версия: /'");
    } else {
        std::cout << "  ✗ libinput не найден" << std::endl;
        std::cout << "    Установите: sudo apt install libinput-dev" << std::endl;
    }
    
    ret = system("pkg-config --exists libudev 2>/dev/null");
    if (ret == 0) {
        std::cout << "  ✓ libudev доступен" << std::endl;
        system("pkg-config --modversion libudev 2>/dev/null | sed 's/^/    Версия: /'");
    } else {
        std::cout << "  ✗ libudev не найден" << std::endl;
        std::cout << "    Установите: sudo apt install libudev-dev" << std::endl;
    }
    
    // Тестируем ScrollEmulator
    std::cout << "\nТестирование ScrollEmulator:" << std::endl;
    ScrollEmulator emulator;
    ScrollEmulator::ScrollConfig config;
    config.verbose = true;
    emulator.setConfig(config);
    
    if (emulator.initialize()) {
        std::cout << "  ✓ ScrollEmulator инициализирован: " << emulator.getMethod() << std::endl;
    } else {
        std::cout << "  ✗ ScrollEmulator не удалось инициализировать" << std::endl;
    }
    
    std::cout << "\n=== Конец информации ===" << std::endl;
}

int main(int argc, char* argv[]) {
    ScrollEmulator::ScrollConfig config;
    bool verbose = false;
    bool quiet = false;
    bool test_mode = false;
    bool daemon_mode = false;
    
    // Парсим опции командной строки
    static struct option long_options[] = {
        {"delay",    required_argument, 0, 'd'},
        {"smooth",   required_argument, 0, 's'},
        {"accel",    required_argument, 0, 'a'},
        {"verbose",  no_argument,       0, 'v'},
        {"quiet",    no_argument,       0, 'q'},
        {"help",     no_argument,       0, 'h'},
        {"test",     no_argument,       0, 't'},
        {"daemon",   no_argument,       0, 'D'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "d:s:a:vqhtD", long_options, &option_index)) != -1) {
        switch (c) {
            case 'd':
                config.delay_ms = atoi(optarg);
                if (config.delay_ms < 1 || config.delay_ms > 1000) {
                    std::cerr << "Ошибка: задержка должна быть от 1 до 1000 мс" << std::endl;
                    return 1;
                }
                break;
            case 's':
                config.smooth_steps = atoi(optarg);
                if (config.smooth_steps < 1 || config.smooth_steps > 50) {
                    std::cerr << "Ошибка: количество шагов должно быть от 1 до 50" << std::endl;
                    return 1;
                }
                break;
            case 'a':
                config.acceleration = atof(optarg);
                if (config.acceleration < 0.1f || config.acceleration > 5.0f) {
                    std::cerr << "Ошибка: ускорение должно быть от 0.1 до 5.0" << std::endl;
                    return 1;
                }
                break;
            case 'v':
                verbose = true;
                config.verbose = true;
                break;
            case 'q':
                quiet = true;
                config.verbose = false;
                verbose = false;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            case 't':
                test_mode = true;
                break;
            case 'D':
                daemon_mode = true;
                break;
            case '?':
                return 1;
            default:
                break;
        }
    }
    
    // Режим тестирования
    if (test_mode) {
        printSystemInfo();
        return 0;
    }
    
    // Проверяем права доступа
    if (geteuid() != 0) {
        // Проверяем принадлежность к группе input
        int ret = system("groups | grep -q input");
        if (ret != 0 && !quiet) {
            std::cout << "Предупреждение: для работы может потребоваться добавление в группу input:" << std::endl;
            std::cout << "  sudo usermod -a -G input $USER" << std::endl;
            std::cout << "  (затем перелогиньтесь)" << std::endl;
            std::cout << "Или запустите с sudo" << std::endl << std::endl;
        }
    }
    
    // Режим демона
    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "Ошибка при создании демона" << std::endl;
            return 1;
        }
        if (pid > 0) {
            // Родительский процесс завершается
            if (!quiet) {
                std::cout << "Демон запущен с PID: " << pid << std::endl;
            }
            return 0;
        }
        // Дочерний процесс продолжает как демон
        setsid();
        quiet = true; // В режиме демона работаем тихо
    }
    
    if (!quiet) {
        std::cout << "=== Gesture Scroll Daemon ===" << std::endl;
        std::cout << "Интеграция touchegg + touch-control для плавной прокрутки" << std::endl;
        std::cout << "Настройки: задержка=" << config.delay_ms << "мс, шаги=" << config.smooth_steps;
        std::cout << ", ускорение=" << config.acceleration << std::endl << std::endl;
    }
    
    // Создаем и инициализируем обработчик жестов
    GestureScrollHandler handler;
    g_handler = &handler;
    
    // Настраиваем обработку сигналов
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    handler.setVerbose(verbose);
    handler.setScrollConfig(config);
    
    if (!handler.initialize()) {
        if (!quiet) {
            std::cerr << "Ошибка: не удалось инициализировать обработчик жестов" << std::endl;
            std::cerr << "Попробуйте:" << std::endl;
            std::cerr << "1. Запустить с sudo" << std::endl;
            std::cerr << "2. Добавить пользователя в группу input: sudo usermod -a -G input $USER" << std::endl;
            std::cerr << "3. Проверить что libinput установлен" << std::endl;
            std::cerr << "4. Использовать --test для диагностики" << std::endl;
        }
        return 1;
    }
    
    if (!quiet) {
        std::cout << "Обработчик жестов готов!" << std::endl;
        std::cout << "Используйте 3 пальца на тачпаде для прокрутки" << std::endl;
        if (!daemon_mode) {
            std::cout << "Нажмите Ctrl+C для выхода" << std::endl << std::endl;
        }
    }
    
    // Запускаем основной цикл
    handler.run();
    
    if (!quiet) {
        std::cout << "Завершение работы..." << std::endl;
    }
    
    return 0;
} 