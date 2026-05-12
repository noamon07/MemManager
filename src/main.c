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

// --- VISUALIZATION TIMING (Microseconds) ---
#define SLEEP_WINDOW_OPEN    1500000
#define SLEEP_PHASE_PAUSE    2000000
#define SLEEP_BUMP_STEP      50000

// --- MEMORY ARCHITECTURE DEFINES ---
#define TOTAL_MEM_SIZE       100000
#define NURSERY_SPAM_COUNT   40
#define SIZE_SMALL_OBJ       32
#define SIZE_MED_OBJ         64
#define SIZE_SPAM_OBJ        128

void* UnifiedShowcaseThread(void* arg) {
    (void)arg;
    usleep(SLEEP_WINDOW_OPEN); // נותנים לחלון להיפתח

    // --- PHASE 1: BIRTH & PINNING IN NURSERY ---
    // האובייקטים נולדים ב-Nursery. נראה את ה-Bump Pointer זז.
    Handle anchor = mm_malloc(SIZE_SMALL_OBJ); 
    write_handle(anchor); // העיגון שומר עליו (ועל כל מי שיחובר אליו) ממוות!
    
    Handle a = mm_malloc(SIZE_MED_OBJ);
    Handle b = mm_malloc(SIZE_MED_OBJ);
    Handle c = mm_malloc(SIZE_MED_OBJ);
    usleep(SLEEP_PHASE_PAUSE);

    // --- PHASE 2: TOPOLOGY IN THE NURSERY ---
    mem_set_ref(anchor, a);
    mem_set_ref(a, b);
    mem_set_ref(b, c);
    usleep(SLEEP_PHASE_PAUSE);

    // --- PHASE 3: THE PROMOTION (CHOKING THE NURSERY) ---
    // האובייקטים הזמניים ימותו, אבל anchor, a, b, c ישרדו ויקפצו ל-General Arena!
    for(int i = 0; i < NURSERY_SPAM_COUNT; i++) {
        mm_malloc(SIZE_SPAM_OBJ); 
        usleep(SLEEP_BUMP_STEP); // השהייה קטנה כדי שהבוחן יראה את ה-Bump מתקדם
    }
    usleep(SLEEP_PHASE_PAUSE);

    // --- PHASE 4: CYCLE MERGE IN GENERAL ARENA ---
    mem_set_ref(c, a);
    usleep(SLEEP_PHASE_PAUSE);

    // --- PHASE 5: TARJAN FRACTURE ---
    mem_remove_ref(b, c);
    usleep(SLEEP_PHASE_PAUSE);

    // --- PHASE 6: CLUSTER DEMOLITION & COALESCING ---
    clear_handle(anchor);
    usleep(SLEEP_PHASE_PAUSE);

    return NULL;
}

int main() {
    if (!mm_init(DEFAULT_MM_CONFIG(TOTAL_MEM_SIZE))) {
        printf("Failed to init memory manager!\n");
        return -1;
    }

    pthread_t test_thread;
    if (pthread_create(&test_thread, NULL, &UnifiedShowcaseThread, NULL) != 0) {
        printf("Failed to create test thread!\n");
        return -1;
    }

    RunMemoryVisualizer();
    pthread_join(test_thread, NULL);
    mm_destroy();

    return 0;
}