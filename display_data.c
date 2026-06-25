#define _POSIX_C_SOURCE 199309L
#include <locale.h>
#include <stdarg.h>
#include "display_data.h"

// Глобальные переменные для меню
static const char *menu_items[] = {
    "1) Show event stream",
    "2) Enable/Disable CSV dump recording",
    "3) Show live data from device",
    "4) Show connected devices list",
    "5) Exit"
};
#define NUM_MENU_ITEMS (sizeof(menu_items) / sizeof(menu_items[0]))

static bool ncurses_initialized = false;
//static int menu_selection = 0;

// ============================================================================
// Инициализация и закрытие ncurses (Теперь публичные)
// ============================================================================
void Display_Init(void) {
    setlocale(LC_ALL, "");
    if (!ncurses_initialized) {
        initscr();            // Инициализация ncurses
        cbreak();             // Отключение буферизации строк
        noecho();             // Отключение эха
        keypad(stdscr, TRUE); // Включение обработки стрелок
        curs_set(0);          // Скрыть курсор
        halfdelay(1);         // Полу-блокирующее ожидание ввода (0.1 сек)
        ncurses_initialized = true;
    }
}

void Display_Deinit(void) {
    if (ncurses_initialized) {
        endwin();             // Восстанавливаем терминал
        ncurses_initialized = false;
    }
}

// ============================================================================
// Заглушка для Logger_Print (в будущем будет писать в очередь)
// ============================================================================
void Logger_Print(const char* format, ...) {
    // В будущем здесь будет Queue_Push(&Queue_logs, ...)
    // Пока ничего не делаем, чтобы не ломать ncurses
}


// ============================================================================
// Функции отрисовки экранов
// ============================================================================
static UI_State_t Draw_Main_Menu(void) {

	static int menu_selection = 0;

    clear();
    attron(A_REVERSE);
    mvprintw(0, 0, " CONTROL MYLOGGER (Luckfox Pico Ultra W) ");
    attroff(A_REVERSE);
    
    mvprintw(2, 0, "NAVIGATION INSTRUCTIONS:");
    mvprintw(3, 2, "Use [UP] and [DOWN] arrows to move.");
    mvprintw(4, 2, "Press [ENTER] to select an option. [Q] - Eixt");

    mvprintw(6, 0, "========================================================================");
    mvprintw(7, 0, "                             Main Menu                                  ");
    mvprintw(8, 0, "========================================================================");
	attron(A_REVERSE);
	mvprintw(10, 2, "%s <---", menu_items[0]);
	attroff(A_REVERSE);
	int i = 1;
	while(i < (int)NUM_MENU_ITEMS){
		mvprintw(10 + i, 2, "%s      ", menu_items[i]);
		i++;
	}
	refresh();

    while(1)
    {
        // Перерисовываем ТОЛЬКО сами пункты меню
        for(int i = 0; i < (int)NUM_MENU_ITEMS; i++) {
            if(i == menu_selection) {
                attron(A_REVERSE);
                // Заметьте: мы пишем пробелы в конце, чтобы "затереть" старые символы
                // на случай, если предыдущая строка была длиннее
                mvprintw(10 + i, 2, "%-40s <---", menu_items[i]); 
                attroff(A_REVERSE);
            } else {
                mvprintw(10 + i, 2, "%-40s      ", menu_items[i]);
            }
        }
        
        refresh(); // ncurses отправит в терминал только измененные пункты
        // --- Блок обработки ввода ---
        int key = getch();
        mvprintw(18, 2, "%d      ", key);
        switch (key) 
		{
            case KEY_UP:
                if (menu_selection > 0) menu_selection--;
                else menu_selection = NUM_MENU_ITEMS - 1;
                break;
                
            case KEY_DOWN:
                if (menu_selection < (int)NUM_MENU_ITEMS - 1) menu_selection++;
                else menu_selection = 0;
                break;
                
            case '\n':       
            case '\r':
            case KEY_ENTER:
                if (menu_selection == 0) return UI_STATE_LOG_VIEWER;
                if (menu_selection == 1) return UI_STATE_CSV_CONTROL;
                if (menu_selection == 2) return UI_STATE_LIVE_AVE;
                if (menu_selection == 3) return UI_STATE_USB_DEVICES;
                if (menu_selection == 4) return UI_STATE_EXIT; 
                break;
                
            case 'q':
            case 'Q':
            case 27: // KEY_ESC
                return UI_STATE_EXIT; 
                
            case ERR:
                // Таймаут. В главном меню нам нечего обновлять по таймауту,
                // поэтому просто ждем дальше.
                break;
        }
    }

	return UI_STATE_EXIT;
}

static UI_State_t Draw_Log_Viewer(void) {
    clear();
    attron(A_REVERSE);
    mvprintw(0, 0, " LOG VIEWER (Events from stdout/stderr) ");
    attroff(A_REVERSE);
    
    mvprintw(2, 0, "Here will be logs from the Queue_logs.");
    mvprintw(4, 0, "Press 'q' or 'Q' to return to Main Menu.");
    refresh();
}

static UI_State_t Draw_CSV_Control(void) {
    clear();
    attron(A_REVERSE);
    mvprintw(0, 0, " CSV LOGGING CONTROL ");
    attroff(A_REVERSE);
    
    mvprintw(2, 0, "Status: [UNKNOWN]"); // В будущем брать статус из атомарного флага
    mvprintw(4, 0, "Press [ENTER] to toggle recording.");
    mvprintw(6, 0, "Press 'q' or 'Q' to return to Main Menu.");
    refresh();
}

static UI_State_t Draw_Live_AVE(void) {
    clear();
    attron(A_REVERSE);
    mvprintw(0, 0, " LIVE DATA (AVE Queue) ");
    attroff(A_REVERSE);
    
    mvprintw(2, 0, "Here will be live data parsed from Queue_ave.");
    mvprintw(4, 0, "Press 'q' or 'Q' to return to Main Menu.");
    refresh();
}

static UI_State_t Draw_USB_List(void) {
    clear();
    attron(A_REVERSE);
    mvprintw(0, 0, " CONNECTED USB DEVICES ");
    attroff(A_REVERSE);
    
    mvprintw(2, 0, "Here will be a list of active /dev/ttyACM* ports.");
    mvprintw(4, 0, "Press 'q' or 'Q' to return to Main Menu.");
    refresh();
}

// ============================================================================
// Главная функция-обработчик экрана (State Machine)
// ============================================================================
bool Display_Task_Loop(void){

    char veDebug;
    static UI_State_t current_state = UI_STATE_MAIN_MENU;
    switch (current_state) {
        case UI_STATE_MAIN_MENU:
            current_state = Draw_Main_Menu(); // Поток "застрянет" внутри, пока не вернется новое состояние
            break;
        case UI_STATE_LOG_VIEWER:
            current_state = Draw_Log_Viewer();
            break;
        case UI_STATE_CSV_CONTROL:
            current_state = Draw_CSV_Control();
            break;
        case UI_STATE_LIVE_AVE:
            current_state = Draw_Live_AVE();
            break;
        case UI_STATE_USB_DEVICES:
            current_state = Draw_USB_List();
            break;
        case UI_STATE_EXIT:
            return false; // Выходим из цикла в main.c
    }
    veDebug = current_state;
    mvprintw(18, 2, "%d      ", veDebug);
    return true;

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