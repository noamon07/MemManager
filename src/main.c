#include "Interface/mem_manager.h"
#include "mem_visualizer.h"
#include "tests/nursery_stress.h"
#include "tests/general_stress.h"
#include "tests/graph_test.h"
#include <stdio.h>
#include <assert.h>
#include "raylib_visualizer.h"
#include <pthread.h>
#include <unistd.h>

void* AllocationTestThread(void* arg) {
    (void)arg;
    
    // ניתן לחלון הגרפיקה חצי שנייה להיפתח לפני שנתחיל להקצות
    usleep(500000); 

    printf("Starting live allocations...\n");
    // Handle handles[50];
    
    // // ניצור אובייקטים בהדרגה כדי לראות את פס ההקצאה (Bump Pointer) זז
    // for (int i = 0; i < 50; i++) {
    //     handles[i] = mm_malloc(128);
    //     write_handle(handles[i]); // עיגון כדי שה-GC לא ימחק אותם
        
    //     // עצירה של 0.1 שניות בין הקצאה להקצאה כדי שהעין תספיק לראות את זה!
    //     usleep(100000); 
    // }

    // printf("Freeing some objects to create holes...\n");
    
    // // נשחרר כל אובייקט זוגי כדי לראות את הבלוקים האדומים (Holes) מופיעים
    // for (int i = 0; i < 50; i += 2) {
    //     clear_handle(handles[i]);
    //     mm_free(handles[i]);
        
    //     usleep(100000); // עצירה לראות את השחרור בזמן אמת
    // }
    Handle h = mm_malloc(500);
    write_handle(h);
    Handle h1 =mm_malloc(500);
    Handle h2 = mm_malloc(500);
    write_handle(h2);
    mm_free(h1);
    mem_set_ref(h,h2);
    graph_visualize_all();
    printf("Test finished. Close the window to exit.\n");
    return NULL;
}

int main() {
    // if (!mm_init(1024 * 1024)) {
    //     printf("Failed to init memory manager!\n");
    //     return -1;
    // }
    // pthread_t test_thread;
    // if (pthread_create(&test_thread, NULL, &AllocationTestThread, NULL) != 0) {
    //     printf("Failed to create test thread!\n");
    //     return -1;
    // }
    // RunMemoryVisualizer();
    // pthread_join(test_thread, NULL);
    // mm_destroy();
    //run_nursery_tests();
    //run_general_tests();
    run_graph_tests();
    return 0;
}