#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define FIFO_PATH "/tmp/temp_pipe"
#define MAX_READINGS 15

// ANSI color codes
#define CLEAR_SCREEN "\033[2J\033[H"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"
#define RESET "\033[0m"
#define BOLD "\033[1m"

typedef struct {
    float temperature;
    char time_str[16];
} TempReading;

static int running = 1;
static TempReading readings[MAX_READINGS];
static int reading_count = 0;
static float current_temp = 0.0;
static char status_msg[128] = "Starting...";

// Simple temperature parser - looks for "temp_c":XX.XX pattern
float extract_temperature(const char* line) {
    char* pos = strstr(line, "temp_c");
    if (!pos) return -999.0;
    
    // Look for the number after the colon
    pos = strchr(pos, ':');
    if (!pos) return -999.0;
    
    pos++; // Move past ':'
    while (*pos == ' ' || *pos == '"') pos++; // Skip spaces and quotes
    
    return atof(pos);
}

void handle_signal(int sig) {
    running = 0;
}

void add_temperature(float temp) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    // Shift array if full
    if (reading_count >= MAX_READINGS) {
        for (int i = 0; i < MAX_READINGS - 1; i++) {
            readings[i] = readings[i + 1];
        }
        reading_count = MAX_READINGS - 1;
    }
    
    // Add new reading
    readings[reading_count].temperature = temp;
    strftime(readings[reading_count].time_str, 16, "%H:%M:%S", tm_info);
    reading_count++;
    current_temp = temp;
}

const char* temp_color(float temp) {
    if (temp < 20.0) return BLUE;
    else if (temp < 25.0) return GREEN;
    else if (temp < 30.0) return YELLOW;
    else return RED;
}

void draw_screen() {
    printf(CLEAR_SCREEN);
    
    // Header
    printf(BOLD CYAN);
    printf("===============================================\n");
    printf("       TEMPERATURE MONITOR\n");
    printf("===============================================\n");
    printf(RESET "\n");
    
    // Current temperature display
    if (current_temp > -900) {
        printf(BOLD "CURRENT TEMPERATURE:\n");
        printf("%s          %6.1f°C\n", temp_color(current_temp), current_temp);
        printf(RESET "\n");
        
        // Temperature bar visualization
        printf("Temperature Scale:\n");
        printf("15°C ");
        int pos = (int)((current_temp - 15.0) / 20.0 * 30);
        if (pos < 0) pos = 0;
        if (pos > 30) pos = 30;
        
        for (int i = 0; i < 30; i++) {
            if (i == pos) {
                printf("%s|%s", temp_color(current_temp), RESET);
            } else if (i % 5 == 0) {
                printf(":");
            } else {
                printf("-");
            }
        }
        printf(" 35°C\n\n");
        
    } else {
        printf(BOLD RED "CURRENT TEMPERATURE:\n");
        printf("        NO DATA\n");
        printf(RESET "\n");
    }
    
    // Status
    printf("Status: %s%s%s\n\n", YELLOW, status_msg, RESET);
    
    // Statistics
    if (reading_count > 0) {
        float min_t = readings[0].temperature;
        float max_t = readings[0].temperature;
        float sum = 0;
        
        for (int i = 0; i < reading_count; i++) {
            if (readings[i].temperature < min_t) min_t = readings[i].temperature;
            if (readings[i].temperature > max_t) max_t = readings[i].temperature;
            sum += readings[i].temperature;
        }
        
        printf("STATISTICS (Last %d readings):\n", reading_count);
        printf("Min: %s%.1f°C%s  Max: %s%.1f°C%s  Avg: %s%.1f°C%s\n\n",
               temp_color(min_t), min_t, RESET,
               temp_color(max_t), max_t, RESET,
               temp_color(sum/reading_count), sum/reading_count, RESET);
    }
    
    // History table
    printf("RECENT READINGS:\n");
    printf("─────────────────────────────\n");
    printf("  Time    │ Temperature\n");
    printf("─────────────────────────────\n");
    
    // Show readings from newest to oldest
    for (int i = reading_count - 1; i >= 0; i--) {
        printf("  %s │ %s%6.1f°C%s\n", 
               readings[i].time_str,
               temp_color(readings[i].temperature),
               readings[i].temperature,
               RESET);
    }
    
    if (reading_count == 0) {
        printf("  No readings yet\n");
    }
    
    printf("─────────────────────────────\n\n");
    
    // Footer
    printf("Reading from: %s\n", FIFO_PATH);
    printf("Press Ctrl+C to exit\n");
    
    fflush(stdout);
}

int main() {
    FILE* fifo_file;
    char line[256];
    time_t last_display = 0;
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    printf("Temperature Monitor GUI Starting...\n");
    printf("Waiting for data from %s...\n", FIFO_PATH);
    sleep(2);
    
    while (running) {
        // Try to open FIFO for reading
        fifo_file = fopen(FIFO_PATH, "r");
        
        if (!fifo_file) {
            snprintf(status_msg, sizeof(status_msg), "FIFO not found - waiting...");
            draw_screen();
            sleep(2);
            continue;
        }
        
        snprintf(status_msg, sizeof(status_msg), "Connected - reading data...");
        
        // Read lines from FIFO
        while (running && fgets(line, sizeof(line), fifo_file)) {
            // Remove newline
            line[strcspn(line, "\n")] = 0;
            
            // Try to extract temperature
            float temp = extract_temperature(line);
            
            if (temp > -900) {  // Valid temperature
                add_temperature(temp);
                snprintf(status_msg, sizeof(status_msg), "Connected ✓ (%.1f°C)", temp);
            }
            
            // Update display every second
            time_t now = time(NULL);
            if (now != last_display) {
                draw_screen();
                last_display = now;
            }
        }
        
        fclose(fifo_file);
        
        if (running) {
            snprintf(status_msg, sizeof(status_msg), "Connection lost - reconnecting...");
            draw_screen();
            sleep(2);
        }
    }
    
    printf(CLEAR_SCREEN);
    printf("Temperature Monitor shutting down.\n");
    return 0;
}
