/*
 * The macro definitions for debug
 *
 * These macros are defined in static. If you want to use debug macro, you can
 * use as following code:
 *
 * In your C/C++ file, enable/disable DEBUG_ENABLE macro, and then include this
 * header file.
 *
 * When the LEVEL_TYPE is 0:
//#define DBG_LVL           DBG_INFO
#define DBG_LVL           DBG_LOG
//#define DBG_LVL           DBG_NODBG
#include <mydbg.h>          // must after of DBG_LVL, DBG_TAG or other options
 * 
 * When the LEVEL_TYPE is 1:
#define DBG_TAG           "MOD_TAG"
#define DBG_LVL           (DBG_LOG | DBG_INFO | DBG_WARNING | DBG_ERROR)
#include <mydbg.h>          // must after of DBG_LVL, DBG_TAG or other options
 * 
 * Then in your C/C++ file, you can use LOG_X macro to print out logs:
 * log_d("this is a debug log!");
 * log_e("this is a error log!");
 * 
 */

#ifndef MY_DBG_H__
#define MY_DBG_H__
#include "stdio.h"
#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif

/* config */
#ifndef _kprintf
#define _kprintf    printf                        // 定义串口输出函数
//#define _kprintf    pr_emerg                      // 定义串口输出函数
#endif
#define DBG_ENABLE                                // 使能debug
// #define DBG_COLOR                                 // 使能颜色信息
#define WRAP_TYPE     "\r\n"                      // 换行类型
#define LEVEL_TYPE    0                           // 等级控制类型 0:使用大于等级输出  1:使用每一位控制输出
#define NODBG         1                           // 全局debug开关

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

// Refer to include/rtcompiler.h
#if defined (__ARMCC_VERSION)            /* ARM Compiler windows */
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#elif defined (__IAR_SYSTEMS_ICC__)     /* for IAR Compiler windows */
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#elif defined (__GNUC__)                /* GNU GCC Compiler */
  #if defined (__WINDOWS__)
    #define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
  #else
    #define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
  #endif
#elif defined (_MSC_VER)                /* for Visual Studio Compiler */

#else
  #error "Error compiler."
#endif

/* DEBUG level */
#if LEVEL_TYPE == 0
  #define DBG_NODBG           0
  #define DBG_ERROR           1
  #define DBG_WARNING         2
  #define DBG_INFO            3
  #define DBG_LOG             4
#elif LEVEL_TYPE == 1
  #define DBG_LOG             1
  #define DBG_INFO            2
  #define DBG_WARNING         4
  #define DBG_ERROR           8
#endif

#ifdef DBG_TAG
  #ifndef DBG_SECTION_NAME
    #define DBG_SECTION_NAME    DBG_TAG
  #endif
#else
/* compatible with old version */
  #ifndef DBG_SECTION_NAME
    #define DBG_SECTION_NAME    __FILENAME__
  #endif
#endif /* DBG_TAG */

#ifdef DBG_ENABLE

#ifdef DBG_LVL
#ifndef DBG_LEVEL
#define DBG_LEVEL         DBG_LVL
#endif
#else
/* compatible with old version */
#ifndef DBG_LEVEL
#define DBG_LEVEL         DBG_WARNING
#endif
#endif /* DBG_LVL */

/*
 * The color for terminal (foreground)
 * BLACK    30
 * RED      31
 * GREEN    32
 * YELLOW   33
 * BLUE     34
 * PURPLE   35
 * CYAN     36
 * WHITE    37
 */
#ifdef DBG_COLOR
  #define _DBG_COLOR(n)        _kprintf("\033["#n"m")
  #define _DBG_LOG_HDR(lvl_name, color_n)                    \
      _kprintf("\033["#color_n"m[" lvl_name "/" "%s" "] ",DBG_SECTION_NAME)
  #define _DBG_LOG_X_END                                     \
      _kprintf("\033[0m"WRAP_TYPE)
#else /* DBG_COLOR */
  #define _DBG_COLOR(n)
  #define _DBG_LOG_HDR(lvl_name, color_n)                    \
      _kprintf("[" lvl_name "/" "%s" "] ",DBG_SECTION_NAME)
  #define _DBG_LOG_X_END                                     \
      _kprintf(WRAP_TYPE)
#endif /* DBG_COLOR */

/*
 * static debug routine
 * NOTE: This is a NOT RECOMMENDED API. Please using LOG_X API.
 *       It will be DISCARDED later. Because it will take up more resources.
 */
#define dbg_log(level, fmt, ...)                            \
    if ((level) <= DBG_LEVEL)                               \
    {                                                       \
        switch(level)                                       \
        {                                                   \
            case DBG_ERROR:   _DBG_LOG_HDR("E", 31); break; \
            case DBG_WARNING: _DBG_LOG_HDR("W", 33); break; \
            case DBG_INFO:    _DBG_LOG_HDR("I", 32); break; \
            case DBG_LOG:     _DBG_LOG_HDR("D", 0); break;  \
            default: break;                                 \
        }                                                   \
        _kprintf(fmt, ##__VA_ARGS__);                       \
        _DBG_COLOR(0);                                      \
    }

#if NODBG == 0
  #define dbg_here                                            \
      if ((DBG_LEVEL) <= DBG_LOG){                            \
          _kprintf( "%s Here %s:%d\r\n" ,        \
              DBG_SECTION_NAME,__FUNCTION__, __LINE__);       \
      }

#ifdef DBG_COLOR
  #define _dbg_here(color_n)                                   \
      if ((DBG_LEVEL) <= DBG_LOG){                            \
          _kprintf("\033["#color_n"m[" "%s"                   \
          "] [%s:%d] Here\033[0m\r\n" ,DBG_SECTION_NAME,         \
          __FUNCTION__, __LINE__);                            \
      }

  #define DBG_HERE   _dbg_here(33)
#else  /* DBG_COLOR */
  #define _dbg_here(color_n)                                   \
      if ((DBG_LEVEL) <= DBG_LOG){                            \
          _kprintf("[" "%s"                   \
          "] [%s:%d] Here\r\n" ,DBG_SECTION_NAME,         \
          __FUNCTION__, __LINE__);                            \
      }

  #define DBG_HERE   _dbg_here(33)
#endif  /* DBG_COLOR */

#else   /* NODBG == 0 */
  #define _dbg_here(...)
  #define dbg_here
  #define DBG_HERE
#endif  /* NODBG == 0 */

/*
#define dbg_log_line(lvl, color_n, fmt, ...)                \
    do                                                      \
    {                                                       \
        _DBG_LOG_HDR(lvl, color_n);                         \
        _kprintf(fmt, ##__VA_ARGS__);                       \
        _DBG_LOG_X_END;                                     \
    }                                                       \
    while (0)
*/
#ifdef DBG_COLOR
  #define dbg_log_line(lvl, color_n, fmt, ...)                \
      do                                                      \
      {                                                       \
          _kprintf("\033["#color_n"m[" lvl "/" "%s" "] "      \
          fmt "\033[0m" WRAP_TYPE,                            \
          DBG_SECTION_NAME ,##__VA_ARGS__);                   \
      }                                                       \
      while (0)

  #define dbg_log_line_fun(lvl, color_n, fmt, ...)            \
      do                                                      \
      {                                                       \
          _kprintf("\033["#color_n"m[" lvl "/" "%s" "] [%s] " \
          fmt "\033[0m" WRAP_TYPE,                            \
          DBG_SECTION_NAME, __func__ ,##__VA_ARGS__);         \
      }                                                       \
      while (0)

  #define dbg_log_line_fun_num(lvl, color_n, fmt, ...)        \
      do                                                      \
      {                                                       \
          _kprintf("\033["#color_n"m[" lvl "/" "%s" "] [%s:%d] " \
          fmt "\033[0m" WRAP_TYPE,                            \
          DBG_SECTION_NAME, __func__ , __LINE__ ,##__VA_ARGS__); \
      }                                                       \
      while (0)

#else /* DBG_COLOR */
  #define dbg_log_line(lvl, color_n, fmt, ...)                \
      do                                                      \
      {                                                       \
          _kprintf("[" lvl "/" "%s" "] " \
          fmt WRAP_TYPE, DBG_SECTION_NAME, ##__VA_ARGS__);    \
      }                                                       \
      while (0)

  #define dbg_log_line_fun(lvl, color_n, fmt, ...)            \
      do                                                      \
      {                                                       \
          _kprintf("[" lvl "/" "%s" "] [%s] " \
          fmt WRAP_TYPE, DBG_SECTION_NAME,                    \
          __func__ ,##__VA_ARGS__);                           \
      }                                                       \
      while (0)

  #define dbg_log_line_fun_num(lvl, color_n, fmt, ...)        \
      do                                                      \
      {                                                       \
          _kprintf("[" lvl "/" "%s" "] [%s:%d] " \
          fmt WRAP_TYPE, DBG_SECTION_NAME,                    \
          __func__ , __LINE__ ,##__VA_ARGS__);                \
      }                                                       \
      while (0)
#endif /* DBG_COLOR */

#define dbg_raw(...)         _kprintf(__VA_ARGS__);

#else /* DBG_ENABLE */
  #define dbg_log(level, fmt, ...)
  #define dbg_here
  #define DBG_HERE
  #define dbg_enter
  #define dbg_exit
  #define dbg_log_line(lvl, color_n, fmt, ...)
  #define dbg_raw(...)
#endif /* DBG_ENABLE */

#if NODBG == 0            /* NODBG */
#if LEVEL_TYPE == 0       /* LEVEL_TYPE */

#if (DBG_LEVEL >= DBG_LOG)
  #define log_draw(...)        _kprintf(__VA_ARGS__)
  #define log_drawc(...)       _kprintf(__VA_ARGS__)
  #define log_d(fmt, ...)      dbg_log_line("D", 0, fmt, ##__VA_ARGS__)
  #define logf_d(fmt, ...)     dbg_log_line_fun("D", 0, fmt, ##__VA_ARGS__)
  #define logfn_d(fmt, ...)    dbg_log_line_fun_num("D", 0, fmt, ##__VA_ARGS__)
#else
  #define log_draw(...)
  #define log_drawc(...)
  #define log_d(...)
  #define logf_d(...)
  #define logfn_d(...)
#endif

#if (DBG_LEVEL >= DBG_INFO)
  #define log_iraw(...)        _kprintf(__VA_ARGS__)
  #define log_irawc(...)       do{_kprintf("\033[32m");_kprintf(__VA_ARGS__);_kprintf("\033[0m");}while(0)
  #define log_i(fmt, ...)      dbg_log_line("I", 32, fmt, ##__VA_ARGS__)
  #define logf_i(fmt, ...)     dbg_log_line_fun("I", 32, fmt, ##__VA_ARGS__)
  #define logfn_i(fmt, ...)    dbg_log_line_fun_num("I", 32, fmt, ##__VA_ARGS__)
#else
  #define log_iraw(...)
  #define log_irawc(...)
  #define log_i(...)
  #define logf_i(...)
  #define logfn_i(...)
#endif

#if (DBG_LEVEL >= DBG_WARNING)
  #define log_wraw(...)        _kprintf(__VA_ARGS__)
  #define log_wrawc(...)       do{_kprintf("\033[33m");_kprintf(__VA_ARGS__);_kprintf("\033[0m");}while(0)
  #define log_w(fmt, ...)      dbg_log_line("W", 33, fmt, ##__VA_ARGS__)
  #define logf_w(fmt, ...)     dbg_log_line_fun("W", 33, fmt, ##__VA_ARGS__)
  #define logfn_w(fmt, ...)    dbg_log_line_fun_num("W", 33, fmt, ##__VA_ARGS__)
#else
  #define log_wraw(...)
  #define log_wrawc(...)
  #define log_w(...)
  #define logf_w(...)
  #define logfn_w(...)
#endif

#if (DBG_LEVEL >= DBG_ERROR)
  #define log_eraw(...)        _kprintf(__VA_ARGS__)
  #define log_erawc(...)       do{_kprintf("\033[31m");_kprintf(__VA_ARGS__);_kprintf("\033[0m");}while(0)
  #define log_e(fmt, ...)      dbg_log_line("E", 31, fmt, ##__VA_ARGS__)
  #define logf_e(fmt, ...)     dbg_log_line_fun("E", 31, fmt, ##__VA_ARGS__)
  #define logfn_e(fmt, ...)    dbg_log_line_fun_num("E", 31, fmt, ##__VA_ARGS__)
#else
  #define log_eraw(...)
  #define log_erawc(...)
  #define log_e(...)
  #define logf_e(...)
  #define logfn_e(...)
#endif

#define LOG_RAW(...)         dbg_raw(__VA_ARGS__)
#define log_raw   LOG_RAW

#elif LEVEL_TYPE == 1       /* LEVEL_TYPE */

#if (DBG_LEVEL & 0x01)
  #define log_d(fmt, ...)      dbg_log_line("D", 0, fmt, ##__VA_ARGS__)
  #define logf_d(fmt, ...)     dbg_log_line_fun("D", 0, fmt, ##__VA_ARGS__)
#else
  #define log_d(...)
  #define logf_d(...)
#endif

#if (DBG_LEVEL & 0x02)
  #define log_i(fmt, ...)      dbg_log_line("I", 32, fmt, ##__VA_ARGS__)
  #define logf_i(fmt, ...)     dbg_log_line_fun("I", 32, fmt, ##__VA_ARGS__)
#else
  #define log_i(...)
  #define logf_i(...)
#endif

#if (DBG_LEVEL & 0x04)
  #define log_w(fmt, ...)      dbg_log_line("W", 33, fmt, ##__VA_ARGS__)
  #define logf_w(fmt, ...)     dbg_log_line_fun("W", 33, fmt, ##__VA_ARGS__)
#else
  #define log_w(...)
  #define logf_w(...)
#endif

#if (DBG_LEVEL & 0x08)
  #define log_e(fmt, ...)      dbg_log_line("E", 31, fmt, ##__VA_ARGS__)
  #define logf_e(fmt, ...)     dbg_log_line_fun("E", 31, fmt, ##__VA_ARGS__)
#else
  #define log_e(...)
  #define logf_e(...)
#endif

#define LOG_RAW(...)         dbg_raw(__VA_ARGS__)
#define log_raw   LOG_RAW

#endif  /* LEVEL_TYPE */
#elif NODBG == 1            /* NODBG */
  #define log_d(...)
  #define log_i(...)
  #define log_w(...)
  #define log_e(...)
  #define logf_d(...)
  #define logf_i(...)
  #define logf_w(...)
  #define logf_e(...)
  #define logfn_d(...)
  #define logfn_i(...)
  #define logfn_w(...)
  #define logfn_e(...)
  #define log_raw(...)        // 可用于 AT 打印信息
  #define log_draw(...)
  #define log_iraw(...)
  #define log_wraw(...)
  #define log_eraw(...)
  #define log_drawc(...)
  #define log_irawc(...)
  #define log_wrawc(...)
  #define log_erawc(...)
  #define LOG_RAW(...)
#endif  /* NODBG */

#ifdef DBG_ENABLE_AT
  #define logln_at(fmt, ...)      {_kprintf(__VA_ARGS__);_kprintf(WRAP_TYPE);}
  #define log_at(fmt, ...)        {_kprintf(__VA_ARGS__);}
#else
  #define logln_at(...)
  #define log_at(...)
#endif

#define LOG_D log_d
#define LOGF_D logf_d
#define LOGFN_D logfn_d
#define LOG_E log_e
#define LOGF_E logf_e
#define LOGFN_E logfn_e
#define LOG_I log_i
#define LOGF_I logf_i
#define LOGFN_I logfn_i
#define LOG_W log_w
#define LOGF_W logf_w
#define LOGFN_W logfn_w
#define LOG_AT log_at

/* assert */
static inline void _assert_handler(const char *ex_string, const char *func, unsigned int line)
{
    volatile char dummy = 0;
    {
        _kprintf("(%s) assertion failed at function:%s, line number:%d " WRAP_TYPE, ex_string, func, line);
        while (dummy == 0);
    }
}

#define _ASSERT(EX)                                                         \
if (!(EX))                                                                    \
{                                                                             \
    _assert_handler(#EX, __FUNCTION__, __LINE__);                           \
}

#ifdef __cplusplus
}
#endif

#endif /* MY_DBG_H__ */
