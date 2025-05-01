/*
================================================================================
Log Level Configuration Guide:

| Level Macro         | Value | Description                                                                 |
|---------------------|-------|-----------------------------------------------------------------------------|
| LOG_LEVEL_NONE      | 0     | No logs printed.                                                            |
| LOG_LEVEL_BASIC     | 1     | Only the message (printf style).                                           |
| LOG_LEVEL_TIME      | 2     | Timestamp + log level + message.                                           |
| LOG_LEVEL_FULL      | 3     | Timestamp + log level + file + line + function + message.                  |

To change log verbosity, set LOG_LEVEL macro below or pass -DLOG_LEVEL=value during compilation.
================================================================================
*/

#include <stdio.h>
#include <string.h>
#include <time.h>

// Logging Levels
#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_BASIC 1
#define LOG_LEVEL_TIME  2
#define LOG_LEVEL_FULL  3

/* ========== CONFIGURATION ========== */
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_FULL  // 0-3 (3 = full debug)
#endif

/* ========== IMPLEMENTATION ========== */
static char* _timestamp() {
    static char buf[20];
    time_t now = time(NULL);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return buf;
}

#define LOG(level, tag, fmt, ...) \
    do { \
        if (level <= LOG_LEVEL) { \
            if (LOG_LEVEL == 1) { \
                printf(tag " " fmt "\n", ##__VA_ARGS__); \
            } else if (LOG_LEVEL == 2) { \
                printf("%s " tag " " fmt "\n", _timestamp(), ##__VA_ARGS__); \
            } else if (LOG_LEVEL >= 3) { \
                printf("%s %-5s [%s:%-4d - %-20s] " fmt "\n", \
                      _timestamp(), tag, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
            } \
        } \
    } while(0)

/* ========== LOG MACROS ========== */
#define LOG_INFO(fmt, ...)  LOG(1, "INFO", fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG(1, "WARN", fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG(1, "ERROR", fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG(1, "DEBUG", fmt, ##__VA_ARGS__)

/* ========== DEMO ========== */
void test_function() {
    LOG_DEBUG("Inside test function");
}

int main() {
    printf("\n=== Testing Log Level %d ===\n", LOG_LEVEL);
    
    LOG_INFO("Application started");
    LOG_WARN("Memory usage is high: %dMB", 875);
    LOG_ERROR("Failed to open file: %s", "data.txt");
    test_function();
    LOG_DEBUG("Current counter value: %d", 42);
    
    int x = 10, y = 20;
    LOG_INFO("Calculating %d + %d", x, y);
    LOG_DEBUG("Intermediate result: %d", x + y);
    
    printf("=== Test complete ===\n\n");
    return 0;
}
