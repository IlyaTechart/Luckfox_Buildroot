#define _POSIX_C_SOURCE 199309L
#include "rw_file.h"
#include "usb_com.h"
#include <time.h>



int FormatFrameInString(char *FileBuffer, size_t buffer_size, ModulData_t *ModulDataMassive, uint32_t count_frame, char *NameDevice)
{
    if (!FileBuffer || !ModulDataMassive || buffer_size == 0){
        printf("Нулевой указатель на буфер или нул. указ. на принем. данные или колличесвто элементов - 0\n");
        return 0;
    } 

    size_t offset = 0;
    
    // Пишем заголовок CSV (один раз)
    int written = snprintf(FileBuffer + offset, buffer_size - offset,
        "Name Device,""Time_ms,"
        "Grid_Status,Byp_Grid_Status,Rect_Status,Inv_Status,Pwr_via_Inv,Pwr_via_Byp,Sync_Status,Load_Mode,Sound_Alarm,Bat_Status,Ups_Mode,"
        "Err_LowIn,Err_HiDC,Err_LowBat,Err_NoBat,Err_InvF,Err_InvOC,Err_HiOut,Err_Fan,Err_ReplBat,Err_RectHot,Err_InvHot,"
        "V_in_AB,V_in_BC,V_in_CA,V_byp_A,V_byp_B,V_byp_C,I_in_A,I_in_B,I_in_C,Freq_in,"
        "V_out_A,V_out_B,V_out_C,Freq_out,I_out_A,I_out_B,I_out_C,P_act_A,P_act_B,P_act_C,P_app_A,P_app_B,P_app_C,Load_A,Load_B,Load_C,Event_Count,"
        "Vbat,Bat_Cap,Bat_Groups,DC_bus,Bat_Current,Backup_time,CRC32\n"
    );

    if(written < 0){
        perror("Ошибка: snprintf не отработал в FormatFrameInString");
        return 0;
    }
    
    if (written > 0 && written < (int)(buffer_size - offset)) {
        offset += written;
    } else {
        printf("Буфер слишком мал даже для загаловка\n");
        return 0; // Буфер слишком мал даже для заголовка
    }

    for (uint32_t i = 0; i < count_frame; i++) {
        FpgaToEspPacket_t *pkt = &ModulDataMassive[i].packet;
        
        // Для тока батареи явно приводим к signed int16_t, как указано в комментариях
        int16_t bat_current_signed = (int16_t)pkt->battery.bat_current;
        
        written = snprintf(FileBuffer + offset, buffer_size - offset,
            "%s""%u,"
            "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u," // Status (11)
            "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u," // Alarms (11)
            "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,"    // Input (10)
            "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u," // Output (17)
            "%u,%u,%u,%u,%d,%u,0x%08X\n",       // Battery (6) + CRC (1)
            
            NameDevice, pkt->system_time_ms,
            
            pkt->status.grid_status, pkt->status.bypass_grid_status, pkt->status.rectifier_status, pkt->status.inverter_status, pkt->status.pwr_via_inverter, pkt->status.pwr_via_bypass, pkt->status.sync_status, pkt->status.load_mode, pkt->status.sound_alarm, pkt->status.battery_status, pkt->status.ups_mode,
            
            pkt->alarms.err_low_input_vol, pkt->alarms.err_high_dc_bus, pkt->alarms.err_low_bat_charge, pkt->alarms.err_bat_not_conn, pkt->alarms.err_inv_fault, pkt->alarms.err_inv_overcurrent, pkt->alarms.err_high_out_vol, pkt->alarms.err_fan_fault, pkt->alarms.err_replace_bat, pkt->alarms.err_rect_overheat, pkt->alarms.err_inv_overheat,
            
            pkt->input.v_in_AB, pkt->input.v_in_BC, pkt->input.v_in_CA, pkt->input.v_bypass_A, pkt->input.v_bypass_B, pkt->input.v_bypass_C, pkt->input.i_in_A, pkt->input.i_in_B, pkt->input.i_in_C, pkt->input.freq_in,
            
            pkt->output.v_out_A, pkt->output.v_out_B, pkt->output.v_out_C, pkt->output.freq_out, pkt->output.i_out_A, pkt->output.i_out_B, pkt->output.i_out_C, pkt->output.p_active_A, pkt->output.p_active_B, pkt->output.p_active_C, pkt->output.p_apparent_A, pkt->output.p_apparent_B, pkt->output.p_apparent_C, pkt->output.load_pct_A, pkt->output.load_pct_B, pkt->output.load_pct_C, pkt->output.event_count,
            
            pkt->battery.bat_voltage, pkt->battery.bat_capacity, pkt->battery.bat_groups_count, pkt->battery.dc_bus_voltage, bat_current_signed, pkt->battery.backup_time,
            
            pkt->crc32
        );
        
        if (written > 0 && written < (int)(buffer_size - offset)) {
            offset += written;
        } else {
            printf("Буфер мал чтобы записать все кадры\n");// Буфер заполнен, прекращаем запись
            return offset; 
        }
    }
    if(offset > 0)
    {
        printf("Успешно отформатированные данные были записаны в буфер: %u байт\n", offset);
        return (int)offset;
    }

    return 0;
}

/// @brief Взятие дискриптора для записи файла O_CREAT - создаёт файл, O_WRONLY - только запись, O_TRUNC - стирает предыдущие данные 
/// @param pathFILE Путь файла 
/// @return Возвращает дискриптор 
int Get_Descriptor_File_Open(char* pathFILE)
{
    int fd = open(pathFILE, O_CREAT | O_WRONLY | O_TRUNC, 0666);

    if(fd == -1){
        perror("Файл не открался/не создался\n");
    }

    return fd;
}

/// @brief 
/// @param buffer 
/// @param count 
/// @param pathFILE 
/// @return 
FILE_enm_type_t File_Wirite(char *buffer, uint32_t count, char* pathFILE)
{
    ssize_t writen_byte = 0;

    if(count <= 0){
        return FILE_WRONG_ARGUMENT;
    }

    int fd = Get_Descriptor_File_Open(pathFILE);

    if(fd == -1){
        return FILE_NO_OPEN;
    }

    writen_byte = write(fd, buffer, count);

    if(writen_byte < count)
    {
        return FILE_NOT_WRITE;
    }

    close(fd);

    return FILE_WRITE;
}
