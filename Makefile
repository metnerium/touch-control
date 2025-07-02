CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -fPIC

# Зависимости системы
LIBINPUT_CFLAGS = $(shell pkg-config --cflags libinput 2>/dev/null)
LIBINPUT_LIBS = $(shell pkg-config --libs libinput 2>/dev/null)
LIBUDEV_CFLAGS = $(shell pkg-config --cflags libudev 2>/dev/null)
LIBUDEV_LIBS = $(shell pkg-config --libs libudev 2>/dev/null)

# Файлы проекта
HEADER = scroll_emulator.h
GESTURE_HEADER = gesture_scroll_handler.h
LIB_SOURCE = scroll_emulator.cpp
GESTURE_SOURCE = gesture_scroll_handler.cpp
TOOL_SOURCE = scroll_tool.cpp
DAEMON_SOURCE = gesture_scroll_daemon.cpp

# Цели сборки
LIB_TARGET = libscrollemulator.so
STATIC_LIB = libscrollemulator.a
TOOL_TARGET = scroll-tool
DAEMON_TARGET = gesture-scroll
OBJECT = scroll_emulator.o
GESTURE_OBJECT = gesture_scroll_handler.o

# Основные цели
all: $(TOOL_TARGET) $(LIB_TARGET) $(DAEMON_TARGET)

# Консольное приложение
$(TOOL_TARGET): $(TOOL_SOURCE) $(OBJECT)
	$(CXX) $(CXXFLAGS) -o $(TOOL_TARGET) $(TOOL_SOURCE) $(OBJECT)
	@echo "✓ Консольное приложение готово: ./$(TOOL_TARGET)"

# Gesture Scroll Daemon
$(DAEMON_TARGET): $(DAEMON_SOURCE) $(OBJECT) $(GESTURE_OBJECT)
	@if [ -z "$(LIBINPUT_LIBS)" ]; then \
		echo "Ошибка: libinput не найден. Установите: sudo apt install libinput-dev"; \
		exit 1; \
	fi
	@if [ -z "$(LIBUDEV_LIBS)" ]; then \
		echo "Ошибка: libudev не найден. Установите: sudo apt install libudev-dev"; \
		exit 1; \
	fi
	$(CXX) $(CXXFLAGS) $(LIBINPUT_CFLAGS) $(LIBUDEV_CFLAGS) -o $(DAEMON_TARGET) $(DAEMON_SOURCE) $(OBJECT) $(GESTURE_OBJECT) $(LIBINPUT_LIBS) $(LIBUDEV_LIBS)
	@echo "✓ Gesture Scroll Daemon готов: ./$(DAEMON_TARGET)"

# Разделяемая библиотека
$(LIB_TARGET): $(OBJECT)
	$(CXX) -shared -o $(LIB_TARGET) $(OBJECT)
	@echo "✓ Разделяемая библиотека готова: $(LIB_TARGET)"

# Статическая библиотека
$(STATIC_LIB): $(OBJECT)
	ar rcs $(STATIC_LIB) $(OBJECT)
	@echo "✓ Статическая библиотека готова: $(STATIC_LIB)"

# Объектные файлы
$(OBJECT): $(LIB_SOURCE) $(HEADER)
	$(CXX) $(CXXFLAGS) -c $(LIB_SOURCE) -o $(OBJECT)

$(GESTURE_OBJECT): $(GESTURE_SOURCE) $(GESTURE_HEADER) $(HEADER)
	@if [ -z "$(LIBINPUT_LIBS)" ]; then \
		echo "Ошибка: libinput не найден. Установите: sudo apt install libinput-dev"; \
		exit 1; \
	fi
	@if [ -z "$(LIBUDEV_LIBS)" ]; then \
		echo "Ошибка: libudev не найден. Установите: sudo apt install libudev-dev"; \
		exit 1; \
	fi
	$(CXX) $(CXXFLAGS) $(LIBINPUT_CFLAGS) $(LIBUDEV_CFLAGS) -c $(GESTURE_SOURCE) -o $(GESTURE_OBJECT)

# Устанавливаем в систему
install: $(TOOL_TARGET) $(LIB_TARGET) $(DAEMON_TARGET) $(HEADER) $(GESTURE_HEADER)
	@echo "Установка ScrollEmulator и Gesture Scroll..."
	sudo cp $(TOOL_TARGET) /usr/local/bin/
	sudo cp $(DAEMON_TARGET) /usr/local/bin/
	sudo cp $(LIB_TARGET) /usr/local/lib/
	sudo cp $(HEADER) /usr/local/include/
	sudo cp $(GESTURE_HEADER) /usr/local/include/
	sudo ldconfig
	@echo "✓ Установка завершена!"
	@echo "Теперь можно использовать:"
	@echo "  scroll-tool --help         # Консольный инструмент"
	@echo "  gesture-scroll --help      # Демон жестов"

# Удаляем из системы
uninstall:
	@echo "Удаление ScrollEmulator и Gesture Scroll..."
	sudo rm -f /usr/local/bin/$(TOOL_TARGET)
	sudo rm -f /usr/local/bin/$(DAEMON_TARGET)
	sudo rm -f /usr/local/lib/$(LIB_TARGET)
	sudo rm -f /usr/local/include/$(HEADER)
	sudo rm -f /usr/local/include/$(GESTURE_HEADER)
	sudo ldconfig
	@echo "✓ Удаление завершено"

# Быстрые тесты
test-simple: $(TOOL_TARGET)
	@echo "=== Простой тест scroll-tool ==="
	./$(TOOL_TARGET) info

test-verbose: $(TOOL_TARGET)
	@echo "=== Подробный тест scroll-tool ==="
	./$(TOOL_TARGET) -v info

test-scroll: $(TOOL_TARGET)
	@echo "=== Тест скролла ==="
	@echo "Откройте браузер или текстовый редактор и нажмите Enter..."
	@read dummy
	./$(TOOL_TARGET) -v down 3
	sleep 1
	./$(TOOL_TARGET) -v up 2

test-smooth: $(TOOL_TARGET)
	@echo "=== Тест плавного скролла ==="
	@echo "Откройте приложение с содержимым и нажмите Enter..."
	@read dummy
	./$(TOOL_TARGET) -v -s 5 smooth-down 10 2000

test-gesture: $(DAEMON_TARGET)
	@echo "=== Тест gesture-scroll демона ==="
	./$(DAEMON_TARGET) --test

test-gesture-live: $(DAEMON_TARGET)
	@echo "=== Живой тест gesture-scroll ==="
	@echo "Будет запущен gesture-scroll с подробным выводом."
	@echo "Используйте 3 пальца на тачпаде для тестирования."
	@echo "Нажмите Ctrl+C для остановки."
	@echo "Нажмите Enter для начала..."
	@read dummy
	./$(DAEMON_TARGET) -v

# Полный тест
test: $(TOOL_TARGET) $(DAEMON_TARGET)
	@echo "=== Полное тестирование ==="
	./$(TOOL_TARGET) test
	@echo ""
	./$(DAEMON_TARGET) --test

# Примеры использования
examples: $(TOOL_TARGET) $(DAEMON_TARGET)
	@echo "=== Примеры использования ScrollEmulator + Gesture Scroll ==="
	@echo ""
	@echo "1. SCROLL-TOOL - ручная прокрутка:"
	@echo "   ./$(TOOL_TARGET) down 5                    # Скролл вниз на 5 шагов"
	@echo "   ./$(TOOL_TARGET) up 3                      # Скролл вверх на 3 шага"
	@echo "   ./$(TOOL_TARGET) smooth-down 10 2000       # Плавно вниз за 2 секунды"
	@echo "   ./$(TOOL_TARGET) -d 100 down 3             # Медленный скролл"
	@echo "   ./$(TOOL_TARGET) -v test                   # Полная демонстрация"
	@echo ""
	@echo "2. GESTURE-SCROLL - жесты тачпада:"
	@echo "   ./$(DAEMON_TARGET)                         # Запуск с настройками по умолчанию"
	@echo "   ./$(DAEMON_TARGET) -v                      # С подробным выводом"
	@echo "   ./$(DAEMON_TARGET) -d 30 -s 3              # Быстрый и плавный скролл"
	@echo "   ./$(DAEMON_TARGET) --daemon -q             # Запуск в фоне"
	@echo "   ./$(DAEMON_TARGET) --test                  # Проверка системы"
	@echo ""
	@echo "3. ЖЕСТЫ (для gesture-scroll):"
	@echo "   3 пальца вверх    -> плавная прокрутка вверх"
	@echo "   3 пальца вниз     -> плавная прокрутка вниз"
	@echo "   3 пальца влево    -> горизонтальная прокрутка влево"
	@echo "   3 пальца вправо   -> горизонтальная прокрутка вправо"
	@echo ""
	@echo "Справка:"
	@echo "   ./$(TOOL_TARGET) --help                    # Справка по scroll-tool"
	@echo "   ./$(DAEMON_TARGET) --help                  # Справка по gesture-scroll"

# Создание пакета для распространения
package: all
	@echo "Создание пакета..."
	mkdir -p scroll-emulator-package
	cp $(TOOL_TARGET) $(LIB_TARGET) $(STATIC_LIB) scroll-emulator-package/
	cp $(HEADER) scroll-emulator-package/
	cp README.md scroll-emulator-package/ 2>/dev/null || echo "# ScrollEmulator Package" > scroll-emulator-package/README.md
	tar -czf scroll-emulator.tar.gz scroll-emulator-package/
	rm -rf scroll-emulator-package/
	@echo "✓ Пакет создан: scroll-emulator.tar.gz"

# Проверка системы
check:
	@echo "=== Проверка системы ==="
	@echo "Сессия: $${XDG_SESSION_TYPE:-unknown}"
	@if [ -n "$$WAYLAND_DISPLAY" ]; then \
		echo "✓ Wayland: $$WAYLAND_DISPLAY"; \
	else \
		echo "✗ Wayland не обнаружен"; \
	fi
	@if [ -n "$$DISPLAY" ]; then \
		echo "✓ X11: $$DISPLAY"; \
	else \
		echo "✗ X11 не обнаружен"; \
	fi
	@echo ""
	@echo "Библиотеки:"
	@if pkg-config --exists libinput 2>/dev/null; then \
		echo "✓ libinput найден"; \
		pkg-config --modversion libinput 2>/dev/null | sed 's/^/    Версия: /'; \
	else \
		echo "✗ libinput не найден (sudo apt install libinput-dev)"; \
	fi
	@if pkg-config --exists libudev 2>/dev/null; then \
		echo "✓ libudev найден"; \
		pkg-config --modversion libudev 2>/dev/null | sed 's/^/    Версия: /'; \
	else \
		echo "✗ libudev не найден (sudo apt install libudev-dev)"; \
	fi
	@echo ""
	@echo "Утилиты:"
	@if command -v xset >/dev/null 2>&1; then \
		echo "✓ xset найден"; \
	else \
		echo "✗ xset не найден (sudo apt install x11-xserver-utils)"; \
	fi
	@echo ""
	@echo "UInput:"
	@for path in /dev/uinput /dev/input/uinput; do \
		if [ -e "$$path" ]; then \
			echo "✓ $$path найден"; \
			ls -l "$$path"; \
		else \
			echo "✗ $$path не найден"; \
		fi; \
	done

# Настройка системы
setup-x11:
	@echo "Настройка X11..."
	sudo apt update
	sudo apt install -y x11-xserver-utils
	@echo "✓ X11 готов"

setup-wayland:
	@echo "Настройка Wayland..."
	sudo modprobe uinput
	sudo usermod -a -G input $(USER)
	echo 'KERNEL=="uinput", GROUP="input", MODE="0664"' | sudo tee /etc/udev/rules.d/99-uinput.rules
	sudo udevadm control --reload-rules
	sudo udevadm trigger
	@echo "✓ Wayland настроен. ПЕРЕЛОГИНЬТЕСЬ!"

setup: check
	@if [ -n "$$WAYLAND_DISPLAY" ]; then \
		make setup-wayland; \
	elif [ -n "$$DISPLAY" ]; then \
		make setup-x11; \
	else \
		echo "Не удалось определить графическую сессию"; \
	fi

# Очистка
clean:
	rm -f $(TOOL_TARGET) $(DAEMON_TARGET) $(LIB_TARGET) $(STATIC_LIB) $(OBJECT) $(GESTURE_OBJECT)
	rm -f scroll-emulator.tar.gz
	@echo "✓ Очистка выполнена"

# Справка
help:
	@echo "Доступные команды:"
	@echo ""
	@echo "Сборка:"
	@echo "  make              - собрать консольное приложение и библиотеку"
	@echo "  make $(TOOL_TARGET)      - только консольное приложение"
	@echo "  make $(LIB_TARGET)   - только разделяемая библиотека"
	@echo "  make $(STATIC_LIB)  - только статическая библиотека"
	@echo ""
	@echo "Установка:"
	@echo "  make install      - установить в систему"
	@echo "  make uninstall    - удалить из системы"
	@echo "  make setup        - настроить систему"
	@echo ""
	@echo "Тестирование:"
	@echo "  make check        - проверить систему"
	@echo "  make test         - полный тест"
	@echo "  make test-simple  - быстрый тест"
	@echo "  make test-scroll  - тест скролла"
	@echo "  make test-smooth  - тест плавного скролла"
	@echo ""
	@echo "Документация:"
	@echo "  make examples     - примеры использования"
	@echo "  make doc          - создать документацию"
	@echo "  make package      - создать пакет для распространения"
	@echo ""
	@echo "Очистка:"
	@echo "  make clean        - удалить собранные файлы"

.PHONY: all install uninstall test test-simple test-verbose test-scroll test-smooth examples package doc check setup-x11 setup-wayland setup clean help