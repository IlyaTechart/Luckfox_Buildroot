#define _POSIX_C_SOURCE 199309L
#include <locale.h>
#include "display_data.h"

// Глобальные переменные для меню
static const char *menu_items[] = {
    "1) Show data stream",
    "2) Enable/Disable CSV dump recording",
    "3) Show live data from device",
    "4) Show connected devices list",
    "5) Exit"
};
#define NUM_MENU_ITEMS (sizeof(menu_items) / sizeof(menu_items[0]))

static bool ncurses_initialized = false;

// ============================================================================
// Инициализация и закрытие ncurses
// ============================================================================
static void init_menu_ui(void) {
       setlocale(LC_ALL, "");
    if (!ncurses_initialized) {
        initscr();            // Инициализация ncurses
        cbreak();             // Отключение буферизации строк (реагируем сразу)
        noecho();             // Отключение эха вводимых символов
        keypad(stdscr, TRUE); // Включение обработки стрелок и доп. клавиш
        curs_set(0);          // Скрыть курсор
        ncurses_initialized = true;
    }
}

static void close_menu_ui(void) {
    if (ncurses_initialized) {
        endwin();             // Восстанавливаем обычный режим терминала
        ncurses_initialized = false;
    }
}

// ============================================================================
// Отрисовка самого меню (внутренняя функция)
// ============================================================================
static void draw_menu(int current_selection) {
    clear();

    // Верхний баннер-инструкция 
    attron(A_REVERSE); // Инвертируем цвета (выделенный фон)
    mvprintw(0, 0, " CONTROL MYLOGGER (Luckfox Pico Ultra W) ");
    attroff(A_REVERSE);
    
    mvprintw(2, 0, "NAVIGATION INSTRUCTIONS:");
    mvprintw(3, 2, "Use [UP] and [DOWN] arrows to move.");
    mvprintw(4, 2, "Press [ENTER] to select an option.");

    mvprintw(6, 0, "========================================================================");
    mvprintw(7, 0, "                             Main Menu                                  ");
    mvprintw(8, 0, "========================================================================");

    // Отрисовка пунктов меню
    for(int i = 0; i < (int)NUM_MENU_ITEMS; i++) {
        if(i == current_selection) {
            // Выделяем текущий пункт
            attron(A_REVERSE);
            mvprintw(10 + i, 2, "%s <---", menu_items[i]);
            attroff(A_REVERSE);
        } else {
            // Обычные пункты
            mvprintw(10 + i, 2, "%s      ", menu_items[i]);
        }
    }
    
    // Применяем изменения
    refresh();
}

// ============================================================================
// 1) Основная функция вызова меню. 
// Она блокирует поток до тех пор, пока пользователь не нажмет Enter.
// Возвращает номер выбранного пункта (от 0 до NUM_MENU_ITEMS - 1).
// ============================================================================
int main_menu(int default_selection)
{
    int current_selection = default_selection;
    if (current_selection < 0 || current_selection >= (int)NUM_MENU_ITEMS) {
        current_selection = 0;
    }

    // Включаем графический режим
    init_menu_ui();
    
    int ch;
    bool menu_running = true;

    while (menu_running) {
        // Отрисовываем меню с текущим выбранным пунктом
        draw_menu(current_selection);

        // Ждем нажатия клавиши (стрелки, Enter)
        ch = getch();

        switch (ch) {
            case KEY_UP:
                if (current_selection > 0) {
                    current_selection--;
                } else {
                    current_selection = NUM_MENU_ITEMS - 1; // Циклический переход в конец
                }
                break;
                
            case KEY_DOWN:
                if (current_selection < (int)NUM_MENU_ITEMS - 1) {
                    current_selection++;
                } else {
                    current_selection = 0; // Циклический переход в начало
                }
                break;
                
            case '\n':       // Клавиша Enter в Linux/ncurses
            case '\r':
            case KEY_ENTER:
                menu_running = false; // Выходим из цикла меню
                break;
                
            case 'q':
            case 'Q':
                // Быстрый выход из программы
                menu_running = false;
                current_selection = NUM_MENU_ITEMS - 1; // Предполагаем, что последний пункт - это Выход
                break;
        }
    }

    // Обязательно закрываем ncurses перед тем, как вернуть управление
    // Это вернет консоль в нормальный вид, чтобы printf работал корректно
    close_menu_ui();
    
    return current_selection;
}


// ============================================================================
// Функция вывода кадров (остается без изменений, так как работает через printf)
// ============================================================================
void logger_print_one_frame(const ModulData_t *m, size_t frame_index, logger_color_t color)
{

       // Таблица соответствия цвет → ANSI-код
    static const char* ansi_colors[] = {
        "",           // LOG_COLOR_DEFAULT — не меняем
        "\033[31m",   // LOG_COLOR_RED
        "\033[32m",   // LOG_COLOR_GREEN
        "\033[33m",   // LOG_COLOR_YELLOW
        "\033[34m",   // LOG_COLOR_BLUE
        "\033[35m",   // LOG_COLOR_MAGENTA
        "\033[36m",   // LOG_COLOR_CYAN
        "\033[37m",   // LOG_COLOR_WHITE
    };

    // Устанавливаем цвет, если не default
    if (color != LOG_COLOR_DEFAULT) {
        printf("%s", ansi_colors[color]);
    }

    const FpgaToEspPacket_t *p = &m->packet;

    printf("\n========== frame #%zu ==========\n", frame_index);
    printf("start_marker:   0x%08" PRIx32 " (%" PRIu32 ")\n", p->start_marker, p->start_marker);

    printf("[STATUS] raw=0x%04x | Grid:%u BypGrid:%u Rect:%u Inv:%u PwrInv:%u PwrByp:%u Sync:%u Load:%u Sound:%u BatSt:%u UpsMode:%u\n",
           (unsigned)p->status.raw,
           (unsigned)p->status.grid_status, (unsigned)p->status.bypass_grid_status,
           (unsigned)p->status.rectifier_status, (unsigned)p->status.inverter_status,
           (unsigned)p->status.pwr_via_inverter, (unsigned)p->status.pwr_via_bypass,
           (unsigned)p->status.sync_status, (unsigned)p->status.load_mode,
           (unsigned)p->status.sound_alarm, (unsigned)p->status.battery_status,
           (unsigned)p->status.ups_mode);

    printf("[ALARM] raw=0x%04x | LowIn:%u HiDC:%u LowBat:%u NoBat:%u InvF:%u InvOC:%u HiOut:%u Fan:%u ReplBat:%u RectHot:%u InvHot:%u\n",
           (unsigned)p->alarms.raw,
           (unsigned)p->alarms.err_low_input_vol, (unsigned)p->alarms.err_high_dc_bus,
           (unsigned)p->alarms.err_low_bat_charge, (unsigned)p->alarms.err_bat_not_conn,
           (unsigned)p->alarms.err_inv_fault, (unsigned)p->alarms.err_inv_overcurrent,
           (unsigned)p->alarms.err_high_out_vol, (unsigned)p->alarms.err_fan_fault,
           (unsigned)p->alarms.err_replace_bat, (unsigned)p->alarms.err_rect_overheat,
           (unsigned)p->alarms.err_inv_overheat);

    printf("[INPUT] V_in AB/BC/CA (x0.1 V): %u %u %u | V_byp A/B/C: %u %u %u\n",
           (unsigned)p->input.v_in_AB, (unsigned)p->input.v_in_BC, (unsigned)p->input.v_in_CA,
           (unsigned)p->input.v_bypass_A, (unsigned)p->input.v_bypass_B, (unsigned)p->input.v_bypass_C);
    printf("[INPUT] I_in A/B/C (x0.1 A): %u %u %u | freq_in (x0.01 Hz): %u\n",
           (unsigned)p->input.i_in_A, (unsigned)p->input.i_in_B, (unsigned)p->input.i_in_C,
           (unsigned)p->input.freq_in);

    printf("[OUTPUT] V_out A/B/C (x0.1 V): %u %u %u | freq_out (x0.01 Hz): %u\n",
           (unsigned)p->output.v_out_A, (unsigned)p->output.v_out_B, (unsigned)p->output.v_out_C,
           (unsigned)p->output.freq_out);
    printf("[OUTPUT] I_out A/B/C (x0.1 A): %u %u %u\n",
           (unsigned)p->output.i_out_A, (unsigned)p->output.i_out_B, (unsigned)p->output.i_out_C);
    printf("[OUTPUT] P_active A/B/C (x0.1 kW): %u %u %u | P_apparent A/B/C (x0.1 kVA): %u %u %u\n",
           (unsigned)p->output.p_active_A, (unsigned)p->output.p_active_B, (unsigned)p->output.p_active_C,
           (unsigned)p->output.p_apparent_A, (unsigned)p->output.p_apparent_B, (unsigned)p->output.p_apparent_C);
    printf("[OUTPUT] Load %% A/B/C (x0.1): %u %u %u | event_count: %u\n",
           (unsigned)p->output.load_pct_A, (unsigned)p->output.load_pct_B, (unsigned)p->output.load_pct_C,
           (unsigned)p->output.event_count);

    {
        int16_t cur = (int16_t)p->battery.bat_current;
        uint16_t cur_abs = (uint16_t)(cur < 0 ? -cur : cur);
        printf("[BAT] Vbat (x0.1 V): %u | Cap (Ah): %u | groups: %u | DC (x0.1 V): %u\n",
               (unsigned)p->battery.bat_voltage, (unsigned)p->battery.bat_capacity,
               (unsigned)p->battery.bat_groups_count, (unsigned)p->battery.dc_bus_voltage);
        printf("[BAT] I (x0.1 A, signed): %s%u.%u | backup (min): %u\n",
               cur < 0 ? "-" : "",
               (unsigned)(cur_abs / 10u), (unsigned)(cur_abs % 10u),
               (unsigned)p->battery.backup_time);
    }

    printf("crc32:          0x%08" PRIx32 "\n", p->crc32);
    printf("system_time_ms: %" PRIu32 "\n", p->system_time_ms);


    printf("\033[0m"); // Возврат в исходный цвет 

}