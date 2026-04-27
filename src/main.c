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


void* UnifiedShowcaseThread(void* arg) {
    (void)arg;
    usleep(1500000); // נותנים לחלון להיפתח

    // --- PHASE 1: BIRTH & PINNING IN NURSERY ---
    // האובייקטים נולדים ב-Nursery. נראה את ה-Bump Pointer זז.
    Handle anchor = mm_malloc(32); 
    write_handle(anchor); // העיגון שומר עליו (ועל כל מי שיחובר אליו) ממוות!
    
    Handle a = mm_malloc(64);
    Handle b = mm_malloc(64);
    Handle c = mm_malloc(64);
    usleep(2000000);

    // --- PHASE 2: TOPOLOGY IN THE NURSERY ---
    // אנחנו מחברים אותם. עכשיו יש להם Edges שמגנים עליהם.
    mem_set_ref(anchor, a);
    mem_set_ref(a, b);
    mem_set_ref(b, c);
    usleep(2000000);

    // --- PHASE 3: THE PROMOTION (CHOKING THE NURSERY) ---
    // עכשיו אנחנו מספימים את ה-Nursery באובייקטים זמניים שלא מעוגנים לכלום.
    // כשה-Nursery יתמלא, ה-defrag_cb שלך יופעל אוטומטית.
    // האובייקטים הזמניים ימותו, אבל anchor, a, b, c ישרדו ויקפצו ל-General Arena!
    for(int i = 0; i < 40; i++) {
        mm_malloc(128); 
        usleep(50000); // השהייה קטנה כדי שהבוחן יראה את ה-Bump מתקדם
    }
    // ברגע זה, הבוחן אמור לראות את הבלוקים עוברים מה-Nursery למעלה, אל ה-General למטה.
    usleep(2000000);

    // --- PHASE 4: CYCLE MERGE IN GENERAL ARENA ---
    // עכשיו כשהם מנוהלים על ידי ה-TLSF, אנחנו סוגרים מעגל.
    mem_set_ref(c, a);
    usleep(2000000);

    // --- PHASE 5: TARJAN FRACTURE ---
    // שוברים את המעגל. Tarjan מזהה ש-C מבודד.
    // C נמחק, והבוחן יראה בלוק של 64 בתים ב-General Arena הופך לירוק/אדום (פנוי).
    mem_remove_ref(b, c);
    usleep(2000000);

    // --- PHASE 6: CLUSTER DEMOLITION & COALESCING ---
    // משחררים את ה-Pin ההתחלתי ששם הכל בחיים מתחילת הטסט.
    clear_handle(anchor);
    // ה-ARC יזהה שהעוגן נמחק, A ימות, ואז B ימות.
    // ה-TLSF ישחרר את הבלוקים שלהם ויאחד (Coalesce) את כל ה-General Arena חזרה לבלוק אחד ענק ופנוי!
    usleep(2000000);

    return NULL;
}

int main() {
    if (!mm_init(DEFAULT_MM_CONFIG(100000))) {
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
    // RunShowcaseTest();
    //run_nursery_tests();
    // run_general_tests();
    // run_graph_tests();
    return 0;
}