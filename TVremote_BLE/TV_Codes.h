#ifndef TV_CODES_H
#define TV_CODES_H

#include <Arduino.h>
#define IR_SEND_PIN 1
#include <IRremote.hpp>

// Supported TV Brands
enum TVBrand {
    BRAND_TOSHIBA = 0,
    BRAND_SAMSUNG = 1,
    BRAND_LG      = 2,
    BRAND_SONY    = 3,
    BRAND_GENERIC = 4  // Generic NEC protocol
};

// Standard TV Remote Commands
enum RemoteCommand {
    CMD_POWER,
    CMD_VOL_UP,
    CMD_VOL_DOWN,
    CMD_MUTE,
    CMD_CH_UP,
    CMD_CH_DOWN,
    CMD_INPUT,
    CMD_UP,
    CMD_DOWN,
    CMD_LEFT,
    CMD_RIGHT,
    CMD_OK,
    CMD_BACK,
    CMD_MENU,
    CMD_0,
    CMD_1,
    CMD_2,
    CMD_3,
    CMD_4,
    CMD_5,
    CMD_6,
    CMD_7,
    CMD_8,
    CMD_9
};

// Forward declaration
const char* getCommandName(RemoteCommand cmd);
const char* getBrandName(TVBrand brand);
void sendTVCommand(TVBrand brand, RemoteCommand cmd);

inline const char* getBrandName(TVBrand brand) {
    switch (brand) {
        case BRAND_TOSHIBA: return "Toshiba";
        case BRAND_SAMSUNG: return "Samsung";
        case BRAND_LG:      return "LG";
        case BRAND_SONY:    return "Sony";
        case BRAND_GENERIC: return "Generic NEC";
        default:            return "Unknown";
    }
}

inline const char* getCommandName(RemoteCommand cmd) {
    switch (cmd) {
        case CMD_POWER:    return "POWER";
        case CMD_VOL_UP:   return "VOL+";
        case CMD_VOL_DOWN: return "VOL-";
        case CMD_MUTE:     return "MUTE";
        case CMD_CH_UP:    return "CH+";
        case CMD_CH_DOWN:  return "CH-";
        case CMD_INPUT:    return "INPUT";
        case CMD_UP:       return "UP";
        case CMD_DOWN:     return "DOWN";
        case CMD_LEFT:     return "LEFT";
        case CMD_RIGHT:    return "RIGHT";
        case CMD_OK:       return "OK";
        case CMD_BACK:     return "BACK";
        case CMD_MENU:     return "MENU";
        case CMD_0:        return "0";
        case CMD_1:        return "1";
        case CMD_2:        return "2";
        case CMD_3:        return "3";
        case CMD_4:        return "4";
        case CMD_5:        return "5";
        case CMD_6:        return "6";
        case CMD_7:        return "7";
        case CMD_8:        return "8";
        case CMD_9:        return "9";
        default:           return "UNKNOWN";
    }
}

inline void sendTVCommand(TVBrand brand, RemoteCommand cmd) {
    Serial.printf("[IR] Sending %s to %s TV (GPIO %d)\n", 
                  getCommandName(cmd), getBrandName(brand), IR_SEND_PIN);

    switch (brand) {
        case BRAND_TOSHIBA: {
            uint16_t addr = 0x40; // Standard Toshiba NEC address (0x40 / 0xBF)
            uint16_t code = 0x00;
            switch (cmd) {
                case CMD_POWER:    code = 0x12; break;
                case CMD_VOL_UP:   code = 0x1A; break;
                case CMD_VOL_DOWN: code = 0x1E; break;
                case CMD_MUTE:     code = 0x10; break;
                case CMD_CH_UP:    code = 0x1B; break;
                case CMD_CH_DOWN:  code = 0x1F; break;
                case CMD_INPUT:    code = 0x14; break;
                case CMD_UP:       code = 0x19; break;
                case CMD_DOWN:     code = 0x1D; break;
                case CMD_LEFT:     code = 0x42; break;
                case CMD_RIGHT:    code = 0x40; break;
                case CMD_OK:       code = 0x21; break;
                case CMD_BACK:     code = 0x64; break;
                case CMD_MENU:     code = 0x5B; break;
                case CMD_0:        code = 0x00; break;
                case CMD_1:        code = 0x01; break;
                case CMD_2:        code = 0x02; break;
                case CMD_3:        code = 0x03; break;
                case CMD_4:        code = 0x04; break;
                case CMD_5:        code = 0x05; break;
                case CMD_6:        code = 0x06; break;
                case CMD_7:        code = 0x07; break;
                case CMD_8:        code = 0x08; break;
                case CMD_9:        code = 0x09; break;
                default: return;
            }
            IrSender.sendNEC(addr, code, 1);
            break;
        }

        case BRAND_SAMSUNG: {
            uint16_t addr = 0x0707;
            uint16_t code = 0x00;
            switch (cmd) {
                case CMD_POWER:    code = 0x02; break;
                case CMD_VOL_UP:   code = 0x07; break;
                case CMD_VOL_DOWN: code = 0x0B; break;
                case CMD_MUTE:     code = 0x0F; break;
                case CMD_CH_UP:    code = 0x12; break;
                case CMD_CH_DOWN:  code = 0x10; break;
                case CMD_INPUT:    code = 0x01; break;
                case CMD_UP:       code = 0x60; break;
                case CMD_DOWN:     code = 0x61; break;
                case CMD_LEFT:     code = 0x65; break;
                case CMD_RIGHT:    code = 0x62; break;
                case CMD_OK:       code = 0x68; break;
                case CMD_BACK:     code = 0x58; break;
                case CMD_MENU:     code = 0x1A; break;
                default: return;
            }
            IrSender.sendSamsung(addr, code, 1);
            break;
        }

        case BRAND_LG: {
            uint8_t addr = 0x04;
            uint16_t code = 0x00;
            switch (cmd) {
                case CMD_POWER:    code = 0x08; break;
                case CMD_VOL_UP:   code = 0x02; break;
                case CMD_VOL_DOWN: code = 0x03; break;
                case CMD_MUTE:     code = 0x09; break;
                case CMD_CH_UP:    code = 0x00; break;
                case CMD_CH_DOWN:  code = 0x01; break;
                case CMD_INPUT:    code = 0x0B; break;
                case CMD_UP:       code = 0x40; break;
                case CMD_DOWN:     code = 0x41; break;
                case CMD_LEFT:     code = 0x07; break;
                case CMD_RIGHT:    code = 0x06; break;
                case CMD_OK:       code = 0x44; break;
                case CMD_BACK:     code = 0x5B; break;
                case CMD_MENU:     code = 0x43; break;
                default: return;
            }
            IrSender.sendLG(addr, code, 1);
            break;
        }

        case BRAND_SONY: {
            uint8_t addr = 0x01; // TV address
            uint8_t code = 0x00;
            switch (cmd) {
                case CMD_POWER:    code = 0x15; break;
                case CMD_VOL_UP:   code = 0x12; break;
                case CMD_VOL_DOWN: code = 0x13; break;
                case CMD_MUTE:     code = 0x14; break;
                case CMD_CH_UP:    code = 0x10; break;
                case CMD_CH_DOWN:  code = 0x11; break;
                case CMD_INPUT:    code = 0x25; break;
                case CMD_UP:       code = 0x74; break;
                case CMD_DOWN:     code = 0x75; break;
                case CMD_LEFT:     code = 0x34; break;
                case CMD_RIGHT:    code = 0x33; break;
                case CMD_OK:       code = 0x65; break;
                case CMD_BACK:     code = 0x23; break;
                case CMD_MENU:     code = 0x60; break;
                default: return;
            }
            // Sony protocols require at least 2 or 3 repeats (SIRC 12-bit)
            IrSender.sendSony(addr, code, 2, 12);
            break;
        }

        case BRAND_GENERIC: {
            uint16_t addr = 0x00;
            uint16_t code = 0x00;
            switch (cmd) {
                case CMD_POWER:    code = 0x12; break;
                case CMD_VOL_UP:   code = 0x10; break;
                case CMD_VOL_DOWN: code = 0x11; break;
                case CMD_MUTE:     code = 0x0D; break;
                case CMD_CH_UP:    code = 0x1E; break;
                case CMD_CH_DOWN:  code = 0x1F; break;
                case CMD_INPUT:    code = 0x0F; break;
                case CMD_UP:       code = 0x14; break;
                case CMD_DOWN:     code = 0x15; break;
                case CMD_LEFT:     code = 0x16; break;
                case CMD_RIGHT:    code = 0x17; break;
                case CMD_OK:       code = 0x18; break;
                case CMD_BACK:     code = 0x1A; break;
                case CMD_MENU:     code = 0x1B; break;
                default: return;
            }
            IrSender.sendNEC(addr, code, 1);
            break;
        }
    }
}

#endif // TV_CODES_H
