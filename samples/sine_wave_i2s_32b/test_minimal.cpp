#include <stdio.h>
#include "pico/stdlib.h"

int main() {
    stdio_init_all();
    
    // 2秒待機（USBシリアル安定化）
    sleep_ms(2000);
    
    printf("=== MINIMAL TEST START ===\n");
    
    int counter = 0;
    while (true) {
        printf("Counter: %d\n", counter++);
        sleep_ms(1000);
        
        if (counter > 10) break;
    }
    
    printf("=== TEST COMPLETE ===\n");
    return 0;
}