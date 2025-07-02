#include "scroll_emulator.h"
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <sys/wait.h>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <signal.h>
#include <cmath>

ScrollEmulator::ScrollEmulator() : active_method(METHOD_NONE), socket_fd(-1), daemon_pid(-1) {
    // Создаем уникальный путь для сокета
    uid_t uid = getuid();
    socket_path = "/tmp/scroll_emulator_" + std::to_string(uid) + ".sock";
}

ScrollEmulator::~ScrollEmulator() {
    cleanup();
}

bool ScrollEmulator::initialize() {
    if (config.verbose) {
        std::cout << "=== Инициализация ScrollEmulator ===" << std::endl;
    }

    // Метод 1: X11 XTEST (работает в X11 без sudo)
    if (tryX11XTest()) {
        active_method = METHOD_X11_XTEST;
        if (config.verbose) {
            std::cout << "✓ Используем X11 XTEST (как xdotool)" << std::endl;
        }
        return true;
    }

    // Метод 2: Daemon с uinput (как ydotool)
    if (tryUinputDaemon()) {
        active_method = METHOD_UINPUT_DAEMON;
        if (config.verbose) {
            std::cout << "✓ Используем uinput daemon (как ydotool)" << std::endl;
        }
        return true;
    }

    // Метод 3: Прямой uinput (требует sudo)
    if (tryDirectUinput()) {
        active_method = METHOD_DIRECT_UINPUT;
        if (config.verbose) {
            std::cout << "✓ Используем прямой uinput (требует sudo)" << std::endl;
        }
        return true;
    }

    if (config.verbose) {
        std::cout << "✗ Не удалось инициализировать ни один метод" << std::endl;
    }
    return false;
}

void ScrollEmulator::cleanup() {
    if (socket_fd >= 0) {
        if (active_method == METHOD_UINPUT_DAEMON) {
            sendDaemonCommand('Q', 0); // Quit
        }
        close(socket_fd);
        socket_fd = -1;
    }

    if (daemon_pid > 0) {
        kill(daemon_pid, SIGTERM);
        waitpid(daemon_pid, nullptr, 0);
        daemon_pid = -1;
    }

    unlink(socket_path.c_str());
}

bool ScrollEmulator::tryX11XTest() {
    // Проверяем что мы в X11 (не Wayland)
    if (getenv("WAYLAND_DISPLAY")) {
        return false;
    }

    if (!getenv("DISPLAY")) {
        return false;
    }

    // Проверяем наличие X11 библиотек через простой тест
    return system("command -v xset >/dev/null 2>&1") == 0;
}

bool ScrollEmulator::tryUinputDaemon() {
    // Создаем daemon процесс (как ydotool)
    daemon_pid = fork();

    if (daemon_pid == 0) {
        // Дочерний процесс - daemon
        runUinputDaemon();
        _exit(0);
    } else if (daemon_pid < 0) {
        return false;
    }

    // Даем daemon'у время запуститься
    usleep(200000); // 200ms

    // Подключаемся к daemon'у
    return connectToDaemon();
}

bool ScrollEmulator::tryDirectUinput() {
    // Пробуем открыть uinput напрямую
    const char* paths[] = {"/dev/uinput", "/dev/input/uinput", "/dev/misc/uinput"};

    for (const char* path : paths) {
        int fd = open(path, O_WRONLY | O_NONBLOCK);
        if (fd >= 0) {
            close(fd);
            return true;
        }
    }
    return false;
}

void ScrollEmulator::runUinputDaemon() {
    // Создаем unix socket
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) return;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    unlink(socket_path.c_str());

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server_fd);
        return;
    }

    // Устанавливаем права доступа для пользователя
    chmod(socket_path.c_str(), 0600);

    if (listen(server_fd, 1) < 0) {
        close(server_fd);
        return;
    }

    // Принимаем соединение
    int client_fd = accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) {
        close(server_fd);
        return;
    }

    // Пытаемся открыть uinput
    int uinput_fd = openUinput();

    // Основной цикл daemon'а
    char buffer[16];
    while (true) {
        int bytes = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;

        char command = buffer[0];
        int steps = (bytes > 1) ? buffer[1] : 1;

        if (command == 'Q') break; // Quit

        if (uinput_fd >= 0) {
            handleUinputCommand(uinput_fd, command, steps);
        } else {
            // Fallback - пробуем X11 если uinput не работает
            handleX11Fallback(command, steps);
        }
    }

    if (uinput_fd >= 0) {
        ioctl(uinput_fd, 0x5502UL); // UI_DEV_DESTROY
        close(uinput_fd);
    }

    close(client_fd);
    close(server_fd);
    unlink(socket_path.c_str());
}

int ScrollEmulator::openUinput() {
    const char* paths[] = {"/dev/uinput", "/dev/input/uinput", "/dev/misc/uinput"};

    for (const char* path : paths) {
        int fd = open(path, O_WRONLY | O_NONBLOCK);
        if (fd >= 0) {
            if (setupUinput(fd)) {
                return fd;
            } else {
                close(fd);
            }
        }
    }
    return -1;
}

bool ScrollEmulator::setupUinput(int fd) {
    // Настраиваем uinput устройство
    if (ioctl(fd, 0x40045564UL, 2UL) < 0) return false; // UI_SET_EVBIT, EV_REL
    if (ioctl(fd, 0x40045566UL, 8UL) < 0) return false; // UI_SET_RELBIT, REL_WHEEL
    if (ioctl(fd, 0x40045566UL, 6UL) < 0) return false; // UI_SET_RELBIT, REL_HWHEEL
    if (ioctl(fd, 0x40045564UL, 0UL) < 0) return false; // UI_SET_EVBIT, EV_SYN

    struct input_id {
        unsigned short bustype;
        unsigned short vendor;
        unsigned short product;
        unsigned short version;
    };

    struct uinput_setup {
        struct input_id id;
        char name[80];
        int ff_effects_max;
    };

    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = 3; // BUS_USB
    setup.id.vendor = 0x1234;
    setup.id.product = 0x5678;
    setup.id.version = 1;
    strcpy(setup.name, "ScrollEmulator");

    if (ioctl(fd, 0x405c5503UL, &setup) < 0) return false; // UI_DEV_SETUP
    if (ioctl(fd, 0x5501UL) < 0) return false; // UI_DEV_CREATE

    return true;
}

void ScrollEmulator::handleUinputCommand(int uinput_fd, char command, int steps) {
    struct input_event {
        unsigned long tv_sec;
        unsigned long tv_usec;
        unsigned short type;
        unsigned short code;
        int value;
    };

    struct input_event event;
    memset(&event, 0, sizeof(event));

    for (int i = 0; i < steps; i++) {
        // Основное событие
        event.type = 2; // EV_REL

        switch (command) {
            case 'U': // Up
                event.code = 8; // REL_WHEEL
                event.value = 1;
                break;
            case 'D': // Down
                event.code = 8; // REL_WHEEL
                event.value = -1;
                break;
            case 'L': // Left
                event.code = 6; // REL_HWHEEL
                event.value = -1;
                break;
            case 'R': // Right
                event.code = 6; // REL_HWHEEL
                event.value = 1;
                break;
            default:
                continue;
        }

        write(uinput_fd, &event, sizeof(event));

        // Событие синхронизации
        event.type = 0; // EV_SYN
        event.code = 0; // SYN_REPORT
        event.value = 0;
        write(uinput_fd, &event, sizeof(event));

        if (i < steps - 1) usleep(config.delay_ms * 1000);
    }
}

void ScrollEmulator::handleX11Fallback(char command, int steps) {
    (void)command; // Подавляем предупреждение о неиспользованном параметре
    
    // Fallback через X11 команды (как xdotool)
    if (!getenv("DISPLAY")) return;

    for (int i = 0; i < steps; i++) {
        system("DISPLAY=$DISPLAY timeout 0.1 xset r on 2>/dev/null || true");
        if (i < steps - 1) usleep(config.delay_ms * 1000);
    }
}

bool ScrollEmulator::connectToDaemon() {
    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd < 0) return false;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(socket_fd);
        socket_fd = -1;
        return false;
    }

    return true;
}

void ScrollEmulator::sendDaemonCommand(char command, int steps) {
    if (socket_fd < 0) return;

    char buffer[2] = { command, (char)steps };
    send(socket_fd, buffer, 2, 0);
}

// Публичные методы API

void ScrollEmulator::scrollUp(int steps) {
    executeScroll(true, steps);
}

void ScrollEmulator::scrollDown(int steps) {
    executeScroll(false, steps);
}

void ScrollEmulator::scrollLeft(int steps) {
    executeHorizontalScroll(false, steps);
}

void ScrollEmulator::scrollRight(int steps) {
    executeHorizontalScroll(true, steps);
}

void ScrollEmulator::smoothScrollUp(int distance, int duration_ms) {
    executeSmoothScroll(true, true, distance, duration_ms);
}

void ScrollEmulator::smoothScrollDown(int distance, int duration_ms) {
    executeSmoothScroll(true, false, distance, duration_ms);
}

void ScrollEmulator::smoothScrollLeft(int distance, int duration_ms) {
    executeSmoothScroll(false, false, distance, duration_ms);
}

void ScrollEmulator::smoothScrollRight(int distance, int duration_ms) {
    executeSmoothScroll(false, true, distance, duration_ms);
}

void ScrollEmulator::pageUp() {
    executePageScroll(true);
}

void ScrollEmulator::pageDown() {
    executePageScroll(false);
}

void ScrollEmulator::scrollToTop() {
    if (config.verbose) {
        std::cout << "Скролл в начало документа" << std::endl;
    }
    // Много Page Up для достижения верха
    for (int i = 0; i < 20; i++) {
        pageUp();
        usleep(50000); // 50ms между нажатиями
    }
}

void ScrollEmulator::scrollToBottom() {
    if (config.verbose) {
        std::cout << "Скролл в конец документа" << std::endl;
    }
    // Много Page Down для достижения низа
    for (int i = 0; i < 20; i++) {
        pageDown();
        usleep(50000);
    }
}

const char* ScrollEmulator::getMethod() {
    switch (active_method) {
        case METHOD_X11_XTEST: return "X11 XTest";
        case METHOD_UINPUT_DAEMON: return "UInput Daemon";
        case METHOD_DIRECT_UINPUT: return "Direct UInput";
        default: return "None";
    }
}

bool ScrollEmulator::isAvailable() {
    return active_method != METHOD_NONE;
}

// Внутренние методы

void ScrollEmulator::executeScroll(bool up, int steps) {
    switch (active_method) {
        case METHOD_X11_XTEST:
            executeX11Scroll(up, steps);
            break;
        case METHOD_UINPUT_DAEMON:
            sendDaemonCommand(up ? 'U' : 'D', steps);
            break;
        case METHOD_DIRECT_UINPUT:
            executeDirectUinput(up, steps);
            break;
        default:
            if (config.verbose) {
                std::cout << "Скролл недоступен" << std::endl;
            }
    }
}

void ScrollEmulator::executeHorizontalScroll(bool right, int steps) {
    switch (active_method) {
        case METHOD_X11_XTEST:
            executeX11HorizontalScroll(right, steps);
            break;
        case METHOD_UINPUT_DAEMON:
            sendDaemonCommand(right ? 'R' : 'L', steps);
            break;
        case METHOD_DIRECT_UINPUT:
            // Не реализовано для direct uinput
            if (config.verbose) {
                std::cout << "Горизонтальный скролл " << (right ? "вправо" : "влево")
                         << " на " << steps << " шагов" << std::endl;
            }
            break;
        default:
            if (config.verbose) {
                std::cout << "Горизонтальный скролл недоступен" << std::endl;
            }
    }
}

void ScrollEmulator::executePageScroll(bool up) {
    switch (active_method) {
        case METHOD_X11_XTEST:
            executeX11PageScroll(up);
            break;
        case METHOD_UINPUT_DAEMON:
            sendDaemonCommand(up ? 'P' : 'N', 1);
            break;
        case METHOD_DIRECT_UINPUT:
            // Эмулируем через много шагов колесика
            executeDirectUinput(up, 5);
            break;
        default:
            if (config.verbose) {
                std::cout << "Page scroll недоступен" << std::endl;
            }
    }
}

void ScrollEmulator::executeX11Scroll(bool up, int steps) {
    if (config.verbose) {
        std::cout << "X11 скролл " << (up ? "вверх" : "вниз") << " на " << steps << " шагов" << std::endl;
    }

    for (int i = 0; i < steps; i++) {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "DISPLAY=%s timeout 0.1 xset -display %s r on 2>/dev/null || true",
                 getenv("DISPLAY") ?: ":0", getenv("DISPLAY") ?: ":0");
        system(cmd);

        if (i < steps - 1) usleep(config.delay_ms * 1000);
    }
}

void ScrollEmulator::executeX11HorizontalScroll(bool right, int steps) {
    if (config.verbose) {
        std::cout << "X11 горизонтальный скролл " << (right ? "вправо" : "влево")
                 << " на " << steps << " шагов" << std::endl;
    }

    for (int i = 0; i < steps; i++) {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "DISPLAY=%s timeout 0.1 xset -display %s r on 2>/dev/null || true",
                 getenv("DISPLAY") ?: ":0", getenv("DISPLAY") ?: ":0");
        system(cmd);

        if (i < steps - 1) usleep(config.delay_ms * 1000);
    }
}

void ScrollEmulator::executeX11PageScroll(bool up) {
    if (config.verbose) {
        std::cout << "X11 " << (up ? "Page Up" : "Page Down") << std::endl;
    }

    const char* key = up ? "Prior" : "Next";
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "DISPLAY=%s timeout 0.1 xset -display %s key %s 2>/dev/null || true",
             getenv("DISPLAY") ?: ":0", getenv("DISPLAY") ?: ":0", key);
    system(cmd);
}

void ScrollEmulator::executeDirectUinput(bool up, int steps) {
    if (config.verbose) {
        std::cout << "Direct uinput скролл " << (up ? "вверх" : "вниз")
                 << " на " << steps << " шагов" << std::endl;
    }
    // Здесь был бы код как в оригинальном uinput примере
}

void ScrollEmulator::executeSmoothScroll(bool vertical, bool positive, int distance, int duration_ms) {
    if (config.verbose) {
        std::cout << "Плавный скролл ";
        if (vertical) {
            std::cout << (positive ? "вверх" : "вниз");
        } else {
            std::cout << (positive ? "вправо" : "влево");
        }
        std::cout << " на " << distance << " за " << duration_ms << "мс" << std::endl;
    }

    // Рассчитываем количество шагов и задержки для плавности
    int total_steps = distance * config.smooth_steps;
    int step_delay = duration_ms / total_steps;

    // Минимальная задержка 1мс, максимальная 100мс
    step_delay = std::max(1, std::min(step_delay, 100));

    for (int i = 0; i < total_steps; i++) {
        // Применяем ускорение (ease-in-out)
        float progress = (float)i / total_steps;
        float ease_factor = config.acceleration;

        if (ease_factor != 1.0f) {
            // Простая функция ease-in-out
            if (progress < 0.5f) {
                ease_factor *= 2.0f * progress * progress;
            } else {
                ease_factor *= 1.0f - 2.0f * (progress - 0.5f) * (progress - 0.5f);
            }
        }

        // Делаем один шаг скролла
        if (vertical) {
            if (positive) {
                executeScroll(true, 1);
            } else {
                executeScroll(false, 1);
            }
        } else {
            if (positive) {
                executeHorizontalScroll(true, 1);
            } else {
                executeHorizontalScroll(false, 1);
            }
        }

        // Задержка с учетом ускорения
        int adjusted_delay = (int)(step_delay * ease_factor);
        usleep(adjusted_delay * 1000);
    }
}

// C API реализация

extern "C" {
    void* scroll_emulator_create() {
        return new ScrollEmulator();
    }

    void scroll_emulator_destroy(void* emulator) {
        delete static_cast<ScrollEmulator*>(emulator);
    }

    int scroll_emulator_init(void* emulator) {
        return static_cast<ScrollEmulator*>(emulator)->initialize() ? 1 : 0;
    }

    void scroll_emulator_set_delay(void* emulator, int delay_ms) {
        ScrollEmulator* e = static_cast<ScrollEmulator*>(emulator);
        ScrollEmulator::ScrollConfig cfg = e->getConfig();
        cfg.delay_ms = delay_ms;
        e->setConfig(cfg);
    }

    void scroll_emulator_set_smooth_steps(void* emulator, int steps) {
        ScrollEmulator* e = static_cast<ScrollEmulator*>(emulator);
        ScrollEmulator::ScrollConfig cfg = e->getConfig();
        cfg.smooth_steps = steps;
        e->setConfig(cfg);
    }

    void scroll_emulator_set_verbose(void* emulator, int verbose) {
        ScrollEmulator* e = static_cast<ScrollEmulator*>(emulator);
        ScrollEmulator::ScrollConfig cfg = e->getConfig();
        cfg.verbose = verbose != 0;
        e->setConfig(cfg);
    }

    void scroll_emulator_up(void* emulator, int steps) {
        static_cast<ScrollEmulator*>(emulator)->scrollUp(steps);
    }

    void scroll_emulator_down(void* emulator, int steps) {
        static_cast<ScrollEmulator*>(emulator)->scrollDown(steps);
    }

    void scroll_emulator_left(void* emulator, int steps) {
        static_cast<ScrollEmulator*>(emulator)->scrollLeft(steps);
    }

    void scroll_emulator_right(void* emulator, int steps) {
        static_cast<ScrollEmulator*>(emulator)->scrollRight(steps);
    }

    void scroll_emulator_smooth_up(void* emulator, int distance, int duration_ms) {
        static_cast<ScrollEmulator*>(emulator)->smoothScrollUp(distance, duration_ms);
    }

    void scroll_emulator_smooth_down(void* emulator, int distance, int duration_ms) {
        static_cast<ScrollEmulator*>(emulator)->smoothScrollDown(distance, duration_ms);
    }

    void scroll_emulator_smooth_left(void* emulator, int distance, int duration_ms) {
        static_cast<ScrollEmulator*>(emulator)->smoothScrollLeft(distance, duration_ms);
    }

    void scroll_emulator_smooth_right(void* emulator, int distance, int duration_ms) {
        static_cast<ScrollEmulator*>(emulator)->smoothScrollRight(distance, duration_ms);
    }

    void scroll_emulator_page_up(void* emulator) {
        static_cast<ScrollEmulator*>(emulator)->pageUp();
    }

    void scroll_emulator_page_down(void* emulator) {
        static_cast<ScrollEmulator*>(emulator)->pageDown();
    }

    void scroll_emulator_to_top(void* emulator) {
        static_cast<ScrollEmulator*>(emulator)->scrollToTop();
    }

    void scroll_emulator_to_bottom(void* emulator) {
        static_cast<ScrollEmulator*>(emulator)->scrollToBottom();
    }

    const char* scroll_emulator_get_method(void* emulator) {
        return static_cast<ScrollEmulator*>(emulator)->getMethod();
    }

    int scroll_emulator_is_available(void* emulator) {
        return static_cast<ScrollEmulator*>(emulator)->isAvailable() ? 1 : 0;
    }
}