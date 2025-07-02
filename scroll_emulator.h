#ifndef SCROLL_EMULATOR_H
#define SCROLL_EMULATOR_H

#include <string>

// Класс для эмуляции скролла без sudo
class ScrollEmulator {
public:
    enum Method {
        METHOD_NONE = 0,
        METHOD_X11_XTEST,
        METHOD_UINPUT_DAEMON,
        METHOD_DIRECT_UINPUT
    };

    struct ScrollConfig {
        int delay_ms = 50;          // Задержка между шагами (мс)
        int smooth_steps = 1;       // Количество промежуточных шагов для плавности
        float acceleration = 1.0f;  // Ускорение (1.0 = постоянная скорость)
        bool verbose = false;       // Подробный вывод
    };

private:
    Method active_method;
    int socket_fd;
    pid_t daemon_pid;
    std::string socket_path;
    ScrollConfig config;

public:
    ScrollEmulator();
    ~ScrollEmulator();

    // Основные методы
    bool initialize();
    void cleanup();

    // Настройки
    void setConfig(const ScrollConfig& cfg) { config = cfg; }
    ScrollConfig getConfig() const { return config; }

    // Простые скроллы
    void scrollUp(int steps = 1);
    void scrollDown(int steps = 1);
    void scrollLeft(int steps = 1);
    void scrollRight(int steps = 1);

    // Плавные скроллы
    void smoothScrollUp(int distance, int duration_ms = 1000);
    void smoothScrollDown(int distance, int duration_ms = 1000);
    void smoothScrollLeft(int distance, int duration_ms = 1000);
    void smoothScrollRight(int distance, int duration_ms = 1000);

    // Специальные скроллы
    void pageUp();
    void pageDown();
    void scrollToTop();
    void scrollToBottom();

    // Информация
    const char* getMethod();
    bool isAvailable();

private:
    // Внутренние методы
    bool tryX11XTest();
    bool tryUinputDaemon();
    bool tryDirectUinput();

    void runUinputDaemon();
    int openUinput();
    bool setupUinput(int fd);
    void handleUinputCommand(int uinput_fd, char command, int steps);
    void handleX11Fallback(char command, int steps);

    bool connectToDaemon();
    void sendDaemonCommand(char command, int steps);

    void executeScroll(bool up, int steps);
    void executeHorizontalScroll(bool right, int steps);
    void executePageScroll(bool up);

    void executeX11Scroll(bool up, int steps);
    void executeX11HorizontalScroll(bool right, int steps);
    void executeX11PageScroll(bool up);
    void executeDirectUinput(bool up, int steps);

    // Плавные скроллы
    void executeSmoothScroll(bool vertical, bool positive, int distance, int duration_ms);
};

// C API для простой интеграции
extern "C" {
    // Создание/удаление
    void* scroll_emulator_create();
    void scroll_emulator_destroy(void* emulator);

    // Инициализация
    int scroll_emulator_init(void* emulator);

    // Настройки
    void scroll_emulator_set_delay(void* emulator, int delay_ms);
    void scroll_emulator_set_smooth_steps(void* emulator, int steps);
    void scroll_emulator_set_verbose(void* emulator, int verbose);

    // Простые скроллы
    void scroll_emulator_up(void* emulator, int steps);
    void scroll_emulator_down(void* emulator, int steps);
    void scroll_emulator_left(void* emulator, int steps);
    void scroll_emulator_right(void* emulator, int steps);

    // Плавные скроллы
    void scroll_emulator_smooth_up(void* emulator, int distance, int duration_ms);
    void scroll_emulator_smooth_down(void* emulator, int distance, int duration_ms);
    void scroll_emulator_smooth_left(void* emulator, int distance, int duration_ms);
    void scroll_emulator_smooth_right(void* emulator, int distance, int duration_ms);

    // Специальные
    void scroll_emulator_page_up(void* emulator);
    void scroll_emulator_page_down(void* emulator);
    void scroll_emulator_to_top(void* emulator);
    void scroll_emulator_to_bottom(void* emulator);

    // Информация
    const char* scroll_emulator_get_method(void* emulator);
    int scroll_emulator_is_available(void* emulator);
}

#endif // SCROLL_EMULATOR_H