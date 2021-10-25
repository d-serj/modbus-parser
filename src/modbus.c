#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "modbus.h"

static const uint16_t crc_table[] = {
  0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241, 0XC601,
  0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440, 0XCC01, 0X0CC0,
  0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40, 0X0A00, 0XCAC1, 0XCB81,
  0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841, 0XD801, 0X18C0, 0X1980, 0XD941,
  0X1B00, 0XDBC1, 0XDA81, 0X1A40, 0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01,
  0X1DC0, 0X1C80, 0XDC41, 0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0,
  0X1680, 0XD641, 0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081,
  0X1040, 0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
  0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441, 0X3C00,
  0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41, 0XFA01, 0X3AC0,
  0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840, 0X2800, 0XE8C1, 0XE981,
  0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41, 0XEE01, 0X2EC0, 0X2F80, 0XEF41,
  0X2D00, 0XEDC1, 0XEC81, 0X2C40, 0XE401, 0X24C0, 0X2580, 0XE541, 0X2700,
  0XE7C1, 0XE681, 0X2640, 0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0,
  0X2080, 0XE041, 0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281,
  0X6240, 0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
  0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41, 0XAA01,
  0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840, 0X7800, 0XB8C1,
  0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41, 0XBE01, 0X7EC0, 0X7F80,
  0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40, 0XB401, 0X74C0, 0X7580, 0XB541,
  0X7700, 0XB7C1, 0XB681, 0X7640, 0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101,
  0X71C0, 0X7080, 0XB041, 0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0,
  0X5280, 0X9241, 0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481,
  0X5440, 0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
  0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841, 0X8801,
  0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40, 0X4E00, 0X8EC1,
  0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41, 0X4400, 0X84C1, 0X8581,
  0X4540, 0X8701, 0X47C0, 0X4680, 0X8641, 0X8201, 0X42C0, 0X4380, 0X8341,
  0X4100, 0X81C1, 0X8081, 0X4040
};

#define CALLBACK_NOTIFY(FOR)                                                   \
  do {                                                                         \
    if (settings->on_##FOR) {                                                  \
      if (settings->on_##FOR(parser) != 0) {                                   \
        parser->err = 1;                                                       \
      }                                                                        \
    }                                                                          \
  } while (0)

void
modbus_parser_init(modbus_parser* parser, enum modbus_parser_type t)
{
  void* arg = parser->arg; /* preserve application data */
  memset(parser, 0, sizeof(*parser));
  parser->arg = arg;
  parser->type = t;
  parser->state = s_slave_addr;
  parser->calc_crc = 0xFFFF;
}

void
modbus_parser_settings_init(modbus_parser_settings* settings)
{
  memset(settings, 0, sizeof(*settings));
}

void
modbus_query_init(struct modbus_query* q)
{
  memset(q, 0, sizeof(*q));
}

const char*
modbus_func_str(enum modbus_func f)
{
  switch (f) {
#define XX(num, name, string)                                                  \
  case MODBUS_FUNC_##name:                                                     \
    return string;
    MODBUS_FUNC_MAP(XX)
#undef XX
    default:
      return "<unknown>";
  }
}

/*
static enum modbus_parser_state
state_after_function(modbus_func f)
{
  switch (f) {
    case MODBUS_FUNC_READ_COILS:
    case MODBUS_FUNC_READ_DISCRETE_IN:
    case MODBUS_FUNC_READ_HOLD_REG:
    case MODBUS_FUNC_READ_IN_REG:
      return s_len;

    case MODBUS_FUNC_WRITE_COIL:
    case MODBUS_FUNC_WRITE_REG:
      return s_single_addr_hi;

    case MODBUS_FUNC_WRITE_COILS:
    case MODBUS_FUNC_WRITE_REGS:
      return s_start_addr_hi;
  }
}
*/

uint16_t
modbus_calc_crc(const uint8_t* data, size_t sz)
{
  uint8_t tmp;
  uint16_t crc = 0xFFFF;

  while (sz--) {
    tmp = *data++ ^ crc;
    crc >>= 8;
    crc ^= crc_table[tmp];
  }
  return crc;
}

void
modbus_crc_update(uint16_t* crc, uint8_t data)
{
  uint8_t tmp;

  tmp = data ^ *crc;
  *crc >>= 8;
  *crc ^= crc_table[tmp];
}

static size_t
parse_query(modbus_parser* parser,
            const modbus_parser_settings* settings,
            const uint8_t* data,
            size_t len)
{
  return 0;
}

static size_t
parse_response(modbus_parser* parser,
               const modbus_parser_settings* settings,
               const uint8_t* data,
               size_t len)
{
  size_t nparsed = 0;

  for (size_t i = 0; i < len; i++) {
    if (parser->err != 0)
      return nparsed;

    /* Update CRC value */
    if (parser->state < s_crc_lo)
      modbus_crc_update(&parser->calc_crc, *data);

    switch (parser->state) {
      case s_slave_addr:
        parser->slave_addr = *data;
        parser->state = s_func;
        CALLBACK_NOTIFY(slave_addr);
        break;

      case s_func:
        parser->function = (enum modbus_func) * data;
        switch (parser->function) {
          case MODBUS_FUNC_READ_COILS:
          case MODBUS_FUNC_READ_DISCRETE_IN:
          case MODBUS_FUNC_READ_HOLD_REG:
          case MODBUS_FUNC_READ_IN_REG:
            parser->state = s_len;
            break;

          case MODBUS_FUNC_WRITE_COIL:
          case MODBUS_FUNC_WRITE_REG:
            parser->state = s_single_addr_hi;
            break;

          case MODBUS_FUNC_WRITE_COILS:
          case MODBUS_FUNC_WRITE_REGS:
            parser->state = s_start_addr_hi;
            break;
        }
        CALLBACK_NOTIFY(function);
        break;

      case s_len:
        parser->data_len = *data;
        parser->state = s_data;
        parser->data_cnt = 0;
        CALLBACK_NOTIFY(data_len);
        break;

      case s_single_addr_hi:
        parser->addr = (uint16_t)*data << 8;
        parser->state = s_single_addr_lo;
        break;

      case s_single_addr_lo:
        parser->addr += *data;
        parser->state = s_data;
        parser->data_cnt = 0;
        parser->data_len = 2;
        CALLBACK_NOTIFY(addr);
        break;

      case s_start_addr_hi:
        parser->addr = (uint16_t)*data << 8;
        parser->state = s_start_addr_lo;
        break;

      case s_start_addr_lo:
        parser->addr += *data;
        parser->state = s_qty_hi;
        CALLBACK_NOTIFY(addr);
        break;

      case s_qty_hi:
        parser->qty = (uint16_t)*data << 8;
        parser->state = s_qty_lo;
        break;

      case s_qty_lo:
        parser->qty += *data;
        parser->state = s_crc_lo;
        CALLBACK_NOTIFY(qty);
        break;

      case s_data:
        if (parser->data_cnt == 0) {
          /* start of data */
          parser->data = data;
          CALLBACK_NOTIFY(data_start);
        }

        parser->data_cnt++;
        if (parser->data_cnt == parser->data_len) {
          /* end data */
          CALLBACK_NOTIFY(data_end);
          parser->state = s_crc_lo;
        }
        break;

      case s_crc_lo:
        parser->frame_crc = *data;
        parser->state = s_crc_hi;
        break;

      case s_crc_hi: {
        parser->frame_crc += (uint16_t)*data << 8;
        parser->state = s_complete;
        if (parser->frame_crc != parser->calc_crc) {
          parser->err = 1; /* TODO: assign right value */
          CALLBACK_NOTIFY(crc_error);
        }
        CALLBACK_NOTIFY(complete);
      } break;

      case s_complete:
        return nparsed;

      default:
        return 0;
    }

    nparsed++;
    data++;
  }

  return nparsed;
}

size_t
modbus_parser_execute(modbus_parser* parser,
                      const modbus_parser_settings* settings,
                      const uint8_t* data,
                      size_t len)
{
  switch (parser->type) {
    case MODBUS_QUERY:
      return parse_query(parser, settings, data, len);

    case MODBUS_RESPONSE:
      return parse_response(parser, settings, data, len);
  }

  return 0;
}

/* Concatenate memory to Modbus Query */
#define MBQ_CAT_MEM(data, len)                                                 \
  do {                                                                         \
    nwrite += len;                                                             \
    if (nwrite > sz) {                                                         \
      return -1;                                                               \
    } else {                                                                   \
      memcpy(buf, data, len);                                                  \
      buf += len;                                                              \
    }                                                                          \
  } while (0)

/* Concatenate single byte to Modbus Query */
#define MBQ_CAT_BYTE(b)                                                        \
  nwrite++;                                                                    \
  do {                                                                         \
    if (nwrite > sz) {                                                         \
      return -1;                                                               \
    } else {                                                                   \
      *buf = b;                                                                \
      buf++;                                                                   \
    }                                                                          \
  } while (0);

/* Concatenate uint16_t as big-endian to modbus query.
 * Modbus uses big-endian for address and data items, except CRC
 */
#define MBQ_CAT_WORD(i)                                                        \
  do {                                                                         \
    nwrite += 2;                                                               \
    if (nwrite > sz) {                                                         \
      return -1;                                                               \
    } else {                                                                   \
      *buf++ = i >> 8;                                                         \
      *buf++ = i & 0x00FF;                                                     \
    }                                                                          \
  } while (0)

int
modbus_gen_query(struct modbus_query* q, uint8_t* buf, size_t sz)
{
  int nwrite = 0;
  uint16_t crc;
  uint8_t* buf_start = buf;

  MBQ_CAT_BYTE(q->slave_addr);
  MBQ_CAT_BYTE(q->function);

  switch (q->function) {
    case MODBUS_FUNC_READ_COILS:
    case MODBUS_FUNC_READ_DISCRETE_IN:
    case MODBUS_FUNC_READ_HOLD_REG:
    case MODBUS_FUNC_READ_IN_REG:
      MBQ_CAT_WORD(q->addr);
      MBQ_CAT_WORD(q->qty);
      break;

    case MODBUS_FUNC_WRITE_COIL:
    case MODBUS_FUNC_WRITE_REG:
      MBQ_CAT_WORD(q->addr);
      /* Check input data */
      if (q->data == NULL || q->data_len != 1)
        return -1;

      // MBQ_CAT_MEM(q->data, q->data_len);
      MBQ_CAT_WORD(*q->data);
      break;

    case MODBUS_FUNC_WRITE_COILS: {
      int16_t nbyte = MODBUS_COILS_BYTE_LEN(q->qty);
      /* Check input data */
      if (q->data == NULL || q->data_len == 0)
        return -1;

      MBQ_CAT_WORD(q->addr);
      MBQ_CAT_WORD(q->qty);
      MBQ_CAT_BYTE(nbyte);

      while (nbyte > 0) {
        nbyte -= 2;
        if (nbyte >= 0) {
          MBQ_CAT_WORD(*q->data);
        } else {
          /* Write last byte, MSB of last register */
          MBQ_CAT_BYTE(*q->data);
        }
        q->data++;
      }
      break;
    }

    case MODBUS_FUNC_WRITE_REGS:
      /* Check input data */
      if (q->data == NULL || q->data_len == 0)
        return -1;

      MBQ_CAT_WORD(q->addr);
      MBQ_CAT_WORD(q->data_len);
      MBQ_CAT_BYTE(q->data_len * 2);

      for (int i = 0; i < q->data_len; i++) {
        MBQ_CAT_WORD(*(q->data + i));
      }
      break;
  }

  crc = modbus_calc_crc(buf_start, nwrite);
  /* CRC must be represent as little-endian */
  MBQ_CAT_BYTE(crc & 0x00FF);
  MBQ_CAT_BYTE(crc >> 8);

  return nwrite;
}
