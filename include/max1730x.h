#ifndef MAX1730X_H
#define MAX1730X_H

#define MAX_1730x_addr_low 0x36
#include "driver/i2c.h"

struct BmsRegisters
{
    uint16_t AvgVoltage;
    uint16_t Voltage;
    int16_t Current;
    int16_t AvgCurrent;
    uint16_t RepFullCap;
    uint16_t RepSOC;
    uint16_t RepCap;
    uint16_t TTE;
    uint16_t TTF;
    int16_t QH;
    int16_t Temp;
    int16_t DieTemp;
    
};

class max1730x
{
private:
    i2c_port_t i2c_port;
    int task_prio;
    unsigned char mac_base[6] = {0};
    char mac_str[64];
    char topic_str[256];

public:
    max1730x(i2c_port_t i2c_num);
    void GetInfo();
    int16_t ReadBmsReg(uint8_t reg_addr);
    void begin();
    void BmsTask(void);
    static void startTaskImpl(void* _this);
    
    BmsRegisters BmsRegs;
};


enum max1730x_register
{   
    MAX1730X_REPCAP = 0x05,
    MAX1730X_REPSOC = 0x06,
    MAX1730X_MAXMINVOLT = 0x08,
    MAX1730X_MAXMINTEMP = 0x09,
    MAX1730X_MAXMINCURR = 0x0A,
    MAX1730X_CONFIG = 0x0B,
    MAX1730X_FULLCAPREP = 0x10,
    MAX1730X_TTE = 0x11,
    MAX1730X_AVGVCELL = 0x19,
    MAX1730X_TTF = 0x20,
    MAX1730X_VCELL = 0x1A,
    MAX1730X_TEMP = 0x1B,
    MAX1730X_CURRENT = 0x1C,
    MAX1730X_AVGCURRENT = 0x1D,
    MAX1730X_MIXCAP = 0x2B,
    MAX1730X_DIETEMP = 0x34,
    MAX1730X_FULLCAP = 0x35,
    MAX1730X_QH = 0x4D,
    MAX1730X_LEARNCFG = 0xA1,
    MAX1730X_MAXPEAKPWR = 0xA4,
    MAX1730X_SUSPEAKPWR = 0xA5,
    MAX1730X_PACKRESISTANCE = 0xA6,
    MAX1730X_SYSRESISTANCE = 0xA7,
    MAX1730X_MINSYSVOLTAGE = 0xA8,
    MAX1730X_MPPCURRENT = 0xA9,
    MAX1730X_SPPCURRENT = 0xAA,
    MAX1730X_CONFIG2 = 0xAB,
    MAX1730X_IALRTTH = 0xAC,
    MAX1730X_MINVOLT = 0xAD,
    MAX1730X_MINCURR = 0xAE,
    MAX1730X_NVPRTTH1BAK = 0xD6,
    MAX1730X_NPROTCFG = 0xD7,

};

#endif