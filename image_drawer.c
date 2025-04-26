/*
 * Modified Image Viewer/Drawer with .vd File Input & Text Background
 * Points require labels: point(x,y,label) without quotes
 * Lines reference point labels: line(label1,label2) without quotes
 * Fixed console flooding by removing repeated printf in draw_thick_line
 * Increased line thickness and changed color to red for visibility
 * Simplified draw_thick_line to test single-line rendering
 */

#define _CRT_SECURE_NO_WARNINGS
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// --- Struct Definitions ---
typedef struct {
    int x;
    int y;
    char* label; // Mandatory label
} Point;

typedef struct {
    char* label1; // Label of first point
    char* label2; // Label of second point
} Line;

typedef struct {
    char* label;
    Point point;
    bool used;
} HashEntry;

typedef struct {
    HashEntry* entries;
    int size;
} HashTable;

// --- Constants ---
#define MAX_DRAW_ELEMENTS 500
#define HASH_TABLE_SIZE 1000
int SCREEN_WIDTH = 800;
int SCREEN_HEIGHT = 600;
const SDL_Color COLOR_BLACK = {0, 0, 0, 255};
const SDL_Color COLOR_RED = {255, 0, 0, 255}; // Added for visible lines
const SDL_Color COLOR_WHITE_BG = {255, 255, 255, 255};
const int DRAW_LINE_THICKNESS = 10; // Increased for visibility
const int DRAW_POINT_RADIUS = 4;
const int FONT_SIZE = 12;

// --- Function Prototypes ---
bool save_screenshot(SDL_Renderer* renderer, int width, int height, const char* filename);

// --- Hash Table Functions ---
unsigned int hash(const char* str) {
    unsigned int h = 0;
    for (int i = 0; str[i]; i++) {
        h = 31 * h + str[i];
    }
    return h % HASH_TABLE_SIZE;
}

HashTable* create_hash_table() {
    HashTable* table = malloc(sizeof(HashTable));
    table->size = HASH_TABLE_SIZE;
    table->entries = calloc(HASH_TABLE_SIZE, sizeof(HashEntry));
    return table;
}

void hash_table_insert(HashTable* table, const char* label, Point point) {
    unsigned int index = hash(label);
    while (table->entries[index].used) {
        index = (index + 1) % HASH_TABLE_SIZE;
    }
    table->entries[index].used = true;
    table->entries[index].label = strdup(label);
    table->entries[index].point = point;
}

Point* hash_table_get(HashTable* table, const char* label) {
    unsigned int index = hash(label);
    int attempts = 0;
    while (table->entries[index].used && attempts < HASH_TABLE_SIZE) {
        if (strcmp(table->entries[index].label, label) == 0) {
            return &table->entries[index].point;
        }
        index = (index + 1) % HASH_TABLE_SIZE;
        attempts++;
    }
    return NULL;
}

void free_hash_table(HashTable* table) {
    for (int i = 0; i < table->size; i++) {
        if (table->entries[i].used) {
            free(table->entries[i].label);
            free(table->entries[i].point.label);
        }
    }
    free(table->entries);
    free(table);
}

// --- Drawing Functions ---
void set_draw_color(SDL_Renderer* renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void draw_filled_circle(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color) {
    set_draw_color(renderer, color);
    for (int y = -radius; y <= radius; y++) {
        float x_squared = radius * radius - y * y;
        if (x_squared < 0) x_squared = 0;
        int x_span = sqrt(x_squared);
        SDL_RenderDrawLine(renderer, cx - x_span, cy + y, cx + x_span, cy + y);
    }
}

void draw_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    if (!font || !text || text[0] == '\0') return;
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, text, color);
    if (!textSurface) {
        fprintf(stderr, "Unable to render text surface! TTF_Error: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (textTexture) {
        int textW, textH;
        SDL_QueryTexture(textTexture, NULL, NULL, &textW, &textH);
        SDL_Rect backgroundRect = {x, y, textW, textH};
        set_draw_color(renderer, COLOR_WHITE_BG);
        SDL_RenderFillRect(renderer, &backgroundRect);
        SDL_Rect renderQuad = {x, y, textW, textH};
        SDL_RenderCopy(renderer, textTexture, NULL, &renderQuad);
        SDL_DestroyTexture(textTexture);
    }
    SDL_FreeSurface(textSurface);
}

void draw_point_with_label(SDL_Renderer* renderer, Point point, int radius, SDL_Color color, TTF_Font* font) {
    draw_filled_circle(renderer, point.x, point.y, radius, color);
    if (point.label) {
        int label_x_offset = radius + 5;
        int label_y_offset = -radius;
        draw_text(renderer, font, point.label, point.x + label_x_offset, point.y + label_y_offset, color);
    }
}

void draw_thick_line(SDL_Renderer* renderer, Line line, int thickness, SDL_Color color, HashTable* point_table) {
    Point* p1 = hash_table_get(point_table, line.label1);
    Point* p2 = hash_table_get(point_table, line.label2);
    if (!p1 || !p2) {
        fprintf(stderr, "Warning: Line references undefined points: %s, %s\n", line.label1, line.label2);
        return;
    }

    // Removed repeated printf to avoid console flooding
    // Use single line rendering for testing visibility
    set_draw_color(renderer, color);
    int x1 = p1->x, y1 = p1->y;
    int x2 = p2->x, y2 = p2->y;
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2); // Simplified to single line

    /* Original thick line code (commented out for testing)
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrt(dx*dx + dy*dy);

    if (length == 0) {
        for (int i = -thickness/2; i <= thickness/2; ++i) {
            for (int j = -thickness/2; j <= thickness/2; ++j) {
                SDL_RenderDrawPoint(renderer, x1 + i, y1 + j);
            }
        }
        return;
    }

    float nx = -dy / length;
    float ny = dx / length;
    for (int i = -thickness / 2; i <= thickness / 2; ++i) {
        SDL_RenderDrawLine(renderer, x1 + (int)(i * nx), y1 + (int)(i * ny), x2 + (int)(i * nx), y2 + (int)(i * ny));
    }
    */
}

// --- Parse Function ---
bool parse_drawing_file(const char* filepath, Point* points, int* point_count, Line* lines, int* line_count, int max_elements, HashTable* point_table) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        fprintf(stderr, "Warning: Could not open drawing file %s. Proceeding without drawing data.\n", filepath);
        return false;
    }

    char line_buffer[512];
    *point_count = 0;
    *line_count = 0;

    // First pass: collect points
    while (fgets(line_buffer, sizeof(line_buffer), file)) {
        line_buffer[strcspn(line_buffer, "\n")] = 0;
        if (line_buffer[0] == '#' || line_buffer[0] == '\0') continue;

        char* point_call_start = strstr(line_buffer, "point(");
        if (point_call_start) {
            char* param_start = point_call_start + strlen("point(");
            char* param_end = strstr(param_start, ")");
            if (!param_end) continue;

            *param_end = '\0';
            char* current_pos = param_start;
            char* first_comma = strchr(current_pos, ',');
            if (!first_comma) continue;

            *first_comma = '\0';
            int x;
            if (sscanf(current_pos, "%d", &x) != 1) continue;
            current_pos = first_comma + 1;

            char* second_comma = strchr(current_pos, ',');
            if (!second_comma) continue;

            *second_comma = '\0';
            int y;
            if (sscanf(current_pos, "%d", &y) != 1) continue;
            current_pos = second_comma + 1;

            char* label_content = current_pos;
            if (*label_content == '\0') {
                fprintf(stderr, "Error: Point missing required label: %s\n", line_buffer);
                continue;
            }

            char* label_end = label_content + strlen(label_content) - 1;
            while (label_end > label_content && isspace(*label_end)) {
                *label_end = '\0';
                label_end--;
            }

            if (*point_count < max_elements) {
                points[*point_count].x = x;
                points[*point_count].y = y;
                points[*point_count].label = strdup(label_content);
                hash_table_insert(point_table, label_content, points[*point_count]);
                (*point_count)++;
                printf("Parsed Point: (%d, %d, %s)\n", x, y, label_content);
            } else {
                fprintf(stderr, "Warning: Max points (%d) reached. Skipping point: %s\n", max_elements, line_buffer);
            }
        }
    }

    // Rewind file for second pass: collect lines
    rewind(file);
    while (fgets(line_buffer, sizeof(line_buffer), file)) {
        line_buffer[strcspn(line_buffer, "\n")] = 0;
        if (line_buffer[0] == '#' || line_buffer[0] == '\0') continue;

        char* line_call_start = strstr(line_buffer, "line(");
        if (line_call_start) {
            char* param_start = line_call_start + strlen("line(");
            char* param_end = strstr(param_start, ")");
            if (!param_end) continue;

            *param_end = '\0';
            char* current_pos = param_start;
            char* comma = strchr(current_pos, ',');
            if (!comma) continue;

            *comma = '\0';
            char* label1 = current_pos;
            while (isspace(*label1)) label1++;
            char* label1_end = label1 + strlen(label1) - 1;
            while (label1_end > label1 && isspace(*label1_end)) {
                *label1_end = '\0';
                label1_end--;
            }

            current_pos = comma + 1;
            char* label2 = current_pos;
            while (isspace(*label2)) label2++;
            char* label2_end = label2 + strlen(label2) - 1;
            while (label2_end > label2 && isspace(*label2_end)) {
                *label2_end = '\0';
                label2_end--;
            }

            if (*label1 == '\0' || *label2 == '\0') {
                fprintf(stderr, "Error: Line missing valid labels: %s\n", line_buffer);
                continue;
            }

            if (*line_count < max_elements) {
                lines[*line_count].label1 = strdup(label1);
                lines[*line_count].label2 = strdup(label2);
                if (!hash_table_get(point_table, label1) || !hash_table_get(point_table, label2)) {
                    fprintf(stderr, "Warning: Line references undefined points: %s, %s\n", label1, label2);
                } else {
                    (*line_count)++;
                    printf("Parsed Line: %s to %s\n", label1, label2);
                }
            } else {
                fprintf(stderr, "Warning: Max lines (%d) reached. Skipping line: %s\n", max_elements, line_buffer);
            }
        }
    }

    fclose(file);
    printf("Finished parsing. Loaded %d points and %d lines.\n", *point_count, *line_count);
    return true;
}

// --- Free Function ---
void free_loaded_drawing_data(Point* points, int point_count, Line* lines, int line_count, HashTable* point_table) {
    for (int i = 0; i < point_count; ++i) {
        points[i].label = NULL; // Freed via hash table
    }
    for (int i = 0; i < line_count; ++i) {
        free(lines[i].label1);
        free(lines[i].label2);
    }
    free_hash_table(point_table);
}

// --- Save Screenshot Function ---
bool save_screenshot(SDL_Renderer* renderer, int width, int height, const char* filename) {
    SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!surface) {
        fprintf(stderr, "Failed to create surface for screenshot: %s\n", SDL_GetError());
        return false;
    }
    if (SDL_RenderReadPixels(renderer, NULL, surface->format->format, surface->pixels, surface->pitch) != 0) {
        fprintf(stderr, "Failed to read pixels from renderer for screenshot: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return false;
    }
    if (IMG_SavePNG(surface, filename) != 0) {
        fprintf(stderr, "Failed to save surface as PNG: %s\n", IMG_GetError());
        SDL_FreeSurface(surface);
        return false;
    }
    SDL_FreeSurface(surface);
    printf("Screenshot saved successfully to %s.\n", filename);
    return true;
}

// --- Main Function ---
int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <image_file_path> [drawing_file.vd]\n", argv[0]);
        return 1;
    }
    const char* image_path = argv[1];
    const char* drawing_file_path = argc > 2 ? argv[2] : NULL;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    int img_flags = IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_WEBP;
    if (!(IMG_Init(img_flags) & img_flags)) {
        fprintf(stderr, "SDL_image could not initialize! IMG_Error: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }
    if (TTF_Init() == -1) {
        fprintf(stderr, "SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Surface* loaded_surface = IMG_Load(image_path);
    if (!loaded_surface) {
        fprintf(stderr, "Failed to load image %s! IMG_Error: %s\n", image_path, IMG_GetError());
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }
    SCREEN_WIDTH = loaded_surface->w;
    SCREEN_HEIGHT = loaded_surface->h;

    SDL_Window* window = SDL_CreateWindow("Image Viewer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_FreeSurface(loaded_surface);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_FreeSurface(loaded_surface);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Texture* image_texture = SDL_CreateTextureFromSurface(renderer, loaded_surface);
    if (!image_texture) {
        fprintf(stderr, "Failed to create texture from surface! SDL Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_FreeSurface(loaded_surface);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_FreeSurface(loaded_surface);

    const char* font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    TTF_Font* gFont = TTF_OpenFont(font_path, FONT_SIZE);
    if (!gFont) {
        fprintf(stderr, "Failed to load font %s! TTF_Error: %s\n", font_path, TTF_GetError());
    }

    Point loaded_points[MAX_DRAW_ELEMENTS];
    int loaded_point_count = 0;
    Line loaded_lines[MAX_DRAW_ELEMENTS];
    int loaded_line_count = 0;
    HashTable* point_table = create_hash_table();

    if (drawing_file_path) {
        parse_drawing_file(drawing_file_path, loaded_points, &loaded_point_count, loaded_lines, &loaded_line_count, MAX_DRAW_ELEMENTS, point_table);
    }

    bool quit = false;
    SDL_Event e;
    bool debug_printed = false; // To print line drawing info once
    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_MOUSEMOTION) {
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);
                char title[100];
                snprintf(title, 100, "Image Viewer - Cursor: (%d, %d)", mouseX, mouseY);
                SDL_SetWindowTitle(window, title);
            } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    printf("Clicked at: (%d, %d)\n", e.button.x, e.button.y);
                }
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_q:
                        quit = true;
                        break;
                    case SDLK_s:
                        if (save_screenshot(renderer, SCREEN_WIDTH, SCREEN_HEIGHT, "image_with_drawing.png")) {
                            printf("Screenshot saved successfully.\n");
                        }
                        break;
                    case SDLK_d: // Press 'd' to print debug info
                        debug_printed = false; // Allow reprinting
                        break;
                }
            }
        }

        set_draw_color(renderer, COLOR_WHITE_BG);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, image_texture, NULL, NULL);

        for (int i = 0; i < loaded_line_count; ++i) {
            draw_thick_line(renderer, loaded_lines[i], DRAW_LINE_THICKNESS, COLOR_RED, point_table);
            // Print debug info only once or when 'd' is pressed
            if (!debug_printed) {
                Point* p1 = hash_table_get(point_table, loaded_lines[i].label1);
                Point* p2 = hash_table_get(point_table, loaded_lines[i].label2);
                if (p1 && p2) {
                    printf("Drawing line from %s (%d,%d) to %s (%d,%d)\n",
                           loaded_lines[i].label1, p1->x, p1->y,
                           loaded_lines[i].label2, p2->x, p2->y);
                }
            }
        }
        debug_printed = true; // Prevent repeated printing

        for (int i = 0; i < loaded_point_count; ++i) {
            draw_point_with_label(renderer, loaded_points[i], DRAW_POINT_RADIUS, COLOR_BLACK, gFont);
        }

        SDL_RenderPresent(renderer);
    }

    free_loaded_drawing_data(loaded_points, loaded_point_count, loaded_lines, loaded_line_count, point_table);
    if (gFont) TTF_CloseFont(gFont);
    SDL_DestroyTexture(image_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}
