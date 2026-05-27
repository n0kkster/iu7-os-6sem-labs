#!/usr/bin/env bash

# ============================================================================
# КОНФИГУРАЦИЯ
# ============================================================================

# Пути к трем испытуемым программам
PROG_PATHS=(
    "~/vuz/sem6/os/lab_01/client-server-af-inet/multithread/server.out"
    "~/vuz/sem6/os/lab_01/client-server-af-inet/multiplex/server.out"
    "~/vuz/sem6/os/lab_02/ftw/chdir.out"
)

# Рабочие директории для каждой программы (должны соответствовать порядку выше)
PROG_DIRS=(
    "~/vuz/sem6/os/lab_01/client-server-af-inet/multithread"
    "~/vuz/sem6/os/lab_01/client-server-af-inet/multiplex"
    "~/vuz/sem6/os/lab_02/ftw"
)

PROG_ARGS=(
	""
	""
	"/home"
)

PROG_NAMES=(
	"mt"
	"mp"
	"ftw"
)

# Таймаут (в секундах) между запуском программы и выполнением post-actions
# Должно быть ровно 3 значения
PROG_TIMEOUTS=(1 1 0)

# Post-actions: строки с командами оболочки. Разделяйте команды точкой с запятой.
# Оставьте пустую строку "", если действия не нужны.
PROG_POST_ACTIONS=(
    "~/vuz/sem6/os/lab_01/client-server-af-inet/multithread/client.out 0.0.0.0 c"
    "~/vuz/sem6/os/lab_01/client-server-af-inet/multiplex/client.out 0.0.0.0"
    ""
)

# Путь к программе-анализатору
ANALYZER_PATH="./main"

# Таймаут (в секундах) перед запуском анализатора
ANALYZER_TIMEOUT=0

# ============================================================================
# ЛОГИКА СКРИПТА
# ============================================================================

rm -r "${PROG_NAMES[@]}"

# Проверка корректности конфигурации
if [[ ${#PROG_PATHS[@]} -ne 3 || ${#PROG_DIRS[@]} -ne 3 || \
      ${#PROG_TIMEOUTS[@]} -ne 3 || ${#PROG_POST_ACTIONS[@]} -ne 3 ]]; then
    echo "ОШИБКА: Все конфигурационные массивы должны содержать ровно 3 элемента."
    exit 1
fi

# Сохраняем исходную директорию, чтобы вернуться после каждого cd
ORIG_DIR="$(pwd)"
PIDS=()

echo "=== Запуск целевых программ ==="

for i in "${!PROG_PATHS[@]}"; do
    PROG="${PROG_PATHS[$i]}"
    DIR="${PROG_DIRS[$i]}"
    ARG="${PROG_ARGS[$i]}"
    TIMEOUT="${PROG_TIMEOUTS[$i]}"
    ACTIONS="${PROG_POST_ACTIONS[$i]}"

    echo "[$((i+1))/3] Запуск: $PROG"

    # Проверка существования директории
    if [[ ! -d "$DIR" ]]; then
        echo "  ⚠ Директория $DIR не найдена. Пропуск."
        PIDS+=(0)
        continue
    fi

    # Переход в рабочую директорию программы
    cd "$DIR" || { echo "  ⚠ Не удалось перейти в $DIR"; PIDS+=(0); continue; }

    # Запуск программы в фоне без sudo
    "$PROG" "$ARG" > /dev/null 2>&1 &
    # "$PROG" "$ARG" &
    PID=$!

    # Возврат в исходную директорию
    cd "$ORIG_DIR"

    PIDS+=("$PID")
    echo "  ✅ PID: $PID"

    # Ожидание указанного таймаута перед post-actions
    if [[ "$TIMEOUT" -gt 0 ]]; then
        echo "  ⏳ Ожидание ${TIMEOUT}с перед выполнением post-actions..."
        sleep "$TIMEOUT"
    fi

    # Выполнение post-actions (если заданы)
    if [[ -n "$ACTIONS" ]]; then
        echo "  🔧 Выполнение post-actions..."
        # bash -c изолирует окружение и безопасно выполняет строку команд
        (cd "$DIR" && bash -c "$ACTIONS" > /dev/null 2>&1 &) || echo "  ⚠ Post-actions завершились с ошибкой (код $?)."
    fi
done

echo ""
echo "=== Все программы запущены ==="
echo "Ожидание ${ANALYZER_TIMEOUT}с перед запуском анализатора..."
sleep "$ANALYZER_TIMEOUT"

# Фильтруем валидные PID (исключаем 0, которые могли появиться при ошибках)
VALID_PIDS=()
for pid in "${PIDS[@]}"; do
    if [[ "$pid" -ne 0 ]]; then
        VALID_PIDS+=("$pid")
    fi
done

if [[ ${#VALID_PIDS[@]} -eq 0 ]]; then
    echo "❌ ОШИБКА: Не удалось получить валидные PID. Анализатор не запущен."
    exit 1
fi

echo "🔍 Запуск анализатора с sudo..."
echo "   Команда: sudo $ANALYZER_PATH ${VALID_PIDS[*]}"
# Передаем массив PID как отдельные аргументы
sudo "$ANALYZER_PATH" "${VALID_PIDS[@]}"

sudo mv "pid_${VALID_PIDS[0]}" "${PROG_NAMES[0]}"
sudo mv "pid_${VALID_PIDS[1]}" "${PROG_NAMES[1]}"
sudo mv "pid_${VALID_PIDS[2]}" "${PROG_NAMES[2]}"

sudo chown -R zero:zero "${PROG_NAMES[0]}"
sudo chown -R zero:zero "${PROG_NAMES[1]}"
sudo chown -R zero:zero "${PROG_NAMES[2]}"

echo "✅ Скрипт завершил работу."
