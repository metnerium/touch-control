CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -fPIC

# Файлы проекта
HEADER = scroll_emulator.h
LIB_SOURCE = scroll_emulator.cpp
TOOL_SOURCE = scroll_tool.cpp

# Цели сборки
LIB_TARGET = libscrollemulator.so
STATIC_LIB = libscrollemulator.a
TOOL_TARGET = scroll-tool
OBJECT = scroll_emulator.o

# Основные цели
all: $(TOOL_TARGET) $(LIB_TARGET)

# Консольное приложение
$(TOOL_TARGET): $(TOOL_SOURCE) $(OBJECT)
	$(CXX) $(CXXFLAGS) -o $(TOOL_TARGET) $(TOOL_SOURCE) $(OBJECT)
	@echo "✓ Консольное приложение готово: ./$(TOOL_TARGET)"

# Разделяемая библиотека
$(LIB_TARGET): $(OBJECT)
	$(CXX) -shared -o $(LIB_TARGET) $(OBJECT)
	@echo "✓ Разделяемая библиотека готова: $(LIB_TARGET)"

# Статическая библиотека
$(STATIC_LIB): $(OBJECT)
	ar rcs $(STATIC_LIB) $(OBJECT)
	@echo "✓ Статическая библиотека готова: $(STATIC_LIB)"

# Объектный файл
$(OBJECT): $(LIB_SOURCE) $(HEADER)
	$(CXX) $(CXXFLAGS) -c $(LIB_SOURCE) -o $(OBJECT)

# Устанавливаем в систему
install: $(TOOL_TARGET) $(LIB_TARGET) $(HEADER)
	@echo "Установка ScrollEmulator..."
	sudo cp $(TOOL_TARGET) /usr/local/bin/
	sudo cp $(LIB_TARGET) /usr/local/lib/
	sudo cp $(HEADER) /usr/local/include/
	sudo ldconfig
	@echo "✓ Установка завершена!"
	@echo "Теперь можно использовать: scroll-tool --help"

# Удаляем из системы
uninstall:
	@echo "Удаление ScrollEmulator..."
	sudo rm -f /usr/local/bin/$(TOOL_TARGET)
	sudo rm -f /usr/local/lib/$(LIB_TARGET)
	sudo rm -f /usr/local/include/$(HEADER)
	sudo ldconfig
	@echo "✓ Удаление завершено"

# Быстрые тесты
test-simple: $(TOOL_TARGET)
	@echo "=== Простой тест ==="
	./$(TOOL_TARGET) info

test-verbose: $(TOOL_TARGET)
	@echo "=== Подробный тест ==="
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

# Полный тест
test: $(TOOL_TARGET)
	./$(TOOL_TARGET) test

# Примеры использования
examples: $(TOOL_TARGET)
	@echo "=== Примеры использования ScrollEmulator ==="
	@echo ""
	@echo "1. Простые команды:"
	@echo "   ./$(TOOL_TARGET) down 5                    # Скролл вниз на 5 шагов"
	@echo "   ./$(TOOL_TARGET) up 3                      # Скролл вверх на 3 шага"
	@echo "   ./$(TOOL_TARGET) page-down                 # Page Down"
	@echo ""
	@echo "2. Плавные скроллы:"
	@echo "   ./$(TOOL_TARGET) smooth-down 10 2000       # Плавно вниз за 2 секунды"
	@echo "   ./$(TOOL_TARGET) smooth-up 5 1000          # Плавно вверх за 1 секунду"
	@echo ""
	@echo "3. С настройками:"
	@echo "   ./$(TOOL_TARGET) -d 100 down 3             # Медленный скролл"
	@echo "   ./$(TOOL_TARGET) -s 10 smooth-down 5       # Очень плавный скролл"
	@echo "   ./$(TOOL_TARGET) -v test                   # Полная демонстрация"
	@echo ""
	@echo "4. Специальные:"
	@echo "   ./$(TOOL_TARGET) to-top                    # В начало документа"
	@echo "   ./$(TOOL_TARGET) to-bottom                 # В конец документа"
	@echo ""
	@echo "Запустите: ./$(TOOL_TARGET) --help для полной справки"

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
	rm -f $(TOOL_TARGET) $(LIB_TARGET) $(STATIC_LIB) $(OBJECT)
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