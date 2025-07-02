#include "scroll_emulator.h"
#include <iostream>
#include <string>
#include <getopt.h>
#include <cstdlib>
#include <unistd.h>

void printUsage(const char* program_name) {
    std::cout << "Использование: " << program_name << " [OPTIONS] COMMAND [ARGS]\n\n";

    std::cout << "КОМАНДЫ:\n";
    std::cout << "  up [STEPS]           Скролл вверх на STEPS шагов (по умолчанию 3)\n";
    std::cout << "  down [STEPS]         Скролл вниз на STEPS шагов (по умолчанию 3)\n";
    std::cout << "  left [STEPS]         Скролл влево на STEPS шагов (по умолчанию 3)\n";
    std::cout << "  right [STEPS]        Скролл вправо на STEPS шагов (по умолчанию 3)\n";
    std::cout << "  smooth-up DIST [DUR] Плавный скролл вверх на DIST за DUR мс (по умолчанию 1000мс)\n";
    std::cout << "  smooth-down DIST [DUR] Плавный скролл вниз на DIST за DUR мс\n";
    std::cout << "  smooth-left DIST [DUR] Плавный скролл влево на DIST за DUR мс\n";
    std::cout << "  smooth-right DIST [DUR] Плавный скролл вправо на DIST за DUR мс\n";
    std::cout << "  page-up              Page Up\n";
    std::cout << "  page-down            Page Down\n";
    std::cout << "  to-top               Скролл в начало документа\n";
    std::cout << "  to-bottom            Скролл в конец документа\n";
    std::cout << "  test                 Демонстрация всех функций\n";
    std::cout << "  info                 Информация о методе эмуляции\n\n";

    std::cout << "ОПЦИИ:\n";
    std::cout << "  -d, --delay DELAY    Задержка между шагами в мс (по умолчанию 50)\n";
    std::cout << "  -s, --smooth STEPS   Количество промежуточных шагов для плавности (по умолчанию 1)\n";
    std::cout << "  -a, --accel FACTOR   Ускорение для плавного скролла (1.0 = постоянная скорость)\n";
    std::cout << "  -v, --verbose        Подробный вывод\n";
    std::cout << "  -q, --quiet          Тихий режим\n";
    std::cout << "  -h, --help           Показать эту справку\n\n";

    std::cout << "ПРИМЕРЫ:\n";
    std::cout << "  " << program_name << " down 5                    # Скролл вниз на 5 шагов\n";
    std::cout << "  " << program_name << " -d 100 up 3               # Медленный скролл вверх\n";
    std::cout << "  " << program_name << " smooth-down 10 2000       # Плавный скролл вниз за 2 секунды\n";
    std::cout << "  " << program_name << " -s 5 -a 1.5 smooth-up 20  # Плавный скролл с ускорением\n";
    std::cout << "  " << program_name << " -v test                   # Демонстрация с подробным выводом\n\n";
}

int main(int argc, char* argv[]) {
    ScrollEmulator::ScrollConfig config;
    bool quiet = false;

    // Парсим опции командной строки
    static struct option long_options[] = {
        {"delay",    required_argument, 0, 'd'},
        {"smooth",   required_argument, 0, 's'},
        {"accel",    required_argument, 0, 'a'},
        {"verbose",  no_argument,       0, 'v'},
        {"quiet",    no_argument,       0, 'q'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, "d:s:a:vqh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'd':
                config.delay_ms = atoi(optarg);
                if (config.delay_ms < 1 || config.delay_ms > 5000) {
                    std::cerr << "Ошибка: задержка должна быть от 1 до 5000 мс" << std::endl;
                    return 1;
                }
                break;
            case 's':
                config.smooth_steps = atoi(optarg);
                if (config.smooth_steps < 1 || config.smooth_steps > 100) {
                    std::cerr << "Ошибка: количество шагов должно быть от 1 до 100" << std::endl;
                    return 1;
                }
                break;
            case 'a':
                config.acceleration = atof(optarg);
                if (config.acceleration < 0.1f || config.acceleration > 10.0f) {
                    std::cerr << "Ошибка: ускорение должно быть от 0.1 до 10.0" << std::endl;
                    return 1;
                }
                break;
            case 'v':
                config.verbose = true;
                break;
            case 'q':
                quiet = true;
                config.verbose = false;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            case '?':
                return 1;
            default:
                break;
        }
    }

    // Проверяем что есть команда
    if (optind >= argc) {
        if (!quiet) {
            std::cerr << "Ошибка: не указана команда" << std::endl;
            std::cerr << "Используйте --help для справки" << std::endl;
        }
        return 1;
    }

    std::string command = argv[optind];

    // Создаем и инициализируем эмулятор
    ScrollEmulator emulator;
    emulator.setConfig(config);

    if (!emulator.initialize()) {
        if (!quiet) {
            std::cerr << "Ошибка: не удалось инициализировать эмулятор скролла" << std::endl;
            std::cerr << "Попробуйте:" << std::endl;
            std::cerr << "1. Для X11: sudo apt install x11-xserver-utils" << std::endl;
            std::cerr << "2. Для Wayland: настройте права на uinput" << std::endl;
            std::cerr << "3. Запустите с sudo" << std::endl;
        }
        return 1;
    }

    if (config.verbose) {
        std::cout << "Используется метод: " << emulator.getMethod() << std::endl;
    }

    // Обрабатываем команды
    try {
        if (command == "up") {
            int steps = 3;
            if (optind + 1 < argc) {
                steps = atoi(argv[optind + 1]);
            }
            emulator.scrollUp(steps);

        } else if (command == "down") {
            int steps = 3;
            if (optind + 1 < argc) {
                steps = atoi(argv[optind + 1]);
            }
            emulator.scrollDown(steps);

        } else if (command == "left") {
            int steps = 3;
            if (optind + 1 < argc) {
                steps = atoi(argv[optind + 1]);
            }
            emulator.scrollLeft(steps);

        } else if (command == "right") {
            int steps = 3;
            if (optind + 1 < argc) {
                steps = atoi(argv[optind + 1]);
            }
            emulator.scrollRight(steps);

        } else if (command == "smooth-up") {
            if (optind + 1 >= argc) {
                std::cerr << "Ошибка: не указано расстояние для smooth-up" << std::endl;
                return 1;
            }
            int distance = atoi(argv[optind + 1]);
            int duration = 1000;
            if (optind + 2 < argc) {
                duration = atoi(argv[optind + 2]);
            }
            emulator.smoothScrollUp(distance, duration);

        } else if (command == "smooth-down") {
            if (optind + 1 >= argc) {
                std::cerr << "Ошибка: не указано расстояние для smooth-down" << std::endl;
                return 1;
            }
            int distance = atoi(argv[optind + 1]);
            int duration = 1000;
            if (optind + 2 < argc) {
                duration = atoi(argv[optind + 2]);
            }
            emulator.smoothScrollDown(distance, duration);

        } else if (command == "smooth-left") {
            if (optind + 1 >= argc) {
                std::cerr << "Ошибка: не указано расстояние для smooth-left" << std::endl;
                return 1;
            }
            int distance = atoi(argv[optind + 1]);
            int duration = 1000;
            if (optind + 2 < argc) {
                duration = atoi(argv[optind + 2]);
            }
            emulator.smoothScrollLeft(distance, duration);

        } else if (command == "smooth-right") {
            if (optind + 1 >= argc) {
                std::cerr << "Ошибка: не указано расстояние для smooth-right" << std::endl;
                return 1;
            }
            int distance = atoi(argv[optind + 1]);
            int duration = 1000;
            if (optind + 2 < argc) {
                duration = atoi(argv[optind + 2]);
            }
            emulator.smoothScrollRight(distance, duration);

        } else if (command == "page-up") {
            emulator.pageUp();

        } else if (command == "page-down") {
            emulator.pageDown();

        } else if (command == "to-top") {
            emulator.scrollToTop();

        } else if (command == "to-bottom") {
            emulator.scrollToBottom();

        } else if (command == "info") {
            std::cout << "Информация об эмуляторе скролла:" << std::endl;
            std::cout << "  Метод: " << emulator.getMethod() << std::endl;
            std::cout << "  Доступен: " << (emulator.isAvailable() ? "Да" : "Нет") << std::endl;
            std::cout << "  Задержка: " << config.delay_ms << " мс" << std::endl;
            std::cout << "  Плавность: " << config.smooth_steps << " шагов" << std::endl;
            std::cout << "  Ускорение: " << config.acceleration << std::endl;
            std::cout << "  Подробный режим: " << (config.verbose ? "Включен" : "Выключен") << std::endl;

        } else if (command == "test") {
            std::cout << "=== Тест эмулятора скролла ===" << std::endl;
            std::cout << "Метод: " << emulator.getMethod() << std::endl;
            std::cout << "\nОткройте приложение с прокручиваемым содержимым и нажмите Enter..." << std::endl;
            std::cin.get();

            std::cout << "\n1. Простой скролл вниз..." << std::endl;
            emulator.scrollDown(3);
            sleep(2);

            std::cout << "2. Простой скролл вверх..." << std::endl;
            emulator.scrollUp(2);
            sleep(2);

            std::cout << "3. Плавный скролл вниз..." << std::endl;
            emulator.smoothScrollDown(5, 2000);
            sleep(1);

            std::cout << "4. Плавный скролл вверх..." << std::endl;
            emulator.smoothScrollUp(3, 1500);
            sleep(1);

            std::cout << "5. Page Down..." << std::endl;
            emulator.pageDown();
            sleep(2);

            std::cout << "6. Page Up..." << std::endl;
            emulator.pageUp();
            sleep(1);

            std::cout << "7. Горизонтальный скролл..." << std::endl;
            emulator.scrollRight(2);
            sleep(1);
            emulator.scrollLeft(2);
            sleep(1);

            std::cout << "\nТест завершен!" << std::endl;

        } else {
            std::cerr << "Ошибка: неизвестная команда '" << command << "'" << std::endl;
            std::cerr << "Используйте --help для списка команд" << std::endl;
            return 1;
        }

    } catch (const std::exception& e) {
        if (!quiet) {
            std::cerr << "Ошибка выполнения: " << e.what() << std::endl;
        }
        return 1;
    }

    return 0;
}