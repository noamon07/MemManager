#include <raylib.h>
#include "Strategies/nursery.h"
#include "Strategies/general.h"
#include "Infrastructure/handle.h"
#include <stdio.h>
#include "raylib_visualizer.h"
#include "Infrastructure/graph.h"
#include <raymath.h>

#define SCREEN_WIDTH 1200
#define SCREEN_HEIGHT 800

// Helper to map memory offsets to screen pixels
static int MapOffsetToPixels(uint32_t offset, uint32_t total_size, int screen_width) {
    if (total_size == 0) return 0;
    return (int)(((float)offset / (float)total_size) * screen_width);
}
float GetPercentage(uint32_t used, uint32_t total) {
    if (total == 0) return 0;
    return ((float)used / (float)total) * 100.0f;
}

// פונקציית עזר לקבלת מיקום מרכז משבצת ב-Handle Table לצורך ציור קשתות
Vector2 GetHandleCenter(uint32_t index, int x, int y, int cell_size) {
    int cols = (1200 - 100) / cell_size;
    int row = index / cols;
    int col = index % cols;
    return (Vector2){
        (float)(x + (col * cell_size) + (cell_size / 2)),
        (float)(y + (row * cell_size) + (cell_size / 2))
    };
}
void DrawStats(int x, int y) {
    Nursery* nursery = get_nursery_instance();
    General* general = get_general_instance();

    char buffer[128];
    
    // סטטיסטיקות Nursery
    float nursery_perc = GetPercentage(nursery->bump.alloc_memory, nursery->bump.mem_size);
    sprintf(buffer, "Nursery Usage: %u/%u bytes (%.2f%%)", nursery->bump.alloc_memory, nursery->bump.mem_size, nursery_perc);
    DrawText(buffer, x, y, 14, (nursery_perc > 80.0f) ? ORANGE : DARKGRAY);

    // סטטיסטיקות General
    float general_perc = GetPercentage(general->alloc_memory, general->mem_size);
    sprintf(buffer, "General Usage: %u/%u bytes (%.2f%%)", general->alloc_memory, general->mem_size, general_perc);
    DrawText(buffer, x, y + 20, 14, (general_perc > 80.0f) ? RED : DARKGRAY);
}
void DrawGradientBezier(Vector2 p1, Vector2 c1, Vector2 c2, Vector2 p2, Color colStart, Color colEnd) {
    int segments = 20; // ככל שיש יותר מקטעים, הקו ייראה חלק יותר
    for (int i = 0; i < segments; i++) {
        float t = (float)i / segments;
        float nextT = (float)(i + 1) / segments;

        // חישוב המיקום על עקומת הבזייה בנקודה הנוכחית ובנקודה הבאה
        Vector2 pos = GetSplinePointBezierCubic(p1, c1, c2, p2, t);
        Vector2 nextPos = GetSplinePointBezierCubic(p1, c1, c2, p2, nextT);

        Color currentCol = {
            (unsigned char)(colStart.r + (colEnd.r - colStart.r) * t),
            (unsigned char)(colStart.g + (colEnd.g - colStart.g) * t),
            (unsigned char)(colStart.b + (colEnd.b - colStart.b) * t),
            255
        };

        DrawLineEx(pos, nextPos, 2.0f, currentCol);
    }
}
void DrawGraphEdges(int x, int y, int cell_size) {
    HandleTable* table = mm_get_handle_table_instance();
    if (!table) return;

    for (uint32_t i = 0; i < table->size; i++) {
        HandleEntry* entry = handle_table_get_entry_by_index(i);
        if (!entry || !entry->is_allocated || entry->first_edge_offset == INVALID_DATA_OFFSET) continue;

        Vector2 startPos = GetHandleCenter(i, x, y, cell_size);
        
        uint32_t edge_offset = entry->first_edge_offset;
        while (edge_offset != INVALID_DATA_OFFSET) {
            Edge edge = graph_get_edge(edge_offset); 
            if (edge.child_handle.index == INVALID_INDEX) break;
            Vector2 endPos = GetHandleCenter(edge.child_handle.index, x, y, cell_size);
            Vector2 cp1 = { startPos.x, startPos.y - 50 };
            Vector2 cp2 = { endPos.x, endPos.y - 50 };
            DrawGradientBezier(startPos, cp1, cp2, endPos, BLUE, RED);
            edge_offset = edge.next_edge_offset;
        }
    }
}
void DrawNurseryArena(int x, int y, int width, int height) {
    Nursery* nursery = get_nursery_instance();
    if (!nursery || !nursery->bump.mem) return;

    // Background for the whole arena capacity
    DrawRectangle(x, y, width, height, DARKGRAY);
    DrawRectangleLines(x, y, width, height, WHITE);

    uint32_t current_offset = 0;
    while (current_offset < nursery->bump.cur_index) {
        BaseHeader* header = (BaseHeader*)(nursery->bump.mem + current_offset);
        uint32_t block_bytes = HEADER_SIZE_TO_BYTES(header->size);
        if (block_bytes == 0) break; // Prevent infinite loop on corruption

        int blockX = x + MapOffsetToPixels(current_offset, nursery->bump.mem_size, width);
        int blockW = MapOffsetToPixels(block_bytes, nursery->bump.mem_size, width);
        if (blockW < 1) blockW = 1; // Always show at least 1 pixel

        Color blockColor = header->is_allocated ? SKYBLUE : RED;
        
        // Darker blue for promoted/older generations
        if (header->is_allocated && header->custom_flags > 0) {
            blockColor = BLUE; 
        }

        DrawRectangle(blockX, y, blockW, height, blockColor);
        DrawRectangleLines(blockX, y, blockW, height, BLACK); // Boundary tags

        current_offset += block_bytes;
    }

    // Draw Bump Pointer (Frontier)
    int bumpX = x + MapOffsetToPixels(nursery->bump.cur_index, nursery->bump.mem_size, width);
    DrawLineEx((Vector2){bumpX, y - 10}, (Vector2){bumpX, y + height + 10}, 3.0f, RED);
    DrawText("BUMP POINTER", bumpX - 40, y - 25, 10, RED);
}

void DrawGeneralArena(int x, int y, int width, int height) {
    General* general = get_general_instance();
    if (!general || !general->mem) return;

    DrawRectangle(x, y, width, height, DARKGRAY);
    DrawRectangleLines(x, y, width, height, WHITE);

    uint32_t current_offset = 0;
    while (current_offset < general->mem_size) {
        BaseHeader* header = (BaseHeader*)(general->mem + current_offset);
        uint32_t block_bytes = HEADER_SIZE_TO_BYTES(header->size);
        if (block_bytes == 0) break;

        int blockX = x + MapOffsetToPixels(current_offset, general->mem_size, width);
        int blockW = MapOffsetToPixels(block_bytes, general->mem_size, width);
        if (blockW < 1) blockW = 1;

        Color blockColor = header->is_allocated ? LIME : MAROON;

        DrawRectangle(blockX, y, blockW, height, blockColor);
        DrawRectangleLines(blockX, y, blockW, height, BLACK);

        current_offset += block_bytes;
    }
}

void DrawHandleTable(int x, int y, int cell_size) {
    HandleTable* table = mm_get_handle_table_instance();
    if (!table || table->size == 0) return;

    int cols = (SCREEN_WIDTH - 100) / cell_size;
    
    for (uint32_t i = 0; i < table->size; i++) {
        HandleEntry* entry = handle_table_get_entry_by_index(i);
        if (!entry) continue;

        int row = i / cols;
        int col = i % cols;
        int drawX = x + (col * cell_size);
        int drawY = y + (row * cell_size);

        Color c = DARKGRAY;
        if (entry->is_allocated) {
            c = ORANGE; // Live Handle
            if (entry->is_scc_root) c = PURPLE; // Highlight SCC Roots
        }

        DrawRectangle(drawX, drawY, cell_size - 2, cell_size - 2, c);
    }
}

// Call this from a dedicated thread, or at the end of your main game loop!
void RunMemoryVisualizer() {
    InitWindow(1200, 800, "Memory Manager Visualizer");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // 1. כותרת וסטטיסטיקות
        DrawText("MEMORY MANAGER REAL-TIME VISUALIZATION", 20, 20, 20, BLACK);
        DrawStats(900, 20); 

        // 2. Nursery
        DrawText("NURSERY", 50, 80, 16, DARKGRAY);
        DrawNurseryArena(50, 105, 1100, 50);

        // 3. General
        DrawText("GENERAL", 50, 190, 16, DARKGRAY);
        DrawGeneralArena(50, 215, 1100, 50);

        // 4. Handle Table & Graph
        DrawText("HANDLE TOPOLOGY & GRAPH EDGES", 50, 330, 16, DARKGRAY);
        DrawHandleTable(50, 360, 20); // הגדלנו מעט את המשבצות ל-20
        DrawGraphEdges(50, 360, 20); // ציור הקשתות מעל המשבצות

        // 5. מקרא (Legend) - מעודכן
        DrawRectangle(50, 750, 15, 15, SKYBLUE); DrawText("Nursery", 70, 750, 12, BLACK);
        DrawRectangle(150, 750, 15, 15, LIME); DrawText("General", 170, 750, 12, BLACK);
        DrawRectangle(250, 750, 15, 15, PURPLE); DrawText("SCC Root", 270, 750, 12, BLACK);
        DrawLine(370, 757, 400, 757, BLUE); DrawText("Reference Origin", 410, 750, 12, BLACK);
        DrawLine(560, 757, 590, 757, RED); DrawText("Reference Destination", 600, 750, 12, BLACK);

        EndDrawing();
    }
    CloseWindow();
}